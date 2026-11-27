/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2016 - 2024 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2016 Jan Bajer aka bajasoft <jbajer@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "QtWebEngineUrlRequestInterceptor.h"
#include "../../../../core/Console.h"
#include "../../../../core/ContentFiltersManager.h"
#include "../../../../core/SettingsManager.h"
#include "../../../../core/Utils.h"
#include "../../../../core/WebBackend.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>

namespace Otter
{

WebBackend* QtWebEngineUrlRequestInterceptor::m_backend(nullptr);
QSet<QString> QtWebEngineUrlRequestInterceptor::m_observedAdHosts;
QMutex QtWebEngineUrlRequestInterceptor::m_observedAdHostsMutex;

QtWebEngineUrlRequestInterceptor::QtWebEngineUrlRequestInterceptor(QtWebEngineWebWidget *parent) : QWebEngineUrlRequestInterceptor(parent),
	m_widget(parent),
	m_doNotTrackPolicy(NetworkManagerFactory::SkipTrackPolicy),
	m_startedRequestsAmount(0),
	m_areImagesEnabled(true),
	m_canSendReferrer(true),
	m_isWorkingOffline(false),
	m_isFlushScheduled(false)
{
}

void QtWebEngineUrlRequestInterceptor::setImageReloadUrl(const QUrl &url)
{
	QMutexLocker locker(&m_imageReloadMutex);

	m_imageReloadUrl = url;
}

void QtWebEngineUrlRequestInterceptor::clearImageReloadUrl()
{
	QMutexLocker locker(&m_imageReloadMutex);

	m_imageReloadUrl.clear();
}

void QtWebEngineUrlRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &request)
{
	const bool isImageRequest(request.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeImage);
	bool isImageReloadRequest(false);

	if (isImageRequest)
	{
		QMutexLocker locker(&m_imageReloadMutex);

		if (m_imageReloadUrl.isValid())
		{
			isImageReloadRequest = (request.requestUrl().adjusted(QUrl::RemoveFragment) == m_imageReloadUrl.adjusted(QUrl::RemoveFragment));
		}
	}

	if (m_isWorkingOffline || (!m_areImagesEnabled && isImageRequest && !isImageReloadRequest))
	{
		if (m_isWorkingOffline && request.resourceType() != QWebEngineUrlRequestInfo::ResourceTypeImage)
		{
			qWarning() << "OTTER-NET request denied: offline mode" << request.requestUrl().host() << "resource type" << request.resourceType();
		}

		request.block(true);

		return;
	}

	if (!isImageReloadRequest && !m_contentBlockingProfiles.isEmpty() && (m_unblockedHosts.isEmpty() || !m_unblockedHosts.contains(Utils::extractHost(request.firstPartyUrl()))))
	{
		NetworkManager::ResourceType resourceType(NetworkManager::OtherType);
		bool storeBlockedUrl(true);

		switch (request.resourceType())
		{
			case QWebEngineUrlRequestInfo::ResourceTypeMainFrame:
				resourceType = NetworkManager::MainFrameType;

				break;
			case QWebEngineUrlRequestInfo::ResourceTypeSubFrame:
				resourceType = NetworkManager::SubFrameType;

				break;
			case QWebEngineUrlRequestInfo::ResourceTypeStylesheet:
				resourceType = NetworkManager::StyleSheetType;
				storeBlockedUrl = false;

				break;
			case QWebEngineUrlRequestInfo::ResourceTypeScript:
				resourceType = NetworkManager::ScriptType;
				storeBlockedUrl = false;

				break;
			case QWebEngineUrlRequestInfo::ResourceTypeImage:
				resourceType = NetworkManager::ImageType;

				break;
			case QWebEngineUrlRequestInfo::ResourceTypeObject:
				resourceType = NetworkManager::ObjectType;

				break;
			case QWebEngineUrlRequestInfo::ResourceTypeMedia:
				// Media is not a plugin/object resource. Treating it as ObjectType
				// makes ordinary adblock "object" rules block HTML5 video/audio.
				resourceType = NetworkManager::OtherType;

				break;
			case QWebEngineUrlRequestInfo::ResourceTypePluginResource:
				resourceType = NetworkManager::ObjectSubrequestType;
				storeBlockedUrl = false;

				break;
			case QWebEngineUrlRequestInfo::ResourceTypeXhr:
				resourceType = NetworkManager::XmlHttpRequestType;

				break;
			default:
				break;
		}

		const ContentFiltersManager::CheckResult result(ContentFiltersManager::checkUrl(m_contentBlockingProfiles, request.firstPartyUrl(), request.requestUrl(), resourceType));

		if (result.isBlocked)
		{
			qWarning() << "OTTER-NET request denied: content filter" << request.requestUrl().host()
				<< "resource type" << request.resourceType() << "rule" << result.rule;

			const ContentFiltersProfile *profile(ContentFiltersManager::getProfile(result.profile));

			Console::addMessage(QCoreApplication::translate("main", "Request blocked by rule from profile %1:\n%2").arg(profile ? profile->getTitle() : QCoreApplication::translate("main", "(Unknown)"), result.rule), Console::NetworkCategory, Console::LogLevel, request.requestUrl().toString(), -1);

			const QString blockedHost(Utils::extractHost(request.requestUrl()));

			if (!blockedHost.isEmpty() && blockedHost != Utils::extractHost(request.firstPartyUrl()))
			{
				QMutexLocker locker(&m_observedAdHostsMutex);

				m_observedAdHosts.insert(blockedHost);
			}

			if (storeBlockedUrl && !m_blockedElements.contains(request.requestUrl().url()))
			{
				m_blockedElements.append(request.requestUrl().url());
			}

			NetworkManager::ResourceInformation resource;
			resource.url = request.requestUrl();
			resource.resourceType = resourceType;
			resource.metaData[NetworkManager::ContentBlockingProfileMetaData] = result.profile;
			resource.metaData[NetworkManager::ContentBlockingRuleMetaData] = result.rule;

			m_blockedRequests.append(resource);

			emit pageInformationChanged(WebWidget::RequestsBlockedInformation, m_blockedRequests.count());
			emit requestBlocked(resource);

			request.block(true);

			return;
		}

		learnAdRequest(request.firstPartyUrl(), request.requestUrl(), resourceType);
	}

	++m_startedRequestsAmount;

	request.setHttpHeader(QByteArrayLiteral("Accept-Language"), (m_acceptLanguage.isEmpty() ? NetworkManagerFactory::getAcceptLanguage().toLatin1() : m_acceptLanguage.toLatin1()));
	request.setHttpHeader(QByteArrayLiteral("User-Agent"), m_userAgent.toUtf8());

	if (m_doNotTrackPolicy != NetworkManagerFactory::SkipTrackPolicy)
	{
		request.setHttpHeader(QByteArrayLiteral("DNT"), ((m_doNotTrackPolicy == NetworkManagerFactory::DoNotAllowToTrackPolicy) ? QByteArrayLiteral("1") : QByteArrayLiteral("0")));
	}

	if (!m_canSendReferrer)
	{
		request.setHttpHeader(QByteArrayLiteral("Referer"), {});
	}

	emit pageInformationChanged(WebWidget::RequestsStartedInformation, m_startedRequestsAmount);
}

void QtWebEngineUrlRequestInterceptor::learnAdRequest(const QUrl &baseUrl, const QUrl &requestUrl, NetworkManager::ResourceType resourceType)
{
	if (!getOption(SettingsManager::ContentBlocking_LearnAdRulesOption, baseUrl).toBool())
	{
		return;
	}

	if (resourceType != NetworkManager::ScriptType && resourceType != NetworkManager::ImageType && resourceType != NetworkManager::ObjectType && resourceType != NetworkManager::SubFrameType && resourceType != NetworkManager::XmlHttpRequestType)
	{
		return;
	}

	const QString baseHost(Utils::extractHost(baseUrl));
	const QString requestHost(Utils::extractHost(requestUrl));

	if (baseHost.isEmpty() || requestHost.isEmpty() || baseHost == requestHost || baseHost == QLatin1String("localhost") || requestHost == QLatin1String("localhost"))
	{
		return;
	}

	const QString relatedBase(QLatin1Char('.') + baseHost);
	const QString relatedRequest(QLatin1Char('.') + requestHost);

	if (requestHost.endsWith(relatedBase) || baseHost.endsWith(relatedRequest))
	{
		return;
	}

	bool isAdLike(false);

	{
		QMutexLocker locker(&m_observedAdHostsMutex);

		isAdLike = m_observedAdHosts.contains(requestHost);
	}

	const QString target(requestUrl.url().toLower());
	static const QStringList indicators({QLatin1String("doubleclick"), QLatin1String("googlesyndication"), QLatin1String("googleadservices"), QLatin1String("amazon-adsystem"), QLatin1String("adnxs"), QLatin1String("adservice"), QLatin1String("criteo"), QLatin1String("taboola"), QLatin1String("outbrain"), QLatin1String("pubmatic"), QLatin1String("rubiconproject"), QLatin1String("scorecardresearch"), QLatin1String("quantserve"), QLatin1String("chartbeat"), QLatin1String("/ads/"), QLatin1String("/ad/"), QLatin1String("advertisement"), QLatin1String("banner"), QLatin1String("track"), QLatin1String("analytics"), QLatin1String("pixel")});

	if (!isAdLike)
	{
		for (const QString &indicator: indicators)
		{
			if (target.contains(indicator))
			{
				isAdLike = true;

				break;
			}
		}
	}

	if (!isAdLike)
	{
		return;
	}

	const QString rule(requestHost + QLatin1String("$third-party,domain=") + baseHost);

	{
		QMutexLocker locker(&m_learnedRulesMutex);

		if (m_pendingLearnedRules.contains(rule))
		{
			return;
		}

		m_pendingLearnedRules.insert(rule);

		if (m_isFlushScheduled)
		{
			return;
		}

		m_isFlushScheduled = true;
	}

	QMetaObject::invokeMethod(this, [this]()
	{
		flushLearnedRules();
	}, Qt::QueuedConnection);
}

void QtWebEngineUrlRequestInterceptor::flushLearnedRules()
{
	const QStringList rules([&]()
	{
		QMutexLocker locker(&m_learnedRulesMutex);
		const QStringList pendingRules(m_pendingLearnedRules.values());

		m_pendingLearnedRules.clear();
		m_isFlushScheduled = false;

		return pendingRules;
	}());

	if (!rules.isEmpty())
	{
		const QUrl pageUrl(m_widget ? m_widget->getUrl() : QUrl());
		const bool needsProfileRefresh(!getOption(SettingsManager::ContentBlocking_ProfilesOption, pageUrl).toStringList().contains(QLatin1String("user-rules")));

		ContentFiltersManager::addUserRules(rules);

		if (needsProfileRefresh && m_widget)
		{
			updateOptions(pageUrl);
		}
	}
}

void QtWebEngineUrlRequestInterceptor::resetStatistics()
{
	m_blockedRequests.clear();
	m_blockedElements.clear();
	m_startedRequestsAmount = 0;
}

void QtWebEngineUrlRequestInterceptor::updateOptions(const QUrl &url)
{
	if (!m_backend)
	{
		m_backend = AddonsManager::getWebBackend(QLatin1String("qtwebengine"));
	}

	if (getOption(SettingsManager::ContentBlocking_EnableContentBlockingOption, url).toBool())
	{
		const QStringList profileNames(getOption(SettingsManager::ContentBlocking_ProfilesOption, url).toStringList());
		m_contentBlockingProfiles = ContentFiltersManager::getProfileIdentifiers(profileNames);
		qDebug() << "OTTER-DEBUG content profiles" << profileNames << m_contentBlockingProfiles;
	}
	else
	{
		m_contentBlockingProfiles.clear();
	}

	QString acceptLanguage(getOption(SettingsManager::Network_AcceptLanguageOption, url).toString());
	acceptLanguage = ((acceptLanguage.isEmpty()) ? QLatin1String(" ") : acceptLanguage.replace(QLatin1String("system"), QLocale::system().bcp47Name()));

	m_acceptLanguage = ((acceptLanguage == NetworkManagerFactory::getAcceptLanguage()) ? QString() : acceptLanguage);
	m_userAgent = m_backend->getUserAgent(NetworkManagerFactory::getUserAgent(getOption(SettingsManager::Network_UserAgentOption, url).toString()).value);
	m_unblockedHosts = getOption(SettingsManager::ContentBlocking_IgnoreHostsOption, url).toStringList();

	const QString doNotTrackPolicyValue(getOption(SettingsManager::Network_DoNotTrackPolicyOption, url).toString());

	if (doNotTrackPolicyValue == QLatin1String("allow"))
	{
		m_doNotTrackPolicy = NetworkManagerFactory::AllowToTrackPolicy;
	}
	else if (doNotTrackPolicyValue == QLatin1String("doNotAllow"))
	{
		m_doNotTrackPolicy = NetworkManagerFactory::DoNotAllowToTrackPolicy;
	}
	else
	{
		m_doNotTrackPolicy = NetworkManagerFactory::SkipTrackPolicy;
	}

	m_areImagesEnabled = (getOption(SettingsManager::Permissions_EnableImagesOption, url).toString() == QLatin1String("enabled"));
	m_canSendReferrer = getOption(SettingsManager::Network_EnableReferrerOption, url).toBool();
	m_isWorkingOffline = getOption(SettingsManager::Network_WorkOfflineOption, url).toBool();
}

QVariant QtWebEngineUrlRequestInterceptor::getOption(int identifier, const QUrl &url) const
{
	return (m_widget ? m_widget->getOption(identifier, url) : SettingsManager::getOption(identifier, Utils::extractHost(url)));
}

QVariant QtWebEngineUrlRequestInterceptor::getPageInformation(WebWidget::PageInformation key) const
{
	switch (key)
	{
		case WebWidget::RequestsBlockedInformation:
			return m_blockedRequests.count();

		case WebWidget::RequestsStartedInformation:
			return m_startedRequestsAmount;
		default:
			break;
	}

	return {};
}

QStringList QtWebEngineUrlRequestInterceptor::getBlockedElements() const
{
	return m_blockedElements;
}

QVector<NetworkManager::ResourceInformation> QtWebEngineUrlRequestInterceptor::getBlockedRequests() const
{
	return m_blockedRequests;
}

}
