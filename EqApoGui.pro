QT = core gui widgets

CONFIG += c++latest

msvc*{
	QMAKE_CXXFLAGS += /MP
	QMAKE_CXXFLAGS_WARN_ON = /W4
}

SOURCES += \
	src/EqApoConfig.cpp \
	src/MainWindow.cpp \
	src/main.cpp

HEADERS += \
	src/EqApoConfig.h \
	src/MainWindow.h \
	src/version.h
