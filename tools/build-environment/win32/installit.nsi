!cd mozilla-build

name "Mozilla Build Setup"
SetCompressor /SOLID lzma
OutFile "..\MozillaBuildSetup.exe"
InstallDir "C:\mozilla-build"

LicenseData "..\license.rtf"
Page license
Page directory
Page instfiles

Section "MozillaBuild"
  MessageBox MB_YESNO|MB_ICONQUESTION "This will delete everything in $INSTDIR. Do you want to continue?" /SD IDYES IDYES continue
  SetErrors
  return
  continue:
  SetOutPath $INSTDIR
  File /r *.*
SectionEnd
