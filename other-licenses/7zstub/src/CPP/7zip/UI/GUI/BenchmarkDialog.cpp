// BenchmarkDialog.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/Defs.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/MyException.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#include "../../../Windows/System.h"
#include "../../../Windows/Thread.h"

#include "../../Common/MethodProps.h"

#include "../FileManager/HelpUtils.h"

#include "../../MyVersion.h"

#include "BenchmarkDialog.h"

using namespace NWindows;

#define kHelpTopic "fm/benchmark.htm"

static const UINT_PTR kTimerID = 4;
static const UINT kTimerElapse = 1000;

#ifdef LANG
#include "../FileManager/LangUtils.h"
#endif

using namespace NWindows;

UString HResultToMessage(HRESULT errorCode);

#ifdef LANG
static const UInt32 kLangIDs[] =
{
  IDT_BENCH_DICTIONARY,
  IDT_BENCH_MEMORY,
  IDT_BENCH_NUM_THREADS,
  IDT_BENCH_SPEED,
  IDT_BENCH_RATING_LABEL,
  IDT_BENCH_USAGE_LABEL,
  IDT_BENCH_RPU_LABEL,
  IDG_BENCH_COMPRESSING,
  IDG_BENCH_DECOMPRESSING,
  IDG_BENCH_TOTAL_RATING,
  IDT_BENCH_CURRENT,
  IDT_BENCH_RESULTING,
  IDT_BENCH_ELAPSED,
  IDT_BENCH_PASSES,
  IDB_STOP,
  IDB_RESTART
};

static const UInt32 kLangIDs_Colon[] =
{
  IDT_BENCH_SIZE
};

#endif

static LPCTSTR const kProcessingString = TEXT("...");
static LPCTSTR const kMB = TEXT(" MB");
static LPCTSTR const kMIPS = TEXT(" MIPS");
static LPCTSTR const kKBs = TEXT(" KB/s");

static const unsigned kMinDicLogSize =
  #ifdef UNDER_CE
    20;
  #else
    21;
  #endif

static const UInt32 kMinDicSize = (1 << kMinDicLogSize);
static const UInt32 kMaxDicSize =
    #ifdef MY_CPU_64BIT
    (1 << 30);
    #else
    (1 << 27);
    #endif

bool CBenchmarkDialog::OnInit()
{
  #ifdef LANG
  LangSetWindowText(*this, IDD_BENCH);
  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));
  LangSetDlgItems_Colon(*this, kLangIDs_Colon, ARRAY_SIZE(kLangIDs_Colon));
  LangSetDlgItemText(*this, IDT_BENCH_CURRENT2, IDT_BENCH_CURRENT);
  LangSetDlgItemText(*this, IDT_BENCH_RESULTING2, IDT_BENCH_RESULTING);
  #endif

  Sync.Init();

  if (TotalMode)
  {
    _consoleEdit.Attach(GetItem(IDE_BENCH2_EDIT));
    LOGFONT f;
    memset(&f, 0, sizeof(f));
    f.lfHeight = 14;
    f.lfWidth = 0;
    f.lfWeight = FW_DONTCARE;
    f.lfCharSet = DEFAULT_CHARSET;
    f.lfOutPrecision = OUT_DEFAULT_PRECIS;
    f.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    f.lfQuality = DEFAULT_QUALITY;

    f.lfPitchAndFamily = FIXED_PITCH;
    // MyStringCopy(f.lfFaceName, TEXT(""));
    // f.lfFaceName[0] = 0;
    _font.Create(&f);
    if (_font._font)
      _consoleEdit.SendMsg(WM_SETFONT, (WPARAM)_font._font, TRUE);
  }

  UInt32 numCPUs = 1;

  {
    UString s ("/ ");
  
    NSystem::CProcessAffinity threadsInfo;
    threadsInfo.InitST();

    #ifndef _7ZIP_ST

    if (threadsInfo.Get() && threadsInfo.processAffinityMask != 0)
      numCPUs = threadsInfo.GetNumProcessThreads();
    else
      numCPUs = NSystem::GetNumberOfProcessors();

    #endif

    s.Add_UInt32(numCPUs);
    s += GetProcessThreadsInfo(threadsInfo);
    SetItemText(IDT_BENCH_HARDWARE_THREADS, s);
  }

  {
    UString s;
    {
      AString s1, s2;
      GetSysInfo(s1, s2);
      s = s1;
      SetItemText(IDT_BENCH_SYS1, s);
      if (s1 != s2 && !s2.IsEmpty())
      {
        s = s2;
        SetItemText(IDT_BENCH_SYS2, s);
      }
    }
    /*
    {
      GetVersionString(s);
      SetItemText(IDT_BENCH_SYSTEM, s);
    }
    */
    {
      AString s2;
      GetCpuName(s2);
      s = s2;
      SetItemText(IDT_BENCH_CPU, s);
    }
    /*
    {
      AString s2;
      GetCpuFeatures(s2);
      s = s2;
      SetItemText(IDT_BENCH_CPU_FEATURE, s);
    }
    */

    s = "7-Zip " MY_VERSION_CPU;
    SetItemText(IDT_BENCH_VER, s);
  }


  if (numCPUs < 1)
    numCPUs = 1;
  numCPUs = MyMin(numCPUs, (UInt32)(1 << 8));

  if (Sync.NumThreads == (UInt32)(Int32)-1)
  {
    Sync.NumThreads = numCPUs;
    if (Sync.NumThreads > 1)
      Sync.NumThreads &= ~1;
  }
  m_NumThreads.Attach(GetItem(IDC_BENCH_NUM_THREADS));
  int cur = 0;
  for (UInt32 num = 1; num <= numCPUs * 2;)
  {
    TCHAR s[16];
    ConvertUInt32ToString(num, s);
    int index = (int)m_NumThreads.AddString(s);
    m_NumThreads.SetItemData(index, num);
    if (num <= Sync.NumThreads)
      cur = index;
    if (num > 1)
      num++;
    num++;
  }
  m_NumThreads.SetCurSel(cur);
  Sync.NumThreads = GetNumberOfThreads();

  m_Dictionary.Attach(GetItem(IDC_BENCH_DICTIONARY));
  cur = 0;
  
  ramSize = (UInt64)(sizeof(size_t)) << 29;
  ramSize_Defined = NSystem::GetRamSize(ramSize);
  
  #ifdef UNDER_CE
  const UInt32 kNormalizedCeSize = (16 << 20);
  if (ramSize > kNormalizedCeSize && ramSize < (33 << 20))
    ramSize = kNormalizedCeSize;
  #endif

  if (Sync.DictionarySize == (UInt32)(Int32)-1)
  {
    unsigned dicSizeLog = 25;

    #ifdef UNDER_CE
    dicSizeLog = 20;
    #endif

    if (ramSize_Defined)
    for (; dicSizeLog > kBenchMinDicLogSize; dicSizeLog--)
      if (GetBenchMemoryUsage(Sync.NumThreads, ((UInt32)1 << dicSizeLog)) + (8 << 20) <= ramSize)
        break;
    Sync.DictionarySize = (1 << dicSizeLog);
  }
  
  if (Sync.DictionarySize < kMinDicSize) Sync.DictionarySize = kMinDicSize;
  if (Sync.DictionarySize > kMaxDicSize) Sync.DictionarySize = kMaxDicSize;

  for (unsigned i = kMinDicLogSize; i <= 30; i++)
    for (unsigned j = 0; j < 2; j++)
    {
      UInt32 dict = ((UInt32)1 << i) + ((UInt32)j << (i - 1));
      if (dict > kMaxDicSize)
        continue;
      TCHAR s[32];
      ConvertUInt32ToString((dict >> 20), s);
      lstrcat(s, kMB);
      int index = (int)m_Dictionary.AddString(s);
      m_Dictionary.SetItemData(index, dict);
      if (dict <= Sync.DictionarySize)
        cur = index;
    }
  m_Dictionary.SetCurSel(cur);

  OnChangeSettings();

  Sync._startEvent.Set();
  _timer = SetTimer(kTimerID, kTimerElapse);

  if (TotalMode)
    NormalizeSize(true);
  else
    NormalizePosition();
  return CModalDialog::OnInit();
}

bool CBenchmarkDialog::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  if (!TotalMode)
    return false;
  int mx, my;
  GetMargins(8, mx, my);
  int bx1, bx2, by;
  GetItemSizes(IDCANCEL, bx1, by);
  GetItemSizes(IDHELP, bx2, by);

  {
    int y = ySize - my - by;
    int x = xSize - mx - bx1;
    
    InvalidateRect(NULL);
    
    MoveItem(IDCANCEL, x, y, bx1, by);
    MoveItem(IDHELP, x - mx - bx2, y, bx2, by);
  }

  if (_consoleEdit)
  {
    int yPos = ySize - my - by;
    RECT rect;
    GetClientRectOfItem(IDE_BENCH2_EDIT, rect);
    int y = rect.top;
    int ySize2 = yPos - my - y;
    const int kMinYSize = 20;
    int xx = xSize - mx * 2;
    if (ySize2 < kMinYSize)
    {
      ySize2 = kMinYSize;
    }
    _consoleEdit.Move(mx, y, xx, ySize2);
  }
  return false;
}

UInt32 CBenchmarkDialog::GetNumberOfThreads()
{
  return (UInt32)m_NumThreads.GetItemData_of_CurSel();
}


void CBenchmarkDialog::SetItemText_Number(int itemID, UInt64 val, LPCTSTR post)
{
  TCHAR s[64];
  ConvertUInt64ToString(val, s);
  if (post)
    lstrcat(s, post);
  SetItemText(itemID, s);
}

static void PrintSize_MB(UString &s, UInt64 size)
{
  char temp[32];
  ConvertUInt64ToString((size + (1 << 20) - 1) >> 20, temp);
  s += temp;
  s += kMB;
}

extern bool g_LargePagesMode;

UInt32 CBenchmarkDialog::OnChangeDictionary()
{
  const UInt32 dict = (UInt32)m_Dictionary.GetItemData_of_CurSel();
  const UInt64 memUsage = GetBenchMemoryUsage(GetNumberOfThreads(), dict);

  UString s;
  PrintSize_MB(s, memUsage);
  if (ramSize_Defined)
  {
    s += " / ";
    PrintSize_MB(s, ramSize);
  }
  if (g_LargePagesMode)
    s += " LP";

  SetItemText(IDT_BENCH_MEMORY_VAL, s);

  return dict;
}

static const UInt32 g_IDs[] =
{
  IDT_BENCH_COMPRESS_USAGE1,
  IDT_BENCH_COMPRESS_USAGE2,
  IDT_BENCH_COMPRESS_SPEED1,
  IDT_BENCH_COMPRESS_SPEED2,
  IDT_BENCH_COMPRESS_RATING1,
  IDT_BENCH_COMPRESS_RATING2,
  IDT_BENCH_COMPRESS_RPU1,
  IDT_BENCH_COMPRESS_RPU2,
  
  IDT_BENCH_DECOMPR_SPEED1,
  IDT_BENCH_DECOMPR_SPEED2,
  IDT_BENCH_DECOMPR_RATING1,
  IDT_BENCH_DECOMPR_RATING2,
  IDT_BENCH_DECOMPR_USAGE1,
  IDT_BENCH_DECOMPR_USAGE2,
  IDT_BENCH_DECOMPR_RPU1,
  IDT_BENCH_DECOMPR_RPU2,
  
  IDT_BENCH_TOTAL_USAGE_VAL,
  IDT_BENCH_TOTAL_RATING_VAL,
  IDT_BENCH_TOTAL_RPU_VAL

  // IDT_BENCH_FREQ_CUR,
  // IDT_BENCH_FREQ_RES
};
  
void CBenchmarkDialog::OnChangeSettings()
{
  EnableItem(IDB_STOP, true);
  UInt32 dict = OnChangeDictionary();
  
  for (int i = 0; i < ARRAY_SIZE(g_IDs); i++)
    SetItemText(g_IDs[i], kProcessingString);
  _startTime = GetTickCount();
  PrintTime();
  NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
  Sync.Init();
  Sync.DictionarySize = dict;
  Sync.Changed = true;
  Sync.NumThreads = GetNumberOfThreads();
}

void CBenchmarkDialog::OnRestartButton()
{
  OnChangeSettings();
}

void CBenchmarkDialog::OnStopButton()
{
  EnableItem(IDB_STOP, false);
  Sync.Pause();
}

void CBenchmarkDialog::OnHelp()
{
  ShowHelpWindow(kHelpTopic);
}

void CBenchmarkDialog::OnCancel()
{
  Sync.Stop();
  KillTimer(_timer);
  CModalDialog::OnCancel();
}

void GetTimeString(UInt64 timeValue, wchar_t *s);

void CBenchmarkDialog::PrintTime()
{
  UInt32 curTime = ::GetTickCount();
  UInt32 elapsedTime = (curTime - _startTime);
  UInt32 elapsedSec = elapsedTime / 1000;
  if (elapsedSec != 0 && Sync.WasPaused())
    return;
  WCHAR s[40];
  GetTimeString(elapsedSec, s);
  SetItemText(IDT_BENCH_ELAPSED_VAL, s);
}

void CBenchmarkDialog::PrintRating(UInt64 rating, UINT controlID)
{
  SetItemText_Number(controlID, rating / 1000000, kMIPS);
}

void CBenchmarkDialog::PrintUsage(UInt64 usage, UINT controlID)
{
  SetItemText_Number(controlID, (usage + 5000) / 10000, TEXT("%"));
}

void CBenchmarkDialog::PrintResults(
    UInt32 dictionarySize,
    const CBenchInfo2 &info,
    UINT usageID, UINT speedID, UINT rpuID, UINT ratingID,
    bool decompressMode)
{
  if (info.GlobalTime == 0)
    return;

  {
    const UInt64 speed = info.UnpackSize * info.NumIterations * info.GlobalFreq / info.GlobalTime;
    SetItemText_Number(speedID, speed >> 10, kKBs);
  }
  UInt64 rating;
  if (decompressMode)
    rating = info.GetDecompressRating();
  else
    rating = info.GetCompressRating(dictionarySize);

  PrintRating(rating, ratingID);
  PrintRating(info.GetRatingPerUsage(rating), rpuID);
  PrintUsage(info.GetUsage(), usageID);
}

bool CBenchmarkDialog::OnTimer(WPARAM /* timerID */, LPARAM /* callback */)
{
  bool printTime = true;
  if (TotalMode)
  {
    if (Sync.WasStopped())
      printTime = false;
  }
  if (printTime)
    PrintTime();

  if (TotalMode)
  {
    bool wasChanged = false;
    {
      NWindows::NSynchronization::CCriticalSectionLock lock(Sync.CS);
      
      if (Sync.TextWasChanged)
      {
        wasChanged = true;
        Bench2Text += Sync.Text;
        Sync.Text.Empty();
        Sync.TextWasChanged = false;
      }
    }
    if (wasChanged)
      _consoleEdit.SetText(Bench2Text);
    return true;
  }

  SetItemText_Number(IDT_BENCH_SIZE_VAL, (Sync.ProcessedSize >> 20), kMB);

  SetItemText_Number(IDT_BENCH_PASSES_VAL, Sync.NumPasses);

  /*
  if (Sync.FirstPath)
    SetItemText_Number(IDT_BENCH_FREQ_CUR, Sync.Freq, TEXT(" MHz"));
  else
    SetItemText_Number(IDT_BENCH_FREQ_RES, Sync.Freq, TEXT(" MHz"));
  */

  /*
  if (Sync.FreqWasChanged)
  {
    SetItemText(IDT_BENCH_FREQ, Sync.Freq);
    Sync.FreqWasChanged  = false;
  }
  */

  {
    UInt32 dicSizeTemp = (UInt32)MyMax(Sync.ProcessedSize, UInt64(1) << 20);
    dicSizeTemp = MyMin(dicSizeTemp, Sync.DictionarySize),
    PrintResults(dicSizeTemp,
      Sync.CompressingInfoTemp,
      IDT_BENCH_COMPRESS_USAGE1,
      IDT_BENCH_COMPRESS_SPEED1,
      IDT_BENCH_COMPRESS_RPU1,
      IDT_BENCH_COMPRESS_RATING1);
  }

  {
    PrintResults(
      Sync.DictionarySize,
      Sync.CompressingInfo,
      IDT_BENCH_COMPRESS_USAGE2,
      IDT_BENCH_COMPRESS_SPEED2,
      IDT_BENCH_COMPRESS_RPU2,
      IDT_BENCH_COMPRESS_RATING2);
  }

  {
    PrintResults(
      Sync.DictionarySize,
      Sync.DecompressingInfoTemp,
      IDT_BENCH_DECOMPR_USAGE1,
      IDT_BENCH_DECOMPR_SPEED1,
      IDT_BENCH_DECOMPR_RPU1,
      IDT_BENCH_DECOMPR_RATING1,
      true);
  }
  {
    PrintResults(
      Sync.DictionarySize,
      Sync.DecompressingInfo,
      IDT_BENCH_DECOMPR_USAGE2,
      IDT_BENCH_DECOMPR_SPEED2,
      IDT_BENCH_DECOMPR_RPU2,
      IDT_BENCH_DECOMPR_RATING2,
      true);
    if (Sync.DecompressingInfo.GlobalTime > 0 &&
        Sync.CompressingInfo.GlobalTime > 0)
    {
      UInt64 comprRating = Sync.CompressingInfo.GetCompressRating(Sync.DictionarySize);
      UInt64 decomprRating = Sync.DecompressingInfo.GetDecompressRating();
      PrintRating((comprRating + decomprRating) / 2, IDT_BENCH_TOTAL_RATING_VAL);
      PrintRating((
          Sync.CompressingInfo.GetRatingPerUsage(comprRating) +
          Sync.DecompressingInfo.GetRatingPerUsage(decomprRating)) / 2, IDT_BENCH_TOTAL_RPU_VAL);
      PrintUsage(
        (Sync.CompressingInfo.GetUsage() +
         Sync.DecompressingInfo.GetUsage()) / 2, IDT_BENCH_TOTAL_USAGE_VAL);
    }
  }
  return true;
}

bool CBenchmarkDialog::OnCommand(int code, int itemID, LPARAM lParam)
{
  if (code == CBN_SELCHANGE &&
      (itemID == IDC_BENCH_DICTIONARY ||
       itemID == IDC_BENCH_NUM_THREADS))
  {
    OnChangeSettings();
    return true;
  }
  return CModalDialog::OnCommand(code, itemID, lParam);
}

bool CBenchmarkDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_RESTART:
      OnRestartButton();
      return true;
    case IDB_STOP:
      OnStopButton();
      return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

struct CThreadBenchmark
{
  CBenchmarkDialog *BenchmarkDialog;
  DECL_EXTERNAL_CODECS_LOC_VARS2;
  // UInt32 dictionarySize;
  // UInt32 numThreads;

  HRESULT Process();
  HRESULT Result;
  static THREAD_FUNC_DECL MyThreadFunction(void *param)
  {
    ((CThreadBenchmark *)param)->Result = ((CThreadBenchmark *)param)->Process();
    return 0;
  }
};

struct CBenchCallback: public IBenchCallback
{
  UInt32 dictionarySize;
  CBenchProgressSync *Sync;
  
  // void AddCpuFreq(UInt64 cpuFreq);
  HRESULT SetFreq(bool showFreq, UInt64 cpuFreq);
  HRESULT SetEncodeResult(const CBenchInfo &info, bool final);
  HRESULT SetDecodeResult(const CBenchInfo &info, bool final);
};

/*
void CBenchCallback::AddCpuFreq(UInt64 cpuFreq)
{
  NSynchronization::CCriticalSectionLock lock(Sync->CS);
  {
    wchar_t s[32];
    ConvertUInt64ToString(cpuFreq, s);
    Sync->Freq.Add_Space_if_NotEmpty();
    Sync->Freq += s;
    Sync->FreqWasChanged = true;
  }
}
*/

HRESULT CBenchCallback::SetFreq(bool /* showFreq */, UInt64 /* cpuFreq */)
{
  return S_OK;
}

HRESULT CBenchCallback::SetEncodeResult(const CBenchInfo &info, bool final)
{
  NSynchronization::CCriticalSectionLock lock(Sync->CS);
  if (Sync->Changed || Sync->Paused || Sync->Stopped)
    return E_ABORT;
  Sync->ProcessedSize = info.UnpackSize * info.NumIterations;
  if (final && Sync->CompressingInfo.GlobalTime == 0)
  {
    (CBenchInfo&)Sync->CompressingInfo = info;
    if (Sync->CompressingInfo.GlobalTime == 0)
      Sync->CompressingInfo.GlobalTime = 1;
  }
  else
    (CBenchInfo&)Sync->CompressingInfoTemp = info;

  return S_OK;
}

HRESULT CBenchCallback::SetDecodeResult(const CBenchInfo &info, bool final)
{
  NSynchronization::CCriticalSectionLock lock(Sync->CS);
  if (Sync->Changed || Sync->Paused || Sync->Stopped)
    return E_ABORT;
  CBenchInfo info2 = info;
  if (final && Sync->DecompressingInfo.GlobalTime == 0)
  {
    (CBenchInfo&)Sync->DecompressingInfo = info2;
    if (Sync->DecompressingInfo.GlobalTime == 0)
      Sync->DecompressingInfo.GlobalTime = 1;
  }
  else
    (CBenchInfo&)Sync->DecompressingInfoTemp = info2;
  return S_OK;
}

struct CBenchCallback2: public IBenchPrintCallback
{
  CBenchProgressSync *Sync;
  bool TotalMode;

  void Print(const char *s);
  void NewLine();
  HRESULT CheckBreak();
};

void CBenchCallback2::Print(const char *s)
{
  if (TotalMode)
  {
    NSynchronization::CCriticalSectionLock lock(Sync->CS);
    Sync->Text += s;
    Sync->TextWasChanged = true;
  }
}

void CBenchCallback2::NewLine()
{
  Print("\xD\n");
}

HRESULT CBenchCallback2::CheckBreak()
{
  if (Sync->Changed || Sync->Paused || Sync->Stopped)
    return E_ABORT;
  return S_OK;
}



/*
struct CFreqCallback: public IBenchFreqCallback
{
  CBenchProgressSync *Sync;

  virtual void AddCpuFreq(UInt64 freq);
};

void CFreqCallback::AddCpuFreq(UInt64 freq)
{
  NSynchronization::CCriticalSectionLock lock(Sync->CS);
  Sync->Freq = freq;
}
*/



HRESULT CThreadBenchmark::Process()
{
  CBenchProgressSync &sync = BenchmarkDialog->Sync;
  sync.WaitCreating();
  try
  {
    for (;;)
    {
      if (sync.WasStopped())
        return 0;
      if (sync.WasPaused())
      {
        Sleep(200);
        continue;
      }
      UInt32 dictionarySize;
      UInt32 numThreads;
      {
        NSynchronization::CCriticalSectionLock lock(sync.CS);
        if (sync.Stopped || sync.Paused)
          continue;
        if (sync.Changed)
          sync.Init();
        dictionarySize = sync.DictionarySize;
        numThreads = sync.NumThreads;
        /*
        if (sync.CompressingInfo.GlobalTime != 0)
          sync.FirstPath = false;
        */
      }
      
      CBenchCallback callback;
      callback.dictionarySize = dictionarySize;
      callback.Sync = &sync;
      CBenchCallback2 callback2;
      callback2.TotalMode = BenchmarkDialog->TotalMode;
      callback2.Sync = &sync;
      // CFreqCallback freqCallback;
      // freqCallback.Sync = &sync;
      HRESULT result;
     
      try
      {
        CObjectVector<CProperty> props;
        if (BenchmarkDialog->TotalMode)
        {
          props = BenchmarkDialog->Props;
        }
        else
        {
          {
            CProperty prop;
            prop.Name = "mt";
            prop.Value.Add_UInt32(numThreads);
            props.Add(prop);
          }
          {
            CProperty prop;
            prop.Name = 'd';
            prop.Name.Add_UInt32(dictionarySize);
            prop.Name += 'b';
            props.Add(prop);
          }
        }
        
        result = Bench(EXTERNAL_CODECS_LOC_VARS
            BenchmarkDialog->TotalMode ? &callback2 : NULL,
            BenchmarkDialog->TotalMode ? NULL : &callback,
            // &freqCallback,
            props, 1, false);
        
        if (BenchmarkDialog->TotalMode)
        {
          sync.Stop();
        }
      }
      catch(...)
      {
        result = E_FAIL;
      }

      if (result != S_OK)
      {
        if (result != E_ABORT)
        {
          {
            NSynchronization::CCriticalSectionLock lock(sync.CS);
            sync.Pause();
          }
          UString message;
          if (result == S_FALSE)
            message = "Decoding error";
          else if (result == CLASS_E_CLASSNOTAVAILABLE)
            message = "Can't find 7z.dll";
          else
            message = HResultToMessage(result);
          BenchmarkDialog->MessageBoxError(message);
        }
      }
      else
      {
        NSynchronization::CCriticalSectionLock lock(sync.CS);
        sync.NumPasses++;
      }
    }
    // return S_OK;
  }
  catch(CSystemException &e)
  {
    BenchmarkDialog->MessageBoxError(HResultToMessage(e.ErrorCode));
    return E_FAIL;
  }
  catch(...)
  {
    BenchmarkDialog->MessageBoxError(HResultToMessage(E_FAIL));
    return E_FAIL;
  }
}

static void ParseNumberString(const UString &s, NCOM::CPropVariant &prop)
{
  const wchar_t *end;
  UInt64 result = ConvertStringToUInt64(s, &end);
  if (*end != 0 || s.IsEmpty())
    prop = s;
  else if (result <= (UInt32)0xFFFFFFFF)
    prop = (UInt32)result;
  else
    prop = result;
}

HRESULT Benchmark(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const CObjectVector<CProperty> &props, HWND hwndParent)
{
  CThreadBenchmark benchmarker;
  #ifdef EXTERNAL_CODECS
  benchmarker.__externalCodecs = __externalCodecs;
  #endif

  CBenchmarkDialog bd;
  bd.Props = props;
  bd.TotalMode = false;
  bd.Sync.DictionarySize = (UInt32)(Int32)-1;
  bd.Sync.NumThreads = (UInt32)(Int32)-1;

  COneMethodInfo method;

  UInt32 numCPUs = 1;
  #ifndef _7ZIP_ST
  numCPUs = NSystem::GetNumberOfProcessors();
  #endif
  UInt32 numThreads = numCPUs;

  FOR_VECTOR (i, props)
  {
    const CProperty &prop = props[i];
    UString name = prop.Name;
    name.MakeLower_Ascii();
    if (name.IsEqualTo_Ascii_NoCase("m") && prop.Value == L"*")
    {
      bd.TotalMode = true;
      continue;
    }

    NCOM::CPropVariant propVariant;
    if (!prop.Value.IsEmpty())
      ParseNumberString(prop.Value, propVariant);
    if (name.IsPrefixedBy(L"mt"))
    {
      #ifndef _7ZIP_ST
      RINOK(ParseMtProp(name.Ptr(2), propVariant, numCPUs, numThreads));
      if (numThreads != numCPUs)
        bd.Sync.NumThreads = numThreads;
      #endif
      continue;
    }
    if (name.IsEqualTo("testtime"))
    {
      // UInt32 testTime = 4;
      // RINOK(ParsePropToUInt32(L"", propVariant, testTime));
      continue;
    }
    RINOK(method.ParseMethodFromPROPVARIANT(name, propVariant));
  }

  if (bd.TotalMode)
  {
    // bd.Bench2Text.Empty();
    bd.Bench2Text = "7-Zip " MY_VERSION_CPU;
    bd.Bench2Text += (char)0xD;
    bd.Bench2Text.Add_LF();
  }

  {
    UInt32 dict;
    if (method.Get_DicSize(dict))
      bd.Sync.DictionarySize = dict;
  }

  benchmarker.BenchmarkDialog = &bd;

  NWindows::CThread thread;
  RINOK(thread.Create(CThreadBenchmark::MyThreadFunction, &benchmarker));
  bd.Create(hwndParent);
  return thread.Wait();
}
