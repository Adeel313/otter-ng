/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2026 Otter Browser contributors
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
**************************************************************************/

#include "FormFillerManager.h"
#include "SettingsManager.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Otter
{

namespace
{

bool isProfileToken(const QString &token)
{
	static const QStringList tokens({QLatin1String("name"), QLatin1String("given-name"), QLatin1String("additional-name"), QLatin1String("family-name"), QLatin1String("honorific-prefix"), QLatin1String("honorific-suffix"), QLatin1String("nickname"), QLatin1String("organization"), QLatin1String("organization-title"), QLatin1String("street-address"), QLatin1String("address-line1"), QLatin1String("address-line2"), QLatin1String("address-line3"), QLatin1String("address-level1"), QLatin1String("address-level2"), QLatin1String("address-level3"), QLatin1String("country"), QLatin1String("country-name"), QLatin1String("postal-code"), QLatin1String("email"), QLatin1String("tel"), QLatin1String("tel-country-code"), QLatin1String("tel-national"), QLatin1String("tel-area-code"), QLatin1String("tel-local"), QLatin1String("tel-extension"), QLatin1String("bday"), QLatin1String("bday-day"), QLatin1String("bday-month"), QLatin1String("bday-year")});
	return tokens.contains(token);
}

}

FormFillerManager* FormFillerManager::m_instance(nullptr);

FormFillerManager::FormFillerManager(QObject *parent) : QObject(parent)
{
}

void FormFillerManager::createInstance()
{
	if (!m_instance)
	{
		m_instance = new FormFillerManager(SettingsManager::getInstance());
	}
}

FormFillerManager* FormFillerManager::getInstance()
{
	return m_instance;
}

QVector<FormFillerManager::Entry> FormFillerManager::getEntries()
{
	QVector<Entry> entries;
	const QJsonDocument document(QJsonDocument::fromJson(SettingsManager::getOption(SettingsManager::Browser_FormDataOption).toString().toUtf8()));
	if (!document.isObject())
	{
		return entries;
	}

	const QJsonObject data(document.object());
	for (auto iterator = data.constBegin(); iterator != data.constEnd(); ++iterator)
	{
		const int separator(iterator.key().indexOf(QLatin1Char('|')));
		if (separator <= 0 || (separator == 1 && iterator.key().startsWith(QLatin1Char('*')) && iterator.key().size() <= 2))
		{
			continue;
		}

		const bool isProfile(iterator.key().left(separator) == QLatin1String("*"));
		const QString field(iterator.key().mid(separator + 1));
		if (field.isEmpty())
		{
			continue;
		}

		for (const QJsonValue &value: iterator.value().toArray())
		{
			if (value.isString() && !value.toString().isEmpty())
			{
				Entry entry;
				entry.field = field;
				entry.value = value.toString();
				entry.website = (isProfile ? QString() : iterator.key().left(separator));
				entry.isProfile = isProfile;
				entries.append(entry);
			}
		}
	}

	return entries;
}

void FormFillerManager::setEntries(const QVector<Entry> &entries)
{
	if (!m_instance)
	{
		return;
	}

	QJsonObject data;
	for (const Entry &entry: entries)
	{
		if (entry.field.isEmpty() || entry.field.size() > 256 || entry.value.trimmed().isEmpty() || entry.value.size() > 1024)
		{
			continue;
		}

		QString key;
		if (entry.isProfile)
		{
			if (!isProfileToken(entry.field))
			{
				continue;
			}

			key = QLatin1String("*|") + entry.field;
		}
		else
		{
			const QUrl website(QUrl::fromUserInput(entry.website));
			const QString scheme(website.scheme().toLower());
			if (website.host().isEmpty() || (scheme != QLatin1String("http") && scheme != QLatin1String("https")))
			{
				continue;
			}

			key = scheme + QLatin1String("://") + website.host().toLower() + QLatin1Char('|') + entry.field;
		}

		QJsonArray values(data.value(key).toArray());
		const QString value(entry.value.trimmed());
		if (!values.contains(value) && values.size() < 10)
		{
			values.append(value);
		}
		data.insert(key, values);
	}

	SettingsManager::setOption(SettingsManager::Browser_FormDataOption, QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
	emit m_instance->dataChanged();
}

QStringList FormFillerManager::getValues(const QUrl &url, const QString &fieldName, const QString &autocomplete)
{
	const QString scheme(url.scheme().toLower());
	if (!m_instance || !SettingsManager::getOption(SettingsManager::Browser_EnableFormFillerOption).toBool() || url.host().isEmpty() || fieldName.isEmpty() || fieldName.size() > 256 || (scheme != QLatin1String("http") && scheme != QLatin1String("https")))
	{
		return {};
	}

	const QJsonDocument document(QJsonDocument::fromJson(SettingsManager::getOption(SettingsManager::Browser_FormDataOption).toString().toUtf8()));
	if (!document.isObject())
	{
		return {};
	}

	const QString origin(url.scheme().toLower() + QLatin1String("://") + url.host().toLower());
	QStringList values;
	const QString token(autocomplete.section(QLatin1Char(' '), -1).toLower());
	QStringList keys;
	if (isProfileToken(token))
	{
		keys.append(QLatin1String("*") + QLatin1Char('|') + token);
	}
	keys.append(origin + QLatin1Char('|') + fieldName);
	for (const QString &key: keys)
	{
		for (const QJsonValue &value: document.object().value(key).toArray())
		{
			const QString text(value.toString());
			if (!text.isEmpty() && !values.contains(text, Qt::CaseInsensitive))
			{
				values.append(text);
			}
		}
	}
	return values;
}

void FormFillerManager::addValue(const QUrl &url, const QString &fieldName, const QString &autocomplete, const QString &value)
{
	const QString scheme(url.scheme().toLower());
	if (!m_instance || !SettingsManager::getOption(SettingsManager::Browser_EnableFormFillerOption).toBool() || url.host().isEmpty() || (scheme != QLatin1String("http") && scheme != QLatin1String("https")) || fieldName.isEmpty() || fieldName.size() > 256 || value.trimmed().isEmpty() || value.size() > 1024)
	{
		return;
	}

	const QString token(autocomplete.section(QLatin1Char(' '), -1).toLower());
	static const QStringList sensitiveTokens({QLatin1String("current-password"), QLatin1String("new-password"), QLatin1String("cc-name"), QLatin1String("cc-number"), QLatin1String("cc-exp"), QLatin1String("cc-exp-month"), QLatin1String("cc-exp-year"), QLatin1String("cc-csc")});
	if (sensitiveTokens.contains(token))
	{
		return;
	}

	const QString origin(url.scheme().toLower() + QLatin1String("://") + url.host().toLower());
	const QString key(isProfileToken(token) ? (QLatin1String("*") + QLatin1Char('|') + token) : (origin + QLatin1Char('|') + fieldName));
	QJsonObject data(QJsonDocument::fromJson(SettingsManager::getOption(SettingsManager::Browser_FormDataOption).toString().toUtf8()).object());
	if (!data.contains(key) && data.size() >= 500)
	{
		return;
	}
	QJsonArray values(data.value(key).toArray());
	const QString normalized(value.trimmed());
	QJsonArray updatedValues;
	updatedValues.append(normalized);
	for (const QJsonValue &existing: values)
	{
		if (existing.toString().compare(normalized, Qt::CaseInsensitive) != 0 && updatedValues.size() < 10)
		{
			updatedValues.append(existing);
		}
	}
	data.insert(key, updatedValues);
	SettingsManager::setOption(SettingsManager::Browser_FormDataOption, QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
	emit m_instance->dataChanged();
}

void FormFillerManager::clear()
{
	if (m_instance)
	{
		SettingsManager::setOption(SettingsManager::Browser_FormDataOption, QString());
		emit m_instance->dataChanged();
	}
}

}
