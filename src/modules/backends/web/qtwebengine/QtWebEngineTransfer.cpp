/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2016 - 2025 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#include "QtWebEngineTransfer.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QMimeDatabase>

namespace Otter
{

QtWebEngineTransfer::QtWebEngineTransfer(QtWebEngineDownload *item, TransferOptions options, QObject *parent) : Transfer(options, parent),
	m_item(item)
{
	m_item->setParent(this);

	markAsStarted();

#if defined(USE_QT5)
	connect(m_item, &QWebEngineDownloadItem::finished, this, &QtWebEngineTransfer::markAsFinished);
	connect(m_item, &QWebEngineDownloadItem::downloadProgress, this, &QtWebEngineTransfer::handleDownloadProgress);
#else
	connect(m_item, &QWebEngineDownloadRequest::receivedBytesChanged, this, [this]()
	{
		handleDownloadProgress(m_item->receivedBytes(), m_item->totalBytes());
	});
	connect(m_item, &QWebEngineDownloadRequest::totalBytesChanged, this, [this]()
	{
		handleDownloadProgress(m_item->receivedBytes(), m_item->totalBytes());
	});
#endif
	connect(m_item, &QtWebEngineDownload::stateChanged, this, [&](QtWebEngineDownload::DownloadState state)
	{
		switch (state)
		{
			case QtWebEngineDownload::DownloadCancelled:
			case QtWebEngineDownload::DownloadCompleted:
			case QtWebEngineDownload::DownloadInterrupted:
				emit stopped();

				break;
			default:
				break;
		}

		if (state == QtWebEngineDownload::DownloadCompleted)
		{
			markAsFinished();
		}

		emit changed();
	});
}

void QtWebEngineTransfer::cancel()
{
	if (m_item)
	{
		QObject::disconnect(m_item, nullptr, this, nullptr);
		m_item->cancel();
		m_item->deleteLater();
		m_item.clear();

		Transfer::cancel();
	}
	else
	{
		Transfer::cancel();
	}
}

QUrl QtWebEngineTransfer::getSource() const
{
	if (!m_item)
	{
		return Transfer::getSource();
	}

	return m_item->url();
}

QString QtWebEngineTransfer::getSuggestedFileName()
{
	if (!m_item)
	{
		return {};
	}

	return m_item->suggestedFileName();
}

QString QtWebEngineTransfer::getTarget() const
{
	if (!m_item)
	{
		return Transfer::getTarget();
	}

	return QDir(m_item->downloadDirectory()).absoluteFilePath(m_item->downloadFileName());
}

QMimeType QtWebEngineTransfer::getMimeType() const
{
	if (!m_item)
	{
		return Transfer::getMimeType();
	}

	return QMimeDatabase().mimeTypeForName(m_item->mimeType());
}

qint64 QtWebEngineTransfer::getBytesReceived() const
{
	if (!m_item)
	{
		return Transfer::getBytesReceived();
	}

	return m_item->receivedBytes();
}

qint64 QtWebEngineTransfer::getBytesTotal() const
{
	if (!m_item)
	{
		return Transfer::getBytesTotal();
	}

	return m_item->totalBytes();
}

Transfer::TransferState QtWebEngineTransfer::getState() const
{
	if (!m_item)
	{
		return Transfer::getState();
	}

	switch (m_item->state())
	{
		case QtWebEngineDownload::DownloadRequested:
		case QtWebEngineDownload::DownloadInProgress:
			return RunningState;
		case QtWebEngineDownload::DownloadCompleted:
			return FinishedState;
		case QtWebEngineDownload::DownloadCancelled:
			return CancelledState;
		case QtWebEngineDownload::DownloadInterrupted:
			return ErrorState;
	}

	return UnknownState;
}

bool QtWebEngineTransfer::setTarget(const QString &target, bool canOverwriteExisting)
{
	if (!m_item)
	{
		return Transfer::setTarget(target, canOverwriteExisting);
	}

	if (m_item->state() == QtWebEngineDownload::DownloadRequested)
	{
		const QFileInfo fileInformation(target);

		m_item->setDownloadDirectory(fileInformation.path());
		m_item->setDownloadFileName(fileInformation.fileName());
		m_item->accept();
	}

	return true;
}

void QtWebEngineTransfer::setOpenCommand(const QString &command)
{
	Transfer::setOpenCommand(command);

	if (m_item && m_item->state() == QtWebEngineDownload::DownloadRequested)
	{
		m_item->accept();
	}
}

}
