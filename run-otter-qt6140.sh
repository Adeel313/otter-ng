#!/bin/bash
# Runs the Qt 6.12-built Otter Browser against Qt WebEngine 6.140.0 on top of
# the Qt 6.12.0 base stack in webengine-bin.
#
# The 6.140 libraries come from Arch's qt6-webengine-6.140.0rc-1 package
# (archived copy), which is built with -DQT_FEATURE_webengine_proprietary_codecs=ON
# and links the system (6.12 tree) ffmpeg, so H.264/AAC/HEVC work — unlike the
# PySide6-WebEngine wheel build in webengine-bin140, which only decodes VP8/VP9.
export LD_LIBRARY_PATH=/home/adeel/otter/webengine-bin140arch/usr/lib:/home/adeel/otter/webengine-bin/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export QT_PLUGIN_PATH=/home/adeel/otter/webengine-bin/usr/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}
# Qt derives the helper-process and resource locations from QtCore's prefix
# (the 6.12.0 tree), so point them at the 6.140 tree explicitly.
export QTWEBENGINEPROCESS_PATH=/home/adeel/otter/webengine-bin140arch/usr/lib/qt6/QtWebEngineProcess
export QTWEBENGINE_RESOURCES_PATH=/home/adeel/otter/webengine-bin140arch/usr/share/qt6/resources
export QTWEBENGINE_LOCALES_PATH=/home/adeel/otter/webengine-bin140arch/usr/share/qt6/translations/qtwebengine_locales
# A foreign chrome-sandbox on the path breaks the zygote; leave it to QtWebEngine.
unset CHROME_DEVEL_SANDBOX
exec /home/adeel/otter/build-qt612/otter-browser "$@"
