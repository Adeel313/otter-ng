find_package(${QT_VERSION} ${QT_MINIMUM_VERSION} QUIET COMPONENTS WebEngineWidgets)

set_package_properties(${QT_VERSION}WebEngineCore PROPERTIES URL "https://www.qt.io/" DESCRIPTION "QtWebEngine based backend (core)" TYPE OPTIONAL)
set_package_properties(${QT_VERSION}WebEngineWidgets PROPERTIES URL "https://www.qt.io/" DESCRIPTION "QtWebEngine based backend (widgets)" TYPE OPTIONAL)

if (NOT TARGET ${QT_VERSION}::WebEngineWidgets)
	return ()
endif ()

if (NOT ENABLE_QT5)
	find_package(Qt6 6.4 REQUIRED COMPONENTS Pdf)
	list(APPEND OTTER_LINK_LIBRARIES Qt6::Pdf)
endif ()

add_definitions(-DOTTER_ENABLE_QTWEBENGINE)

list(APPEND OTTER_BACKENDS_WEB "QtWebEngine")
list(APPEND OTTER_LINK_LIBRARIES ${QT_VERSION}::WebEngineCore ${QT_VERSION}::WebEngineWidgets)

set(OTTER_SOURCES
	${OTTER_SOURCES}
	src/modules/backends/web/qtwebengine/QtWebEnginePage.cpp
	src/modules/backends/web/qtwebengine/QtWebEngineUrlRequestInterceptor.cpp
	src/modules/backends/web/qtwebengine/QtWebEngineTransfer.cpp
	src/modules/backends/web/qtwebengine/QtWebEngineWebBackend.cpp
	src/modules/backends/web/qtwebengine/QtWebEngineWebWidget.cpp
)

qt_add_resources(OTTER_RESOURCES
	src/modules/backends/web/qtwebengine/QtWebEngineResources.qrc
)
