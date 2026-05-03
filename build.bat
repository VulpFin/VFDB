@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "ACTION=%~1"
if "%ACTION%"=="" set "ACTION=all"

if defined VFDB_PYTHON (
  set "PY=%VFDB_PYTHON%"
) else if exist "%ROOT%\.wvenv\Scripts\python.exe" (
  set "PY=%ROOT%\.wvenv\Scripts\python.exe"
) else if exist "%ROOT%\.venv-win\Scripts\python.exe" (
  set "PY=%ROOT%\.venv-win\Scripts\python.exe"
) else (
  set "PY=python"
)

echo VFDB %ACTION%
echo Root: %ROOT%
echo Python: %PY%
echo.

if /I "%ACTION%"=="deps" goto deps
if /I "%ACTION%"=="build" goto build
if /I "%ACTION%"=="test" goto test
if /I "%ACTION%"=="smoke" goto smoke
if /I "%ACTION%"=="wheel" goto wheel
if /I "%ACTION%"=="clean" goto clean
if /I "%ACTION%"=="all" goto all
goto usage

:deps
"%PY%" -m pip install --upgrade wheel pytest build
if errorlevel 1 exit /b 1
goto done

:build
"%PY%" setup.py build_ext --inplace
if errorlevel 1 exit /b 1
goto done

:test
"%PY%" -c "import pytest" >NUL 2>NUL
if errorlevel 1 (
  echo pytest is missing; installing test dependency...
  "%PY%" -m pip install pytest
  if errorlevel 1 exit /b 1
)
"%PY%" -m pytest -q
if errorlevel 1 exit /b 1
goto done

:smoke
"%PY%" python\full_db_test.py
if errorlevel 1 exit /b 1
goto done

:wheel
"%PY%" -c "import build" >NUL 2>NUL
if errorlevel 1 (
  echo build is missing; installing wheel build dependency...
  "%PY%" -m pip install build
  if errorlevel 1 exit /b 1
)
"%PY%" -m build --wheel
if errorlevel 1 exit /b 1
goto done

:clean
for %%D in (build dist .pytest_cache vfdb.egg-info python\dist python\test tests\__pycache__ python\__pycache__ python\vfdb\__pycache__) do (
  if exist "%ROOT%\%%D" rmdir /s /q "%ROOT%\%%D"
)
for %%F in ("%ROOT%\python\vfdb\_native*.pyd" "%ROOT%\python\vfdb\_native*.lib" "%ROOT%\python\vfdb\_native*.exp" "%ROOT%\python\vfdb\vfdb_core.lib") do (
  if exist %%F del /q %%F
)
for %%F in ("%ROOT%\*.vfdb" "%ROOT%\*.vfdb.meta" "%ROOT%\*.vfheap" "%ROOT%\temp\*.vfdb" "%ROOT%\temp\*.vfdb.meta" "%ROOT%\temp\*.vfheap") do (
  if exist %%F del /q %%F
)
goto done

:all
call "%~f0" build
if errorlevel 1 exit /b 1
call "%~f0" test
if errorlevel 1 exit /b 1
call "%~f0" smoke
if errorlevel 1 exit /b 1
goto done

:usage
echo Usage: build.bat [deps^|build^|test^|smoke^|wheel^|clean^|all]
exit /b 2

:done
echo.
echo VFDB %ACTION% complete.
exit /b 0
