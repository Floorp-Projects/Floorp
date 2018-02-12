// HashGUI.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/ErrorMsg.h"

#include "../FileManager/FormatUtils.h"
#include "../FileManager/LangUtils.h"
#include "../FileManager/OverwriteDialogRes.h"
#include "../FileManager/ProgressDialog2.h"
#include "../FileManager/ProgressDialog2Res.h"
#include "../FileManager/PropertyNameRes.h"
#include "../FileManager/resourceGui.h"

#include "HashGUI.h"

using namespace NWindows;

class CHashCallbackGUI: public CProgressThreadVirt, public IHashCallbackUI
{
  UInt64 NumFiles;
  bool _curIsFolder;
  UString FirstFileName;

  HRESULT ProcessVirt();

public:
  const NWildcard::CCensor *censor;
  const CHashOptions *options;

  DECL_EXTERNAL_CODECS_LOC_VARS2;

  CHashCallbackGUI() {}
  ~CHashCallbackGUI() { }

  INTERFACE_IHashCallbackUI(;)

  void AddErrorMessage(DWORD systemError, const wchar_t *name)
  {
    ProgressDialog.Sync.AddError_Code_Name(systemError, name);
  }
};

static void AddValuePair(UString &s, UINT resourceID, UInt64 value)
{
  AddLangString(s, resourceID);
  s += ": ";
  char sz[32];
  ConvertUInt64ToString(value, sz);
  s += sz;
  s.Add_LF();
}

static void AddSizeValuePair(UString &s, UINT resourceID, UInt64 value)
{
  AddLangString(s, resourceID);
  s += ": ";
  {
    wchar_t sz[32];
    ConvertUInt64ToString(value, sz);
    s += MyFormatNew(IDS_FILE_SIZE, sz);
  }
  {
    char sz[32];
    ConvertUInt64ToString(value >> 20, sz);
    s += " (";
    s += sz;
    s += " MB)";
    s.Add_LF();
  }
}

HRESULT CHashCallbackGUI::StartScanning()
{
  CProgressSync &sync = ProgressDialog.Sync;
  sync.Set_Status(LangString(IDS_SCANNING));
  return CheckBreak();
}

HRESULT CHashCallbackGUI::ScanProgress(const CDirItemsStat &st, const FString &path, bool isDir)
{
  return ProgressDialog.Sync.ScanProgress(st.NumFiles, st.GetTotalBytes(), path, isDir);
}

HRESULT CHashCallbackGUI::ScanError(const FString &path, DWORD systemError)
{
  AddErrorMessage(systemError, fs2us(path));
  return CheckBreak();
}

HRESULT CHashCallbackGUI::FinishScanning(const CDirItemsStat &st)
{
  return ScanProgress(st, FString(), false);
}

HRESULT CHashCallbackGUI::CheckBreak()
{
  return ProgressDialog.Sync.CheckStop();
}

HRESULT CHashCallbackGUI::SetNumFiles(UInt64 numFiles)
{
  CProgressSync &sync = ProgressDialog.Sync;
  sync.Set_NumFilesTotal(numFiles);
  return CheckBreak();
}

HRESULT CHashCallbackGUI::SetTotal(UInt64 size)
{
  CProgressSync &sync = ProgressDialog.Sync;
  sync.Set_NumBytesTotal(size);
  return CheckBreak();
}

HRESULT CHashCallbackGUI::SetCompleted(const UInt64 *completed)
{
  return ProgressDialog.Sync.Set_NumBytesCur(completed);
}

HRESULT CHashCallbackGUI::BeforeFirstFile(const CHashBundle & /* hb */)
{
  return S_OK;
}

HRESULT CHashCallbackGUI::GetStream(const wchar_t *name, bool isFolder)
{
  if (NumFiles == 0)
    FirstFileName = name;
  _curIsFolder = isFolder;
  CProgressSync &sync = ProgressDialog.Sync;
  sync.Set_FilePath(name, isFolder);
  return CheckBreak();
}

HRESULT CHashCallbackGUI::OpenFileError(const FString &path, DWORD systemError)
{
  // if (systemError == ERROR_SHARING_VIOLATION)
  {
    AddErrorMessage(systemError, fs2us(path));
    return S_FALSE;
  }
  // return systemError;
}

HRESULT CHashCallbackGUI::SetOperationResult(UInt64 /* fileSize */, const CHashBundle & /* hb */, bool /* showHash */)
{
  CProgressSync &sync = ProgressDialog.Sync;
  if (!_curIsFolder)
    NumFiles++;
  sync.Set_NumFilesCur(NumFiles);
  return CheckBreak();
}

static void AddHashString(UString &s, const CHasherState &h, unsigned digestIndex, const wchar_t *title)
{
  s += title;
  s.Add_Space();
  char temp[k_HashCalc_DigestSize_Max * 2 + 4];
  AddHashHexToString(temp, h.Digests[digestIndex], h.DigestSize);
  s += temp;
  s.Add_LF();
}

static void AddHashResString(UString &s, const CHasherState &h, unsigned digestIndex, UInt32 resID)
{
  UString s2 = LangString(resID);
  UString name (h.Name);
  s2.Replace(L"CRC", name);
  AddHashString(s, h, digestIndex, s2);
}

void AddHashBundleRes(UString &s, const CHashBundle &hb, const UString &firstFileName)
{
  if (hb.NumErrors != 0)
  {
    AddValuePair(s, IDS_PROP_NUM_ERRORS, hb.NumErrors);
    s.Add_LF();
  }
  
  if (hb.NumFiles == 1 && hb.NumDirs == 0 && !firstFileName.IsEmpty())
  {
    AddLangString(s, IDS_PROP_NAME);
    s += ": ";
    s += firstFileName;
    s.Add_LF();
  }
  else
  {
    AddValuePair(s, IDS_PROP_FOLDERS, hb.NumDirs);
    AddValuePair(s, IDS_PROP_FILES, hb.NumFiles);
  }

  AddSizeValuePair(s, IDS_PROP_SIZE, hb.FilesSize);

  if (hb.NumAltStreams != 0)
  {
    s.Add_LF();
    AddValuePair(s, IDS_PROP_NUM_ALT_STREAMS, hb.NumAltStreams);
    AddSizeValuePair(s, IDS_PROP_ALT_STREAMS_SIZE, hb.AltStreamsSize);
  }

  if (hb.NumErrors == 0 && hb.Hashers.IsEmpty())
  {
    s.Add_LF();
    AddLangString(s, IDS_MESSAGE_NO_ERRORS);
  }

  FOR_VECTOR (i, hb.Hashers)
  {
    s.Add_LF();
    const CHasherState &h = hb.Hashers[i];
    if (hb.NumFiles == 1 && hb.NumDirs == 0)
    {
      s += h.Name;
      AddHashString(s, h, k_HashCalc_Index_DataSum, L":");
    }
    else
    {
      AddHashResString(s, h, k_HashCalc_Index_DataSum, IDS_CHECKSUM_CRC_DATA);
      AddHashResString(s, h, k_HashCalc_Index_NamesSum, IDS_CHECKSUM_CRC_DATA_NAMES);
    }
    if (hb.NumAltStreams != 0)
    {
      AddHashResString(s, h, k_HashCalc_Index_StreamsSum, IDS_CHECKSUM_CRC_STREAMS_NAMES);
    }
  }
}

HRESULT CHashCallbackGUI::AfterLastFile(const CHashBundle &hb)
{
  UString s;
  AddHashBundleRes(s, hb, FirstFileName);
  
  CProgressSync &sync = ProgressDialog.Sync;
  sync.Set_NumFilesCur(hb.NumFiles);

  CProgressMessageBoxPair &pair = GetMessagePair(hb.NumErrors != 0);
  pair.Message = s;
  LangString(IDS_CHECKSUM_INFORMATION, pair.Title);

  return S_OK;
}

HRESULT CHashCallbackGUI::ProcessVirt()
{
  NumFiles = 0;

  AString errorInfo;
  HRESULT res = HashCalc(EXTERNAL_CODECS_LOC_VARS
      *censor, *options, errorInfo, this);

  return res;
}

HRESULT HashCalcGUI(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const NWildcard::CCensor &censor,
    const CHashOptions &options,
    bool &messageWasDisplayed)
{
  CHashCallbackGUI t;
  #ifdef EXTERNAL_CODECS
  t.__externalCodecs = __externalCodecs;
  #endif
  t.censor = &censor;
  t.options = &options;

  t.ProgressDialog.ShowCompressionInfo = false;

  const UString title = LangString(IDS_CHECKSUM_CALCULATING);

  t.ProgressDialog.MainTitle = "7-Zip"; // LangString(IDS_APP_TITLE);
  t.ProgressDialog.MainAddTitle = title;
  t.ProgressDialog.MainAddTitle.Add_Space();

  RINOK(t.Create(title));
  messageWasDisplayed = t.ThreadFinishedOK && t.ProgressDialog.MessagesDisplayed;
  return S_OK;
}
