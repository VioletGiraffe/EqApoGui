QT = core gui widgets

CONFIG += c++2a

msvc*{
	QMAKE_CXXFLAGS += /MP
	QMAKE_CXXFLAGS_WARN_ON = /W4
}

SOURCES += \
	EqApoConfig.cpp \
	MainWindow.cpp \
	main.cpp

HEADERS += \
	EqApoConfig.h \
	MainWindow.h
