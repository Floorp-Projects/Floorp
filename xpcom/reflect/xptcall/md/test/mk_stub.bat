@echo off
@echo deleing old output
if exist stub_test.obj del stub_test.obj > NUL
if exist stub_test.ilk del stub_test.ilk > NUL
if exist *.pdb         del *.pdb         > NUL
if exist stub_test.exe del stub_test.exe > NUL

@echo building...
cl /nologo -Zi -DWIN32 stub_test.cpp