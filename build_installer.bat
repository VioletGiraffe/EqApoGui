@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Building EqApoGui Installer
echo ========================================

:: Set paths - these will be overridden by environment variables if set
if "%QT_DIR%"=="" set QT_DIR=C:\Qt\6.9.3\msvc2022_64
if "%INNO_SETUP%"=="" set INNO_SETUP=C:\Program Files (x86)\Inno Setup 6

set PATH=%QT_DIR%\bin;%PATH%

:: Verify Qt exists
if not exist "%QT_DIR%\bin\qmake.exe" (
	echo ERROR: Qt not found at %QT_DIR%
	exit /b 1
)

:: Verify Inno Setup exists
if not exist "%INNO_SETUP%\ISCC.exe" (
	echo ERROR: Inno Setup not found at "%INNO_SETUP%"
	exit /b 1
)

:: Add Qt to PATH

:: Create build directory
if exist build\ rmdir /s /q build
mkdir build
cd build

echo.
echo ========================================
echo Setting up MSVC environment...
echo ========================================

:: Try to find vcvarsall.bat in common VS 2022 locations
set "VCVARSALL="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
	set "VCVARSALL=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
	set "VCVARSALL=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
	set "VCVARSALL=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
)

if "%VCVARSALL%"=="" (
	echo ERROR: Could not find vcvarsall.bat for Visual Studio 2022
	cd ..
	exit /b 1
)

echo Found: %VCVARSALL%
call "%VCVARSALL%" x64
if errorlevel 1 (
	echo ERROR: vcvarsall.bat failed
	cd ..
	exit /b 1
)

echo.
echo ========================================
echo Running qmake...
echo ========================================
qmake ..\EqApoGui.pro -tp vc -r "CONFIG+=release"
if errorlevel 1 (
	echo ERROR: qmake failed
	cd ..
	exit /b 1
)

echo.
echo ========================================
echo Building with MSBuild...
echo ========================================
msbuild.exe EqApoGui.vcxproj /p:Configuration=Release /p:Platform=x64
if errorlevel 1 (
	echo ERROR: MSBuild failed
	cd ..
	exit /b 1
)

:: Create deployment directory
if exist deploy rmdir /s /q deploy
mkdir deploy

:: Copy executable
echo.
echo ========================================
echo Copying executable...
echo ========================================
copy release\EqApoGui.exe deploy\
if errorlevel 1 (
	echo ERROR: Failed to copy executable
	cd ..
	exit /b 1
)

:: Run windeployqt
echo.
echo ========================================
echo Running windeployqt...
echo ========================================
cd deploy
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw --no-network --no-svg --skip-plugin-types imageformats EqApoGui.exe
if errorlevel 1 (
	echo ERROR: windeployqt failed
	cd ..\..
	exit /b 1
)

:: Cleanup unnecessary DLLs
echo.
echo ========================================
echo Cleaning up unnecessary files...
echo ========================================

if exist opengl32sw.dll del opengl32sw.dll
if exist d3dcompiler_*.dll del d3dcompiler_*.dll
if exist d3dcompiler_*.dll del d3dcompiler_*.dll
for %%F in (
	opengl32sw.dll
	d3dcompiler_*.dll
	dx*compiler.dll
	dxil.dll
) do if exist %%F del %%F

move vc_redist.x64.exe ..\

cd ..\..

:: Run Inno Setup
echo.
echo ========================================
echo Creating installer with Inno Setup...
echo ========================================
"%INNO_SETUP%\ISCC.exe" installer.iss
if errorlevel 1 (
	echo ERROR: Inno Setup failed
	exit /b 1
)

echo ========================================
echo Build completed successfully!
echo ========================================
echo.

exit /b 0