/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2026 Otter Browser contributors
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
**************************************************************************/

#include "FormFillerPreferencesPage.h"
#include "../../../core/FormFillerManager.h"
#include "../../../core/SettingsManager.h"

#include <QtCore/QUrl>
#include <QtCore/QHash>
#include <QtCore/QSignalBlocker>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

namespace Otter
{

namespace
{

class FormFillerEntryDialog final : public QDialog
{
public:
	explicit FormFillerEntryDialog(const FormFillerManager::Entry &entry, QWidget *parent) : QDialog(parent),
		m_isProfileCheckBox(new QCheckBox(tr("Use as a personal detail on all websites"), this)),
		m_profileFieldComboBox(new QComboBox(this)),
		m_historyFieldLineEdit(new QLineEdit(this)),
		m_websiteLineEdit(new QLineEdit(this)),
		m_valueLineEdit(new QLineEdit(this)),
		m_profileFieldLabel(nullptr),
		m_historyFieldLabel(nullptr),
		m_websiteLabel(nullptr)
	{
		setWindowTitle(tr("Form Filler Entry"));

		const QVector<QPair<QString, QString> > fields({
			{tr("Full name"), QLatin1String("name")},
			{tr("Given name"), QLatin1String("given-name")},
			{tr("Additional name"), QLatin1String("additional-name")},
			{tr("Family name"), QLatin1String("family-name")},
			{tr("Nickname"), QLatin1String("nickname")},
			{tr("Organization"), QLatin1String("organization")},
			{tr("Job title"), QLatin1String("organization-title")},
			{tr("Street address"), QLatin1String("street-address")},
			{tr("Address line 1"), QLatin1String("address-line1")},
			{tr("Address line 2"), QLatin1String("address-line2")},
			{tr("City"), QLatin1String("address-level2")},
			{tr("State or province"), QLatin1String("address-level1")},
			{tr("Postal code"), QLatin1String("postal-code")},
			{tr("Country"), QLatin1String("country-name")},
			{tr("Email"), QLatin1String("email")},
			{tr("Phone"), QLatin1String("tel")},
			{tr("Birthday"), QLatin1String("bday")}
		});
		for (const QPair<QString, QString> &field: fields)
		{
			m_profileFieldComboBox->addItem(field.first, field.second);
		}

		m_historyFieldLineEdit->setPlaceholderText(tr("HTML field name"));
		m_historyFieldLineEdit->setMaxLength(256);
		m_websiteLineEdit->setPlaceholderText(tr("https://example.com"));
		m_valueLineEdit->setMaxLength(1024);

		QFormLayout *layout(new QFormLayout(this));
		layout->addRow(m_isProfileCheckBox);
		layout->addRow(tr("Personal detail"), m_profileFieldComboBox);
		m_profileFieldLabel = layout->labelForField(m_profileFieldComboBox);
		layout->addRow(tr("Form field name"), m_historyFieldLineEdit);
		m_historyFieldLabel = layout->labelForField(m_historyFieldLineEdit);
		layout->addRow(tr("Website"), m_websiteLineEdit);
		m_websiteLabel = layout->labelForField(m_websiteLineEdit);
		layout->addRow(tr("Value"), m_valueLineEdit);

		QDialogButtonBox *buttons(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this));
		layout->addRow(buttons);
		connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(m_isProfileCheckBox, &QCheckBox::toggled, this, &FormFillerEntryDialog::updateFieldControls);

		m_isProfileCheckBox->setChecked(entry.isProfile);
		const int fieldIndex(m_profileFieldComboBox->findData(entry.field));
		if (fieldIndex >= 0)
		{
			m_profileFieldComboBox->setCurrentIndex(fieldIndex);
		}
		m_historyFieldLineEdit->setText(entry.field);
		m_websiteLineEdit->setText(entry.website);
		m_valueLineEdit->setText(entry.value);
		updateFieldControls(entry.isProfile);
	}

	FormFillerManager::Entry getEntry() const
	{
		FormFillerManager::Entry entry;
		entry.isProfile = m_isProfileCheckBox->isChecked();
		entry.field = (entry.isProfile ? m_profileFieldComboBox->currentData().toString() : m_historyFieldLineEdit->text().trimmed());
		entry.website = (entry.isProfile ? QString() : QUrl::fromUserInput(m_websiteLineEdit->text().trimmed()).adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveUserInfo).toString(QUrl::StripTrailingSlash));
		entry.value = m_valueLineEdit->text();
		return entry;
	}

protected:
	void accept() override
	{
		const FormFillerManager::Entry entry(getEntry());
		if (entry.field.isEmpty() || entry.value.trimmed().isEmpty())
		{
			QMessageBox::warning(this, tr("Form Filler"), tr("Enter a field and value."));
			return;
		}

		if (!entry.isProfile)
		{
			const QUrl website(entry.website);
			const QString scheme(website.scheme().toLower());
			if (website.host().isEmpty() || (scheme != QLatin1String("http") && scheme != QLatin1String("https")))
			{
				QMessageBox::warning(this, tr("Form Filler"), tr("Enter a valid website address."));
				return;
			}
		}

		QDialog::accept();
	}

private:
	void updateFieldControls(bool isProfile)
	{
		m_profileFieldComboBox->setVisible(isProfile);
		m_profileFieldLabel->setVisible(isProfile);
		m_historyFieldLineEdit->setVisible(!isProfile);
		m_historyFieldLabel->setVisible(!isProfile);
		m_websiteLineEdit->setVisible(!isProfile);
		m_websiteLabel->setVisible(!isProfile);
	}

	QCheckBox *m_isProfileCheckBox;
	QComboBox *m_profileFieldComboBox;
	QLineEdit *m_historyFieldLineEdit;
	QLineEdit *m_websiteLineEdit;
	QLineEdit *m_valueLineEdit;
	QWidget *m_profileFieldLabel;
	QWidget *m_historyFieldLabel;
	QWidget *m_websiteLabel;
};

QString profileFieldTitle(const QString &field)
{
	static const QHash<QString, QString> titles({
		{QLatin1String("name"), QObject::tr("Full name")},
		{QLatin1String("given-name"), QObject::tr("Given name")},
		{QLatin1String("additional-name"), QObject::tr("Additional name")},
		{QLatin1String("family-name"), QObject::tr("Family name")},
		{QLatin1String("nickname"), QObject::tr("Nickname")},
		{QLatin1String("organization"), QObject::tr("Organization")},
		{QLatin1String("organization-title"), QObject::tr("Job title")},
		{QLatin1String("street-address"), QObject::tr("Street address")},
		{QLatin1String("address-line1"), QObject::tr("Address line 1")},
		{QLatin1String("address-line2"), QObject::tr("Address line 2")},
		{QLatin1String("address-level2"), QObject::tr("City")},
		{QLatin1String("address-level1"), QObject::tr("State or province")},
		{QLatin1String("postal-code"), QObject::tr("Postal code")},
		{QLatin1String("country-name"), QObject::tr("Country")},
		{QLatin1String("email"), QObject::tr("Email")},
		{QLatin1String("tel"), QObject::tr("Phone")},
		{QLatin1String("bday"), QObject::tr("Birthday")}
	});
	return titles.value(field, field);
}

}

FormFillerPreferencesPage::FormFillerPreferencesPage(QWidget *parent) : CategoryPage(parent),
	m_entriesTable(new QTableWidget(this)),
	m_editButton(new QPushButton(tr("Edit…"), this)),
	m_removeButton(new QPushButton(tr("Remove"), this)),
	m_enableCheckBox(new QCheckBox(tr("Enable form filler"), this)),
	m_hasChanges(false)
{
	QVBoxLayout *rootLayout(new QVBoxLayout(this));
	rootLayout->addWidget(m_enableCheckBox);
	QLabel *descriptionLabel(new QLabel(tr("Otter offers saved personal details and form values as you type. Passwords and payment details are not stored here."), this));
	descriptionLabel->setWordWrap(true);
	rootLayout->addWidget(descriptionLabel);

	m_entriesTable->setColumnCount(4);
	m_entriesTable->setHorizontalHeaderLabels({tr("Type"), tr("Field"), tr("Value"), tr("Website")});
	m_entriesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_entriesTable->setSelectionMode(QAbstractItemView::SingleSelection);
	m_entriesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_entriesTable->setAlternatingRowColors(true);
	m_entriesTable->horizontalHeader()->setStretchLastSection(true);
	m_entriesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	rootLayout->addWidget(m_entriesTable);

	QHBoxLayout *buttonsLayout(new QHBoxLayout());
	QPushButton *addButton(new QPushButton(tr("Add…"), this));
	QPushButton *clearButton(new QPushButton(tr("Clear All"), this));
	m_editButton->setEnabled(false);
	m_removeButton->setEnabled(false);
	buttonsLayout->addWidget(addButton);
	buttonsLayout->addWidget(m_editButton);
	buttonsLayout->addWidget(m_removeButton);
	buttonsLayout->addWidget(clearButton);
	buttonsLayout->addStretch(1);
	rootLayout->addLayout(buttonsLayout);

	connect(addButton, &QPushButton::clicked, this, &FormFillerPreferencesPage::addEntry);
	connect(m_editButton, &QPushButton::clicked, this, &FormFillerPreferencesPage::editEntry);
	connect(m_removeButton, &QPushButton::clicked, this, &FormFillerPreferencesPage::removeEntry);
	connect(clearButton, &QPushButton::clicked, this, &FormFillerPreferencesPage::removeAllEntries);
	connect(m_entriesTable, &QTableWidget::itemSelectionChanged, this, &FormFillerPreferencesPage::updateActions);
	connect(m_entriesTable, &QTableWidget::itemDoubleClicked, this, &FormFillerPreferencesPage::editEntry);
	connect(m_enableCheckBox, &QCheckBox::toggled, this, [this]()
	{
		m_hasChanges = true;
		emit settingsModified();
	});
	connect(FormFillerManager::getInstance(), &FormFillerManager::dataChanged, this, [this]()
	{
		if (!m_hasChanges && wasLoaded())
		{
			populateEntries();
		}
	});
}

void FormFillerPreferencesPage::changeEvent(QEvent *event)
{
	QWidget::changeEvent(event);
}

void FormFillerPreferencesPage::load()
{
	if (wasLoaded())
	{
		return;
	}

	populateEntries();
	const QSignalBlocker blocker(m_enableCheckBox);
	m_enableCheckBox->setChecked(SettingsManager::getOption(SettingsManager::Browser_EnableFormFillerOption).toBool());
	markAsLoaded();
}

void FormFillerPreferencesPage::save()
{
	if (!m_hasChanges)
	{
		return;
	}

	FormFillerManager::setEntries(getEntries());
	SettingsManager::setOption(SettingsManager::Browser_EnableFormFillerOption, m_enableCheckBox->isChecked());
	m_hasChanges = false;
}

QString FormFillerPreferencesPage::getTitle() const
{
	return tr("Form Filler");
}

void FormFillerPreferencesPage::populateEntries()
{
	const QVector<FormFillerManager::Entry> entries(FormFillerManager::getEntries());
	m_entriesTable->setSortingEnabled(false);
	m_entriesTable->setRowCount(entries.count());
	for (int i = 0; i < entries.count(); ++i)
	{
		const FormFillerManager::Entry &entry(entries.at(i));
		m_entriesTable->setItem(i, 0, new QTableWidgetItem(entry.isProfile ? tr("Personal detail") : tr("Form history")));
		QTableWidgetItem *fieldItem(new QTableWidgetItem(entry.isProfile ? profileFieldTitle(entry.field) : entry.field));
		fieldItem->setData(Qt::UserRole, entry.field);
		m_entriesTable->setItem(i, 1, fieldItem);
		m_entriesTable->setItem(i, 2, new QTableWidgetItem(entry.value));
		m_entriesTable->setItem(i, 3, new QTableWidgetItem(entry.isProfile ? tr("All websites") : entry.website));
	}
	m_entriesTable->setSortingEnabled(true);
	m_entriesTable->sortItems(0, Qt::AscendingOrder);
	updateActions();
}

void FormFillerPreferencesPage::addEntry()
{
	FormFillerManager::Entry entry;
	entry.isProfile = true;
	FormFillerEntryDialog dialog(entry, this);
	if (dialog.exec() == QDialog::Accepted)
	{
		const FormFillerManager::Entry newEntry(dialog.getEntry());
		m_entriesTable->setSortingEnabled(false);
		m_entriesTable->insertRow(m_entriesTable->rowCount());
		const int row(m_entriesTable->rowCount() - 1);
		m_entriesTable->setItem(row, 0, new QTableWidgetItem(newEntry.isProfile ? tr("Personal detail") : tr("Form history")));
		QTableWidgetItem *fieldItem(new QTableWidgetItem(newEntry.isProfile ? profileFieldTitle(newEntry.field) : newEntry.field));
		fieldItem->setData(Qt::UserRole, newEntry.field);
		m_entriesTable->setItem(row, 1, fieldItem);
		m_entriesTable->setItem(row, 2, new QTableWidgetItem(newEntry.value));
		m_entriesTable->setItem(row, 3, new QTableWidgetItem(newEntry.isProfile ? tr("All websites") : newEntry.website));
		m_entriesTable->setSortingEnabled(true);
		m_hasChanges = true;
		emit settingsModified();
	}
}

void FormFillerPreferencesPage::editEntry()
{
	const int row(m_entriesTable->currentRow());
	if (row < 0)
	{
		return;
	}

	FormFillerEntryDialog dialog(getEntry(row), this);
	if (dialog.exec() == QDialog::Accepted)
	{
		const FormFillerManager::Entry entry(dialog.getEntry());
		m_entriesTable->setSortingEnabled(false);
		m_entriesTable->setItem(row, 0, new QTableWidgetItem(entry.isProfile ? tr("Personal detail") : tr("Form history")));
		QTableWidgetItem *fieldItem(new QTableWidgetItem(entry.isProfile ? profileFieldTitle(entry.field) : entry.field));
		fieldItem->setData(Qt::UserRole, entry.field);
		m_entriesTable->setItem(row, 1, fieldItem);
		m_entriesTable->setItem(row, 2, new QTableWidgetItem(entry.value));
		m_entriesTable->setItem(row, 3, new QTableWidgetItem(entry.isProfile ? tr("All websites") : entry.website));
		m_entriesTable->setSortingEnabled(true);
		m_hasChanges = true;
		emit settingsModified();
	}
}

void FormFillerPreferencesPage::removeEntry()
{
	const int row(m_entriesTable->currentRow());
	if (row >= 0)
	{
		m_entriesTable->removeRow(row);
		m_hasChanges = true;
		emit settingsModified();
	}
}

void FormFillerPreferencesPage::removeAllEntries()
{
	if (m_entriesTable->rowCount() > 0 && QMessageBox::question(this, tr("Question"), tr("Remove all saved form filler data?"), (QMessageBox::Yes | QMessageBox::Cancel)) == QMessageBox::Yes)
	{
		m_entriesTable->setRowCount(0);
		m_hasChanges = true;
		emit settingsModified();
	}
}

void FormFillerPreferencesPage::updateActions()
{
	const bool hasSelection(m_entriesTable->currentRow() >= 0);
	m_editButton->setEnabled(hasSelection);
	m_removeButton->setEnabled(hasSelection);
}

FormFillerManager::Entry FormFillerPreferencesPage::getEntry(int row) const
{
	FormFillerManager::Entry entry;
	if (row < 0 || row >= m_entriesTable->rowCount())
	{
		return entry;
	}

	entry.isProfile = (m_entriesTable->item(row, 0)->text() == tr("Personal detail"));
	entry.field = (entry.isProfile ? m_entriesTable->item(row, 1)->data(Qt::UserRole).toString() : m_entriesTable->item(row, 1)->text());
	entry.value = m_entriesTable->item(row, 2)->text();
	entry.website = (entry.isProfile ? QString() : m_entriesTable->item(row, 3)->text());
	return entry;
}

QVector<FormFillerManager::Entry> FormFillerPreferencesPage::getEntries() const
{
	QVector<FormFillerManager::Entry> entries;
	entries.reserve(m_entriesTable->rowCount());
	for (int i = 0; i < m_entriesTable->rowCount(); ++i)
	{
		entries.append(getEntry(i));
	}
	return entries;
}

}
