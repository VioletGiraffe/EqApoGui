QT = core gui widgets

CONFIG += c++latest

msvc*{
	QMAKE_CXXFLAGS += /MP
	QMAKE_CXXFLAGS_WARN_ON = /W4
}

SOURCES += \
	src/EqApoConfig.cpp \
	src/Filter.cpp \
	src/FrequencyResponseWidget.cpp \
	src/MainWindow.cpp \
	src/ProfileEditorWindow.cpp \
	src/ProfileParser.cpp \
	src/main.cpp


HEADERS += \
	src/EqApoConfig.h \
	src/Filter.h \
	src/FrequencyResponse.h \
	src/FrequencyResponseWidget.h \
	src/MainWindow.h \
	src/ProfileEditorWindow.h \
	src/ProfileParser.h \
	src/version.h

