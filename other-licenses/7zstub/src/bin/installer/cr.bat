del archive.7z
del archive.exe
..\7zr a archive.7z ..\7zr.exe -mx -mf=BCJ2
copy /b ..\7zSD.sfx + config.txt + archive.7z archive.exe

