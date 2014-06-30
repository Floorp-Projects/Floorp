@echo off
@echo deleing old output
if exist invoke_test.obj del invoke_test.obj > NUL
if exist invoke_test.ilk del invoke_test.ilk > NUL
if exist *.pdb           del *.pdb           > NUL
if exist invoke_test.exe del invoke_test.exe > NUL

@echo building...
cl /nologo -Zi -DWIN32 invoke_test.cpp