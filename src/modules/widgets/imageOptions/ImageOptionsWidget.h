/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2025 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#ifndef OTTER_IMAGEOPTIONSWIDGET_H
#define OTTER_IMAGEOPTIONSWIDGET_H

#include "../../../ui/ToolButtonWidget.h"
#include "../../../ui/Window.h"

namespace Otter
{

class ImageOptionsWidget final : public ToolButtonWidget
{
	Q_OBJECT

public:
	explicit ImageOptionsWidget(Window *window, const ToolBarsManager::ToolBarDefinition::Entry &definition, QWidget *parent = nullptr);

	QString getText() const override;
	QString getToolTip() const override;
	QIcon getIcon() const override;

protected:
	void changeEvent(QEvent *event) override;

private:
	void setWindow(Window *window);
	void updateState();

	QPointer<Window> m_window;
	QString m_policy;
};

}

#endif
