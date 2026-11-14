/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2026 Otter Browser contributors
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
**************************************************************************/

#ifndef OTTER_FORMFILLERPREFERENCESPAGE_H
#define OTTER_FORMFILLERPREFERENCESPAGE_H

#include "../../../core/FormFillerManager.h"
#include "../../../ui/CategoriesTabWidget.h"

class QTableWidget;
class QPushButton;
class QCheckBox;

namespace Otter
{

class FormFillerPreferencesPage final : public CategoryPage
{
	Q_OBJECT

public:
	explicit FormFillerPreferencesPage(QWidget *parent);

	void load() override;
	QString getTitle() const override;

public slots:
	void save() override;

protected:
	void changeEvent(QEvent *event) override;

private:
	void populateEntries();
	void addEntry();
	void editEntry();
	void removeEntry();
	void removeAllEntries();
	void updateActions();

	FormFillerManager::Entry getEntry(int row) const;
	QVector<FormFillerManager::Entry> getEntries() const;

	QTableWidget *m_entriesTable;
	QPushButton *m_editButton;
	QPushButton *m_removeButton;
	QCheckBox *m_enableCheckBox;
	bool m_hasChanges;
};

}

#endif
