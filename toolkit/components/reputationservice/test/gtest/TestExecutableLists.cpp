/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/3.0/ */

#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "nsLocalFileCommon.h"
#include "ApplicationReputation.h"

// PLEASE read the comment in ApplicationReputation.cpp before modifying this
// list.
static const char* const kTestFileExtensions[] = {
    ".action",  // Nac script
    ".ad",      // Windows (ignored for app rep)
    ".ade",     // MS Access
    ".adp",     // MS Access
    ".air",     // Adobe Air (ignored for app rep)
    ".apk",     // Android package
    ".app",     // Executable application
    ".applescript",
    ".application",  // MS ClickOnce
    ".appref-ms",    // MS ClickOnce
    ".as",           // Mac archive
    ".asp",          // Windows Server script
    ".asx",          // Windows Media Player
    ".bas",          // Basic script
    ".bash",         // Linux shell
    ".bat",          // Windows shell
    ".bin",
    ".btapp",          // uTorrent and Transmission
    ".btinstall",      // uTorrent and Transmission
    ".btkey",          // uTorrent and Transmission
    ".btsearch",       // uTorrent and Transmission
    ".btskin",         // uTorrent and Transmission
    ".bz",             // Linux archive (bzip)
    ".bz2",            // Linux archive (bzip2)
    ".bzip2",          // Linux archive (bzip2)
    ".cab",            // Windows archive
    ".caction",        // Automator action
    ".cdr",            // Mac disk image
    ".cfg",            // Windows
    ".chi",            // Windows Help
    ".chm",            // Windows Help
    ".class",          // Java
    ".cmd",            // Windows executable
    ".com",            // Windows executable
    ".command",        // Mac script
    ".configprofile",  // Configuration file for Apple systems
    ".cpgz",           // Mac archive
    ".cpi",            // Control Panel Item. Executable used for adding icons
                       // to Control Panel
    ".cpl",            // Windows executable
    ".crt",            // Windows signed certificate
    ".crx",            // Chrome extensions
    ".csh",            // Linux shell
    ".dart",           // Mac disk image
    ".dc42",           // Apple DiskCopy Image
    ".deb",            // Linux package
    ".definition",     // Automator action
    ".desktop",        // A shortcut that runs other files
    ".dex",            // Android
    ".dht",            // HTML
    ".dhtm",           // HTML
    ".dhtml",          // HTML
    ".diskcopy42",     // Apple DiskCopy Image
    ".dll",            // Windows executable
    ".dmg",            // Mac disk image
    ".dmgpart",        // Mac disk image
    ".doc",            // MS Office
    ".docb",           // MS Office
    ".docm",           // MS Word
    ".docx",           // MS Word
    ".dot",            // MS Word
    ".dotm",           // MS Word
    ".dott",           // MS Office
    ".dotx",           // MS Word
    ".drv",            // Windows driver
    ".dvdr",           // Mac Disk image
    ".dylib",          // Mach object dynamic library file
    ".efi",            // Firmware
    ".eml",            // MS Outlook
    ".exe",            // Windows executable
    ".fon",            // Windows font
    ".fxp",            // MS FoxPro
    ".gadget",         // Windows
    ".grp",            // Windows
    ".gz",             // Linux archive (gzip)
    ".gzip",           // Linux archive (gzip)
    ".hfs",            // Mac disk image
    ".hlp",            // Windows Help
    ".hqx",            // Mac archive
    ".hta",            // HTML trusted application
    ".htm", ".html",
    ".htt",                // MS HTML template
    ".img",                // Mac disk image
    ".imgpart",            // Mac disk image
    ".inf",                // Windows installer
    ".ini",                // Generic config file
    ".ins",                // IIS config
    ".internetconnect",    // Configuration file for Apple system
    ".iso",                // CD image
    ".isp",                // IIS config
    ".jar",                // Java
    ".jnlp",               // Java
    ".js",                 // JavaScript script
    ".jse",                // JScript
    ".ksh",                // Linux shell
    ".lnk",                // Windows
    ".local",              // Windows
    ".mad",                // MS Access
    ".maf",                // MS Access
    ".mag",                // MS Access
    ".mam",                // MS Access
    ".manifest",           // Windows
    ".maq",                // MS Access
    ".mar",                // MS Access
    ".mas",                // MS Access
    ".mat",                // MS Access
    ".mau",                // Media attachment
    ".mav",                // MS Access
    ".maw",                // MS Access
    ".mda",                // MS Access
    ".mdb",                // MS Access
    ".mde",                // MS Access
    ".mdt",                // MS Access
    ".mdw",                // MS Access
    ".mdz",                // MS Access
    ".mht",                // MS HTML
    ".mhtml",              // MS HTML
    ".mim",                // MS Mail
    ".mmc",                // MS Office
    ".mobileconfig",       // Configuration file for Apple systems
    ".mof",                // Windows
    ".mpkg",               // Mac installer
    ".msc",                // Windows executable
    ".msg",                // MS Outlook
    ".msh",                // Windows shell
    ".msh1",               // Windows shell
    ".msh1xml",            // Windows shell
    ".msh2",               // Windows shell
    ".msh2xml",            // Windows shell
    ".mshxml",             // Windows
    ".msi",                // Windows installer
    ".msp",                // Windows installer
    ".mst",                // Windows installer
    ".ndif",               // Mac disk image
    ".networkconnect",     // Configuration file for Apple system
    ".ocx",                // ActiveX
    ".ops",                // MS Office
    ".osas",               // AppleScript
    ".osax",               // AppleScript
    ".oxt",                // OpenOffice extension, can execute arbitrary code
    ".partial",            // Downloads
    ".pax",                // Mac archive
    ".pcd",                // Microsoft Visual Test
    ".pdf",                // Adobe Acrobat
    ".pet",                // Linux package
    ".pif",                // Windows
    ".pkg",                // Mac installer
    ".pl",                 // Perl script
    ".plg",                // MS Visual Studio
    ".pot",                // MS PowerPoint
    ".potm",               // MS PowerPoint
    ".potx",               // MS PowerPoint
    ".ppam",               // MS PowerPoint
    ".pps",                // MS PowerPoint
    ".ppsm",               // MS PowerPoint
    ".ppsx",               // MS PowerPoint
    ".ppt",                // MS PowerPoint
    ".pptm",               // MS PowerPoint
    ".pptx",               // MS PowerPoint
    ".prf",                // MS Outlook
    ".prg",                // Windows
    ".ps1",                // Windows shell
    ".ps1xml",             // Windows shell
    ".ps2",                // Windows shell
    ".ps2xml",             // Windows shell
    ".psc1",               // Windows shell
    ".psc2",               // Windows shell
    ".pst",                // MS Outlook
    ".pup",                // Linux package
    ".py",                 // Python script
    ".pyc",                // Python binary
    ".pyd",                // Equivalent of a DLL, for python libraries
    ".pyo",                // Compiled python code
    ".pyw",                // Python GUI
    ".rb",                 // Ruby script
    ".reg",                // Windows Registry
    ".rels",               // MS Office
    ".rpm",                // Linux package
    ".rtf",                // MS Office
    ".scf",                // Windows shell
    ".scpt",               // AppleScript
    ".scptd",              // AppleScript
    ".scr",                // Windows
    ".sct",                // Windows shell
    ".search-ms",          // Windows
    ".seplugin",           // AppleScript
    ".service",            // Systemd service unit file
    ".settingcontent-ms",  // Windows settings
    ".sh",                 // Linux shell
    ".shar",               // Linux shell
    ".shb",                // Windows
    ".shs",                // Windows shell
    ".sht",                // HTML
    ".shtm",               // HTML
    ".shtml",              // HTML
    ".sldm",               // MS PowerPoint
    ".sldx",               // MS PowerPoint
    ".slk",                // MS Excel
    ".slp",                // Linux package
    ".smi",                // Mac disk image
    ".sparsebundle",       // Mac disk image
    ".sparseimage",        // Mac disk image
    ".spl",                // Adobe Flash
    ".svg",
    ".swf",       // Adobe Flash
    ".swm",       // Windows Imaging
    ".sys",       // Windows
    ".tar",       // Linux archive
    ".taz",       // Linux archive (bzip2)
    ".tbz",       // Linux archive (bzip2)
    ".tbz2",      // Linux archive (bzip2)
    ".tcsh",      // Linux shell
    ".tgz",       // Linux archive (gzip)
    ".torrent",   // Bittorrent
    ".tpz",       // Linux archive (gzip)
    ".txz",       // Linux archive (xz)
    ".tz",        // Linux archive (gzip)
    ".udf",       // MS Excel
    ".udif",      // Mac disk image
    ".url",       // Windows
    ".vb",        // Visual Basic script
    ".vbe",       // Visual Basic script
    ".vbs",       // Visual Basic script
    ".vdx",       // MS Visio
    ".vhd",       // Windows virtual hard drive
    ".vhdx",      // Windows virtual hard drive
    ".vmdk",      // VMware virtual disk
    ".vsd",       // MS Visio
    ".vsdm",      // MS Visio
    ".vsdx",      // MS Visio
    ".vsmacros",  // MS Visual Studio
    ".vss",       // MS Visio
    ".vssm",      // MS Visio
    ".vssx",      // MS Visio
    ".vst",       // MS Visio
    ".vstm",      // MS Visio
    ".vstx",      // MS Visio
    ".vsw",       // MS Visio
    ".vsx",       // MS Visio
    ".vtx",       // MS Visio
    ".website",   // Windows
    ".wflow",     // Automator action
    ".wim",       // Windows Imaging
    ".workflow",  // Mac Automator
    ".ws",        // Windows script
    ".wsc",       // Windows script
    ".wsf",       // Windows script
    ".wsh",       // Windows script
    ".xar",       // MS Excel
    ".xbap",      // XAML Browser Application
    ".xht", ".xhtm", ".xhtml",
    ".xip",     // Mac archive
    ".xla",     // MS Excel
    ".xlam",    // MS Excel
    ".xldm",    // MS Excel
    ".xll",     // MS Excel
    ".xlm",     // MS Excel
    ".xls",     // MS Excel
    ".xlsb",    // MS Excel
    ".xlsm",    // MS Excel
    ".xlsx",    // MS Excel
    ".xlt",     // MS Excel
    ".xltm",    // MS Excel
    ".xltx",    // MS Excel
    ".xlw",     // MS Excel
    ".xml",     // MS Excel
    ".xnk",     // MS Exchange
    ".xrm-ms",  // Windows
    ".xsl",     // XML Stylesheet
    ".xz",      // Linux archive (xz)
    ".z",       // InstallShield
#ifdef XP_WIN   // disable on Mac/Linux, see 1167493
    ".zip",     // Generic archive
#endif
    ".zipx",  // WinZip
};

#define CheckListSorted(_list)                                   \
  {                                                              \
    for (size_t i = 1; i < mozilla::ArrayLength(_list); ++i) {   \
      nsDependentCString str1((_list)[i - 1]);                   \
      nsDependentCString str2((_list)[i]);                       \
      EXPECT_LE(Compare(str1, str2), -1)                         \
          << "Expected " << str1.get() << " to be sorted after " \
          << str2.get();                                         \
    }                                                            \
  }

// First, verify that the 2 lists are both sorted. This helps when checking for
// duplicates manually, ensures we could start doing more efficient lookups if
// we felt that was necessary (e.g. if the lists get much bigger), and that it's
// easy for humans to find things...
TEST(TestExecutableLists, ListsAreSorted)
{
  CheckListSorted(sExecutableExts);
  CheckListSorted(ApplicationReputationService::kBinaryFileExtensions);
  CheckListSorted(ApplicationReputationService::kNonBinaryExecutables);
  CheckListSorted(kTestFileExtensions);
}

bool _IsInList(const char* ext, const char* const _list[], const size_t len) {
  nsDependentCString extStr(ext);
  for (size_t i = 0; i < len; ++i) {
    if (extStr.EqualsASCII(_list[i])) {
      return true;
    }
  }
  return false;
}

#define IsInList(_ext, _list) \
  _IsInList(_ext, _list, mozilla::ArrayLength(_list))

TEST(TestExecutableLists, NonBinariesInExecutablesList)
{
  for (auto nonBinary : ApplicationReputationService::kNonBinaryExecutables) {
    EXPECT_TRUE(IsInList(nonBinary, sExecutableExts))
        << "Expected " << nonBinary << " to be part of sExecutableExts list";
  }
}

TEST(TestExecutableLists, AllExtensionsInTestList)
{
  for (auto ext : ApplicationReputationService::kBinaryFileExtensions) {
    EXPECT_TRUE(IsInList(ext, kTestFileExtensions))
        << "Expected binary extension " << ext
        << " to be listed in the test extension list";
  }

  for (auto ext : sExecutableExts) {
    EXPECT_TRUE(IsInList(ext, kTestFileExtensions))
        << "Expected executable extension " << ext
        << " to be listed in the test extension list";
  }

  for (auto ext : ApplicationReputationService::kNonBinaryExecutables) {
    EXPECT_TRUE(IsInList(ext, kTestFileExtensions))
        << "Expected non-binary executable extension " << ext
        << " to be listed in the test extension list";
  }
}

TEST(TestExecutableLists, TestListExtensionsExistSomewhere)
{
  for (auto ext : kTestFileExtensions) {
    EXPECT_TRUE(
        IsInList(ext, ApplicationReputationService::kBinaryFileExtensions) !=
        IsInList(ext, sExecutableExts))
        << "Expected test extension " << ext
        << " to be in exactly one of the other lists.";
  }
}
