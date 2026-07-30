@echo off

echo Building ArtakhaV2 Release...
cmake --build out/build/x64-Debug --config Release

echo Running ArtakhaV2 Release...
.\out\build\x64-Debug\Release\ArtakhaV2.exe

pause