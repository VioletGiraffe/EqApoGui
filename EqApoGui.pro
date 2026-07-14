QT = core gui widgets

CONFIG += c++latest

msvc*{
	QMAKE_CXXFLAGS += /MP
	QMAKE_CXXFLAGS_WARN_ON = /W4
}

include(3rdparty/3rdparty.pri)

SOURCES += \
	src/EqApoConfig.cpp \
	src/Filter.cpp \
	src/FrequencyResponseWidget.cpp \
	src/MainWindow.cpp \
	src/ProfileEditorWindow.cpp \
	src/ProfileParser.cpp \
	src/main.cpp \
	src/spectrum/SpectrumAnalyzer.cpp \
	src/spectrum/WasapiLoopbackCapture.cpp


HEADERS += \
	src/EqApoConfig.h \
	src/Filter.h \
	src/FrequencyResponseWidget.h \
	src/MainWindow.h \
	src/ProfileEditorWindow.h \
	src/ProfileParser.h \
	src/spectrum/SpectrumAnalyzer.h \
	src/spectrum/WasapiLoopbackCapture.h \
	src/version.h

LIBS += -lOle32
