/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef OTTER_WEBENGINECONFIGURATION_H
#define OTTER_WEBENGINECONFIGURATION_H

#include <QtCore/QByteArray>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtCore/QVector>

namespace Otter
{

inline QByteArray configureWebEngineDownloadFlags(const QByteArray &existingFlags)
{
	QString flags(QString::fromUtf8(existingFlags));
	const QRegularExpression switchPattern(QStringLiteral("(?<!\\S)--enable-features=(?:\"([^\"]*)\"|'([^']*)'|(\\S*))"));
	const QRegularExpression downloadFeature(QLatin1String("^ParallelDownloading(?:[<:]|$)"));
	QVector<QRegularExpressionMatch> matches;
	QStringList features;
	auto iterator(switchPattern.globalMatch(flags));

	while (iterator.hasNext())
	{
		const QRegularExpressionMatch match(iterator.next());
		matches.append(match);
		const QString value(match.captured(1) + match.captured(2) + match.captured(3));

		for (const QString &feature: value.split(QLatin1Char(','), Qt::SkipEmptyParts))
		{
			if (!downloadFeature.match(feature.trimmed()).hasMatch())
			{
				features.append(feature.trimmed());
			}
		}
	}

	for (auto match = matches.crbegin(); match != matches.crend(); ++match)
	{
		flags.remove(match->capturedStart(), match->capturedLength());
	}

	// Chromium counts the original stream as one of these eight requests. Its
	// downloader decides eligibility and handles ranges, validation and recovery.
	features.append(QLatin1String("ParallelDownloading:request_count/8"));
	flags = flags.trimmed();

	if (!flags.isEmpty())
	{
		flags.append(QLatin1Char(' '));
	}

	// Keep explicit --disable-features overrides, as well as unrelated switches.
	return (flags + QLatin1String("--enable-features=") + features.join(QLatin1Char(','))).toUtf8();
}

}

#endif
