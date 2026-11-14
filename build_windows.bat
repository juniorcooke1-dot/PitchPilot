@echo off
rem Builds PitchPilot (VST3) and installs it for FL Studio.
rem Needs: Visual Studio 2022 (Desktop development with C++) and CMake.
rem Run from "x64 Native Tools Command Prompt for VS 2022", AS ADMINISTRATOR (for the install step).

cmake -B build -G "Visual Studio 17 2022" -A x64 || goto :err
cmake --build build --config Release --parallel || goto :err

echo.
echo Installing to C:\Program Files\Common Files\VST3 ...
xcopy /E /I /Y "build\PitchPilot_artefacts\Release\VST3\PitchPilot.vst3" "C:\Program Files\Common Files\VST3\PitchPilot.vst3" || goto :err

echo.
echo Done. In FL Studio: Options ^> Manage plugins ^> Find installed plugins, then add PitchPilot to a mixer insert.
goto :eof

:err
echo.
echo Something failed - see the messages above.
exit /b 1
