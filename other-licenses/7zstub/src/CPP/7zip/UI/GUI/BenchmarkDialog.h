// BenchmarkDialog.h

#ifndef __BENCHMARK_DIALOG_H
#define __BENCHMARK_DIALOG_H

#include "../../../Windows/Synchronization.h"

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Edit.h"

#include "../Common/Bench.h"

#include "../FileManager/DialogSize.h"

#include "BenchmarkDialogRes.h"

struct CBenchInfo2 : public CBenchInfo
{
  void Init()  { GlobalTime = UserTime = 0; }

  UInt64 GetCompressRating(UInt32 dictSize) const
  {
    return ::GetCompressRating(dictSize, GlobalTime, GlobalFreq, UnpackSize * NumIterations);
  }

  UInt64 GetDecompressRating() const
  {
    return ::GetDecompressRating(GlobalTime, GlobalFreq, UnpackSize, PackSize, NumIterations);
  }
};

class CBenchProgressSync
{
public:
  bool Stopped;
  bool Paused;
  bool Changed;
  UInt32 DictionarySize;
  UInt32 NumThreads;
  UInt64 NumPasses;
  NWindows::NSynchronization::CManualResetEvent _startEvent;
  NWindows::NSynchronization::CCriticalSection CS;

  CBenchInfo2 CompressingInfoTemp;
  CBenchInfo2 CompressingInfo;
  UInt64 ProcessedSize;

  CBenchInfo2 DecompressingInfoTemp;
  CBenchInfo2 DecompressingInfo;

  AString Text;
  bool TextWasChanged;

  // bool FirstPath;
  // UInt64 Freq;
  // UString Freq;
  // bool FreqWasChanged;

  CBenchProgressSync()
  {
    if (_startEvent.Create() != S_OK)
      throw 3986437;
  }
  
  void Init()
  {
    Changed = false;
    Stopped = false;
    Paused = false;
    CompressingInfoTemp.Init();
    CompressingInfo.Init();
    ProcessedSize = 0;
    
    DecompressingInfoTemp.Init();
    DecompressingInfo.Init();

    NumPasses = 0;

    // FirstPath = true;
    // Freq = 0;
    // Freq.SetFromAscii("MHz: ");
    // FreqWasChanged = true;

    Text.Empty();
    TextWasChanged = true;
  }
  
  void Stop()
  {
    NWindows::NSynchronization::CCriticalSectionLock lock(CS);
    Stopped = true;
  }
  bool WasStopped()
  {
    NWindows::NSynchronization::CCriticalSectionLock lock(CS);
    return Stopped;
  }
  void Pause()
  {
    NWindows::NSynchronization::CCriticalSectionLock lock(CS);
    Paused = true;
  }
  void Start()
  {
    NWindows::NSynchronization::CCriticalSectionLock lock(CS);
    Paused = false;
  }
  bool WasPaused()
  {
    NWindows::NSynchronization::CCriticalSectionLock lock(CS);
    return Paused;
  }
  void WaitCreating() { _startEvent.Lock(); }
};

struct CMyFont
{
  HFONT _font;
  CMyFont(): _font(NULL) {}
  ~CMyFont()
  {
    if (_font)
      DeleteObject(_font);
  }
  void Create(const LOGFONT *lplf)
  {
    _font = CreateFontIndirect(lplf);
  }
};


class CBenchmarkDialog:
  public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox m_Dictionary;
  NWindows::NControl::CComboBox m_NumThreads;
  NWindows::NControl::CEdit _consoleEdit;
  UINT_PTR _timer;
  UInt32 _startTime;
  CMyFont _font;

  UInt64 ramSize;
  bool ramSize_Defined;

  bool OnSize(WPARAM /* wParam */, int xSize, int ySize);
  bool OnTimer(WPARAM timerID, LPARAM callback);
  virtual bool OnInit();
  void OnRestartButton();
  void OnStopButton();
  void OnHelp();
  virtual void OnCancel();
  bool OnButtonClicked(int buttonID, HWND buttonHWND);
  bool OnCommand(int code, int itemID, LPARAM lParam);

  void PrintTime();
  void PrintRating(UInt64 rating, UINT controlID);
  void PrintUsage(UInt64 usage, UINT controlID);
  void PrintResults(
      UInt32 dictionarySize,
      const CBenchInfo2 &info, UINT usageID, UINT speedID, UINT rpuID, UINT ratingID,
      bool decompressMode = false);

  UInt32 GetNumberOfThreads();
  UInt32 OnChangeDictionary();
  void OnChangeSettings();

  void SetItemText_Number(int itemID, UInt64 val, LPCTSTR post = NULL);

public:
  CBenchProgressSync Sync;
  bool TotalMode;
  CObjectVector<CProperty> Props;

  CSysString Bench2Text;

  CBenchmarkDialog(): _timer(0), TotalMode(false) {}
  INT_PTR Create(HWND wndParent = 0)
  {
    BIG_DIALOG_SIZE(332, 228);
    return CModalDialog::Create(TotalMode ? IDD_BENCH_TOTAL : SIZED_DIALOG(IDD_BENCH), wndParent);
  }
  void MessageBoxError(LPCWSTR message)
  {
    MessageBoxW(*this, message, L"7-Zip", MB_ICONERROR);
  }
};

HRESULT Benchmark(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const CObjectVector<CProperty> &props, HWND hwndParent = NULL);

#endif
