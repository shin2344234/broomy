@echo off
rem Broom Mount build: MSVC Build Tools 2022 + the CMake and Ninja they bundle.
rem   build.bat          configure (first run) and build Release into build\, stage dist\
rem   build.bat clean    wipe build\ first
rem   build.bat check IN OUT   build and run the offline check of Broomy's
rem                      builders (tests\buildcheck.cpp says what IN and OUT hold)
rem   build.bat chartcheck IN OUT   the same for Broomy's charts (tests\chartcheck.cpp)
rem Call it by full quoted path from PowerShell; the space in the repo path
rem breaks a bare `cmd /c build.bat`.
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "HERE=%~dp0"

if "%1"=="clean" if exist "%HERE%build" rmdir /s /q "%HERE%build"

call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
  echo vcvars64.bat not found at "%VCVARS%"
  exit /b 1
)

if not exist "%HERE%build\build.ninja" (
  "%CMAKE%" -S "%HERE%." -B "%HERE%build" -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_BUILD_TYPE=Release
  if errorlevel 1 exit /b 1
)
if "%1"=="check" (
  "%CMAKE%" --build "%HERE%build" --target buildcheck
  if errorlevel 1 exit /b 1
  "%HERE%build\buildcheck.exe" %2 %3
  exit /b
)
if "%1"=="chartcheck" (
  "%CMAKE%" --build "%HERE%build" --target chartcheck
  if errorlevel 1 exit /b 1
  "%HERE%build\chartcheck.exe" %2 %3
  exit /b
)
"%CMAKE%" --build "%HERE%build"
if errorlevel 1 exit /b 1
echo.
echo staged in "%HERE%dist"
endlocal
