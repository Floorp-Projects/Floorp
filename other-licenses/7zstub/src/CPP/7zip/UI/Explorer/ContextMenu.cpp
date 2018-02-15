// ContextMenu.cpp

#include "StdAfx.h"

#include "../../../Common/StringConvert.h"

#include "../../../Windows/COM.h"
#include "../../../Windows/DLL.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/MemoryGlobal.h"
#include "../../../Windows/Menu.h"
#include "../../../Windows/ProcessUtils.h"
#include "../../../Windows/Shell.h"

#include "../Common/ArchiveName.h"
#include "../Common/CompressCall.h"
#include "../Common/ExtractingFilePath.h"
#include "../Common/ZipRegistry.h"

#include "../FileManager/FormatUtils.h"

#ifdef LANG
#include "../FileManager/LangUtils.h"
#endif

#include "ContextMenu.h"
#include "ContextMenuFlags.h"
#include "MyMessages.h"

#include "resource.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

#ifndef UNDER_CE
#define EMAIL_SUPPORT 1
#endif

extern LONG g_DllRefCount;

#ifdef _WIN32
extern HINSTANCE g_hInstance;
#endif
    
CZipContextMenu::CZipContextMenu():
   _isMenuForFM(false),
   _bitmap(NULL)
{
  InterlockedIncrement(&g_DllRefCount);
  _bitmap = ::LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_MENU_LOGO));
}

CZipContextMenu::~CZipContextMenu()
{
  if (_bitmap != NULL)
    DeleteObject(_bitmap);
  InterlockedDecrement(&g_DllRefCount);
}

HRESULT CZipContextMenu::GetFileNames(LPDATAOBJECT dataObject, UStringVector &fileNames)
{
  fileNames.Clear();
  if (dataObject == NULL)
    return E_FAIL;

  #ifndef UNDER_CE
  
  FORMATETC fmte = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  NCOM::CStgMedium stgMedium;
  HRESULT result = dataObject->GetData(&fmte, &stgMedium);
  if (result != S_OK)
    return result;
  stgMedium._mustBeReleased = true;

  NShell::CDrop drop(false);
  NMemory::CGlobalLock globalLock(stgMedium->hGlobal);
  drop.Attach((HDROP)globalLock.GetPointer());
  drop.QueryFileNames(fileNames);
  
  #endif

  return S_OK;
}

// IShellExtInit

STDMETHODIMP CZipContextMenu::Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT dataObject, HKEY /* hkeyProgID */)
{
  // OutputDebugString(TEXT("::Initialize\r\n"));
  _dropMode = false;
  _dropPath.Empty();
  if (pidlFolder != 0)
  {
    #ifndef UNDER_CE
    if (NShell::GetPathFromIDList(pidlFolder, _dropPath))
    {
      // OutputDebugString(path);
      // OutputDebugString(TEXT("\r\n"));
      NName::NormalizeDirPathPrefix(_dropPath);
      _dropMode = !_dropPath.IsEmpty();
    }
    else
    #endif
      _dropPath.Empty();
  }

  /*
  m_IsFolder = false;
  if (pidlFolder == 0)
  */
  // pidlFolder is NULL :(
  return GetFileNames(dataObject, _fileNames);
}

HRESULT CZipContextMenu::InitContextMenu(const wchar_t * /* folder */, const wchar_t * const *names, unsigned numFiles)
{
  _isMenuForFM = true;
  _fileNames.Clear();
  for (UInt32 i = 0; i < numFiles; i++)
    _fileNames.Add(names[i]);
  _dropMode = false;
  return S_OK;
}


/////////////////////////////
// IContextMenu

static LPCSTR const kMainVerb = "SevenZip";
static LPCSTR const kOpenCascadedVerb = "SevenZip.OpenWithType.";
static LPCSTR const kCheckSumCascadedVerb = "SevenZip.Checksum";


struct CContextMenuCommand
{
  UInt32 flag;
  CZipContextMenu::ECommandInternalID CommandInternalID;
  LPCSTR Verb;
  UINT ResourceID;
};

static const CContextMenuCommand g_Commands[] =
{
  {
    NContextMenuFlags::kOpen,
    CZipContextMenu::kOpen,
    "Open",
    IDS_CONTEXT_OPEN
  },
  {
    NContextMenuFlags::kExtract,
    CZipContextMenu::kExtract,
    "Extract",
    IDS_CONTEXT_EXTRACT
  },
  {
    NContextMenuFlags::kExtractHere,
    CZipContextMenu::kExtractHere,
    "ExtractHere",
    IDS_CONTEXT_EXTRACT_HERE
  },
  {
    NContextMenuFlags::kExtractTo,
    CZipContextMenu::kExtractTo,
    "ExtractTo",
    IDS_CONTEXT_EXTRACT_TO
  },
  {
    NContextMenuFlags::kTest,
    CZipContextMenu::kTest,
    "Test",
    IDS_CONTEXT_TEST
  },
  {
    NContextMenuFlags::kCompress,
    CZipContextMenu::kCompress,
    "Compress",
    IDS_CONTEXT_COMPRESS
  },
  {
    NContextMenuFlags::kCompressEmail,
    CZipContextMenu::kCompressEmail,
    "CompressEmail",
    IDS_CONTEXT_COMPRESS_EMAIL
  },
  {
    NContextMenuFlags::kCompressTo7z,
    CZipContextMenu::kCompressTo7z,
    "CompressTo7z",
    IDS_CONTEXT_COMPRESS_TO
  },
  {
    NContextMenuFlags::kCompressTo7zEmail,
    CZipContextMenu::kCompressTo7zEmail,
    "CompressTo7zEmail",
    IDS_CONTEXT_COMPRESS_TO_EMAIL
  },
  {
    NContextMenuFlags::kCompressToZip,
    CZipContextMenu::kCompressToZip,
    "CompressToZip",
    IDS_CONTEXT_COMPRESS_TO
  },
  {
    NContextMenuFlags::kCompressToZipEmail,
    CZipContextMenu::kCompressToZipEmail,
    "CompressToZipEmail",
    IDS_CONTEXT_COMPRESS_TO_EMAIL
  }
};

struct CHashCommand
{
  CZipContextMenu::ECommandInternalID CommandInternalID;
  LPCSTR UserName;
  LPCSTR MethodName;
};

static const CHashCommand g_HashCommands[] =
{
  { CZipContextMenu::kHash_CRC32,  "CRC-32",  "CRC32" },
  { CZipContextMenu::kHash_CRC64,  "CRC-64",  "CRC64" },
  { CZipContextMenu::kHash_SHA1,   "SHA-1",   "SHA1" },
  { CZipContextMenu::kHash_SHA256, "SHA-256", "SHA256" },
  { CZipContextMenu::kHash_All,    "*",       "*" }
};

static int FindCommand(CZipContextMenu::ECommandInternalID &id)
{
  for (unsigned i = 0; i < ARRAY_SIZE(g_Commands); i++)
    if (g_Commands[i].CommandInternalID == id)
      return i;
  return -1;
}

bool CZipContextMenu::FillCommand(ECommandInternalID id, UString &mainString, CCommandMapItem &commandMapItem)
{
  mainString.Empty();
  int i = FindCommand(id);
  if (i < 0)
    return false;
  const CContextMenuCommand &command = g_Commands[i];
  commandMapItem.CommandInternalID = command.CommandInternalID;
  commandMapItem.Verb = kMainVerb;
  commandMapItem.Verb += command.Verb;
  // LangString(command.ResourceHelpID, command.LangID + 1, commandMapItem.HelpString);
  LangString(command.ResourceID, mainString);
  return true;
}

static bool MyInsertMenu(CMenu &menu, int pos, UINT id, const UString &s, HBITMAP bitmap)
{
  CMenuItem mi;
  mi.fType = MFT_STRING;
  mi.fMask = MIIM_TYPE | MIIM_ID;
  if (bitmap)
    mi.fMask |= MIIM_CHECKMARKS;
  mi.wID = id;
  mi.StringValue = s;
  mi.hbmpUnchecked = bitmap;
  // mi.hbmpChecked = bitmap; // do we need hbmpChecked ???
  return menu.InsertItem(pos, true, mi);

  // SetMenuItemBitmaps also works
  // ::SetMenuItemBitmaps(menu, pos, MF_BYPOSITION, bitmap, NULL);
}

static const char * const kArcExts[] =
{
    "7z"
  , "bz2"
  , "gz"
  , "rar"
  , "zip"
};

static bool IsItArcExt(const UString &ext)
{
  for (unsigned i = 0; i < ARRAY_SIZE(kArcExts); i++)
    if (ext.IsEqualTo_Ascii_NoCase(kArcExts[i]))
      return true;
  return false;
}

UString GetSubFolderNameForExtract(const UString &arcName)
{
  int dotPos = arcName.ReverseFind_Dot();
  if (dotPos < 0)
    return Get_Correct_FsFile_Name(arcName) + L'~';

  const UString ext = arcName.Ptr(dotPos + 1);
  UString res = arcName.Left(dotPos);
  res.TrimRight();
  dotPos = res.ReverseFind_Dot();
  if (dotPos > 0)
  {
    const UString ext2 = res.Ptr(dotPos + 1);
    if (ext.IsEqualTo_Ascii_NoCase("001") && IsItArcExt(ext2)
        || ext.IsEqualTo_Ascii_NoCase("rar") &&
          (  ext2.IsEqualTo_Ascii_NoCase("part001")
          || ext2.IsEqualTo_Ascii_NoCase("part01")
          || ext2.IsEqualTo_Ascii_NoCase("part1")))
      res.DeleteFrom(dotPos);
    res.TrimRight();
  }
  return Get_Correct_FsFile_Name(res);
}

static void ReduceString(UString &s)
{
  const unsigned kMaxSize = 64;
  if (s.Len() <= kMaxSize)
    return;
  s.Delete(kMaxSize / 2, s.Len() - kMaxSize);
  s.Insert(kMaxSize / 2, L" ... ");
}

static UString GetQuotedReducedString(const UString &s)
{
  UString s2 = s;
  ReduceString(s2);
  s2.Replace(L"&", L"&&");
  return GetQuotedString(s2);
}

static void MyFormatNew_ReducedName(UString &s, const UString &name)
{
  s = MyFormatNew(s, GetQuotedReducedString(name));
}

static const char * const kExtractExludeExtensions =
  " 3gp"
  " aac ans ape asc asm asp aspx avi awk"
  " bas bat bmp"
  " c cs cls clw cmd cpp csproj css ctl cxx"
  " def dep dlg dsp dsw"
  " eps"
  " f f77 f90 f95 fla flac frm"
  " gif"
  " h hpp hta htm html hxx"
  " ico idl inc ini inl"
  " java jpeg jpg js"
  " la lnk log"
  " mak manifest wmv mov mp3 mp4 mpe mpeg mpg m4a"
  " ofr ogg"
  " pac pas pdf php php3 php4 php5 phptml pl pm png ps py pyo"
  " ra rb rc reg rka rm rtf"
  " sed sh shn shtml sln sql srt swa"
  " tcl tex tiff tta txt"
  " vb vcproj vbs"
  " wav wma wv"
  " xml xsd xsl xslt"
  " ";

/*
static const char * const kNoOpenAsExtensions =
  " 7z arj bz2 cab chm cpio flv gz lha lzh lzma rar swm tar tbz2 tgz wim xar xz z zip ";
*/

static const char * const kOpenTypes[] =
{
    ""
  , "*"
  , "#"
  , "#:e"
  // , "#:a"
  , "7z"
  , "zip"
  , "cab"
  , "rar"
};

static bool FindExt(const char *p, const FString &name)
{
  int dotPos = name.ReverseFind_Dot();
  if (dotPos < 0 || dotPos == (int)name.Len() - 1)
    return false;

  AString s;
  
  for (unsigned pos = dotPos + 1;; pos++)
  {
    wchar_t c = name[pos];
    if (c == 0)
      break;
    if (c >= 0x80)
      return false;
    s += (char)MyCharLower_Ascii((char)c);
  }
  
  for (unsigned i = 0; p[i] != 0;)
  {
    unsigned j;
    for (j = i; p[j] != ' '; j++);
    if (s.Len() == j - i && memcmp(p + i, (const char *)s, s.Len()) == 0)
      return true;
    i = j + 1;
  }
  
  return false;
}

static bool DoNeedExtract(const FString &name)
{
  return !FindExt(kExtractExludeExtensions, name);
}

// we must use diferent Verbs for Popup subMenu.
void CZipContextMenu::AddMapItem_ForSubMenu(const char *verb)
{
  CCommandMapItem commandMapItem;
  commandMapItem.CommandInternalID = kCommandNULL;
  commandMapItem.Verb = verb;
  _commandMap.Add(commandMapItem);
}


STDMETHODIMP CZipContextMenu::QueryContextMenu(HMENU hMenu, UINT indexMenu,
      UINT commandIDFirst, UINT commandIDLast, UINT flags)
{
  // OutputDebugStringA("QueryContextMenu");

  LoadLangOneTime();
  if (_fileNames.Size() == 0)
    return E_FAIL;
  UINT currentCommandID = commandIDFirst;
  if ((flags & 0x000F) != CMF_NORMAL  &&
      (flags & CMF_VERBSONLY) == 0 &&
      (flags & CMF_EXPLORE) == 0)
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, currentCommandID);

  _commandMap.Clear();

  CMenu popupMenu;
  CMenuDestroyer menuDestroyer;

  CContextMenuInfo ci;
  ci.Load();

  _elimDup = ci.ElimDup;

  HBITMAP bitmap = NULL;
  if (ci.MenuIcons.Val)
    bitmap = _bitmap;

  UINT subIndex = indexMenu;
  
  if (ci.Cascaded.Val)
  {
    if (!popupMenu.CreatePopup())
      return E_FAIL;
    menuDestroyer.Attach(popupMenu);

    /* 9.31: we commented the following code. Probably we don't need.
    Check more systems. Maybe it was for old Windows? */
    /*
    AddMapItem_ForSubMenu();
    currentCommandID++;
    */
    subIndex = 0;
  }
  else
  {
    popupMenu.Attach(hMenu);
    CMenuItem mi;
    mi.fType = MFT_SEPARATOR;
    mi.fMask = MIIM_TYPE;
    popupMenu.InsertItem(subIndex++, true, mi);
  }

  UInt32 contextMenuFlags = ci.Flags;

  NFind::CFileInfo fi0;
  FString folderPrefix;
  
  if (_fileNames.Size() > 0)
  {
    const UString &fileName = _fileNames.Front();

    #if defined(_WIN32) && !defined(UNDER_CE)
    if (NName::IsDevicePath(us2fs(fileName)))
    {
      // CFileInfo::Find can be slow for device files. So we don't call it.
      // we need only name here.
      fi0.Name = us2fs(fileName.Ptr(NName::kDevicePathPrefixSize)); // change it 4 - must be constant
      folderPrefix =
        #ifdef UNDER_CE
          "\\";
        #else
          "C:\\";
        #endif
    }
    else
    #endif
    {
      if (!fi0.Find(us2fs(fileName)))
        return E_FAIL;
      GetOnlyDirPrefix(us2fs(fileName), folderPrefix);
    }
  }

  UString mainString;
  
  if (_fileNames.Size() == 1 && currentCommandID + 14 <= commandIDLast)
  {
    if (!fi0.IsDir() && DoNeedExtract(fi0.Name))
    {
      // Open
      bool thereIsMainOpenItem = ((contextMenuFlags & NContextMenuFlags::kOpen) != 0);
      if (thereIsMainOpenItem)
      {
        CCommandMapItem commandMapItem;
        FillCommand(kOpen, mainString, commandMapItem);
        MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
        _commandMap.Add(commandMapItem);
      }
      if ((contextMenuFlags & NContextMenuFlags::kOpenAs) != 0
          // && (!thereIsMainOpenItem || !FindExt(kNoOpenAsExtensions, fi0.Name))
          )
      {
        CMenu subMenu;
        if (subMenu.CreatePopup())
        {
          CMenuItem mi;
          mi.fType = MFT_STRING;
          mi.fMask = MIIM_SUBMENU | MIIM_TYPE | MIIM_ID;
          if (bitmap)
            mi.fMask |= MIIM_CHECKMARKS;
          mi.wID = currentCommandID++;
          mi.hSubMenu = subMenu;
          mi.hbmpUnchecked = bitmap;

          LangString(IDS_CONTEXT_OPEN, mi.StringValue);
          popupMenu.InsertItem(subIndex++, true, mi);
          AddMapItem_ForSubMenu(kOpenCascadedVerb);
          
          UINT subIndex2 = 0;
          for (unsigned i = (thereIsMainOpenItem ? 1 : 0); i < ARRAY_SIZE(kOpenTypes); i++)
          {
            CCommandMapItem commandMapItem;
            if (i == 0)
              FillCommand(kOpen, mainString, commandMapItem);
            else
            {
              mainString = kOpenTypes[i];
              commandMapItem.CommandInternalID = kOpen;
              commandMapItem.Verb = kMainVerb;
              commandMapItem.Verb += ".Open.";
              commandMapItem.Verb += mainString;
              commandMapItem.HelpString = mainString;
              commandMapItem.ArcType = mainString;
            }
            MyInsertMenu(subMenu, subIndex2++, currentCommandID++, mainString, bitmap);
            _commandMap.Add(commandMapItem);
          }

          subMenu.Detach();
        }
      }
    }
  }

  if (_fileNames.Size() > 0 && currentCommandID + 10 <= commandIDLast)
  {
    bool needExtract = (!fi0.IsDir() && DoNeedExtract(fi0.Name));
    
    if (!needExtract)
    {
      for (unsigned i = 1; i < _fileNames.Size(); i++)
      {
        NFind::CFileInfo fi;
        if (!fi.Find(us2fs(_fileNames[i])))
          return E_FAIL;
        if (!fi.IsDir() && DoNeedExtract(fi.Name))
        {
          needExtract = true;
          break;
        }
      }
    }
    
    const UString &fileName = _fileNames.Front();
    
    if (needExtract)
    {
      {
        UString baseFolder = fs2us(folderPrefix);
        if (_dropMode)
          baseFolder = _dropPath;
    
        UString specFolder ('*');
        if (_fileNames.Size() == 1)
          specFolder = GetSubFolderNameForExtract(fs2us(fi0.Name));
        specFolder.Add_PathSepar();

        if ((contextMenuFlags & NContextMenuFlags::kExtract) != 0)
        {
          // Extract
          CCommandMapItem commandMapItem;
          FillCommand(kExtract, mainString, commandMapItem);
          commandMapItem.Folder = baseFolder + specFolder;
          MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
          _commandMap.Add(commandMapItem);
        }

        if ((contextMenuFlags & NContextMenuFlags::kExtractHere) != 0)
        {
          // Extract Here
          CCommandMapItem commandMapItem;
          FillCommand(kExtractHere, mainString, commandMapItem);
          commandMapItem.Folder = baseFolder;
          MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
          _commandMap.Add(commandMapItem);
        }

        if ((contextMenuFlags & NContextMenuFlags::kExtractTo) != 0)
        {
          // Extract To
          CCommandMapItem commandMapItem;
          UString s;
          FillCommand(kExtractTo, s, commandMapItem);
          commandMapItem.Folder = baseFolder + specFolder;
          MyFormatNew_ReducedName(s, specFolder);
          MyInsertMenu(popupMenu, subIndex++, currentCommandID++, s, bitmap);
          _commandMap.Add(commandMapItem);
        }
      }

      if ((contextMenuFlags & NContextMenuFlags::kTest) != 0)
      {
        // Test
        CCommandMapItem commandMapItem;
        FillCommand(kTest, mainString, commandMapItem);
        MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
        _commandMap.Add(commandMapItem);
      }
    }
    
    UString arcName;
    if (_fileNames.Size() == 1)
      arcName = CreateArchiveName(fi0, false);
    else
      arcName = CreateArchiveName(fileName, _fileNames.Size() > 1, false);
    
    UString arcName7z = arcName;
    arcName7z += ".7z";
    UString arcNameZip = arcName;
    arcNameZip += ".zip";

    // Compress
    if ((contextMenuFlags & NContextMenuFlags::kCompress) != 0)
    {
      CCommandMapItem commandMapItem;
      if (_dropMode)
        commandMapItem.Folder = _dropPath;
      else
        commandMapItem.Folder = fs2us(folderPrefix);
      commandMapItem.ArcName = arcName;
      FillCommand(kCompress, mainString, commandMapItem);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
      _commandMap.Add(commandMapItem);
    }

    #ifdef EMAIL_SUPPORT
    // CompressEmail
    if ((contextMenuFlags & NContextMenuFlags::kCompressEmail) != 0 && !_dropMode)
    {
      CCommandMapItem commandMapItem;
      commandMapItem.ArcName = arcName;
      FillCommand(kCompressEmail, mainString, commandMapItem);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
      _commandMap.Add(commandMapItem);
    }
    #endif

    // CompressTo7z
    if (contextMenuFlags & NContextMenuFlags::kCompressTo7z &&
        !arcName7z.IsEqualTo_NoCase(fs2us(fi0.Name)))
    {
      CCommandMapItem commandMapItem;
      UString s;
      FillCommand(kCompressTo7z, s, commandMapItem);
      if (_dropMode)
        commandMapItem.Folder = _dropPath;
      else
        commandMapItem.Folder = fs2us(folderPrefix);
      commandMapItem.ArcName = arcName7z;
      commandMapItem.ArcType = "7z";
      MyFormatNew_ReducedName(s, arcName7z);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, s, bitmap);
      _commandMap.Add(commandMapItem);
    }

    #ifdef EMAIL_SUPPORT
    // CompressTo7zEmail
    if ((contextMenuFlags & NContextMenuFlags::kCompressTo7zEmail) != 0  && !_dropMode)
    {
      CCommandMapItem commandMapItem;
      UString s;
      FillCommand(kCompressTo7zEmail, s, commandMapItem);
      commandMapItem.ArcName = arcName7z;
      commandMapItem.ArcType = "7z";
      MyFormatNew_ReducedName(s, arcName7z);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, s, bitmap);
      _commandMap.Add(commandMapItem);
    }
    #endif

    // CompressToZip
    if (contextMenuFlags & NContextMenuFlags::kCompressToZip &&
        !arcNameZip.IsEqualTo_NoCase(fs2us(fi0.Name)))
    {
      CCommandMapItem commandMapItem;
      UString s;
      FillCommand(kCompressToZip, s, commandMapItem);
      if (_dropMode)
        commandMapItem.Folder = _dropPath;
      else
        commandMapItem.Folder = fs2us(folderPrefix);
      commandMapItem.ArcName = arcNameZip;
      commandMapItem.ArcType = "zip";
      MyFormatNew_ReducedName(s, arcNameZip);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, s, bitmap);
      _commandMap.Add(commandMapItem);
    }

    #ifdef EMAIL_SUPPORT
    // CompressToZipEmail
    if ((contextMenuFlags & NContextMenuFlags::kCompressToZipEmail) != 0  && !_dropMode)
    {
      CCommandMapItem commandMapItem;
      UString s;
      FillCommand(kCompressToZipEmail, s, commandMapItem);
      commandMapItem.ArcName = arcNameZip;
      commandMapItem.ArcType = "zip";
      MyFormatNew_ReducedName(s, arcNameZip);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, s, bitmap);
      _commandMap.Add(commandMapItem);
    }
    #endif
  }


  // don't use InsertMenu:  See MSDN:
  // PRB: Duplicate Menu Items In the File Menu For a Shell Context Menu Extension
  // ID: Q214477
  
  if (ci.Cascaded.Val)
  {
    CMenuItem mi;
    mi.fType = MFT_STRING;
    mi.fMask = MIIM_SUBMENU | MIIM_TYPE | MIIM_ID;
    if (bitmap)
      mi.fMask |= MIIM_CHECKMARKS;
    mi.wID = currentCommandID++;
    mi.hSubMenu = popupMenu.Detach();
    mi.StringValue = "7-Zip"; // LangString(IDS_CONTEXT_POPUP_CAPTION);
    mi.hbmpUnchecked = bitmap;
    
    CMenu menu;
    menu.Attach(hMenu);
    menuDestroyer.Disable();
    menu.InsertItem(indexMenu++, true, mi);
    
    AddMapItem_ForSubMenu(kMainVerb);
  }
  else
  {
    popupMenu.Detach();
    indexMenu = subIndex;
  }

  
  if (!_isMenuForFM &&
      ((contextMenuFlags & NContextMenuFlags::kCRC) != 0
      && currentCommandID + 6 <= commandIDLast))
  {
    CMenu subMenu;
    // CMenuDestroyer menuDestroyer_CRC;
    
    UINT subIndex_CRC = 0;
    
    if (subMenu.CreatePopup())
    {
      // menuDestroyer_CRC.Attach(subMenu);
      CMenuItem mi;
      mi.fType = MFT_STRING;
      mi.fMask = MIIM_SUBMENU | MIIM_TYPE | MIIM_ID;
      if (bitmap)
        mi.fMask |= MIIM_CHECKMARKS;
      mi.wID = currentCommandID++;
      mi.hSubMenu = subMenu;
      mi.StringValue = "CRC SHA";
      mi.hbmpUnchecked = bitmap;
      
      CMenu menu;
      menu.Attach(hMenu);
      // menuDestroyer_CRC.Disable();
      menu.InsertItem(indexMenu++, true, mi);
      
      AddMapItem_ForSubMenu(kCheckSumCascadedVerb);

      for (unsigned i = 0; i < ARRAY_SIZE(g_HashCommands); i++)
      {
        const CHashCommand &hc = g_HashCommands[i];
        CCommandMapItem commandMapItem;
        commandMapItem.CommandInternalID = hc.CommandInternalID;
        commandMapItem.Verb = kCheckSumCascadedVerb;
        commandMapItem.Verb += hc.MethodName;
        // commandMapItem.HelpString = hc.Name;
        MyInsertMenu(subMenu, subIndex_CRC++, currentCommandID++, (UString)hc.UserName, bitmap);
        _commandMap.Add(commandMapItem);
      }
      
      subMenu.Detach();
    }
  }

  return MAKE_HRESULT(SEVERITY_SUCCESS, 0, currentCommandID - commandIDFirst);
}


int CZipContextMenu::FindVerb(const UString &verb)
{
  FOR_VECTOR (i, _commandMap)
    if (_commandMap[i].Verb == verb)
      return i;
  return -1;
}

static UString Get7zFmPath()
{
  return fs2us(NWindows::NDLL::GetModuleDirPrefix()) + L"7zFM.exe";
}

STDMETHODIMP CZipContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO commandInfo)
{
  // ::OutputDebugStringA("1");
  int commandOffset;

  // It's fix for bug: crashing in XP. See example in MSDN: "Creating Context Menu Handlers".

  #if !defined(UNDER_CE) && defined(_MSC_VER)
  if (commandInfo->cbSize == sizeof(CMINVOKECOMMANDINFOEX) &&
      (commandInfo->fMask & CMIC_MASK_UNICODE) != 0)
  {
    LPCMINVOKECOMMANDINFOEX commandInfoEx = (LPCMINVOKECOMMANDINFOEX)commandInfo;
    if (HIWORD(commandInfoEx->lpVerbW) == 0)
      commandOffset = LOWORD(commandInfo->lpVerb);
    else
      commandOffset = FindVerb(commandInfoEx->lpVerbW);
  }
  else
  #endif
    if (HIWORD(commandInfo->lpVerb) == 0)
      commandOffset = LOWORD(commandInfo->lpVerb);
    else
      commandOffset = FindVerb(GetUnicodeString(commandInfo->lpVerb));

  if (commandOffset < 0 || (unsigned)commandOffset >= _commandMap.Size())
    return E_FAIL;

  const CCommandMapItem commandMapItem = _commandMap[commandOffset];
  ECommandInternalID cmdID = commandMapItem.CommandInternalID;

  try
  {
    switch (cmdID)
    {
      case kOpen:
      {
        UString params;
        params = GetQuotedString(_fileNames[0]);
        if (!commandMapItem.ArcType.IsEmpty())
        {
          params += " -t";
          params += commandMapItem.ArcType;
        }
        MyCreateProcess(Get7zFmPath(), params);
        break;
      }
      case kExtract:
      case kExtractHere:
      case kExtractTo:
      {
        ExtractArchives(_fileNames, commandMapItem.Folder,
            (cmdID == kExtract), // showDialog
            (cmdID == kExtractTo) && _elimDup.Val // elimDup
            );
        break;
      }
      case kTest:
      {
        TestArchives(_fileNames);
        break;
      }
      case kCompress:
      case kCompressEmail:
      case kCompressTo7z:
      case kCompressTo7zEmail:
      case kCompressToZip:
      case kCompressToZipEmail:
      {
        bool email =
            (cmdID == kCompressEmail) ||
            (cmdID == kCompressTo7zEmail) ||
            (cmdID == kCompressToZipEmail);
        bool showDialog =
            (cmdID == kCompress) ||
            (cmdID == kCompressEmail);
        bool addExtension = (cmdID == kCompress || cmdID == kCompressEmail);
        CompressFiles(commandMapItem.Folder,
            commandMapItem.ArcName, commandMapItem.ArcType,
            addExtension,
            _fileNames, email, showDialog, false);
        break;
      }
      
      case kHash_CRC32:
      case kHash_CRC64:
      case kHash_SHA1:
      case kHash_SHA256:
      case kHash_All:
      {
        for (unsigned i = 0; i < ARRAY_SIZE(g_HashCommands); i++)
        {
          const CHashCommand &hc = g_HashCommands[i];
          if (hc.CommandInternalID == cmdID)
          {
            CalcChecksum(_fileNames, (UString)hc.MethodName);
            break;
          }
        }
        break;
      }
    }
  }
  catch(...)
  {
    ::MessageBoxW(0, L"Error", L"7-Zip", MB_ICONERROR);
  }
  return S_OK;
}

static void MyCopyString(void *dest, const wchar_t *src, bool writeInUnicode)
{
  if (writeInUnicode)
  {
    MyStringCopy((wchar_t *)dest, src);
  }
  else
    MyStringCopy((char *)dest, (const char *)GetAnsiString(src));
}

STDMETHODIMP CZipContextMenu::GetCommandString(UINT_PTR commandOffset, UINT uType,
    UINT * /* pwReserved */ , LPSTR pszName, UINT /* cchMax */)
{
  int cmdOffset = (int)commandOffset;
  switch (uType)
  {
    #ifdef UNDER_CE
    case GCS_VALIDATE:
    #else
    case GCS_VALIDATEA:
    case GCS_VALIDATEW:
    #endif
      if (cmdOffset < 0 || (unsigned)cmdOffset >= _commandMap.Size())
        return S_FALSE;
      else
        return S_OK;
  }
  if (cmdOffset < 0 || (unsigned)cmdOffset >= _commandMap.Size())
    return E_FAIL;
  #ifdef UNDER_CE
  if (uType == GCS_HELPTEXT)
  #else
  if (uType == GCS_HELPTEXTA || uType == GCS_HELPTEXTW)
  #endif
  {
    MyCopyString(pszName, _commandMap[cmdOffset].HelpString,
      #ifdef UNDER_CE
      true
      #else
      uType == GCS_HELPTEXTW
      #endif
      );
    return NO_ERROR;
  }
  #ifdef UNDER_CE
  if (uType == GCS_VERB)
  #else
  if (uType == GCS_VERBA || uType == GCS_VERBW)
  #endif
  {
    MyCopyString(pszName, _commandMap[cmdOffset].Verb,
      #ifdef UNDER_CE
      true
      #else
      uType == GCS_VERBW
      #endif
      );
    return NO_ERROR;
  }
  return E_FAIL;
}
