/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2015 - 2025 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2016 - 2017 Jan Bajer aka bajasoft <jbajer@gmail.com>
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

#include "QtWebEnginePage.h"
#include "QtWebEngineWebBackend.h"
#include "QtWebEngineWebWidget.h"
#include "../../../../core/FormFillerManager.h"
#include "../../../../core/Console.h"
#include "../../../../core/ContentFiltersManager.h"
#include "../../../../core/HandlersManager.h"
#include "../../../../core/HistoryManager.h"
#include "../../../../core/SettingsManager.h"
#include "../../../../core/ThemesManager.h"
#include "../../../../core/UserScript.h"
#include "../../../../core/Utils.h"
#include "../../../../ui/ContentsDialog.h"
#include "../../../../ui/LineEditWidget.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUuid>
#include <QtNetwork/QSslSocket>
#if defined(USE_QT5)
#include <QtWebEngineWidgets/QWebEngineHistory>
#include <QtWebEngineWidgets/QWebEngineProfile>
#include <QtWebEngineWidgets/QWebEngineScript>
#include <QtWebEngineWidgets/QWebEngineScriptCollection>
#include <QtWebEngineWidgets/QWebEngineSettings>
#else
#include <QtWebEngineCore/QWebEngineHistory>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineScript>
#include <QtWebEngineCore/QWebEngineScriptCollection>
#include <QtWebEngineCore/QWebEngineSettings>
#endif
#include <QtWidgets/QMessageBox>
#include <QtCore/QStringListModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QCompleter>
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QtWebEngineCore/QWebEngineFrame>
#endif

namespace Otter
{

namespace
{

bool hasFormFrameUrl(QWebEnginePage *page, const QUrl &url)
{
	if (url.host().isEmpty() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")))
	{
		return false;
	}
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	QList<QWebEngineFrame> frames({page->mainFrame()});
	while (!frames.isEmpty())
	{
		const QWebEngineFrame frame(frames.takeLast());
		if (frame.url() == url)
		{
			return true;
		}
		frames.append(frame.children());
	}
	return false;
#else
	return page->url() == url;
#endif
}

}

QtWebEnginePage::QtWebEnginePage(bool isPrivate, QtWebEngineWebWidget *parent) : QtWebEnginePage((isPrivate ? new QWebEngineProfile(parent) : qobject_cast<QtWebEngineWebBackend*>(parent->getBackend())->getDefaultProfile()), parent)
{
	if (isPrivate && m_widget)
	{
		connect(profile(), &QWebEngineProfile::downloadRequested, qobject_cast<QtWebEngineWebBackend*>(m_widget->getBackend()), &QtWebEngineWebBackend::handleDownloadRequested);
	}
}

QtWebEnginePage::QtWebEnginePage(QWebEngineProfile *profile, QtWebEngineWebWidget *parent) : QWebEnginePage(profile, parent),
	m_widget(parent),
	m_formCompleter(nullptr),
	m_previousNavigationType(QtWebEnginePage::NavigationTypeOther),
	m_editingStateMessageToken(QUuid::createUuid().toString(QUuid::WithoutBraces)),
	m_passwordMessageToken(QUuid::createUuid().toString(QUuid::WithoutBraces)),
	m_formDataMessageToken(QUuid::createUuid().toString(QUuid::WithoutBraces)),
	m_isIgnoringJavaScriptPopups(false),
	m_isViewingMedia(false),
	m_isPopup(false)
{
	connect(this, &QtWebEnginePage::loadFinished, this, &QtWebEnginePage::handleLoadFinished);

	if (!profile->isOffTheRecord())
	{
		QFile file(QLatin1String(":/modules/backends/web/qtwebengine/resources/formData.js"));
		if (file.open(QIODevice::ReadOnly))
		{
			QWebEngineScript script;
			script.setName(QLatin1String("otter-form-data"));
			script.setSourceCode(QString::fromUtf8(file.readAll()).arg(m_formDataMessageToken));
			script.setInjectionPoint(QWebEngineScript::DocumentReady);
			script.setWorldId(QWebEngineScript::UserWorld);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
			script.setRunsOnSubFrames(true);
#else
			script.setRunsOnSubFrames(false);
#endif
			scripts().insert(script);
		}
	}

	connect(this, &QtWebEnginePage::loadStarted, this, [this]()
	{
		if (m_formCompleter)
		{
			m_formCompleter->popup()->hide();
			disconnect(m_formCompleter, nullptr, this, nullptr);
		}
	});

#if !defined(USE_QT5)
	connect(this, &QWebEnginePage::certificateError, this, [this](QWebEngineCertificateError error)
	{
		if (shouldAcceptCertificate(error))
		{
			error.acceptCertificate();
		}
		else
		{
			error.rejectCertificate();
		}
	});
#endif
}

void QtWebEnginePage::markAsPopup()
{
	m_isPopup = true;
}

void QtWebEnginePage::javaScriptAlert(const QUrl &url, const QString &message)
{
	if (m_isIgnoringJavaScriptPopups)
	{
		return;
	}

	if (!m_widget || !m_widget->parentWidget())
	{
		QWebEnginePage::javaScriptAlert(url, message);

		return;
	}

	emit m_widget->needsAttention();

	ContentsDialog dialog(ThemesManager::createIcon(QLatin1String("dialog-information")), tr("JavaScript"), message, {}, QDialogButtonBox::Ok, nullptr, m_widget);
	dialog.setCheckBox(tr("Disable JavaScript popups"), false);

	connect(m_widget, &QtWebEngineWebWidget::aboutToReload, &dialog, &ContentsDialog::close);

	m_widget->showDialog(&dialog);

	if (dialog.getCheckBoxState())
	{
		m_isIgnoringJavaScriptPopups = true;
	}
}

void QtWebEnginePage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &note, int line, const QString &source)
{
	const bool debugForms(qEnvironmentVariableIsSet("OTTER_FORM_DEBUG"));
	const QString formDataMessagePrefix(QLatin1String("otter-form-"));
	const QString formDataToken(QLatin1Char(':') + m_formDataMessageToken + QLatin1Char(':'));

	if (note.startsWith(formDataMessagePrefix + QLatin1String("ready") + formDataToken))
	{
		if (debugForms) qInfo() << "Form filler: capture script ready";
		return;
	}

	if (note.startsWith(formDataMessagePrefix + QLatin1String("save") + formDataToken))
	{
		if (m_widget && !m_widget->isPrivate())
		{
			const QByteArray encoded(note.mid((formDataMessagePrefix + QLatin1String("save") + formDataToken).length()).toLatin1());
			QJsonParseError parseError;
			const QJsonDocument document(QJsonDocument::fromJson(QByteArray::fromBase64(encoded), &parseError));
			const QUrl pageUrl(document.object().value(QLatin1String("url")).toString());

			if (parseError.error == QJsonParseError::NoError && hasFormFrameUrl(this, pageUrl))
			{
				for (const QJsonValue &fieldValue: document.object().value(QLatin1String("fields")).toArray())
				{
					const QJsonObject field(fieldValue.toObject());
					FormFillerManager::addValue(pageUrl, field.value(QLatin1String("name")).toString(), field.value(QLatin1String("autocomplete")).toString(), field.value(QLatin1String("value")).toString());
				}
			}
		}

		return;
	}

	if (note.startsWith(formDataMessagePrefix + QLatin1String("suggest") + formDataToken))
	{
		if (m_widget && !m_widget->isPrivate())
		{
			const QByteArray encoded(note.mid((formDataMessagePrefix + QLatin1String("suggest") + formDataToken).length()).toLatin1());
			QJsonParseError parseError;
			const QJsonDocument document(QJsonDocument::fromJson(QByteArray::fromBase64(encoded), &parseError));
			const QJsonObject field(document.object());
			const QUrl frameUrl(field.value(QLatin1String("url")).toString());
			if (debugForms) qInfo() << "Form filler: suggestion received; frame accepted:" << hasFormFrameUrl(this, frameUrl);
			const QString fieldName(field.value(QLatin1String("name")).toString());
			const QString autocomplete(field.value(QLatin1String("autocomplete")).toString());
			const QString prefix(field.value(QLatin1String("value")).toString());
			const QRect fieldRectangle(qRound(field.value(QLatin1String("x")).toDouble()), qRound(field.value(QLatin1String("y")).toDouble()), qRound(field.value(QLatin1String("width")).toDouble()), qRound(field.value(QLatin1String("height")).toDouble()));

			if (parseError.error == QJsonParseError::NoError && hasFormFrameUrl(this, frameUrl) && !fieldName.isEmpty() && fieldName.size() <= 256 && prefix.size() < 256 && fieldRectangle.isValid() && fieldRectangle.width() < 2000 && fieldRectangle.height() < 500)
			{
				QStringList values;
				for (const QString &value: FormFillerManager::getValues(frameUrl, fieldName, autocomplete))
				{
					if (value.startsWith(prefix, Qt::CaseInsensitive) && value.compare(prefix, Qt::CaseInsensitive) != 0)
					{
						values.append(value);
					}
				}

				if (m_formCompleter)
				{
					m_formCompleter->popup()->hide();
					disconnect(m_formCompleter, nullptr, this, nullptr);
				}

				QWidget *editor(QApplication::focusWidget());
				if (debugForms) qInfo() << "Form filler: matching values:" << values.size() << "focus present:" << bool(editor) << "focus belongs to page:" << bool(editor && (editor == m_widget || m_widget->isAncestorOf(editor)));
				if (!values.isEmpty() && editor && (editor == m_widget || m_widget->isAncestorOf(editor)))
				{
					if (!m_formCompleter)
					{
						m_formCompleter = new QCompleter(this);
						m_formCompleter->setModel(new QStringListModel(m_formCompleter));
						m_formCompleter->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
					}
					m_formCompleter->setWidget(editor);
					m_formCompleter->setCompletionPrefix(QString());
					static_cast<QStringListModel*>(m_formCompleter->model())->setStringList(values);
					connect(m_formCompleter, QOverload<const QString&>::of(&QCompleter::activated), this, [this, frameUrl, fieldName, autocomplete, prefix](const QString &value)
					{
						QJsonArray arguments;
						arguments.append(fieldName);
						arguments.append(prefix);
						arguments.append(value);
						arguments.append(autocomplete.section(QLatin1Char(' '), -1));
						const QString valuesJson(QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact)));
						const QString script(QStringLiteral("(function(a){const field=document.activeElement;if(!document.hasFocus()||!field||!['INPUT','TEXTAREA','SELECT'].includes(field.tagName)||((field.name||field.id||field.getAttribute('aria-label')||field.getAttribute('placeholder')||a[3]||'').trim()!==a[0])||field.value!==a[1])return;const setter=Object.getOwnPropertyDescriptor(Object.getPrototypeOf(field),'value').set;setter.call(field,a[2]);field.dispatchEvent(new Event('input',{bubbles:true}));field.dispatchEvent(new Event('change',{bubbles:true}));})(%1);").arg(valuesJson));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
						QList<QWebEngineFrame> frames({mainFrame()});
						while (!frames.isEmpty())
						{
							QWebEngineFrame frame(frames.takeLast());
							if (frame.url() == frameUrl)
							{
								frame.runJavaScript(script);
							}
							frames.append(frame.children());
						}
#else
						runJavaScript(script);
#endif
					});
					const QRect popupRectangle(editor->mapFrom(m_widget, fieldRectangle.topLeft()), fieldRectangle.size());
					m_formCompleter->complete(popupRectangle);
				}
			}
		}

		return;
	}

	const QString editingStateMessagePrefix(QLatin1String("otter-editing:") + m_editingStateMessageToken + QLatin1Char(':'));

	if (note.startsWith(editingStateMessagePrefix))
	{
		if (m_widget)
		{
			m_widget->m_isEditing = (note.mid(editingStateMessagePrefix.length()) == QLatin1String("1"));
		}

		return;
	}

	const QString passwordMessagePrefix(QLatin1String("otter-save-password:") + m_passwordMessageToken + QLatin1Char(':'));

	if (note.startsWith(passwordMessagePrefix))
	{
		if (!m_widget || m_widget->isPrivate() || !m_widget->getOption(SettingsManager::Browser_RememberPasswordsOption).toBool())
		{
			return;
		}

		QJsonParseError parseError;
		const QJsonDocument document(QJsonDocument::fromJson(QByteArray::fromBase64(note.mid(passwordMessagePrefix.length()).toLatin1()), &parseError));

		if (parseError.error != QJsonParseError::NoError || !document.isObject())
		{
			return;
		}

		const QJsonObject payload(document.object());
		const QUrl passwordUrl(payload.value(QLatin1String("url")).toString());
		const QString scheme(passwordUrl.scheme().toLower());

		if (!passwordUrl.isValid() || passwordUrl.host().isEmpty() || (scheme != QLatin1String("http") && scheme != QLatin1String("https")))
		{
			return;
		}

		const QJsonArray fieldsArray(payload.value(QLatin1String("fields")).toArray());
		PasswordsManager::Password password;
		password.url = passwordUrl;
		password.timeAdded = QDateTime::currentDateTimeUtc();
		password.type = PasswordsManager::FormPassword;
		bool hasPassword(false);

		for (const QJsonValue &fieldValue: fieldsArray)
		{
			const QJsonObject fieldObject(fieldValue.toObject());
			PasswordsManager::Password::Field field;
			field.name = fieldObject.value(QLatin1String("name")).toString();
			field.value = fieldObject.value(QLatin1String("value")).toString();
			field.type = ((fieldObject.value(QLatin1String("type")).toString() == QLatin1String("password")) ? PasswordsManager::PasswordField : PasswordsManager::TextField);

			if (field.name.isEmpty())
			{
				continue;
			}

			hasPassword = hasPassword || (field.type == PasswordsManager::PasswordField && !field.value.isEmpty());
			password.fields.append(field);
		}

		if (hasPassword)
		{
			const PasswordsManager::PasswordMatch match(PasswordsManager::hasPassword(password));

			if (match != PasswordsManager::FullMatch)
			{
				m_widget->notifySavePasswordRequested(password, (match == PasswordsManager::PartialMatch));
			}
		}

		return;
	}

	Console::MessageLevel mappedLevel(Console::LogLevel);

	if (level == WarningMessageLevel)
	{
		mappedLevel = Console::WarningLevel;
	}
	else if (level == ErrorMessageLevel)
	{
		mappedLevel = Console::ErrorLevel;
	}

	Console::addMessage(note, Console::JavaScriptCategory, mappedLevel, source, line, (m_widget ? m_widget->getWindowIdentifier() : 0));
}

void QtWebEnginePage::probeSslInformation(const QUrl &url)
{
	QSslSocket *socket(new QSslSocket(this));
	socket->setPeerVerifyMode(QSslSocket::VerifyNone);

	connect(socket, &QSslSocket::encrypted, this, [this, socket, url]()
	{
		if (this->url() == url)
		{
			m_sslInformation.certificates = socket->peerCertificateChain().toVector();
			m_sslInformation.cipher = socket->sessionCipher();
		}

		socket->disconnectFromHost();
		socket->deleteLater();
	});
	connect(socket, &QSslSocket::errorOccurred, this, [socket](QAbstractSocket::SocketError)
	{
		socket->deleteLater();
	});

	socket->connectToHostEncrypted(url.host(), url.port(443));
}

void QtWebEnginePage::handleLoadFinished()
{
	m_isIgnoringJavaScriptPopups = false;

	if (url().scheme() == QLatin1String("https") && m_sslInformation.errors.isEmpty())
	{
		probeSslInformation(url());
	}

	runJavaScript(QStringLiteral(R"JS((function()
	{
		const updateEditingState = function()
		{
			const element = document.activeElement;
			let isEditing = false;

			if (element && !element.disabled && !element.readOnly)
			{
				if (element.isContentEditable || element.tagName.toLowerCase() === 'textarea')
				{
					isEditing = true;
				}
				else if (element.tagName.toLowerCase() === 'input')
				{
					isEditing = !['button', 'checkbox', 'color', 'file', 'hidden', 'image', 'radio', 'range', 'reset', 'submit'].includes(element.type.toLowerCase());
				}
			}

			console.info('otter-editing:%1:' + (isEditing ? '1' : '0'));
		};

		document.addEventListener('focusin', updateEditingState, true);
		document.addEventListener('focusout', function()
		{
			setTimeout(updateEditingState, 0);
		}, true);
		updateEditingState();
	})();)JS").arg(m_editingStateMessageToken));

	if (m_widget && !m_widget->isPrivate() && m_widget->getOption(SettingsManager::Browser_RememberPasswordsOption).toBool())
	{
		QFile file(QLatin1String(":/modules/backends/web/qtwebengine/resources/formExtractor.js"));

		if (file.open(QIODevice::ReadOnly))
		{
			runJavaScript(QString::fromUtf8(file.readAll()).arg(m_passwordMessageToken));
		}
		else
		{
			qWarning() << "Failed to open password form extractor resource:" << file.errorString();
		}
	}

	const int historyIndex(history()->currentItemIndex());
	HistoryEntryInformation entry(m_history.value(historyIndex));

	if (entry.isValid)
	{
		entry.icon = icon();

		if (entry.identifier > 0 && !profile()->isOffTheRecord())
		{
			HistoryManager::updateEntry(entry.identifier, url(), (m_widget ? m_widget->getTitle() : title()), icon());
		}

		m_history[historyIndex] = entry;
	}

	if (m_widget)
	{
		const QUrl url(m_widget->getUrl());
		const ContentFiltersManager::CosmeticFiltersResult cosmeticFilters(ContentFiltersManager::getCosmeticFilters(ContentFiltersManager::getProfileIdentifiers(m_widget->getOption(SettingsManager::ContentBlocking_ProfilesOption).toStringList()), url));

		if (!cosmeticFilters.rules.isEmpty() || !cosmeticFilters.exceptions.isEmpty())
		{
			QFile file(QLatin1String(":/modules/backends/web/qtwebengine/resources/hideElements.js"));

			if (file.open(QIODevice::ReadOnly))
			{
				runJavaScript(QString::fromUtf8(file.readAll()).arg(createJavaScriptList(cosmeticFilters.exceptions), createJavaScriptList(cosmeticFilters.rules)));

				file.close();
			}
		}

		const QStringList blockedRequests(m_widget->getBlockedElements());

		if (!blockedRequests.isEmpty())
		{
			QFile file(QLatin1String(":/modules/backends/web/qtwebengine/resources/hideBlockedRequests.js"));

			if (file.open(QIODevice::ReadOnly))
			{
				runJavaScript(QString::fromLatin1(file.readAll()).arg(createJavaScriptList(blockedRequests)));

				file.close();
			}
		}
	}

	runJavaScript(QStringLiteral(R"JS((function()
	{
		const url = location.href;
		const image = document.querySelector('img[style*="user-select: none"]');

		if (image && image.src === url)
		{
			return 'img';
		}

		const source = document.querySelector('video[name="media"] > source, video[controls][autoplay] > source');

		if (source && source.src === url)
		{
			return 'video';
		}

		return '';
	})())JS"), [this](const QVariant &result)
	{
		const QString kind(result.toString());
		const bool isViewingMedia(!kind.isEmpty());

		if (kind == QLatin1String("img"))
		{
			settings()->setAttribute(QWebEngineSettings::AutoLoadImages, true);
			settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);

			QFile file(SessionsManager::getReadableDataPath(QLatin1String("imageViwer.js")));

			if (file.open(QIODevice::ReadOnly))
			{
				runJavaScript(QString::fromLatin1(file.readAll()));

				file.close();
			}
		}

		if (isViewingMedia != m_isViewingMedia)
		{
			m_isViewingMedia = isViewingMedia;

			emit viewingMediaChanged(m_isViewingMedia);
		}
	});
}

void QtWebEnginePage::setHistory(const Session::Window::History &history)
{
	m_history.clear();

	if (history.entries.isEmpty())
	{
		this->history()->clear();

		const QVector<UserScript*> scripts(UserScript::getUserScriptsForUrl(QUrl(QLatin1String("about:blank"))));

		for (int i = 0; i < scripts.count(); ++i)
		{
			runJavaScript(scripts.at(i)->getSource(), QWebEngineScript::UserWorld);
		}

		return;
	}

	m_history.reserve(history.entries.count());

	QByteArray byteArray;
	QDataStream stream(&byteArray, QIODevice::ReadWrite);
	stream << static_cast<int>(3) << history.entries.count() << history.index;

	for (int i = 0; i < history.entries.count(); ++i)
	{
		const Session::Window::History::Entry entry(history.entries.at(i));

		stream << QUrl(entry.url) << entry.title << QByteArray() << static_cast<qint32>(0) << false << QUrl() << static_cast<qint32>(0) << QUrl(entry.url) << false << QDateTime::currentSecsSinceEpoch() << static_cast<int>(200);

		HistoryEntryInformation entryInformation;
		entryInformation.timeVisited = entry.time;
		entryInformation.position = entry.position;
		entryInformation.zoom = entry.zoom;
		entryInformation.isValid = true;

		m_history.append(entryInformation);
	}

	stream.device()->reset();
	stream >> *(this->history());

	if (this->history()->count() == history.entries.count() && !Utils::isUrlEmpty(this->history()->currentItem().url()))
	{
		this->history()->goToItem(this->history()->itemAt(history.index));
	}
	else
	{
		setUrl(history.entries.value(history.index).url);
	}
}

QtWebEngineWebWidget* QtWebEnginePage::getWebWidget() const
{
	return m_widget;
}

void QtWebEnginePage::detachWidget()
{
	m_widget = nullptr;
}

QWebEnginePage* QtWebEnginePage::createWindow(WebWindowType type)
{
	Q_UNUSED(type)

	if (!m_widget)
	{
		return nullptr;
	}

	const QString popupsPolicy(m_widget->getOption(SettingsManager::Permissions_ScriptsCanOpenWindowsOption).toString());

	if (m_widget->getLastUrlClickTime().isNull() || m_widget->getLastUrlClickTime().msecsTo(QDateTime::currentDateTimeUtc()) > 1000)
	{
		if (popupsPolicy == QLatin1String("blockAll"))
		{
			return nullptr;
		}

		if (popupsPolicy == QLatin1String("ask") || !m_widget->getOption(SettingsManager::ContentBlocking_ProfilesOption, m_widget->getUrl()).isNull())
		{
			QtWebEnginePage *page(new QtWebEnginePage(profile(), m_widget));
			page->markAsPopup();

			connect(page, &QtWebEnginePage::aboutToNavigate, this, [=](const QUrl &url)
			{
				m_popups.removeAll(page);

				page->deleteLater();

				const QVector<int> profiles(ContentFiltersManager::getProfileIdentifiers(m_widget->getOption(SettingsManager::ContentBlocking_ProfilesOption, m_widget->getUrl()).toStringList()));

				if (!profiles.isEmpty())
				{
					const ContentFiltersManager::CheckResult result(ContentFiltersManager::checkUrl(profiles, m_widget->getUrl(), url, NetworkManager::PopupType));

					if (result.isBlocked)
					{
						Console::addMessage(QCoreApplication::translate("main", "Request blocked by rule from profile %1:\n%2").arg(ContentFiltersManager::getProfile(result.profile)->getTitle(), result.rule), Console::NetworkCategory, Console::LogLevel, url.url(), -1, m_widget->getWindowIdentifier());

						return;
					}
				}

				const QString popupsPolicy(SettingsManager::getOption(SettingsManager::Permissions_ScriptsCanOpenWindowsOption, Utils::extractHost(m_widget->getRequestedUrl())).toString());

				if (popupsPolicy == QLatin1String("ask"))
				{
					emit requestedPopupWindow(m_widget->getUrl(), url);
				}
				else
				{
					SessionsManager::OpenHints hints(SessionsManager::NewTabOpen);

					if (popupsPolicy == QLatin1String("openAllInBackground"))
					{
						hints |= SessionsManager::BackgroundOpen;
					}

					QtWebEngineWebWidget *widget(createWidget(hints));
					widget->setUrl(url);
				}
			});

			return page;
		}
	}

	return createWidget(SessionsManager::calculateOpenHints((popupsPolicy == QLatin1String("openAllInBackground")) ? (SessionsManager::NewTabOpen | SessionsManager::BackgroundOpen) : SessionsManager::NewTabOpen))->getPage();
}

QtWebEngineWebWidget* QtWebEnginePage::createWidget(SessionsManager::OpenHints hints)
{
	QtWebEngineWebWidget *widget(nullptr);

	if (m_widget)
	{
		widget = qobject_cast<QtWebEngineWebWidget*>(m_widget->clone(false, m_widget->isPrivate(), SettingsManager::getOption(SettingsManager::Sessions_OptionsExludedFromInheritingOption).toStringList()));
	}
	else if (profile()->isOffTheRecord())
	{
		widget = new QtWebEngineWebWidget({{QLatin1String("hints"), SessionsManager::PrivateOpen}}, nullptr, nullptr);
	}
	else
	{
		widget = new QtWebEngineWebWidget({}, nullptr, nullptr);
	}

	widget->handleLoadStarted();

	emit requestedNewWindow(widget, hints, {});

	return widget;
}

QString QtWebEnginePage::createJavaScriptList(const QStringList &rules) const
{
	// JSON preserves CSS escapes, quotes and control characters in filter lists.
	const QByteArray json(QJsonDocument(QJsonArray::fromStringList(rules)).toJson(QJsonDocument::Compact));

	return QString::fromUtf8(json.mid(1, json.size() - 2));
}

QString QtWebEnginePage::createScriptSource(const QString &path, const QStringList &parameters) const
{
	QFile file(QLatin1String(":/modules/backends/web/qtwebengine/resources/") + path + QLatin1String(".js"));

	if (!file.open(QIODevice::ReadOnly))
	{
		return {};
	}

	QString script(QString::fromLatin1(file.readAll()));

	file.close();

	for (int i = 0; i < parameters.count(); ++i)
	{
		script = script.arg(parameters.at(i));
	}

	return script;
}

QVariant QtWebEnginePage::runScriptSource(const QString &script)
{
	QVariant result;
	QEventLoop eventLoop;

	runJavaScript(script, QWebEngineScript::UserWorld, [&](const QVariant &runResult)
	{
		result = runResult;

		eventLoop.quit();
	});

	if (m_widget)
	{
		connect(m_widget, &QtWebEngineWebWidget::aboutToReload, &eventLoop, &QEventLoop::quit);
	}

	connect(this, &QtWebEnginePage::destroyed, &eventLoop, &QEventLoop::quit);

	eventLoop.exec();

	return result;
}

QVariant QtWebEnginePage::runScriptFile(const QString &path, const QStringList &parameters)
{
	return runScriptSource(createScriptSource(path, parameters));
}

WebWidget::SslInformation QtWebEnginePage::getSslInformation() const
{
	return m_sslInformation;
}

Session::Window::History QtWebEnginePage::getHistory() const
{
	QWebEngineHistory *pageHistory(history());
	const int historyCount(pageHistory->count());
	Session::Window::History history;
	history.entries.reserve(historyCount);
	history.index = pageHistory->currentItemIndex();

	const QString url(requestedUrl().toString());

	for (int i = 0; i < historyCount; ++i)
	{
		const QWebEngineHistoryItem item(pageHistory->itemAt(i));
		const HistoryEntryInformation entryInformation(m_history.value(i));
		Session::Window::History::Entry entry;
		entry.url = ((item.url().toString() == QLatin1String("about:blank")) ? item.originalUrl() : item.url()).toString();
		entry.title = item.title();

		if (entryInformation.isValid)
		{
			entry.icon = entryInformation.icon;
			entry.position = entryInformation.position;
			entry.time = entryInformation.timeVisited;
			entry.zoom = entryInformation.zoom;
		}

		history.entries.append(entry);
	}

	if (m_widget && m_widget->getLoadingState() == WebWidget::OngoingLoadingState && url != pageHistory->itemAt(pageHistory->currentItemIndex()).url().toString())
	{
		Session::Window::History::Entry entry;
		entry.url = url;
		entry.title = m_widget->getTitle();
		entry.icon = icon();
		entry.time = QDateTime::currentDateTimeUtc();
		entry.position = scrollPosition().toPoint();
		entry.zoom = static_cast<int>(zoomFactor() * 100);

		history.entries.append(entry);
		history.index = historyCount;
	}

	return history;
}

QStringList QtWebEnginePage::chooseFiles(FileSelectionMode mode, const QStringList &oldFiles, const QStringList &acceptedMimeTypes)
{
	Q_UNUSED(acceptedMimeTypes)

	return Utils::getOpenPaths(oldFiles, {}, (mode == FileSelectOpenMultiple));
}

bool QtWebEnginePage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
	if (m_isPopup)
	{
		emit aboutToNavigate(url, type);

		return false;
	}

	if (HandlersManager::handleUrl(url))
	{
		return false;
	}

	if (!isMainFrame)
	{
		return true;
	}

	if (url.scheme() == QLatin1String("javascript"))
	{
		runJavaScript(url.path());

		return false;
	}

	if (m_widget && requestedUrl().isEmpty())
	{
		m_widget->setRequestedUrl(url, false, true);
	}

	if (type == NavigationTypeReload && m_previousNavigationType == NavigationTypeFormSubmitted && SettingsManager::getOption(SettingsManager::Choices_WarnFormResendOption).toBool())
	{
		bool shouldCancelRequest(false);
		bool shouldWarnNextTime(true);

		if (m_widget)
		{
			ContentsDialog dialog(ThemesManager::createIcon(QLatin1String("dialog-warning")), tr("Question"), tr("Are you sure that you want to send form data again?"), tr("Do you want to resend data?"), (QDialogButtonBox::Yes | QDialogButtonBox::Cancel), nullptr, m_widget);
			dialog.setCheckBox(tr("Do not show this message again"), false);

			connect(m_widget, &QtWebEngineWebWidget::aboutToReload, &dialog, &ContentsDialog::close);

			m_widget->showDialog(&dialog);

			shouldCancelRequest = !dialog.isAccepted();
			shouldWarnNextTime = !dialog.getCheckBoxState();
		}
		else
		{
			QMessageBox dialog;
			dialog.setWindowTitle(tr("Question"));
			dialog.setText(tr("Are you sure that you want to send form data again?"));
			dialog.setInformativeText(tr("Do you want to resend data?"));
			dialog.setIcon(QMessageBox::Question);
			dialog.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
			dialog.setDefaultButton(QMessageBox::Cancel);
			dialog.setCheckBox(new QCheckBox(tr("Do not show this message again")));

			shouldCancelRequest = (dialog.exec() == QMessageBox::Cancel);
			shouldWarnNextTime = !dialog.checkBox()->isChecked();
		}

		SettingsManager::setOption(SettingsManager::Choices_WarnFormResendOption, shouldWarnNextTime);

		if (shouldCancelRequest)
		{
			return false;
		}
	}

	m_sslInformation = {};

	if (type != NavigationTypeReload)
	{
		m_previousNavigationType = type;

		if (type != NavigationTypeBackForward)
		{
			m_history.resize(qMax(0, history()->currentItemIndex()));

			HistoryEntryInformation entry;
			entry.icon = icon();
			entry.timeVisited = QDateTime::currentDateTimeUtc();
			entry.position = scrollPosition().toPoint();
			entry.zoom = static_cast<int>(zoomFactor() * 100);
			entry.isValid = true;

			if (!profile()->isOffTheRecord())
			{
				entry.identifier = HistoryManager::addEntry(url, {}, {}, (m_widget ? m_widget->isTypedIn() : false));
			}

			m_history.append(entry);
		}
	}

	// Keep Qt's per-page script registry alive while its frame observer is attached.
	const QList<QWebEngineScript> navigationScripts(scripts().toList());

	for (const QWebEngineScript &script: navigationScripts)
	{
		scripts().remove(script);
	}

	if (m_widget && m_widget->getOption(SettingsManager::ContentBlocking_EnableContentBlockingOption, url).toBool())
	{
		const auto filters(ContentFiltersManager::getCosmeticFilters(ContentFiltersManager::getProfileIdentifiers(m_widget->getOption(SettingsManager::ContentBlocking_ProfilesOption, url).toStringList()), url));
		QFile file(QLatin1String(":/modules/backends/web/qtwebengine/resources/hideElements.js"));

		if (!filters.rules.isEmpty() && file.open(QIODevice::ReadOnly))
		{
			QWebEngineScript script;
			script.setName(QLatin1String("otter-cosmetic-filters"));
			script.setSourceCode(QString::fromUtf8(file.readAll()).arg(createJavaScriptList(filters.exceptions), createJavaScriptList(filters.rules)));
			script.setWorldId(QWebEngineScript::ApplicationWorld);
			script.setInjectionPoint(QWebEngineScript::DocumentCreation);
			scripts().insert(script);
		}
	}

	if (m_widget && m_widget->getOption(SettingsManager::Permissions_EnableImagesOption, url).toString() == QLatin1String("disabled"))
	{
		QWebEngineScript imageVisibilityScript;
		imageVisibilityScript.setName(QLatin1String("otter-image-visibility"));
		imageVisibilityScript.setSourceCode(m_widget->getImageVisibilityScript(true));
		imageVisibilityScript.setWorldId(QWebEngineScript::ApplicationWorld);
		imageVisibilityScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
		imageVisibilityScript.setRunsOnSubFrames(true);

		scripts().insert(imageVisibilityScript);
	}

	const QVector<UserScript*> userScripts(UserScript::getUserScriptsForUrl(url));

	for (int i = 0; i < userScripts.count(); ++i)
	{
		QWebEngineScript script;
		script.setSourceCode(userScripts.at(i)->getSource());
		script.setRunsOnSubFrames(userScripts.at(i)->shouldRunOnSubFrames());

		switch (userScripts.at(i)->getInjectionTime())
		{
			case UserScript::DeferredTime:
				script.setInjectionPoint(QWebEngineScript::Deferred);

				break;
			case UserScript::DocumentCreationTime:
				script.setInjectionPoint(QWebEngineScript::DocumentCreation);

				break;
			default:
				script.setInjectionPoint(QWebEngineScript::DocumentReady);

				break;
		}

		scripts().insert(script);
	}

	emit aboutToNavigate(url, type);

	return true;
}

bool QtWebEnginePage::shouldAcceptCertificate(const QWebEngineCertificateError &error)
{
	if (!m_widget || error.certificateChain().isEmpty())
	{
		return false;
	}

	const QList<QSslError> errors(QSslCertificate::verify(error.certificateChain(), error.url().host()));

	m_sslInformation.errors.reserve(m_sslInformation.errors.count() + errors.count());

	for (int i = 0; i < errors.count(); ++i)
	{
		m_sslInformation.errors.append({errors.at(i), error.url()});
	}

	const QString firstPartyUrl(m_widget->getUrl().toString());
	const QString thirdPartyUrl(error.url().toString());

	if (m_widget->getOption(SettingsManager::Security_IgnoreSslErrorsOption, m_widget->getUrl()).toStringList().contains(QString::fromLatin1(error.certificateChain().constFirst().digest().toBase64())))
	{
		Console::addMessage(QStringLiteral("[accepted] The page at %1 was allowed to display insecure content from %2").arg(firstPartyUrl, thirdPartyUrl), Console::SecurityCategory, Console::WarningLevel, thirdPartyUrl, -1, m_widget->getWindowIdentifier());

		return true;
	}

	Console::addMessage(QStringLiteral("[blocked] The page at %1 was not allowed to display insecure content from %2").arg(firstPartyUrl, thirdPartyUrl), Console::SecurityCategory, Console::WarningLevel, thirdPartyUrl, -1, m_widget->getWindowIdentifier());

	return false;
}

#if defined(USE_QT5)
bool QtWebEnginePage::certificateError(const QWebEngineCertificateError &error)
{
	return shouldAcceptCertificate(error);
}
#endif

bool QtWebEnginePage::javaScriptConfirm(const QUrl &url, const QString &message)
{
	if (m_isIgnoringJavaScriptPopups)
	{
		return false;
	}

	if (!m_widget || !m_widget->parentWidget())
	{
		return QWebEnginePage::javaScriptConfirm(url, message);
	}

	emit m_widget->needsAttention();

	ContentsDialog dialog(ThemesManager::createIcon(QLatin1String("dialog-information")), tr("JavaScript"), message, {}, (QDialogButtonBox::Ok | QDialogButtonBox::Cancel), nullptr, m_widget);
	dialog.setCheckBox(tr("Disable JavaScript popups"), false);

	connect(m_widget, &QtWebEngineWebWidget::aboutToReload, &dialog, &ContentsDialog::close);

	m_widget->showDialog(&dialog);

	if (dialog.getCheckBoxState())
	{
		m_isIgnoringJavaScriptPopups = true;
	}

	return dialog.isAccepted();
}

bool QtWebEnginePage::javaScriptPrompt(const QUrl &url, const QString &message, const QString &defaultValue, QString *result)
{
	if (m_isIgnoringJavaScriptPopups)
	{
		return false;
	}

	if (!m_widget || !m_widget->parentWidget())
	{
		return QWebEnginePage::javaScriptPrompt(url, message, defaultValue, result);
	}

	emit m_widget->needsAttention();

	QWidget *widget(new QWidget(m_widget));
	LineEditWidget *lineEdit(new LineEditWidget(defaultValue, widget));
	QLabel *label(new QLabel(message, widget));
	label->setBuddy(lineEdit);
	label->setTextFormat(Qt::PlainText);

	QVBoxLayout *layout(new QVBoxLayout(widget));
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(label);
	layout->addWidget(lineEdit);

	ContentsDialog dialog(ThemesManager::createIcon(QLatin1String("dialog-information")), tr("JavaScript"), {}, {}, (QDialogButtonBox::Ok | QDialogButtonBox::Cancel), widget, m_widget);
	dialog.setCheckBox(tr("Disable JavaScript popups"), false);

	connect(m_widget, &QtWebEngineWebWidget::aboutToReload, &dialog, &ContentsDialog::close);

	m_widget->showDialog(&dialog);

	if (dialog.isAccepted())
	{
		*result = lineEdit->text();
	}

	if (dialog.getCheckBoxState())
	{
		m_isIgnoringJavaScriptPopups = true;
	}

	return dialog.isAccepted();
}

bool QtWebEnginePage::isPopup() const
{
	return m_isPopup;
}

bool QtWebEnginePage::isViewingMedia() const
{
	return m_isViewingMedia;
}

}
