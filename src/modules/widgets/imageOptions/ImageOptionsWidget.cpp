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

#include "ImageOptionsWidget.h"
#include "../../../core/SettingsManager.h"
#include "../../../core/ThemesManager.h"
#include "../../../core/Utils.h"
#include "../../../ui/ToolBarWidget.h"
#include "../../../ui/MainWindow.h"

#include <QtCore/QEvent>
#if defined(USE_QT5)
#include <QtWidgets/QActionGroup>
#else
#include <QtGui/QActionGroup>
#endif
#include <QtWidgets/QMenu>

namespace Otter
{

ImageOptionsWidget::ImageOptionsWidget(Window *window, const ToolBarsManager::ToolBarDefinition::Entry &definition, QWidget *parent) : ToolButtonWidget(definition, parent),
	m_policy(QLatin1String("enabled"))
{
	setObjectName(QLatin1String("imageOptionsWidget"));
	setPopupMode(QToolButton::InstantPopup);

	QMenu *menu(new QMenu(this));
	QActionGroup *group(new QActionGroup(menu));
	group->setExclusive(true);

	for (const QString &policy: {QLatin1String("enabled"), QLatin1String("onlyCached"), QLatin1String("disabled"), QLatin1String("enabledForDomain")})
	{
		QAction *action(menu->addAction(QString()));
		action->setCheckable(true);
		action->setData(policy);
		group->addAction(action);
	}

	setMenu(menu);
	setWindow(window);

	const ToolBarWidget *toolBar(qobject_cast<ToolBarWidget*>(parent));

	if (toolBar && toolBar->getDefinition().isGlobal())
	{
		setWindow(toolBar->getWindow() ? toolBar->getWindow() : (toolBar->getMainWindow() ? toolBar->getMainWindow()->getActiveWindow() : nullptr));
		connect(toolBar, &ToolBarWidget::windowChanged, this, &ImageOptionsWidget::setWindow);
	}

	connect(menu, &QMenu::aboutToShow, this, &ImageOptionsWidget::updateState);
	connect(group, &QActionGroup::triggered, this, [this](QAction *action)
	{
		if (m_window && m_window->getType() == QLatin1String("web"))
		{
			if (action->data() == QLatin1String("enabledForDomain"))
			{
				const QString domain(Utils::getRegistrableDomain(m_window->getUrl().host()));

				if (!domain.isEmpty())
				{
					SettingsManager::setDomainOption(SettingsManager::Permissions_EnableImagesOption, QLatin1String("enabled"), domain);
				}

				m_window->setOption(SettingsManager::Permissions_EnableImagesOption, QLatin1String("enabled"));
			}
			else
			{
				m_window->setOption(SettingsManager::Permissions_EnableImagesOption, action->data());
			}

			updateState();
		}
	});
	connect(SettingsManager::getInstance(), &SettingsManager::optionChanged, this, [this](int identifier)
	{
		if (identifier == SettingsManager::Permissions_EnableImagesOption)
		{
			updateState();
		}
	});
	connect(SettingsManager::getInstance(), &SettingsManager::hostOptionChanged, this, [this](int identifier)
	{
		if (identifier == SettingsManager::Permissions_EnableImagesOption)
		{
			updateState();
		}
	});
}

void ImageOptionsWidget::changeEvent(QEvent *event)
{
	ToolButtonWidget::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		updateState();
	}
}

void ImageOptionsWidget::setWindow(Window *window)
{
	if (m_window)
	{
		disconnect(m_window, nullptr, this, nullptr);
	}

	m_window = window;

	if (window)
	{
		connect(window, &Window::optionChanged, this, [this](int identifier)
		{
			if (identifier == SettingsManager::Permissions_EnableImagesOption)
			{
				updateState();
			}
		});
		connect(window, &Window::urlChanged, this, &ImageOptionsWidget::updateState);
		connect(window, &Window::loadingStateChanged, this, &ImageOptionsWidget::updateState);
		connect(window, &Window::aboutToClose, this, [this]() { setWindow(nullptr); });
	}

	updateState();
}

void ImageOptionsWidget::updateState()
{
	m_policy = (m_window ? m_window->getOption(SettingsManager::Permissions_EnableImagesOption) : SettingsManager::getOption(SettingsManager::Permissions_EnableImagesOption)).toString();
	setEnabled(m_window && m_window->getType() == QLatin1String("web"));

	if (menu())
	{
		const QString domain(Utils::getRegistrableDomain(m_window ? m_window->getUrl().host() : QString()));
		const bool hasDomainOverride(!domain.isEmpty() && SettingsManager::hasOverride(domain, SettingsManager::Permissions_EnableImagesOption) && SettingsManager::hasOverride(QLatin1String("*.") + domain, SettingsManager::Permissions_EnableImagesOption) && SettingsManager::getOption(SettingsManager::Permissions_EnableImagesOption, domain).toString() == QLatin1String("enabled") && SettingsManager::getOption(SettingsManager::Permissions_EnableImagesOption, QLatin1String("*.") + domain).toString() == QLatin1String("enabled"));
		const bool isDomainPolicy(hasDomainOverride && m_policy == QLatin1String("enabled"));
		const QStringList titles({tr("Show images"), tr("Cached images"), tr("No images"), tr("Enable images for full domain")});
		const QList<QAction*> actions(menu()->actions());

		for (int i = 0; i < actions.count(); ++i)
		{
			actions.at(i)->setText(titles.value(i));
			actions.at(i)->setChecked((i == 3) ? isDomainPolicy : (!isDomainPolicy && actions.at(i)->data().toString() == m_policy));
		}
	}

	updateAction();
}

QString ImageOptionsWidget::getText() const
{
	if (m_policy == QLatin1String("onlyCached"))
	{
		return tr("Cached images");
	}

	return (m_policy == QLatin1String("disabled") ? tr("No images") : tr("Show images"));
}

QString ImageOptionsWidget::getToolTip() const
{
	return tr("Images for this tab: %1").arg(getText());
}

QIcon ImageOptionsWidget::getIcon() const
{
	return ThemesManager::createIcon(QLatin1String("image-loading"));
}

}
