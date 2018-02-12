7-Zip 18.01 Sources
-------------------

7-Zip is a file archiver for Windows. 

7-Zip Copyright (C) 1999-2018 Igor Pavlov.


License Info
------------

7-Zip is free software distributed under the GNU LGPL 
(except for unRar code).
read License.txt for more infomation about license.

Notes about unRAR license:

Please check main restriction from unRar license:

   2. The unRAR sources may be used in any software to handle RAR
      archives without limitations free of charge, but cannot be used
      to re-create the RAR compression algorithm, which is proprietary.
      Distribution of modified unRAR sources in separate form or as a
      part of other software is permitted, provided that it is clearly
      stated in the documentation and source comments that the code may
      not be used to develop a RAR (WinRAR) compatible archiver.

In brief it means:
1) You can compile and use compiled files under GNU LGPL rules, since 
   unRAR license almost has no restrictions for compiled files.
   You can link these compiled files to LGPL programs.
2) You can fix bugs in source code and use compiled fixed version.
3) You can not use unRAR sources to re-create the RAR compression algorithm.


LZMA SDK
--------

This package also contains some files from LZMA SDK
You can download LZMA SDK from:
  http://www.7-zip.org/sdk.html
LZMA SDK is written and placed in the public domain by Igor Pavlov.


How to compile
--------------
To compile sources you need Visual C++ 6.0.
For compiling some files you also need 
new Platform SDK from Microsoft' Site:
http://www.microsoft.com/msdownload/platformsdk/sdkupdate/psdk-full.htm
or
http://www.microsoft.com/msdownload/platformsdk/sdkupdate/XPSP2FULLInstall.htm
or
http://www.microsoft.com/msdownload/platformsdk/sdkupdate/

If you use MSVC6, specify SDK directories at top of directories lists:
Tools / Options / Directories
  - Include files
  - Library files


To compile 7-Zip for AMD64 and IA64 you need:
  Windows Server 2003 SP1 Platform SDK from microsoft.com

Also you need Microsoft Macro Assembler:
  - ml.exe for x86 
  - ml64.exe for AMD64
You can use ml.exe from Windows SDK for Windows Vista or some other version.


Compiling under Unix/Linux
--------------------------
Check this site for Posix/Linux version:
http://sourceforge.net/projects/p7zip/


Notes:
------
7-Zip consists of COM modules (DLL files).
But 7-Zip doesn't use standard COM interfaces for creating objects.
Look at
7zip\UI\Client7z folder for example of using DLL files of 7-Zip. 
Some DLL files can use other DLL files from 7-Zip.
If you don't like it, you must use standalone version of DLL.
To compile standalone version of DLL you must include all used parts
to project and define some defs. 
For example, 7zip\Bundles\Format7z is a standalone version  of 7z.dll 
that works with 7z format. So you can use such DLL in your project 
without additional DLL files.


Description of 7-Zip sources package
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

DOC                Documentation
---
  7zFormat.txt   - 7z format description
  copying.txt    - GNU LGPL license
  unRarLicense.txt - License for unRAR part of source code
  src-history.txt  - Sources history
  Methods.txt    - Compression method IDs
  readme.txt     - Readme file
  lzma.txt       - LZMA compression description
  7zip.nsi       - installer script for NSIS
  7zip.wix       - installer script for WIX


Asm - Source code in Assembler (optimized code for CRC calculation and Intel-AES encryption)

C   - Source code in C

CPP - Source code in C++

Common            common files for C++ projects

Windows           common files for Windows related code

7zip

  Common          Common modules for 7-zip

  Archive         files related to archiving

  Bundle          Modules that are bundles of other modules (files)

    Alone         7za.exe: Standalone version of 7-Zip console that supports only 7z/xz/cab/zip/gzip/bzip2/tar.
    Alone7z       7zr.exe: Standalone version of 7-Zip console that supports only 7z (reduced version)
    Fm            Standalone version of 7-Zip File Manager
    Format7z            7za.dll:  .7z support
    Format7zExtract     7zxa.dll: .7z support, extracting only
    Format7zR           7zr.dll:  .7z support, reduced version
    Format7zExtractR    7zxr.dll: .7z support, reduced version, extracting only
    Format7zF           7z.dll:   all formats
    LzmaCon       lzma.exe: LZMA compression/decompression
    SFXCon        7zCon.sfx: Console 7z SFX module
    SFXWin        7z.sfx: Windows 7z SFX module
    SFXSetup      7zS.sfx: Windows 7z SFX module for Installers

  Compress        files for compression/decompression

  Crypto          files for encryption / decompression

  UI

    Agent         Intermediary modules for FAR plugin and Explorer plugin
    Client7z      Test application for 7za.dll 
    Common        Common UI files
    Console       7z.exe : Console version
    Explorer      7-zip.dll: 7-Zip Shell extension
    Far           plugin for Far Manager
    FileManager   7zFM.exe: 7-Zip File Manager
    GUI           7zG.exe: 7-Zip GUI version



---
Igor Pavlov
http://www.7-zip.org
