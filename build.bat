@echo off
copy Release\7zsfx.exe 7z\
cd 7z
7za a archive.7z .\*
echo ;!@Install@!UTF-8!>>config.txt
echo RunProgram="7zsfx.exe">>config.txt
echo ;!@InstallEnd@!>>config.txt
md ..\build
copy/b 7zS.sfx+config.txt+archive.7z ..\build\7zsfx.exe
del 7zsfx.exe config.txt archive.7z
