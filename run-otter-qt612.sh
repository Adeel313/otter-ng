#!/bin/bash
# Runs the Qt 6.12-built Otter Browser against the local Qt/WebEngine tree
export LD_LIBRARY_PATH=/home/adeel/otter/webengine-bin/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export QT_PLUGIN_PATH=/home/adeel/otter/webengine-bin/usr/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}
export QSG_RHI_BACKEND=x11
exec /home/adeel/otter/build-qt612/otter-browser "$@"
