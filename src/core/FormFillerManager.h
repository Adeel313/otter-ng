/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2026 Otter Browser contributors
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
**************************************************************************/

#ifndef OTTER_FORMFILLERMANAGER_H
#define OTTER_FORMFILLERMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QVector>

namespace Otter
{

class FormFillerManager final : public QObject
{
	Q_OBJECT

public:
	struct Entry final
	{
		QString field;
		QString value;
		QString website;
		bool isProfile = false;
	};

	static void createInstance();
	static FormFillerManager* getInstance();
	static QVector<Entry> getEntries();
	static void setEntries(const QVector<Entry> &entries);
	static QStringList getValues(const QUrl &url, const QString &fieldName, const QString &autocomplete);
	static void addValue(const QUrl &url, const QString &fieldName, const QString &autocomplete, const QString &value);
	static void clear();

protected:
	explicit FormFillerManager(QObject *parent);

private:
	static FormFillerManager *m_instance;

signals:
	void dataChanged();
};

}

#endif
