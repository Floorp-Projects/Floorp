; Any copyright is dedicated to the Public Domain.
; http://creativecommons.org/publicdomain/zero/1.0/

; Simple driver for HttpPostFile, passes command line args to HttpPostFile::Post and
; writes the result string to a file for automated checking.
; Always specifies Content-Type: application/json
;
; Usage: posttest /postfile=postfile.json /url=http://example.com /resultfile=result.txt

!include LogicLib.nsh
!include FileFunc.nsh

OutFile "postdriver.exe"
RequestExecutionLevel user
ShowInstDetails show
Unicode true

!addplugindir ..\..\..\Plugins

Var PostFileArg
Var UrlArg
Var ResultFileArg
Var ResultString

Section

StrCpy $ResultString "error getting command line arguments"

ClearErrors
${GetParameters} $0
IfErrors done

ClearErrors
${GetOptions} " $0" " /postfile=" $PostFileArg
IfErrors done

${GetOptions} " $0" " /url=" $UrlArg
IfErrors done

${GetOptions} " $0" " /resultfile=" $ResultFileArg
IfErrors done

DetailPrint "POST File = $PostFileArg"
DetailPrint "URL = $UrlArg"
DetailPrint "Result File = $ResultFileArg"

StrCpy $ResultString "error running plugin"
HttpPostFile::Post $PostFileArg "Content-Type: application/json$\r$\n" $UrlArg
Pop $ResultString

done:
${If} $ResultString != "success"
DetailPrint $ResultString
${EndIf}

ClearErrors
FileOpen $0 $ResultFileArg "w"
${Unless} ${Errors}
FileWrite $0 $ResultString
FileClose $0
${EndUnless}

SectionEnd
