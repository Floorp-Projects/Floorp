@echo OFF

:: Move to Top Dir
cd ..\..\..\

:: Copy dlg's files from `subprojects\dlg' to `src\dlg'
IF NOT EXIST include\dlg (
	mkdir include\dlg
	COPY subprojects\dlg\include\dlg\dlg.h include\dlg
	COPY subprojects\dlg\include\dlg\output.h include\dlg
	COPY subprojects\dlg\src\dlg\dlg.c src\dlg\ )
