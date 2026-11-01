@echo off
setlocal
cd /d "%~dp0"
"%~dp0out\build\turbo-make\AMD_3DC.exe" %*
