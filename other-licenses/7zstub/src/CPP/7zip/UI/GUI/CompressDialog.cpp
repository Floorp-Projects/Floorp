// CompressDialog.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/System.h"

#include "../FileManager/BrowseDialog.h"
#include "../FileManager/FormatUtils.h"
#include "../FileManager/HelpUtils.h"
#include "../FileManager/SplitUtils.h"

#include "../Explorer/MyMessages.h"

#include "../Common/ZipRegistry.h"

#include "CompressDialog.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

#ifdef LANG
#include "../FileManager/LangUtils.h"
#endif

#include "CompressDialogRes.h"
#include "ExtractRes.h"

#ifdef LANG
static const UInt32 kLangIDs[] =
{
  IDT_COMPRESS_ARCHIVE,
  IDT_COMPRESS_UPDATE_MODE,
  IDT_COMPRESS_FORMAT,
  IDT_COMPRESS_LEVEL,
  IDT_COMPRESS_METHOD,
  IDT_COMPRESS_DICTIONARY,
  IDT_COMPRESS_ORDER,
  IDT_COMPRESS_SOLID,
  IDT_COMPRESS_THREADS,
  IDT_COMPRESS_PARAMETERS,
  
  IDG_COMPRESS_OPTIONS,
  IDX_COMPRESS_SFX,
  IDX_COMPRESS_SHARED,
  IDX_COMPRESS_DEL,

  IDT_COMPRESS_MEMORY,
  IDT_COMPRESS_MEMORY_DE,

  IDX_COMPRESS_NT_SYM_LINKS,
  IDX_COMPRESS_NT_HARD_LINKS,
  IDX_COMPRESS_NT_ALT_STREAMS,
  IDX_COMPRESS_NT_SECUR,

  IDG_COMPRESS_ENCRYPTION,
  IDT_COMPRESS_ENCRYPTION_METHOD,
  IDX_COMPRESS_ENCRYPT_FILE_NAMES,

  IDT_PASSWORD_ENTER,
  IDT_PASSWORD_REENTER,
  IDX_PASSWORD_SHOW,

  IDT_SPLIT_TO_VOLUMES,
  IDT_COMPRESS_PATH_MODE
};
#endif

using namespace NWindows;
using namespace NFile;
using namespace NName;
using namespace NDir;

static const unsigned kHistorySize = 20;

static const UInt32 kNoSolidBlockSize = 0;
static const UInt32 kSolidBlockSize = 64;

static LPCSTR const kExeExt = ".exe";

#define k7zFormat "7z"

static const UInt32 g_Levels[] =
{
  IDS_METHOD_STORE,
  IDS_METHOD_FASTEST,
  0,
  IDS_METHOD_FAST,
  0,
  IDS_METHOD_NORMAL,
  0,
  IDS_METHOD_MAXIMUM,
  0,
  IDS_METHOD_ULTRA
};

enum EMethodID
{
  kCopy,
  kLZMA,
  kLZMA2,
  kPPMd,
  kBZip2,
  kDeflate,
  kDeflate64,
  kPPMdZip
};

static LPCSTR const kMethodsNames[] =
{
    "Copy"
  , "LZMA"
  , "LZMA2"
  , "PPMd"
  , "BZip2"
  , "Deflate"
  , "Deflate64"
  , "PPMd"
};

static const EMethodID g_7zMethods[] =
{
  kLZMA2,
  kLZMA,
  kPPMd,
  kBZip2
};

static const EMethodID g_7zSfxMethods[] =
{
  kCopy,
  kLZMA,
  kLZMA2,
  kPPMd
};

static const EMethodID g_ZipMethods[] =
{
  kDeflate,
  kDeflate64,
  kBZip2,
  kLZMA,
  kPPMdZip
};

static const EMethodID g_GZipMethods[] =
{
  kDeflate
};

static const EMethodID g_BZip2Methods[] =
{
  kBZip2
};

static const EMethodID g_XzMethods[] =
{
  kLZMA2
};

static const EMethodID g_SwfcMethods[] =
{
  kDeflate
  // kLZMA
};

struct CFormatInfo
{
  LPCSTR Name;
  UInt32 LevelsMask;
  unsigned NumMethods;
  const EMethodID *MathodIDs;
  bool Filter;
  bool Solid;
  bool MultiThread;
  bool SFX;
  
  bool Encrypt;
  bool EncryptFileNames;
};

#define METHODS_PAIR(x) ARRAY_SIZE(x), x

static const CFormatInfo g_Formats[] =
{
  {
    "",
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    0, 0,
    false, false, false, false, false, false
  },
  {
    k7zFormat,
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_7zMethods),
    true, true, true, true, true, true
  },
  {
    "Zip",
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_ZipMethods),
    false, false, true, false, true, false
  },
  {
    "GZip",
    (1 << 1) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_GZipMethods),
    false, false, false, false, false, false
  },
  {
    "BZip2",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_BZip2Methods),
    false, false, true, false, false, false
  },
  {
    "xz",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_XzMethods),
    false, true, true, false, false, false
  },
  {
    "Swfc",
    (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9),
    METHODS_PAIR(g_SwfcMethods),
    false, false, true, false, false, false
  },
  {
    "Tar",
    (1 << 0),
    0, 0,
    false, false, false, false, false, false
  },
  {
    "wim",
    (1 << 0),
    0, 0,
    false, false, false, false, false, false
  }
};

static bool IsMethodSupportedBySfx(int methodID)
{
  for (int i = 0; i < ARRAY_SIZE(g_7zSfxMethods); i++)
    if (methodID == g_7zSfxMethods[i])
      return true;
  return false;
}

static bool GetMaxRamSizeForProgram(UInt64 &physSize)
{
  physSize = (UInt64)(sizeof(size_t)) << 29;
  bool ramSize_Defined = NSystem::GetRamSize(physSize);
  const UInt64 kMinSysSize = (1 << 24);
  if (physSize <= kMinSysSize)
    physSize = 0;
  else
    physSize -= kMinSysSize;
  const UInt64 kMinUseSize = (1 << 24);
  if (physSize < kMinUseSize)
    physSize = kMinUseSize;
  return ramSize_Defined;
}


static const
  // NCompressDialog::NUpdateMode::EEnum
  int
  k_UpdateMode_Vals[] =
{
  NCompressDialog::NUpdateMode::kAdd,
  NCompressDialog::NUpdateMode::kUpdate,
  NCompressDialog::NUpdateMode::kFresh,
  NCompressDialog::NUpdateMode::kSync
};
  
static const UInt32 k_UpdateMode_IDs[] =
{
  IDS_COMPRESS_UPDATE_MODE_ADD,
  IDS_COMPRESS_UPDATE_MODE_UPDATE,
  IDS_COMPRESS_UPDATE_MODE_FRESH,
  IDS_COMPRESS_UPDATE_MODE_SYNC
};

static const
  // NWildcard::ECensorPathMode
  int
  k_PathMode_Vals[] =
{
  NWildcard::k_RelatPath,
  NWildcard::k_FullPath,
  NWildcard::k_AbsPath,
};

static const UInt32 k_PathMode_IDs[] =
{
  IDS_PATH_MODE_RELAT,
  IDS_EXTRACT_PATHS_FULL,
  IDS_EXTRACT_PATHS_ABS
};

void AddComboItems(NControl::CComboBox &combo, const UInt32 *langIDs, unsigned numItems, const int *values, int curVal);
bool GetBoolsVal(const CBoolPair &b1, const CBoolPair &b2);

void CCompressDialog::CheckButton_TwoBools(UINT id, const CBoolPair &b1, const CBoolPair &b2)
{
  CheckButton(id, GetBoolsVal(b1, b2));
}

void CCompressDialog::GetButton_Bools(UINT id, CBoolPair &b1, CBoolPair &b2)
{
  bool val = IsButtonCheckedBool(id);
  bool oldVal = GetBoolsVal(b1, b2);
  if (val != oldVal)
    b1.Def = b2.Def = true;
  b1.Val = b2.Val = val;
}

                                            
bool CCompressDialog::OnInit()
{
  #ifdef LANG
  LangSetWindowText(*this, IDD_COMPRESS);
  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));
  #endif

  _password1Control.Attach(GetItem(IDE_COMPRESS_PASSWORD1));
  _password2Control.Attach(GetItem(IDE_COMPRESS_PASSWORD2));
  _password1Control.SetText(Info.Password);
  _password2Control.SetText(Info.Password);
  _encryptionMethod.Attach(GetItem(IDC_COMPRESS_ENCRYPTION_METHOD));

  m_ArchivePath.Attach(GetItem(IDC_COMPRESS_ARCHIVE));
  m_Format.Attach(GetItem(IDC_COMPRESS_FORMAT));
  m_Level.Attach(GetItem(IDC_COMPRESS_LEVEL));
  m_Method.Attach(GetItem(IDC_COMPRESS_METHOD));
  m_Dictionary.Attach(GetItem(IDC_COMPRESS_DICTIONARY));
  m_Order.Attach(GetItem(IDC_COMPRESS_ORDER));
  m_Solid.Attach(GetItem(IDC_COMPRESS_SOLID));
  m_NumThreads.Attach(GetItem(IDC_COMPRESS_THREADS));
  
  m_UpdateMode.Attach(GetItem(IDC_COMPRESS_UPDATE_MODE));
  m_PathMode.Attach(GetItem(IDC_COMPRESS_PATH_MODE));

  m_Volume.Attach(GetItem(IDC_COMPRESS_VOLUME));
  m_Params.Attach(GetItem(IDE_COMPRESS_PARAMETERS));

  AddVolumeItems(m_Volume);

  m_RegistryInfo.Load();
  CheckButton(IDX_PASSWORD_SHOW, m_RegistryInfo.ShowPassword);
  CheckButton(IDX_COMPRESS_ENCRYPT_FILE_NAMES, m_RegistryInfo.EncryptHeaders);
  
  CheckButton_TwoBools(IDX_COMPRESS_NT_SYM_LINKS,   Info.SymLinks,   m_RegistryInfo.SymLinks);
  CheckButton_TwoBools(IDX_COMPRESS_NT_HARD_LINKS,  Info.HardLinks,  m_RegistryInfo.HardLinks);
  CheckButton_TwoBools(IDX_COMPRESS_NT_ALT_STREAMS, Info.AltStreams, m_RegistryInfo.AltStreams);
  CheckButton_TwoBools(IDX_COMPRESS_NT_SECUR,       Info.NtSecurity, m_RegistryInfo.NtSecurity);

  UpdatePasswordControl();

  {
    bool needSetMain = (Info.FormatIndex < 0);
    FOR_VECTOR(i, ArcIndices)
    {
      unsigned arcIndex = ArcIndices[i];
      const CArcInfoEx &ai = (*ArcFormats)[arcIndex];
      int index = (int)m_Format.AddString(ai.Name);
      m_Format.SetItemData(index, arcIndex);
      if (!needSetMain)
      {
        if (Info.FormatIndex == (int)arcIndex)
          m_Format.SetCurSel(index);
        continue;
      }
      if (i == 0 || ai.Name.IsEqualTo_NoCase(m_RegistryInfo.ArcType))
      {
        m_Format.SetCurSel(index);
        Info.FormatIndex = arcIndex;
      }
    }
  }

  CheckButton(IDX_COMPRESS_SFX, Info.SFXMode);

  {
    UString fileName;
    SetArcPathFields(Info.ArcPath, fileName, true);
    StartDirPrefix = DirPrefix;
    SetArchiveName(fileName);
  }
  SetLevel();
  SetParams();
  
  for (unsigned i = 0; i < m_RegistryInfo.ArcPaths.Size() && i < kHistorySize; i++)
    m_ArchivePath.AddString(m_RegistryInfo.ArcPaths[i]);

  AddComboItems(m_UpdateMode, k_UpdateMode_IDs, ARRAY_SIZE(k_UpdateMode_IDs),
      k_UpdateMode_Vals, Info.UpdateMode);

  AddComboItems(m_PathMode, k_PathMode_IDs, ARRAY_SIZE(k_PathMode_IDs),
      k_PathMode_Vals, Info.PathMode);

  SetSolidBlockSize();
  SetNumThreads();

  TCHAR s[40] = { TEXT('/'), TEXT(' '), 0 };
  ConvertUInt32ToString(NSystem::GetNumberOfProcessors(), s + 2);
  SetItemText(IDT_COMPRESS_HARDWARE_THREADS, s);

  CheckButton(IDX_COMPRESS_SHARED, Info.OpenShareForWrite);
  CheckButton(IDX_COMPRESS_DEL, Info.DeleteAfterCompressing);

  CheckControlsEnable();

  // OnButtonSFX();

  SetEncryptionMethod();
  SetMemoryUsage();

  NormalizePosition();

  return CModalDialog::OnInit();
}

/*
namespace NCompressDialog
{
  bool CInfo::GetFullPathName(UString &result) const
  {
    #ifndef UNDER_CE
    // NDirectory::MySetCurrentDirectory(CurrentDirPrefix);
    #endif
    FString resultF;
    bool res = MyGetFullPathName(us2fs(ArchiveName), resultF);
    result = fs2us(resultF);
    return res;
  }
}
*/

void CCompressDialog::UpdatePasswordControl()
{
  bool showPassword = IsShowPasswordChecked();
  TCHAR c = showPassword ? (TCHAR)0: TEXT('*');
  _password1Control.SetPasswordChar(c);
  _password2Control.SetPasswordChar(c);
  UString password;
  _password1Control.GetText(password);
  _password1Control.SetText(password);
  _password2Control.GetText(password);
  _password2Control.SetText(password);

  ShowItem_Bool(IDT_PASSWORD_REENTER, !showPassword);
  _password2Control.Show_Bool(!showPassword);
}

bool CCompressDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_COMPRESS_SET_ARCHIVE:
    {
      OnButtonSetArchive();
      return true;
    }
    case IDX_COMPRESS_SFX:
    {
      SetMethod(GetMethodID());
      OnButtonSFX();
      SetMemoryUsage();
      return true;
    }
    case IDX_PASSWORD_SHOW:
    {
      UpdatePasswordControl();
      return true;
    }
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CCompressDialog::CheckSFXControlsEnable()
{
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  bool enable = fi.SFX;
  if (enable)
  {
    int methodID = GetMethodID();
    enable = (methodID == -1 || IsMethodSupportedBySfx(methodID));
  }
  if (!enable)
    CheckButton(IDX_COMPRESS_SFX, false);
  EnableItem(IDX_COMPRESS_SFX, enable);
}

/*
void CCompressDialog::CheckVolumeEnable()
{
  bool isSFX = IsSFX();
  m_Volume.Enable(!isSFX);
  if (isSFX)
    m_Volume.SetText(TEXT(""));
}
*/

void CCompressDialog::CheckControlsEnable()
{
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  Info.SolidIsSpecified = fi.Solid;
  bool multiThreadEnable = fi.MultiThread;
  Info.MultiThreadIsAllowed = multiThreadEnable;
  Info.EncryptHeadersIsAllowed = fi.EncryptFileNames;
  
  EnableItem(IDC_COMPRESS_SOLID, fi.Solid);
  EnableItem(IDC_COMPRESS_THREADS, multiThreadEnable);

  CheckSFXControlsEnable();

  {
    const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
    
    ShowItem_Bool(IDX_COMPRESS_NT_SYM_LINKS, ai.Flags_SymLinks());
    ShowItem_Bool(IDX_COMPRESS_NT_HARD_LINKS, ai.Flags_HardLinks());
    ShowItem_Bool(IDX_COMPRESS_NT_ALT_STREAMS, ai.Flags_AltStreams());
    ShowItem_Bool(IDX_COMPRESS_NT_SECUR, ai.Flags_NtSecure());

    ShowItem_Bool(IDG_COMPRESS_NTFS,
           ai.Flags_SymLinks()
        || ai.Flags_HardLinks()
        || ai.Flags_AltStreams()
        || ai.Flags_NtSecure());
  }
  // CheckVolumeEnable();

  EnableItem(IDG_COMPRESS_ENCRYPTION, fi.Encrypt);

  EnableItem(IDT_PASSWORD_ENTER, fi.Encrypt);
  EnableItem(IDT_PASSWORD_REENTER, fi.Encrypt);
  EnableItem(IDE_COMPRESS_PASSWORD1, fi.Encrypt);
  EnableItem(IDE_COMPRESS_PASSWORD2, fi.Encrypt);
  EnableItem(IDX_PASSWORD_SHOW, fi.Encrypt);

  EnableItem(IDT_COMPRESS_ENCRYPTION_METHOD, fi.Encrypt);
  EnableItem(IDC_COMPRESS_ENCRYPTION_METHOD, fi.Encrypt);
  EnableItem(IDX_COMPRESS_ENCRYPT_FILE_NAMES, fi.EncryptFileNames);

  ShowItem_Bool(IDX_COMPRESS_ENCRYPT_FILE_NAMES, fi.EncryptFileNames);
}

bool CCompressDialog::IsSFX()
{
  CWindow sfxButton = GetItem(IDX_COMPRESS_SFX);
  return sfxButton.IsEnabled() && IsButtonCheckedBool(IDX_COMPRESS_SFX);
}

static int GetExtDotPos(const UString &s)
{
  int dotPos = s.ReverseFind_Dot();
  if (dotPos > s.ReverseFind_PathSepar() + 1)
    return dotPos;
  return -1;
}

void CCompressDialog::OnButtonSFX()
{
  UString fileName;
  m_ArchivePath.GetText(fileName);
  int dotPos = GetExtDotPos(fileName);
  if (IsSFX())
  {
    if (dotPos >= 0)
      fileName.DeleteFrom(dotPos);
    fileName += kExeExt;
    m_ArchivePath.SetText(fileName);
  }
  else
  {
    if (dotPos >= 0)
    {
      UString ext = fileName.Ptr(dotPos);
      if (ext.IsEqualTo_Ascii_NoCase(kExeExt))
      {
        fileName.DeleteFrom(dotPos);
        m_ArchivePath.SetText(fileName);
      }
    }
    SetArchiveName2(false); // it's for OnInit
  }

  // CheckVolumeEnable();
}

bool CCompressDialog::GetFinalPath_Smart(UString &resPath)
{
  UString name;
  m_ArchivePath.GetText(name);
  name.Trim();
  UString tempPath = name;
  if (!IsAbsolutePath(name))
  {
    UString newDirPrefix = DirPrefix;
    if (newDirPrefix.IsEmpty())
      newDirPrefix = StartDirPrefix;
    FString resultF;
    if (!MyGetFullPathName(us2fs(newDirPrefix + name), resultF))
      return false;
    tempPath = fs2us(resultF);
  }
  if (!SetArcPathFields(tempPath, name, false))
    return false;
  FString resultF;
  if (!MyGetFullPathName(us2fs(DirPrefix + name), resultF))
    return false;
  resPath = fs2us(resultF);
  return true;
}

bool CCompressDialog::SetArcPathFields(const UString &path, UString &name, bool always)
{
  FString resDirPrefix;
  FString resFileName;
  bool res = GetFullPathAndSplit(us2fs(path), resDirPrefix, resFileName);
  if (res)
  {
    DirPrefix = fs2us(resDirPrefix);
    name = fs2us(resFileName);
  }
  else
  {
    if (!always)
      return false;
    DirPrefix.Empty();
    name = path;
  }
  SetItemText(IDT_COMPRESS_ARCHIVE_FOLDER, DirPrefix);
  m_ArchivePath.SetText(name);
  return res;
}

static const wchar_t * const k_IncorrectPathMessage = L"Incorrect archive path";

void CCompressDialog::OnButtonSetArchive()
{
  UString path;
  if (!GetFinalPath_Smart(path))
  {
    ShowErrorMessage(*this, k_IncorrectPathMessage);
    return;
  }

  UString title = LangString(IDS_COMPRESS_SET_ARCHIVE_BROWSE);
  UString filterDescription = LangString(IDS_OPEN_TYPE_ALL_FILES);
  filterDescription += " (*.*)";
  UString resPath;
  CurrentDirWasChanged = true;
  if (!MyBrowseForFile(*this, title,
      // DirPrefix.IsEmpty() ? NULL : (const wchar_t *)DirPrefix,
      // NULL,
      path,
      filterDescription,
      NULL, // L"*.*",
      resPath))
    return;
  UString dummyName;
  SetArcPathFields(resPath, dummyName, true);
}

// in ExtractDialog.cpp
extern void AddUniqueString(UStringVector &strings, const UString &srcString);

static bool IsAsciiString(const UString &s)
{
  for (unsigned i = 0; i < s.Len(); i++)
  {
    wchar_t c = s[i];
    if (c < 0x20 || c > 0x7F)
      return false;
  }
  return true;
}

void CCompressDialog::OnOK()
{
  _password1Control.GetText(Info.Password);
  if (IsZipFormat())
  {
    if (!IsAsciiString(Info.Password))
    {
      ShowErrorMessageHwndRes(*this, IDS_PASSWORD_USE_ASCII);
      return;
    }
    UString method = GetEncryptionMethodSpec();
    if (method.IsPrefixedBy_Ascii_NoCase("aes"))
    {
      if (Info.Password.Len() > 99)
      {
        ShowErrorMessageHwndRes(*this, IDS_PASSWORD_TOO_LONG);
        return;
      }
    }
  }
  if (!IsShowPasswordChecked())
  {
    UString password2;
    _password2Control.GetText(password2);
    if (password2 != Info.Password)
    {
      ShowErrorMessageHwndRes(*this, IDS_PASSWORD_NOT_MATCH);
      return;
    }
  }

  SaveOptionsInMem();
  {
    UString s;
    if (!GetFinalPath_Smart(s))
    {
      ShowErrorMessage(*this, k_IncorrectPathMessage);
      return;
    }
    
    m_RegistryInfo.ArcPaths.Clear();
    AddUniqueString(m_RegistryInfo.ArcPaths, s);
    Info.ArcPath = s;
  }
  
  Info.UpdateMode = (NCompressDialog::NUpdateMode::EEnum)k_UpdateMode_Vals[m_UpdateMode.GetCurSel()];;
  Info.PathMode = (NWildcard::ECensorPathMode)k_PathMode_Vals[m_PathMode.GetCurSel()];

  Info.Level = GetLevelSpec();
  Info.Dictionary = GetDictionarySpec();
  Info.Order = GetOrderSpec();
  Info.OrderMode = GetOrderMode();
  Info.NumThreads = GetNumThreadsSpec();

  {
    // Info.SolidIsSpecified = g_Formats[GetStaticFormatIndex()].Solid;
    UInt32 solidLogSize = GetBlockSizeSpec();
    Info.SolidBlockSize = 0;
    if (solidLogSize == (UInt32)(Int32)-1)
      Info.SolidIsSpecified = false;
    else if (solidLogSize > 0)
      Info.SolidBlockSize = (solidLogSize >= 64) ?
          (UInt64)(Int64)-1 :
          ((UInt64)1 << solidLogSize);
  }

  Info.Method = GetMethodSpec();
  Info.EncryptionMethod = GetEncryptionMethodSpec();
  Info.FormatIndex = GetFormatIndex();
  Info.SFXMode = IsSFX();
  Info.OpenShareForWrite = IsButtonCheckedBool(IDX_COMPRESS_SHARED);
  Info.DeleteAfterCompressing = IsButtonCheckedBool(IDX_COMPRESS_DEL);

  m_RegistryInfo.EncryptHeaders =
    Info.EncryptHeaders = IsButtonCheckedBool(IDX_COMPRESS_ENCRYPT_FILE_NAMES);


  GetButton_Bools(IDX_COMPRESS_NT_SYM_LINKS,   Info.SymLinks,   m_RegistryInfo.SymLinks);
  GetButton_Bools(IDX_COMPRESS_NT_HARD_LINKS,  Info.HardLinks,  m_RegistryInfo.HardLinks);
  GetButton_Bools(IDX_COMPRESS_NT_ALT_STREAMS, Info.AltStreams, m_RegistryInfo.AltStreams);
  GetButton_Bools(IDX_COMPRESS_NT_SECUR,       Info.NtSecurity, m_RegistryInfo.NtSecurity);

  {
    const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
    if (!ai.Flags_SymLinks()) Info.SymLinks.Val = false;
    if (!ai.Flags_HardLinks()) Info.HardLinks.Val = false;
    if (!ai.Flags_AltStreams()) Info.AltStreams.Val = false;
    if (!ai.Flags_NtSecure()) Info.NtSecurity.Val = false;
  }

  m_Params.GetText(Info.Options);
  
  UString volumeString;
  m_Volume.GetText(volumeString);
  volumeString.Trim();
  Info.VolumeSizes.Clear();
  
  if (!volumeString.IsEmpty())
  {
    if (!ParseVolumeSizes(volumeString, Info.VolumeSizes))
    {
      ShowErrorMessageHwndRes(*this, IDS_INCORRECT_VOLUME_SIZE);
      return;
    }
    if (!Info.VolumeSizes.IsEmpty())
    {
      const UInt64 volumeSize = Info.VolumeSizes.Back();
      if (volumeSize < (100 << 10))
      {
        wchar_t s[32];
        ConvertUInt64ToString(volumeSize, s);
        if (::MessageBoxW(*this, MyFormatNew(IDS_SPLIT_CONFIRM, s),
            L"7-Zip", MB_YESNOCANCEL | MB_ICONQUESTION) != IDYES)
          return;
      }
    }
  }

  for (int i = 0; i < m_ArchivePath.GetCount(); i++)
  {
    UString sTemp;
    m_ArchivePath.GetLBText(i, sTemp);
    sTemp.Trim();
    AddUniqueString(m_RegistryInfo.ArcPaths, sTemp);
  }
  
  if (m_RegistryInfo.ArcPaths.Size() > kHistorySize)
    m_RegistryInfo.ArcPaths.DeleteBack();
  
  if (Info.FormatIndex >= 0)
    m_RegistryInfo.ArcType = (*ArcFormats)[Info.FormatIndex].Name;
  m_RegistryInfo.ShowPassword = IsShowPasswordChecked();

  m_RegistryInfo.Save();
  
  CModalDialog::OnOK();
}

#define kHelpTopic "fm/plugins/7-zip/add.htm"

void CCompressDialog::OnHelp()
{
  ShowHelpWindow(kHelpTopic);
}

bool CCompressDialog::OnCommand(int code, int itemID, LPARAM lParam)
{
  if (code == CBN_SELCHANGE)
  {
    switch (itemID)
    {
      case IDC_COMPRESS_ARCHIVE:
      {
        // we can 't change m_ArchivePath in that handler !
        DirPrefix.Empty();
        SetItemText(IDT_COMPRESS_ARCHIVE_FOLDER, DirPrefix);

        /*
        UString path;
        m_ArchivePath.GetText(path);
        m_ArchivePath.SetText(L"");
        if (IsAbsolutePath(path))
        {
          UString fileName;
          SetArcPathFields(path, fileName);
          SetArchiveName(fileName);
        }
        */
        return true;
      }
      
      case IDC_COMPRESS_FORMAT:
      {
        bool isSFX = IsSFX();
        SaveOptionsInMem();
        m_Solid.ResetContent();
        SetLevel();
        SetSolidBlockSize();
        SetNumThreads();
        SetParams();
        CheckControlsEnable();
        SetArchiveName2(isSFX);
        SetEncryptionMethod();
        SetMemoryUsage();
        return true;
      }
      
      case IDC_COMPRESS_LEVEL:
      {
        {
          const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
          int index = FindRegistryFormatAlways(ai.Name);
          NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
          fo.ResetForLevelChange();
        }

        SetMethod();
        SetSolidBlockSize();
        SetNumThreads();
        CheckSFXNameChange();
        SetMemoryUsage();
        return true;
      }
      
      case IDC_COMPRESS_METHOD:
      {
        SetDictionary();
        SetOrder();
        SetSolidBlockSize();
        SetNumThreads();
        CheckSFXNameChange();
        SetMemoryUsage();
        return true;
      }
      
      case IDC_COMPRESS_DICTIONARY:
      {
        UInt32 blockSizeLog = GetBlockSizeSpec();
        if (blockSizeLog != (UInt32)(Int32)-1
            && blockSizeLog != kNoSolidBlockSize
            && blockSizeLog != kSolidBlockSize)
        {
          const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
          int index = FindRegistryFormatAlways(ai.Name);
          NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
          fo.Reset_BlockLogSize();
          SetSolidBlockSize(true);
        }

        SetMemoryUsage();
        return true;
      }
      
      case IDC_COMPRESS_ORDER:
        return true;
      
      case IDC_COMPRESS_SOLID:
      {
        SetMemoryUsage();
        return true;
      }

      case IDC_COMPRESS_THREADS:
      {
        SetMemoryUsage();
        return true;
      }
    }
  }
  return CModalDialog::OnCommand(code, itemID, lParam);
}

void CCompressDialog::CheckSFXNameChange()
{
  bool isSFX = IsSFX();
  CheckSFXControlsEnable();
  if (isSFX != IsSFX())
    SetArchiveName2(isSFX);
}

void CCompressDialog::SetArchiveName2(bool prevWasSFX)
{
  UString fileName;
  m_ArchivePath.GetText(fileName);
  const CArcInfoEx &prevArchiverInfo = (*ArcFormats)[m_PrevFormat];
  if (prevArchiverInfo.Flags_KeepName() || Info.KeepName)
  {
    UString prevExtension;
    if (prevWasSFX)
      prevExtension = kExeExt;
    else
    {
      prevExtension += '.';
      prevExtension += prevArchiverInfo.GetMainExt();
    }
    const unsigned prevExtensionLen = prevExtension.Len();
    if (fileName.Len() >= prevExtensionLen)
      if (StringsAreEqualNoCase(fileName.RightPtr(prevExtensionLen), prevExtension))
        fileName.DeleteFrom(fileName.Len() - prevExtensionLen);
  }
  SetArchiveName(fileName);
}

// if type.KeepName then use OriginalFileName
// else if !KeepName remove extension
// add new extension

void CCompressDialog::SetArchiveName(const UString &name)
{
  UString fileName = name;
  Info.FormatIndex = GetFormatIndex();
  const CArcInfoEx &ai = (*ArcFormats)[Info.FormatIndex];
  m_PrevFormat = Info.FormatIndex;
  if (ai.Flags_KeepName())
  {
    fileName = OriginalFileName;
  }
  else
  {
    if (!Info.KeepName)
    {
      int dotPos = GetExtDotPos(fileName);
      if (dotPos >= 0)
        fileName.DeleteFrom(dotPos);
    }
  }

  if (IsSFX())
    fileName += kExeExt;
  else
  {
    fileName += '.';
    fileName += ai.GetMainExt();
  }
  m_ArchivePath.SetText(fileName);
}

int CCompressDialog::FindRegistryFormat(const UString &name)
{
  FOR_VECTOR (i, m_RegistryInfo.Formats)
  {
    const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[i];
    if (name.IsEqualTo_NoCase(GetUnicodeString(fo.FormatID)))
      return i;
  }
  return -1;
}

int CCompressDialog::FindRegistryFormatAlways(const UString &name)
{
  int index = FindRegistryFormat(name);
  if (index < 0)
  {
    NCompression::CFormatOptions fo;
    fo.FormatID = GetSystemString(name);
    index = m_RegistryInfo.Formats.Add(fo);
  }
  return index;
}

int CCompressDialog::GetStaticFormatIndex()
{
  const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
  for (unsigned i = 0; i < ARRAY_SIZE(g_Formats); i++)
    if (ai.Name.IsEqualTo_Ascii_NoCase(g_Formats[i].Name))
      return i;
  return 0; // -1;
}

void CCompressDialog::SetNearestSelectComboBox(NControl::CComboBox &comboBox, UInt32 value)
{
  for (int i = comboBox.GetCount() - 1; i >= 0; i--)
    if ((UInt32)comboBox.GetItemData(i) <= value)
    {
      comboBox.SetCurSel(i);
      return;
    }
  if (comboBox.GetCount() > 0)
    comboBox.SetCurSel(0);
}

void CCompressDialog::SetLevel()
{
  m_Level.ResetContent();
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
  UInt32 level = 5;
  {
    int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      if (fo.Level <= 9)
        level = fo.Level;
      else if (fo.Level == (UInt32)(Int32)-1)
        level = 5;
      else
        level = 9;
    }
  }
  
  for (unsigned i = 0; i <= 9; i++)
  {
    if ((fi.LevelsMask & (1 << i)) != 0)
    {
      UInt32 langID = g_Levels[i];
      int index = (int)m_Level.AddString(LangString(langID));
      m_Level.SetItemData(index, i);
    }
  }
  SetNearestSelectComboBox(m_Level, level);
  SetMethod();
}


static LRESULT ComboBox_AddStringAscii(NControl::CComboBox &cb, const char *s)
{
  return cb.AddString((CSysString)s);
}


void CCompressDialog::SetMethod(int keepMethodId)
{
  m_Method.ResetContent();
  UInt32 level = GetLevel();
  if (level == 0)
  {
    SetDictionary();
    SetOrder();
    return;
  }
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
  int index = FindRegistryFormat(ai.Name);
  UString defaultMethod;
  if (index >= 0)
  {
    const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
    defaultMethod = fo.Method;
  }
  bool isSfx = IsSFX();
  bool weUseSameMethod = false;
  
  for (unsigned m = 0; m < fi.NumMethods; m++)
  {
    EMethodID methodID = fi.MathodIDs[m];
    if (isSfx)
      if (!IsMethodSupportedBySfx(methodID))
        continue;
    const char *method = kMethodsNames[methodID];
    int itemIndex = (int)ComboBox_AddStringAscii(m_Method, method);
    m_Method.SetItemData(itemIndex, methodID);
    if (keepMethodId == methodID)
    {
      m_Method.SetCurSel(itemIndex);
      weUseSameMethod = true;
      continue;
    }
    if ((defaultMethod.IsEqualTo_Ascii_NoCase(method) || m == 0) && !weUseSameMethod)
      m_Method.SetCurSel(itemIndex);
  }
  
  if (!weUseSameMethod)
  {
    SetDictionary();
    SetOrder();
  }
}

bool CCompressDialog::IsZipFormat()
{
  const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
  return ai.Name.IsEqualTo_Ascii_NoCase("zip");
}

bool CCompressDialog::IsXzFormat()
{
  const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
  return ai.Name.IsEqualTo_Ascii_NoCase("xz");
}

void CCompressDialog::SetEncryptionMethod()
{
  _encryptionMethod.ResetContent();
  const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
  if (ai.Name.IsEqualTo_Ascii_NoCase("7z"))
  {
    ComboBox_AddStringAscii(_encryptionMethod, "AES-256");
    _encryptionMethod.SetCurSel(0);
  }
  else if (ai.Name.IsEqualTo_Ascii_NoCase("zip"))
  {
    int index = FindRegistryFormat(ai.Name);
    UString encryptionMethod;
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      encryptionMethod = fo.EncryptionMethod;
    }
    ComboBox_AddStringAscii(_encryptionMethod, "ZipCrypto");
    ComboBox_AddStringAscii(_encryptionMethod, "AES-256");
    _encryptionMethod.SetCurSel(encryptionMethod.IsPrefixedBy_Ascii_NoCase("aes") ? 1 : 0);
  }
}

int CCompressDialog::GetMethodID()
{
  if (m_Method.GetCount() <= 0)
    return -1;
  return (int)(UInt32)m_Method.GetItemData_of_CurSel();
}

UString CCompressDialog::GetMethodSpec()
{
  UString s;
  if (m_Method.GetCount() > 1)
    s = kMethodsNames[GetMethodID()];
  return s;
}

UString CCompressDialog::GetEncryptionMethodSpec()
{
  UString s;
  if (_encryptionMethod.GetCount() > 1
      && _encryptionMethod.GetCurSel() > 0)
  {
    _encryptionMethod.GetText(s);
    s.RemoveChar(L'-');
  }
  return s;
}

void CCompressDialog::AddDictionarySize(UInt32 size)
{
  Byte c = 0;
  unsigned moveBits = 0;
  if ((size & 0xFFFFF) == 0)    { moveBits = 20; c = 'M'; }
  else if ((size & 0x3FF) == 0) { moveBits = 10; c = 'K'; }
  TCHAR s[40];
  ConvertUInt32ToString(size >> moveBits, s);
  unsigned pos = MyStringLen(s);
  s[pos++] = ' ';
  if (moveBits != 0)
    s[pos++] = c;
  s[pos++] = 'B';
  s[pos++] = 0;
  int index = (int)m_Dictionary.AddString(s);
  m_Dictionary.SetItemData(index, size);
}

void CCompressDialog::SetDictionary()
{
  m_Dictionary.ResetContent();
  const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
  int index = FindRegistryFormat(ai.Name);
  UInt32 defaultDict = (UInt32)(Int32)-1;
  
  if (index >= 0)
  {
    const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
    if (fo.Method.IsEqualTo_NoCase(GetMethodSpec()))
      defaultDict = fo.Dictionary;
  }
  
  int methodID = GetMethodID();
  UInt32 level = GetLevel2();
  if (methodID < 0)
    return;
  UInt64 maxRamSize;
  bool maxRamSize_Defined = GetMaxRamSizeForProgram(maxRamSize);
  
  switch (methodID)
  {
    case kLZMA:
    case kLZMA2:
    {
      static const UInt32 kMinDicSize = (1 << 16);
      if (defaultDict == (UInt32)(Int32)-1)
      {
             if (level >= 9) defaultDict = (1 << 26);
        else if (level >= 7) defaultDict = (1 << 25);
        else if (level >= 5) defaultDict = (1 << 24);
        else if (level >= 3) defaultDict = (1 << 20);
        else                 defaultDict = (kMinDicSize);
      }
      
      AddDictionarySize(kMinDicSize);
      m_Dictionary.SetCurSel(0);
      
      for (unsigned i = 20; i <= 31; i++)
        for (unsigned j = 0; j < 2; j++)
        {
          if (i == 20 && j > 0)
            continue;
          UInt32 dict = ((UInt32)(2 + j) << (i - 1));
          
          if (dict >
            #ifdef MY_CPU_64BIT
              (3 << 29)
            #else
              (1 << 26)
            #endif
            )
            continue;
          
          AddDictionarySize(dict);
          UInt64 decomprSize;
          UInt64 requiredComprSize = GetMemoryUsage(dict, decomprSize);
          if (dict <= defaultDict && (!maxRamSize_Defined || requiredComprSize <= maxRamSize))
            m_Dictionary.SetCurSel(m_Dictionary.GetCount() - 1);
        }

      // SetNearestSelectComboBox(m_Dictionary, defaultDict);
      break;
    }
    
    case kPPMd:
    {
      if (defaultDict == (UInt32)(Int32)-1)
      {
             if (level >= 9) defaultDict = (192 << 20);
        else if (level >= 7) defaultDict = ( 64 << 20);
        else if (level >= 5) defaultDict = ( 16 << 20);
        else                 defaultDict = (  4 << 20);
      }

      for (unsigned i = 20; i < 31; i++)
        for (unsigned j = 0; j < 2; j++)
        {
          if (i == 20 && j > 0)
            continue;
          UInt32 dict = ((UInt32)(2 + j) << (i - 1));
          if (dict >
            #ifdef MY_CPU_64BIT
              (1 << 30)
            #else
              (1 << 29)
            #endif
            )
            continue;
          AddDictionarySize(dict);
          UInt64 decomprSize;
          UInt64 requiredComprSize = GetMemoryUsage(dict, decomprSize);
          if ((dict <= defaultDict && (!maxRamSize_Defined || requiredComprSize <= maxRamSize))
              || m_Dictionary.GetCount() == 1)
            m_Dictionary.SetCurSel(m_Dictionary.GetCount() - 1);
        }
      
      // SetNearestSelectComboBox(m_Dictionary, defaultDict);
      break;
    }

    case kDeflate:
    {
      AddDictionarySize(32 << 10);
      m_Dictionary.SetCurSel(0);
      break;
    }
    
    case kDeflate64:
    {
      AddDictionarySize(64 << 10);
      m_Dictionary.SetCurSel(0);
      break;
    }
    
    case kBZip2:
    {
      if (defaultDict == (UInt32)(Int32)-1)
      {
             if (level >= 5) defaultDict = (900 << 10);
        else if (level >= 3) defaultDict = (500 << 10);
        else                 defaultDict = (100 << 10);
      }
      
      for (unsigned i = 1; i <= 9; i++)
      {
        UInt32 dict = ((UInt32)i * 100) << 10;
        AddDictionarySize(dict);
        if (dict <= defaultDict || m_Dictionary.GetCount() == 0)
          m_Dictionary.SetCurSel(m_Dictionary.GetCount() - 1);
      }
      
      break;
    }
    
    case kPPMdZip:
    {
      if (defaultDict == (UInt32)(Int32)-1)
        defaultDict = (1 << (19 + (level > 8 ? 8 : level)));
      
      for (unsigned i = 20; i <= 28; i++)
      {
        UInt32 dict = (1 << i);
        AddDictionarySize(dict);
        UInt64 decomprSize;
        UInt64 requiredComprSize = GetMemoryUsage(dict, decomprSize);
        if ((dict <= defaultDict && (!maxRamSize_Defined || requiredComprSize <= maxRamSize))
            || m_Dictionary.GetCount() == 1)
          m_Dictionary.SetCurSel(m_Dictionary.GetCount() - 1);
      }
      
      // SetNearestSelectComboBox(m_Dictionary, defaultDict);
      break;
    }
  }
}

UInt32 CCompressDialog::GetComboValue(NWindows::NControl::CComboBox &c, int defMax)
{
  if (c.GetCount() <= defMax)
    return (UInt32)(Int32)-1;
  return (UInt32)c.GetItemData_of_CurSel();
}

UInt32 CCompressDialog::GetLevel2()
{
  UInt32 level = GetLevel();
  if (level == (UInt32)(Int32)-1)
    level = 5;
  return level;
}

int CCompressDialog::AddOrder(UInt32 size)
{
  TCHAR s[40];
  ConvertUInt32ToString(size, s);
  int index = (int)m_Order.AddString(s);
  m_Order.SetItemData(index, size);
  return index;
}

void CCompressDialog::SetOrder()
{
  m_Order.ResetContent();
  const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
  int index = FindRegistryFormat(ai.Name);
  UInt32 defaultOrder = (UInt32)(Int32)-1;
  
  if (index >= 0)
  {
    const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
    if (fo.Method.IsEqualTo_NoCase(GetMethodSpec()))
      defaultOrder = fo.Order;
  }

  int methodID = GetMethodID();
  UInt32 level = GetLevel2();
  if (methodID < 0)
    return;
  
  switch (methodID)
  {
    case kLZMA:
    case kLZMA2:
    {
      if (defaultOrder == (UInt32)(Int32)-1)
        defaultOrder = (level >= 7) ? 64 : 32;
      for (unsigned i = 3; i <= 8; i++)
        for (unsigned j = 0; j < 2; j++)
        {
          UInt32 order = ((UInt32)(2 + j) << (i - 1));
          if (order <= 256)
            AddOrder(order);
        }
      AddOrder(273);
      SetNearestSelectComboBox(m_Order, defaultOrder);
      break;
    }
    
    case kPPMd:
    {
      if (defaultOrder == (UInt32)(Int32)-1)
      {
             if (level >= 9) defaultOrder = 32;
        else if (level >= 7) defaultOrder = 16;
        else if (level >= 5) defaultOrder = 6;
        else                 defaultOrder = 4;
      }
      
      AddOrder(2);
      AddOrder(3);
      
      for (unsigned i = 2; i < 8; i++)
        for (unsigned j = 0; j < 4; j++)
        {
          UInt32 order = (4 + j) << (i - 2);
          if (order < 32)
            AddOrder(order);
        }
      
      AddOrder(32);
      SetNearestSelectComboBox(m_Order, defaultOrder);
      break;
    }
    
    case kDeflate:
    case kDeflate64:
    {
      if (defaultOrder == (UInt32)(Int32)-1)
      {
             if (level >= 9) defaultOrder = 128;
        else if (level >= 7) defaultOrder = 64;
        else                 defaultOrder = 32;
      }
      
      for (unsigned i = 3; i <= 8; i++)
        for (unsigned j = 0; j < 2; j++)
        {
          UInt32 order = ((UInt32)(2 + j) << (i - 1));;
          if (order <= 256)
            AddOrder(order);
        }
      
      AddOrder(methodID == kDeflate64 ? 257 : 258);
      SetNearestSelectComboBox(m_Order, defaultOrder);
      break;
    }
    
    case kBZip2:
      break;
    
    case kPPMdZip:
    {
      if (defaultOrder == (UInt32)(Int32)-1)
        defaultOrder = level + 3;
      for (unsigned i = 2; i <= 16; i++)
        AddOrder(i);
      SetNearestSelectComboBox(m_Order, defaultOrder);
      break;
    }
  }
}

bool CCompressDialog::GetOrderMode()
{
  switch (GetMethodID())
  {
    case kPPMd:
    case kPPMdZip:
      return true;
  }
  return false;
}


void CCompressDialog::SetSolidBlockSize(bool useDictionary)
{
  m_Solid.ResetContent();
  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  if (!fi.Solid)
    return;

  UInt32 level = GetLevel2();
  if (level == 0)
    return;

  UInt32 dict = GetDictionarySpec();
  if (dict == (UInt32)(Int32)-1)
    dict = 1;

  UInt32 defaultBlockSize = (UInt32)(Int32)-1;

  const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];

  if (useDictionary)
    defaultBlockSize = GetBlockSizeSpec();
  else
  {
    int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      if (fo.Method.IsEqualTo_NoCase(GetMethodSpec()))
        defaultBlockSize = fo.BlockLogSize;
    }
  }

  bool is7z = ai.Name.IsEqualTo_Ascii_NoCase("7z");

  {
    UString s ('-');
    if (is7z)
      LangString(IDS_COMPRESS_NON_SOLID, s);
    int index = (int)m_Solid.AddString(s);
    m_Solid.SetItemData(index, (UInt32)kNoSolidBlockSize);
    if (defaultBlockSize == kNoSolidBlockSize)
      m_Solid.SetCurSel(0);
  }
  
  UInt64 blockSize;

  if (is7z)
  {
    blockSize = (UInt64)dict << 7;
    const UInt32 kMinSize = (UInt32)1 << 24;
    const UInt64 kMaxSize = (UInt64)1 << 32;
    if (blockSize < kMinSize) blockSize = kMinSize;
    if (blockSize > kMaxSize) blockSize = kMaxSize;
  }
  else
  {
    blockSize = (UInt64)dict << 2;
    const UInt32 kMinSize = (UInt32)1 << 20;
    const UInt32 kMaxSize = (UInt32)1 << 28;
    if (blockSize < kMinSize) blockSize = kMinSize;
    if (blockSize > kMaxSize) blockSize = kMaxSize;
    if (blockSize < dict) blockSize = dict;
  }
  
  for (unsigned i = 20; i <= 36; i++)
  {
    if (defaultBlockSize == (UInt32)(Int32)-1 && ((UInt64)1 << i) >= blockSize)
      defaultBlockSize = i;

    TCHAR s[40];
    char post;
    ConvertUInt32ToString(1 << (i % 10), s);
         if (i < 20) post = 'K';
    else if (i < 30) post = 'M';
    else             post = 'G';
    unsigned pos = (unsigned)lstrlen(s);
    s[pos++] = ' ';
    s[pos++] = post;
    s[pos++] = 'B';
    s[pos] = 0;
    int index = (int)m_Solid.AddString(s);
    m_Solid.SetItemData(index, (UInt32)i);
  }
  
  {
    int index = (int)m_Solid.AddString(LangString(IDS_COMPRESS_SOLID));
    m_Solid.SetItemData(index, kSolidBlockSize);
  }
  
  if (defaultBlockSize == (UInt32)(Int32)-1)
    defaultBlockSize = kSolidBlockSize;
  if (defaultBlockSize != kNoSolidBlockSize)
    SetNearestSelectComboBox(m_Solid, defaultBlockSize);
}

void CCompressDialog::SetNumThreads()
{
  m_NumThreads.ResetContent();

  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  if (!fi.MultiThread)
    return;

  UInt32 numHardwareThreads = NSystem::GetNumberOfProcessors();
  UInt32 defaultValue = numHardwareThreads;

  {
    const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
    int index = FindRegistryFormat(ai.Name);
    if (index >= 0)
    {
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
      if (fo.Method.IsEqualTo_NoCase(GetMethodSpec()) && fo.NumThreads != (UInt32)(Int32)-1)
        defaultValue = fo.NumThreads;
    }
  }

  UInt32 numAlgoThreadsMax = 1;
  int methodID = GetMethodID();
  switch (methodID)
  {
    case kLZMA: numAlgoThreadsMax = 2; break;
    case kLZMA2: numAlgoThreadsMax = 32; break;
    case kBZip2: numAlgoThreadsMax = 32; break;
  }
  if (IsZipFormat())
    numAlgoThreadsMax = 128;
  for (UInt32 i = 1; i <= numHardwareThreads * 2 && i <= numAlgoThreadsMax; i++)
  {
    TCHAR s[40];
    ConvertUInt32ToString(i, s);
    int index = (int)m_NumThreads.AddString(s);
    m_NumThreads.SetItemData(index, (UInt32)i);
  }
  SetNearestSelectComboBox(m_NumThreads, defaultValue);
}

UInt64 CCompressDialog::GetMemoryUsage(UInt32 dict, UInt64 &decompressMemory)
{
  decompressMemory = UInt64(Int64(-1));
  UInt32 level = GetLevel2();
  if (level == 0)
  {
    decompressMemory = (1 << 20);
    return decompressMemory;
  }
  UInt64 size = 0;

  const CFormatInfo &fi = g_Formats[GetStaticFormatIndex()];
  if (fi.Filter && level >= 9)
    size += (12 << 20) * 2 + (5 << 20);
  UInt32 numThreads = GetNumThreads2();
  
  if (IsZipFormat())
  {
    UInt32 numSubThreads = 1;
    if (GetMethodID() == kLZMA && numThreads > 1 && level >= 5)
      numSubThreads = 2;
    UInt32 numMainThreads = numThreads / numSubThreads;
    if (numMainThreads > 1)
      size += (UInt64)numMainThreads << 25;
  }
  
  int methidId = GetMethodID();
  
  switch (methidId)
  {
    case kLZMA:
    case kLZMA2:
    {
      UInt32 hs = dict - 1;
      hs |= (hs >> 1);
      hs |= (hs >> 2);
      hs |= (hs >> 4);
      hs |= (hs >> 8);
      hs >>= 1;
      hs |= 0xFFFF;
      if (hs > (1 << 24))
        hs >>= 1;
      hs++;
      UInt64 size1 = (UInt64)hs * 4;
      size1 += (UInt64)dict * 4;
      if (level >= 5)
        size1 += (UInt64)dict * 4;
      size1 += (2 << 20);

      UInt32 numThreads1 = 1;
      if (numThreads > 1 && level >= 5)
      {
        size1 += (2 << 20) + (4 << 20);
        numThreads1 = 2;
      }
      
      UInt32 numBlockThreads = numThreads / numThreads1;
    
      UInt64 chunkSize = 0;

      if (methidId != kLZMA && numBlockThreads != 1)
      {
        chunkSize = (UInt64)dict << 2;
        chunkSize = MyMax(chunkSize, (UInt64)(1 << 20));
        chunkSize = MyMin(chunkSize, (UInt64)(1 << 28));
        chunkSize = MyMax(chunkSize, (UInt64)dict);

        if (IsXzFormat())
        {
          UInt32 blockSizeLog = GetBlockSizeSpec();
          if (blockSizeLog != (UInt32)(Int32)-1)
          {
            if (blockSizeLog == kSolidBlockSize)
            {
              numBlockThreads = 1;
              chunkSize = 0;
            }
            else if (blockSizeLog != kNoSolidBlockSize)
              chunkSize = (UInt64)1 << blockSizeLog;
          }
        }
      }

      if (chunkSize == 0)
        size += numBlockThreads * (size1 + (UInt64)dict * 3 / 2);
      else
      {
        size += numBlockThreads * (size1 + chunkSize);
        UInt64 numPackChunks = numBlockThreads + (numBlockThreads / 4) + 2;
        size += numPackChunks * chunkSize;
      }

      decompressMemory = dict + (2 << 20);
      return size;
    }
  
    case kPPMd:
    {
      decompressMemory = dict + (2 << 20);
      return size + decompressMemory;
    }
    
    case kDeflate:
    case kDeflate64:
    {
      /*
      UInt32 order = GetOrder();
      if (order == (UInt32)(Int32)-1)
        order = 32;
      */
      if (level >= 7)
        size += (1 << 20);
      size += 3 << 20;
      decompressMemory = (2 << 20);
      return size;
    }
    
    case kBZip2:
    {
      decompressMemory = (7 << 20);
      UInt64 memForOneThread = (10 << 20);
      return size + memForOneThread * numThreads;
    }
    
    case kPPMdZip:
    {
      decompressMemory = dict + (2 << 20);
      return size + (UInt64)decompressMemory * numThreads;
    }
  }
  
  return (UInt64)(Int64)-1;
}

UInt64 CCompressDialog::GetMemoryUsage(UInt64 &decompressMemory)
{
  return GetMemoryUsage(GetDictionary(), decompressMemory);
}

void CCompressDialog::PrintMemUsage(UINT res, UInt64 value)
{
  if (value == (UInt64)(Int64)-1)
  {
    SetItemText(res, TEXT("?"));
    return;
  }
  value = (value + (1 << 20) - 1) >> 20;
  TCHAR s[40];
  ConvertUInt64ToString(value, s);
  lstrcat(s, TEXT(" MB"));
  SetItemText(res, s);
}
    
void CCompressDialog::SetMemoryUsage()
{
  UInt64 decompressMem;
  UInt64 memUsage = GetMemoryUsage(decompressMem);
  PrintMemUsage(IDT_COMPRESS_MEMORY_VALUE, memUsage);
  PrintMemUsage(IDT_COMPRESS_MEMORY_DE_VALUE, decompressMem);
}

void CCompressDialog::SetParams()
{
  const CArcInfoEx &ai = (*ArcFormats)[GetFormatIndex()];
  m_Params.SetText(TEXT(""));
  int index = FindRegistryFormat(ai.Name);
  if (index >= 0)
  {
    const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
    m_Params.SetText(fo.Options);
  }
}

void CCompressDialog::SaveOptionsInMem()
{
  const CArcInfoEx &ai = (*ArcFormats)[Info.FormatIndex];
  int index = FindRegistryFormatAlways(ai.Name);
  m_Params.GetText(Info.Options);
  Info.Options.Trim();
  NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];
  fo.Options = Info.Options;
  fo.Level = GetLevelSpec();
  fo.Dictionary = GetDictionarySpec();
  fo.Order = GetOrderSpec();
  fo.Method = GetMethodSpec();
  fo.EncryptionMethod = GetEncryptionMethodSpec();
  fo.NumThreads = GetNumThreadsSpec();
  fo.BlockLogSize = GetBlockSizeSpec();
}

unsigned CCompressDialog::GetFormatIndex()
{
  return (unsigned)m_Format.GetItemData_of_CurSel();
}
