@echo off
echo deleting intermediate files
if exist *.obj  del *.obj > NUL
if exist *.ilk  del *.ilk > NUL
if exist *.pdb  del *.pdb > NUL