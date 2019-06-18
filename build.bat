@echo off
copy %1\7zsfx.exe 7z\
cd 7z
7za a archive.7z .\*
echo ;!@Install@!UTF-8!>>config.txt
echo RunProgram="7zsfx.exe">>config.txt
echo ;!@InstallEnd@!>>config.txt
cd ..
md build
setlocal EnableDelayedExpansion
set name=7zsfx.exe
if %1==Debug set name=7zsfxd.exe
copy/b 7z\7zS.sfx+7z\config.txt+7z\archive.7z build\!name!
cd build
for /f %%i in ('dir /b !name!') do echo 文件已输出至 %%~dpnxi
cd ..
del 7z\7zsfx.exe 7z\config.txt 7z\archive.7z
