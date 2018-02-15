;--------------------------------
;Defines

!define VERSION_MAJOR 18
!define VERSION_MINOR 01
!define VERSION_POSTFIX_FULL ""
!ifdef WIN64
!ifdef IA64
!define VERSION_SYS_POSTFIX_FULL " for Windows IA-64"
!else
!define VERSION_SYS_POSTFIX_FULL " for Windows x64"
!endif
!else
!define VERSION_SYS_POSTFIX_FULL ""
!endif
!define NAME_FULL "7-Zip ${VERSION_MAJOR}.${VERSION_MINOR}${VERSION_POSTFIX_FULL}${VERSION_SYS_POSTFIX_FULL}"
!define VERSION_POSTFIX ""
!ifdef WIN64
!ifdef IA64
!define VERSION_SYS_POSTFIX "-ia64"
!else
!define VERSION_SYS_POSTFIX "-x64"
!endif
!else
!define VERSION_SYS_POSTFIX ""
!endif



!define FM_LINK "7-Zip File Manager.lnk"
!define HELP_LINK "7-Zip Help.lnk"

!define CLSID_CONTEXT_MENU {23170F69-40C1-278A-1000-000100020000}

#!define NO_COMPRESSION

  !include "Library.nsh"
  !include "MUI.nsh"

;--------------------------------
;Configuration

  ;General
  Name "${NAME_FULL}"
  BrandingText "www.7-zip.org"
  OutFile "..\7z${VERSION_MAJOR}${VERSION_MINOR}${VERSION_POSTFIX}${VERSION_SYS_POSTFIX}.exe"

  ;Folder selection page
  InstallDir "$PROGRAMFILES\7-Zip"
  
  ;Get install folder from registry if available
  InstallDirRegKey HKCU "Software\7-Zip" "Path32"

  ;Compressor
!ifndef NO_COMPRESSION
  SetCompressor /solid lzma
  ; SetCompressorFilter 1
!ifdef IA64
  SetCompressorDictSize 8
!else
  SetCompressorDictSize 4
!endif
!else
  SetCompress off
!endif


;--------------------------------
;Variables

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  #!insertmacro MUI_PAGE_LICENSE "License.txt"
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH
  
;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Reserve Files
  
  ;These files should be inserted before other files in the data block
  ;Keep these lines before any File command
  ;Only for solid compression (by default, solid compression is enabled for BZIP2 and LZMA)
  
  !insertmacro MUI_RESERVEFILE_LANGDLL

;--------------------------------
;Installer Sections

Section
  !ifndef WIN64
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\7zFMn.exe"
  !endif

  # delete old unwanted files

  Delete $INSTDIR\7zFMn.exe
  Delete $INSTDIR\7zgn.exe
  Delete $INSTDIR\7zn.exe
  Delete $INSTDIR\7zan.exe
  Delete $INSTDIR\7zC.sfx
  Delete $INSTDIR\7-zipn.dll
  Delete $INSTDIR\7zipDoc.txt
  Delete $INSTDIR\file_id.diz
  Delete $INSTDIR\7zip_pad.xml
  Delete $INSTDIR\copying.txt

  Delete $INSTDIR\Codecs\7zAES.dll
  Delete $INSTDIR\Codecs\AES.dll
  Delete $INSTDIR\Codecs\Branch.dll
  Delete $INSTDIR\Codecs\BZip2.dll
  Delete $INSTDIR\Codecs\Copy.dll
  Delete $INSTDIR\Codecs\Deflate.dll
  Delete $INSTDIR\Codecs\Implode.dll
  Delete $INSTDIR\Codecs\LZMA.dll
  Delete $INSTDIR\Codecs\PPMD.dll
  Delete $INSTDIR\Codecs\Rar29.dll
  Delete $INSTDIR\Codecs\Swap.dll

  RMDir $INSTDIR\Codecs

  Delete $INSTDIR\Formats\7z.dll
  Delete $INSTDIR\Formats\arj.dll
  Delete $INSTDIR\Formats\bz2.dll
  Delete $INSTDIR\Formats\cab.dll
  Delete $INSTDIR\Formats\chm.dll
  Delete $INSTDIR\Formats\cpio.dll
  Delete $INSTDIR\Formats\deb.dll
  Delete $INSTDIR\Formats\gz.dll
  Delete $INSTDIR\Formats\iso.dll
  Delete $INSTDIR\Formats\lzh.dll
  Delete $INSTDIR\Formats\nsis.dll
  Delete $INSTDIR\Formats\rar.dll
  Delete $INSTDIR\Formats\rpm.dll
  Delete $INSTDIR\Formats\split.dll
  Delete $INSTDIR\Formats\tar.dll
  Delete $INSTDIR\Formats\z.dll
  Delete $INSTDIR\Formats\zip.dll

  RMDir $INSTDIR\Formats

  Delete $INSTDIR\Lang\no.txt

  # install files

  SetOutPath $INSTDIR

  File descript.ion
  File History.txt
  File License.txt
  File readme.txt
  File 7-zip.chm

  # File 7-zip.dll

  File 7z.dll
  File 7zFM.exe
  File 7zG.exe
  File 7z.exe
  File 7z.sfx
  File 7zCon.sfx

  SetOutPath $INSTDIR\Lang

  File en.ttt
  File af.txt
  File an.txt
  File ar.txt
  File ast.txt
  File az.txt
  File ba.txt
  File be.txt
  File bg.txt
  File bn.txt
  File br.txt
  File ca.txt
  File co.txt
  File cs.txt
  File cy.txt
  File da.txt
  File de.txt
  File el.txt
  File eo.txt
  File es.txt
  File et.txt
  File eu.txt
  File ext.txt
  File fa.txt
  File fi.txt
  File fr.txt
  File fur.txt
  File fy.txt
  File ga.txt
  File gl.txt
  File gu.txt
  File he.txt
  File hi.txt
  File hr.txt
  File hu.txt
  File hy.txt
  File id.txt
  File io.txt
  File is.txt
  File it.txt
  File ja.txt
  File ka.txt
  File kaa.txt
  File kk.txt
  File ko.txt
  File ku.txt
  File ku-ckb.txt
  File ky.txt
  File lij.txt
  File lt.txt
  File lv.txt
  File mk.txt
  File mn.txt
  File mng.txt
  File mng2.txt
  File mr.txt
  File ms.txt
  File ne.txt
  File nl.txt
  File nb.txt
  File nn.txt
  File pa-in.txt
  File pl.txt
  File ps.txt
  File pt.txt
  File pt-br.txt
  File ro.txt
  File ru.txt
  File sa.txt
  File si.txt
  File sk.txt
  File sl.txt
  File sq.txt
  File sr-spc.txt
  File sr-spl.txt
  File sv.txt
  File ta.txt
  File th.txt
  File tr.txt
  File tt.txt
  File ug.txt
  File uk.txt
  File uz.txt
  File va.txt
  File vi.txt
  File yo.txt
  File zh-cn.txt
  File zh-tw.txt

  SetOutPath $INSTDIR

  # delete "current user" menu items

  Delete "$SMPROGRAMS\7-Zip\${FM_LINK}"
  Delete "$SMPROGRAMS\7-Zip\${HELP_LINK}"
  RMDir $SMPROGRAMS\7-Zip

  # set "all users" mode

  SetShellVarContext all

  !insertmacro InstallLib DLL NOTSHARED REBOOT_NOTPROTECTED 7-zip.dll $INSTDIR\7-zip.dll $INSTDIR

  ClearErrors

  # create start menu icons

  SetOutPath $INSTDIR

  CreateDirectory $SMPROGRAMS\7-Zip
  CreateShortCut "$SMPROGRAMS\7-Zip\${FM_LINK}" $INSTDIR\7zFM.exe
  CreateShortCut "$SMPROGRAMS\7-Zip\${HELP_LINK}" $INSTDIR\7-zip.chm

  IfErrors 0 noScErrors

  SetShellVarContext current

  CreateDirectory $SMPROGRAMS\7-Zip
  CreateShortCut "$SMPROGRAMS\7-Zip\${FM_LINK}" $INSTDIR\7zFM.exe
  CreateShortCut "$SMPROGRAMS\7-Zip\${HELP_LINK}" $INSTDIR\7-zip.chm

noScErrors:

  # store install folder

  WriteRegStr HKLM Software\7-Zip Path32 $INSTDIR
  WriteRegStr HKLM Software\7-Zip Path   $INSTDIR
  WriteRegStr HKCU Software\7-Zip Path32 $INSTDIR
  WriteRegStr HKCU Software\7-Zip Path   $INSTDIR

  # write reg entries

  WriteRegStr HKCR "CLSID\${CLSID_CONTEXT_MENU}" "" "7-Zip Shell Extension"
  WriteRegStr HKCR "CLSID\${CLSID_CONTEXT_MENU}\InprocServer32" "" $INSTDIR\7-zip.dll
  WriteRegStr HKCR "CLSID\${CLSID_CONTEXT_MENU}\InprocServer32" ThreadingModel Apartment
  DeleteRegValue HKCR "CLSID\${CLSID_CONTEXT_MENU}\InprocServer32" "InprocServer32"

  WriteRegStr HKCR "*\shellex\ContextMenuHandlers\7-Zip" "" "${CLSID_CONTEXT_MENU}"
  WriteRegStr HKCR "Directory\shellex\ContextMenuHandlers\7-Zip" "" "${CLSID_CONTEXT_MENU}"
  WriteRegStr HKCR "Folder\shellex\ContextMenuHandlers\7-Zip" "" "${CLSID_CONTEXT_MENU}"

  WriteRegStr HKCR "Directory\shellex\DragDropHandlers\7-Zip" "" "${CLSID_CONTEXT_MENU}"
  WriteRegStr HKCR "Drive\shellex\DragDropHandlers\7-Zip" "" "${CLSID_CONTEXT_MENU}"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" "${CLSID_CONTEXT_MENU}" "7-Zip Shell Extension"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\7zFM.exe" "" $INSTDIR\7zFM.exe
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\7zFM.exe" Path $INSTDIR

  # create uninstaller

  WriteRegStr HKLM Software\Microsoft\Windows\CurrentVersion\Uninstall\7-Zip DisplayName "${NAME_FULL}"
  WriteRegStr HKLM Software\Microsoft\Windows\CurrentVersion\Uninstall\7-Zip UninstallString '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKLM Software\Microsoft\Windows\CurrentVersion\Uninstall\7-Zip NoModify 1
  WriteRegDWORD HKLM Software\Microsoft\Windows\CurrentVersion\Uninstall\7-Zip NoRepair 1

  WriteUninstaller $INSTDIR\Uninstall.exe

  !ifdef WIN64
  ExecWait 'regsvr32 /s "$INSTDIR\7-zip.dll"'
  !endif

SectionEnd
 

;--------------------------------
;Installer Functions

/*
Function .onInit

  !insertmacro MUI_LANGDLL_DISPLAY

FunctionEnd
*/




;--------------------------------
;Uninstaller Section

Section Uninstall

  ExecWait 'regsvr32 /u /s "$INSTDIR\7-zip.dll"'

  # delete files

  Delete $INSTDIR\descript.ion
  Delete $INSTDIR\History.txt
  Delete $INSTDIR\License.txt
  Delete $INSTDIR\readme.txt
  Delete $INSTDIR\7-zip.chm

  Delete $INSTDIR\7z.dll
  Delete $INSTDIR\7zFM.exe
  Delete $INSTDIR\7zG.exe
  Delete $INSTDIR\7z.exe
  Delete $INSTDIR\7z.sfx
  Delete $INSTDIR\7zCon.sfx

  Delete $INSTDIR\Lang\en.ttt
  Delete $INSTDIR\Lang\af.txt
  Delete $INSTDIR\Lang\an.txt
  Delete $INSTDIR\Lang\ar.txt
  Delete $INSTDIR\Lang\ast.txt
  Delete $INSTDIR\Lang\az.txt
  Delete $INSTDIR\Lang\ba.txt
  Delete $INSTDIR\Lang\be.txt
  Delete $INSTDIR\Lang\bg.txt
  Delete $INSTDIR\Lang\bn.txt
  Delete $INSTDIR\Lang\br.txt
  Delete $INSTDIR\Lang\ca.txt
  Delete $INSTDIR\Lang\co.txt
  Delete $INSTDIR\Lang\cs.txt
  Delete $INSTDIR\Lang\cy.txt
  Delete $INSTDIR\Lang\da.txt
  Delete $INSTDIR\Lang\de.txt
  Delete $INSTDIR\Lang\el.txt
  Delete $INSTDIR\Lang\eo.txt
  Delete $INSTDIR\Lang\es.txt
  Delete $INSTDIR\Lang\et.txt
  Delete $INSTDIR\Lang\eu.txt
  Delete $INSTDIR\Lang\ext.txt
  Delete $INSTDIR\Lang\fa.txt
  Delete $INSTDIR\Lang\fi.txt
  Delete $INSTDIR\Lang\fr.txt
  Delete $INSTDIR\Lang\fur.txt
  Delete $INSTDIR\Lang\fy.txt
  Delete $INSTDIR\Lang\ga.txt
  Delete $INSTDIR\Lang\gl.txt
  Delete $INSTDIR\Lang\gu.txt
  Delete $INSTDIR\Lang\he.txt
  Delete $INSTDIR\Lang\hi.txt
  Delete $INSTDIR\Lang\hr.txt
  Delete $INSTDIR\Lang\hu.txt
  Delete $INSTDIR\Lang\hy.txt
  Delete $INSTDIR\Lang\id.txt
  Delete $INSTDIR\Lang\io.txt
  Delete $INSTDIR\Lang\is.txt
  Delete $INSTDIR\Lang\it.txt
  Delete $INSTDIR\Lang\ja.txt
  Delete $INSTDIR\Lang\ka.txt
  Delete $INSTDIR\Lang\kaa.txt
  Delete $INSTDIR\Lang\kk.txt
  Delete $INSTDIR\Lang\ko.txt
  Delete $INSTDIR\Lang\ku.txt
  Delete $INSTDIR\Lang\ku-ckb.txt
  Delete $INSTDIR\Lang\ky.txt
  Delete $INSTDIR\Lang\lij.txt
  Delete $INSTDIR\Lang\lt.txt
  Delete $INSTDIR\Lang\lv.txt
  Delete $INSTDIR\Lang\mk.txt
  Delete $INSTDIR\Lang\mn.txt
  Delete $INSTDIR\Lang\mng.txt
  Delete $INSTDIR\Lang\mng2.txt
  Delete $INSTDIR\Lang\mr.txt
  Delete $INSTDIR\Lang\ms.txt
  Delete $INSTDIR\Lang\ne.txt
  Delete $INSTDIR\Lang\nl.txt
  Delete $INSTDIR\Lang\nb.txt
  Delete $INSTDIR\Lang\nn.txt
  Delete $INSTDIR\Lang\pa-in.txt
  Delete $INSTDIR\Lang\pl.txt
  Delete $INSTDIR\Lang\ps.txt
  Delete $INSTDIR\Lang\pt.txt
  Delete $INSTDIR\Lang\pt-br.txt
  Delete $INSTDIR\Lang\ro.txt
  Delete $INSTDIR\Lang\ru.txt
  Delete $INSTDIR\Lang\sa.txt
  Delete $INSTDIR\Lang\si.txt
  Delete $INSTDIR\Lang\sk.txt
  Delete $INSTDIR\Lang\sl.txt
  Delete $INSTDIR\Lang\sq.txt
  Delete $INSTDIR\Lang\sr.txt
  Delete $INSTDIR\Lang\sr-spc.txt
  Delete $INSTDIR\Lang\sr-spl.txt
  Delete $INSTDIR\Lang\sv.txt
  Delete $INSTDIR\Lang\ta.txt
  Delete $INSTDIR\Lang\th.txt
  Delete $INSTDIR\Lang\tr.txt
  Delete $INSTDIR\Lang\tt.txt
  Delete $INSTDIR\Lang\ug.txt
  Delete $INSTDIR\Lang\uk.txt
  Delete $INSTDIR\Lang\uz.txt
  Delete $INSTDIR\Lang\va.txt
  Delete $INSTDIR\Lang\vi.txt
  Delete $INSTDIR\Lang\vr.txt
  Delete $INSTDIR\Lang\yo.txt
  Delete $INSTDIR\Lang\zh-cn.txt
  Delete $INSTDIR\Lang\zh-tw.txt

  RMDir $INSTDIR\Lang

  Delete /REBOOTOK $INSTDIR\7-zip.dll
  Delete $INSTDIR\Uninstall.exe

  RMDir $INSTDIR

  # delete start menu entires

  SetShellVarContext all

  # ClearErrors

  Delete "$SMPROGRAMS\7-Zip\${FM_LINK}"
  Delete "$SMPROGRAMS\7-Zip\${HELP_LINK}"
  RMDir $SMPROGRAMS\7-Zip

  # IfErrors 0 noScErrors

  SetShellVarContext current

  Delete "$SMPROGRAMS\7-Zip\${FM_LINK}"
  Delete "$SMPROGRAMS\7-Zip\${HELP_LINK}"
  RMDir $SMPROGRAMS\7-Zip

  # noScErrors:


  # delete registry entries

  DeleteRegKey HKLM Software\Microsoft\Windows\CurrentVersion\Uninstall\7-Zip
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\App Paths\7zFM.exe"

  DeleteRegKey HKLM Software\7-Zip
  DeleteRegKey HKCU Software\7-Zip

  DeleteRegKey HKCR CLSID\${CLSID_CONTEXT_MENU}

  DeleteRegKey HKCR *\shellex\ContextMenuHandlers\7-Zip
  DeleteRegKey HKCR Directory\shellex\ContextMenuHandlers\7-Zip
  DeleteRegKey HKCR Folder\shellex\ContextMenuHandlers\7-Zip

  DeleteRegKey HKCR Drive\shellex\DragDropHandlers\7-Zip
  DeleteRegKey HKCR Directory\shellex\DragDropHandlers\7-Zip
  DeleteRegKey HKCR Folder\shellex\DragDropHandlers\7-Zip

  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" "${CLSID_CONTEXT_MENU}"

  DeleteRegKey HKCR 7-Zip.001
  DeleteRegKey HKCR 7-Zip.7z
  DeleteRegKey HKCR 7-Zip.arj
  DeleteRegKey HKCR 7-Zip.bz2
  DeleteRegKey HKCR 7-Zip.bzip2
  DeleteRegKey HKCR 7-Zip.tbz
  DeleteRegKey HKCR 7-Zip.tbz2
  DeleteRegKey HKCR 7-Zip.cab
  DeleteRegKey HKCR 7-Zip.cpio
  DeleteRegKey HKCR 7-Zip.deb
  DeleteRegKey HKCR 7-Zip.dmg
  DeleteRegKey HKCR 7-Zip.fat
  DeleteRegKey HKCR 7-Zip.gz
  DeleteRegKey HKCR 7-Zip.gzip
  DeleteRegKey HKCR 7-Zip.hfs
  DeleteRegKey HKCR 7-Zip.iso
  DeleteRegKey HKCR 7-Zip.lha
  DeleteRegKey HKCR 7-Zip.lzh
  DeleteRegKey HKCR 7-Zip.lzma
  DeleteRegKey HKCR 7-Zip.ntfs
  DeleteRegKey HKCR 7-Zip.rar
  DeleteRegKey HKCR 7-Zip.rpm
  DeleteRegKey HKCR 7-Zip.split
  DeleteRegKey HKCR 7-Zip.squashfs
  DeleteRegKey HKCR 7-Zip.swm
  DeleteRegKey HKCR 7-Zip.tar
  DeleteRegKey HKCR 7-Zip.taz
  DeleteRegKey HKCR 7-Zip.tgz
  DeleteRegKey HKCR 7-Zip.tpz
  DeleteRegKey HKCR 7-Zip.txz
  DeleteRegKey HKCR 7-Zip.vhd
  DeleteRegKey HKCR 7-Zip.wim
  DeleteRegKey HKCR 7-Zip.xar
  DeleteRegKey HKCR 7-Zip.xz
  DeleteRegKey HKCR 7-Zip.z
  DeleteRegKey HKCR 7-Zip.zip

SectionEnd
