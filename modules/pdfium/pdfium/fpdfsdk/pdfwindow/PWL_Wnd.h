// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PDFWINDOW_PWL_WND_H_
#define FPDFSDK_PDFWINDOW_PWL_WND_H_

#include <memory>
#include <vector>

#include "core/fpdfdoc/cpdf_formcontrol.h"
#include "core/fxcrt/cfx_observable.h"
#include "core/fxcrt/fx_basic.h"
#include "fpdfsdk/cfx_systemhandler.h"
#include "fpdfsdk/cpdfsdk_widget.h"
#include "fpdfsdk/pdfwindow/cpwl_color.h"

class CPWL_MsgControl;
class CPWL_ScrollBar;
class CPWL_Timer;
class CPWL_TimerHandler;
class CPWL_Wnd;
class CFX_SystemHandler;
class IPVT_FontMap;
class IPWL_Provider;

// window styles
#define PWS_CHILD 0x80000000L
#define PWS_BORDER 0x40000000L
#define PWS_BACKGROUND 0x20000000L
#define PWS_HSCROLL 0x10000000L
#define PWS_VSCROLL 0x08000000L
#define PWS_VISIBLE 0x04000000L
#define PWS_DISABLE 0x02000000L
#define PWS_READONLY 0x01000000L
#define PWS_AUTOFONTSIZE 0x00800000L
#define PWS_AUTOTRANSPARENT 0x00400000L
#define PWS_NOREFRESHCLIP 0x00200000L

// edit and label styles
#define PES_MULTILINE 0x0001L
#define PES_PASSWORD 0x0002L
#define PES_LEFT 0x0004L
#define PES_RIGHT 0x0008L
#define PES_MIDDLE 0x0010L
#define PES_TOP 0x0020L
#define PES_BOTTOM 0x0040L
#define PES_CENTER 0x0080L
#define PES_CHARARRAY 0x0100L
#define PES_AUTOSCROLL 0x0200L
#define PES_AUTORETURN 0x0400L
#define PES_UNDO 0x0800L
#define PES_RICH 0x1000L
#define PES_SPELLCHECK 0x2000L
#define PES_TEXTOVERFLOW 0x4000L
#define PES_NOREAD 0x8000L

// listbox styles
#define PLBS_MULTIPLESEL 0x0001L
#define PLBS_HOVERSEL 0x0008L

// combobox styles
#define PCBS_ALLOWCUSTOMTEXT 0x0001L

// richedit styles
#define PRES_MULTILINE 0x0001L
#define PRES_AUTORETURN 0x0002L
#define PRES_AUTOSCROLL 0x0004L
#define PRES_UNDO 0x0100L
#define PRES_MULTIPAGES 0x0200L
#define PRES_TEXTOVERFLOW 0x0400L

// notification messages
#define PNM_ADDCHILD 0x00000000L
#define PNM_REMOVECHILD 0x00000001L
#define PNM_SETSCROLLINFO 0x00000002L
#define PNM_SETSCROLLPOS 0x00000003L
#define PNM_SCROLLWINDOW 0x00000004L
#define PNM_LBUTTONDOWN 0x00000005L
#define PNM_LBUTTONUP 0x00000006L
#define PNM_MOUSEMOVE 0x00000007L
#define PNM_NOTERESET 0x00000008L
#define PNM_SETCARETINFO 0x00000009L
#define PNM_SELCHANGED 0x0000000AL
#define PNM_NOTEEDITCHANGED 0x0000000BL

#define PWL_CLASSNAME_EDIT "CPWL_Edit"

struct CPWL_Dash {
  CPWL_Dash() : nDash(0), nGap(0), nPhase(0) {}
  CPWL_Dash(int32_t dash, int32_t gap, int32_t phase)
      : nDash(dash), nGap(gap), nPhase(phase) {}

  void Reset() {
    nDash = 0;
    nGap = 0;
    nPhase = 0;
  }

  int32_t nDash;
  int32_t nGap;
  int32_t nPhase;
};

inline bool operator==(const CPWL_Color& c1, const CPWL_Color& c2) {
  return c1.nColorType == c2.nColorType && c1.fColor1 - c2.fColor1 < 0.0001 &&
         c1.fColor1 - c2.fColor1 > -0.0001 &&
         c1.fColor2 - c2.fColor2 < 0.0001 &&
         c1.fColor2 - c2.fColor2 > -0.0001 &&
         c1.fColor3 - c2.fColor3 < 0.0001 &&
         c1.fColor3 - c2.fColor3 > -0.0001 &&
         c1.fColor4 - c2.fColor4 < 0.0001 && c1.fColor4 - c2.fColor4 > -0.0001;
}

inline bool operator!=(const CPWL_Color& c1, const CPWL_Color& c2) {
  return !(c1 == c2);
}

#define PWL_SCROLLBAR_WIDTH 12.0f
#define PWL_SCROLLBAR_BUTTON_WIDTH 9.0f
#define PWL_SCROLLBAR_POSBUTTON_MINWIDTH 2.0f
#define PWL_SCROLLBAR_TRANSPARENCY 150
#define PWL_SCROLLBAR_BKCOLOR \
  CPWL_Color(COLORTYPE_RGB, 220.0f / 255.0f, 220.0f / 255.0f, 220.0f / 255.0f)
#define PWL_DEFAULT_SELTEXTCOLOR CPWL_Color(COLORTYPE_RGB, 1, 1, 1)
#define PWL_DEFAULT_SELBACKCOLOR \
  CPWL_Color(COLORTYPE_RGB, 0, 51.0f / 255.0f, 113.0f / 255.0f)
#define PWL_DEFAULT_BACKCOLOR PWL_DEFAULT_SELTEXTCOLOR
#define PWL_DEFAULT_TEXTCOLOR CPWL_Color(COLORTYPE_RGB, 0, 0, 0)
#define PWL_DEFAULT_FONTSIZE 9.0f
#define PWL_DEFAULT_BLACKCOLOR CPWL_Color(COLORTYPE_GRAY, 0)
#define PWL_DEFAULT_WHITECOLOR CPWL_Color(COLORTYPE_GRAY, 1)
#define PWL_DEFAULT_HEAVYGRAYCOLOR CPWL_Color(COLORTYPE_GRAY, 0.50)
#define PWL_DEFAULT_LIGHTGRAYCOLOR CPWL_Color(COLORTYPE_GRAY, 0.75)
#define PWL_TRIANGLE_HALFLEN 2.0f
#define PWL_CBBUTTON_TRIANGLE_HALFLEN 3.0f
#define PWL_INVALIDATE_INFLATE 2

class IPWL_Provider : public CFX_Observable<IPWL_Provider> {
 public:
  virtual ~IPWL_Provider() {}

  // get a matrix which map user space to CWnd client space
  virtual CFX_Matrix GetWindowMatrix(void* pAttachedData) = 0;

  /*
  0 L"&Undo\tCtrl+Z"
  1 L"&Redo\tCtrl+Shift+Z"
  2 L"Cu&t\tCtrl+X"
  3 L"&Copy\tCtrl+C"
  4 L"&Paste\tCtrl+V"
  5 L"&Delete"
  6  L"&Select All\tCtrl+A"
  */
  virtual CFX_WideString LoadPopupMenuString(int32_t nIndex) = 0;
};

class IPWL_FocusHandler {
 public:
  virtual ~IPWL_FocusHandler() {}
  virtual void OnSetFocus(CPWL_Wnd* pWnd) = 0;
};

struct PWL_CREATEPARAM {
 public:
  PWL_CREATEPARAM();
  PWL_CREATEPARAM(const PWL_CREATEPARAM& other);

  void Reset() {
    rcRectWnd.Reset();
    pSystemHandler = nullptr;
    pFontMap = nullptr;
    pProvider.Reset();
    pFocusHandler = nullptr;
    dwFlags = 0;
    sBackgroundColor.Reset();
    pAttachedWidget.Reset();
    nBorderStyle = BorderStyle::SOLID;
    dwBorderWidth = 0;
    sBorderColor.Reset();
    sTextColor.Reset();
    nTransparency = 0;
    fFontSize = 0.0f;
    sDash.Reset();
    pAttachedData = nullptr;
    pParentWnd = nullptr;
    pMsgControl = nullptr;
    eCursorType = 0;
    mtChild.SetIdentity();
  }

  CFX_FloatRect rcRectWnd;            // required
  CFX_SystemHandler* pSystemHandler;  // required
  IPVT_FontMap* pFontMap;             // required
  IPWL_Provider::ObservedPtr pProvider;  // required
  IPWL_FocusHandler* pFocusHandler;   // optional
  uint32_t dwFlags;                   // optional
  CPWL_Color sBackgroundColor;        // optional
  CPDFSDK_Widget::ObservedPtr pAttachedWidget;  // required
  BorderStyle nBorderStyle;           // optional
  int32_t dwBorderWidth;              // optional
  CPWL_Color sBorderColor;            // optional
  CPWL_Color sTextColor;              // optional
  int32_t nTransparency;              // optional
  FX_FLOAT fFontSize;                 // optional
  CPWL_Dash sDash;                    // optional
  void* pAttachedData;                // optional
  CPWL_Wnd* pParentWnd;               // ignore
  CPWL_MsgControl* pMsgControl;       // ignore
  int32_t eCursorType;                // ignore
  CFX_Matrix mtChild;                 // ignore
};

class CPWL_Timer {
 public:
  CPWL_Timer(CPWL_TimerHandler* pAttached, CFX_SystemHandler* pSystemHandler);
  virtual ~CPWL_Timer();

  int32_t SetPWLTimer(int32_t nElapse);
  void KillPWLTimer();
  static void TimerProc(int32_t idEvent);

 private:
  int32_t m_nTimerID;
  CPWL_TimerHandler* m_pAttached;
  CFX_SystemHandler* m_pSystemHandler;
};

class CPWL_TimerHandler {
 public:
  CPWL_TimerHandler();
  virtual ~CPWL_TimerHandler();

  void BeginTimer(int32_t nElapse);
  void EndTimer();
  virtual void TimerProc();
  virtual CFX_SystemHandler* GetSystemHandler() const = 0;

 private:
  std::unique_ptr<CPWL_Timer> m_pTimer;
};

class CPWL_Wnd : public CPWL_TimerHandler {
 public:
  CPWL_Wnd();
  ~CPWL_Wnd() override;

  virtual CFX_ByteString GetClassName() const;
  virtual void InvalidateRect(CFX_FloatRect* pRect = nullptr);

  virtual bool OnKeyDown(uint16_t nChar, uint32_t nFlag);
  virtual bool OnChar(uint16_t nChar, uint32_t nFlag);
  virtual bool OnLButtonDblClk(const CFX_PointF& point, uint32_t nFlag);
  virtual bool OnLButtonDown(const CFX_PointF& point, uint32_t nFlag);
  virtual bool OnLButtonUp(const CFX_PointF& point, uint32_t nFlag);
  virtual bool OnRButtonDown(const CFX_PointF& point, uint32_t nFlag);
  virtual bool OnRButtonUp(const CFX_PointF& point, uint32_t nFlag);
  virtual bool OnMouseMove(const CFX_PointF& point, uint32_t nFlag);
  virtual bool OnMouseWheel(short zDelta,
                            const CFX_PointF& point,
                            uint32_t nFlag);
  virtual void OnNotify(CPWL_Wnd* pWnd,
                        uint32_t msg,
                        intptr_t wParam = 0,
                        intptr_t lParam = 0);
  virtual void SetFocus();
  virtual void KillFocus();
  virtual void SetCursor();
  virtual void SetVisible(bool bVisible);
  virtual void SetFontSize(FX_FLOAT fFontSize);
  virtual FX_FLOAT GetFontSize() const;

  virtual CFX_FloatRect GetFocusRect() const;
  virtual CFX_FloatRect GetClientRect() const;

  void InvalidateFocusHandler(IPWL_FocusHandler* handler);
  void InvalidateProvider(IPWL_Provider* provider);
  void Create(const PWL_CREATEPARAM& cp);
  void Destroy();
  void Move(const CFX_FloatRect& rcNew, bool bReset, bool bRefresh);

  void SetCapture();
  void ReleaseCapture();

  void DrawAppearance(CFX_RenderDevice* pDevice, CFX_Matrix* pUser2Device);

  CPWL_Color GetBackgroundColor() const;
  void SetBackgroundColor(const CPWL_Color& color);
  CPWL_Color GetBorderColor() const;
  CPWL_Color GetTextColor() const;
  void SetTextColor(const CPWL_Color& color);
  CPWL_Color GetBorderLeftTopColor(BorderStyle nBorderStyle) const;
  CPWL_Color GetBorderRightBottomColor(BorderStyle nBorderStyle) const;

  void SetBorderStyle(BorderStyle eBorderStyle);
  BorderStyle GetBorderStyle() const;
  const CPWL_Dash& GetBorderDash() const;

  int32_t GetBorderWidth() const;
  int32_t GetInnerBorderWidth() const;
  CFX_FloatRect GetWindowRect() const;
  CFX_PointF GetCenterPoint() const;

  bool IsVisible() const { return m_bVisible; }
  bool HasFlag(uint32_t dwFlags) const;
  void AddFlag(uint32_t dwFlags);
  void RemoveFlag(uint32_t dwFlags);

  void SetClipRect(const CFX_FloatRect& rect);
  const CFX_FloatRect& GetClipRect() const;

  CPWL_Wnd* GetParentWindow() const;
  void* GetAttachedData() const;

  bool WndHitTest(const CFX_PointF& point) const;
  bool ClientHitTest(const CFX_PointF& point) const;
  bool IsCaptureMouse() const;

  void EnableWindow(bool bEnable);
  bool IsEnabled() const { return m_bEnabled; }
  const CPWL_Wnd* GetFocused() const;
  bool IsFocused() const;
  bool IsReadOnly() const;
  CPWL_ScrollBar* GetVScrollBar() const;

  IPVT_FontMap* GetFontMap() const;
  IPWL_Provider* GetProvider() const;
  IPWL_FocusHandler* GetFocusHandler() const;

  int32_t GetTransparency();
  void SetTransparency(int32_t nTransparency);

  CFX_Matrix GetChildToRoot() const;
  CFX_Matrix GetChildMatrix() const;
  void SetChildMatrix(const CFX_Matrix& mt);
  CFX_Matrix GetWindowMatrix() const;

 protected:
  friend class CPWL_MsgControl;

  // CPWL_TimerHandler
  CFX_SystemHandler* GetSystemHandler() const override;

  virtual void CreateChildWnd(const PWL_CREATEPARAM& cp);
  virtual void RePosChildWnd();
  virtual void GetThisAppearanceStream(CFX_ByteTextBuf& sAppStream);

  virtual void DrawThisAppearance(CFX_RenderDevice* pDevice,
                                  CFX_Matrix* pUser2Device);

  virtual void OnCreate(PWL_CREATEPARAM& cp);
  virtual void OnCreated();
  virtual void OnDestroy();

  virtual void OnSetFocus();
  virtual void OnKillFocus();

  void GetAppearanceStream(CFX_ByteTextBuf& sAppStream);
  void SetNotifyFlag(bool bNotifying = true) { m_bNotifying = bNotifying; }

  bool IsValid() const;
  const PWL_CREATEPARAM& GetCreationParam() const;
  bool IsNotifying() const { return m_bNotifying; }

  void InvalidateRectMove(const CFX_FloatRect& rcOld,
                          const CFX_FloatRect& rcNew);

  bool IsWndCaptureMouse(const CPWL_Wnd* pWnd) const;
  bool IsWndCaptureKeyboard(const CPWL_Wnd* pWnd) const;
  const CPWL_Wnd* GetRootWnd() const;

  bool IsCTRLpressed(uint32_t nFlag) const;
  bool IsSHIFTpressed(uint32_t nFlag) const;
  bool IsALTpressed(uint32_t nFlag) const;

 private:
  CFX_PointF ParentToChild(const CFX_PointF& point) const;
  CFX_FloatRect ParentToChild(const CFX_FloatRect& rect) const;

  void GetChildAppearanceStream(CFX_ByteTextBuf& sAppStream);
  void DrawChildAppearance(CFX_RenderDevice* pDevice, CFX_Matrix* pUser2Device);

  FX_RECT PWLtoWnd(const CFX_FloatRect& rect) const;

  void AddChild(CPWL_Wnd* pWnd);
  void RemoveChild(CPWL_Wnd* pWnd);

  void CreateScrollBar(const PWL_CREATEPARAM& cp);
  void CreateVScrollBar(const PWL_CREATEPARAM& cp);

  void AdjustStyle();
  void CreateMsgControl();
  void DestroyMsgControl();

  CPWL_MsgControl* GetMsgControl() const;

  std::vector<CPWL_Wnd*> m_Children;
  PWL_CREATEPARAM m_sPrivateParam;
  CPWL_ScrollBar* m_pVScrollBar;
  CFX_FloatRect m_rcWindow;
  CFX_FloatRect m_rcClip;
  bool m_bCreated;
  bool m_bVisible;
  bool m_bNotifying;
  bool m_bEnabled;
};

#endif  // FPDFSDK_PDFWINDOW_PWL_WND_H_
