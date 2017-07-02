// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <map>
#include <vector>

#include "fpdfsdk/pdfwindow/PWL_ScrollBar.h"
#include "fpdfsdk/pdfwindow/PWL_Utils.h"
#include "fpdfsdk/pdfwindow/PWL_Wnd.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

static std::map<int32_t, CPWL_Timer*>& GetPWLTimeMap() {
  // Leak the object at shutdown.
  static auto timeMap = new std::map<int32_t, CPWL_Timer*>;
  return *timeMap;
}

PWL_CREATEPARAM::PWL_CREATEPARAM()
    : rcRectWnd(0, 0, 0, 0),
      pSystemHandler(nullptr),
      pFontMap(nullptr),
      pProvider(nullptr),
      pFocusHandler(nullptr),
      dwFlags(0),
      sBackgroundColor(),
      pAttachedWidget(nullptr),
      nBorderStyle(BorderStyle::SOLID),
      dwBorderWidth(1),
      sBorderColor(),
      sTextColor(),
      nTransparency(255),
      fFontSize(PWL_DEFAULT_FONTSIZE),
      sDash(3, 0, 0),
      pAttachedData(nullptr),
      pParentWnd(nullptr),
      pMsgControl(nullptr),
      eCursorType(FXCT_ARROW),
      mtChild(1, 0, 0, 1, 0, 0) {}

PWL_CREATEPARAM::PWL_CREATEPARAM(const PWL_CREATEPARAM& other) = default;

CPWL_Timer::CPWL_Timer(CPWL_TimerHandler* pAttached,
                       CFX_SystemHandler* pSystemHandler)
    : m_nTimerID(0), m_pAttached(pAttached), m_pSystemHandler(pSystemHandler) {
  ASSERT(m_pAttached);
  ASSERT(m_pSystemHandler);
}

CPWL_Timer::~CPWL_Timer() {
  KillPWLTimer();
}

int32_t CPWL_Timer::SetPWLTimer(int32_t nElapse) {
  if (m_nTimerID != 0)
    KillPWLTimer();
  m_nTimerID = m_pSystemHandler->SetTimer(nElapse, TimerProc);

  GetPWLTimeMap()[m_nTimerID] = this;
  return m_nTimerID;
}

void CPWL_Timer::KillPWLTimer() {
  if (m_nTimerID == 0)
    return;

  m_pSystemHandler->KillTimer(m_nTimerID);
  GetPWLTimeMap().erase(m_nTimerID);
  m_nTimerID = 0;
}

void CPWL_Timer::TimerProc(int32_t idEvent) {
  auto it = GetPWLTimeMap().find(idEvent);
  if (it == GetPWLTimeMap().end())
    return;

  CPWL_Timer* pTimer = it->second;
  if (pTimer->m_pAttached)
    pTimer->m_pAttached->TimerProc();
}

CPWL_TimerHandler::CPWL_TimerHandler() {}

CPWL_TimerHandler::~CPWL_TimerHandler() {}

void CPWL_TimerHandler::BeginTimer(int32_t nElapse) {
  if (!m_pTimer)
    m_pTimer = pdfium::MakeUnique<CPWL_Timer>(this, GetSystemHandler());

  m_pTimer->SetPWLTimer(nElapse);
}

void CPWL_TimerHandler::EndTimer() {
  if (m_pTimer)
    m_pTimer->KillPWLTimer();
}

void CPWL_TimerHandler::TimerProc() {}

class CPWL_MsgControl {
  friend class CPWL_Wnd;

 public:
  explicit CPWL_MsgControl(CPWL_Wnd* pWnd) {
    m_pCreatedWnd = pWnd;
    Default();
  }

  ~CPWL_MsgControl() { Default(); }

  void Default() {
    m_aMousePath.clear();
    m_aKeyboardPath.clear();
    m_pMainMouseWnd = nullptr;
    m_pMainKeyboardWnd = nullptr;
  }

  bool IsWndCreated(const CPWL_Wnd* pWnd) const {
    return m_pCreatedWnd == pWnd;
  }

  bool IsMainCaptureMouse(const CPWL_Wnd* pWnd) const {
    return pWnd == m_pMainMouseWnd;
  }

  bool IsWndCaptureMouse(const CPWL_Wnd* pWnd) const {
    return pWnd && pdfium::ContainsValue(m_aMousePath, pWnd);
  }

  bool IsMainCaptureKeyboard(const CPWL_Wnd* pWnd) const {
    return pWnd == m_pMainKeyboardWnd;
  }

  bool IsWndCaptureKeyboard(const CPWL_Wnd* pWnd) const {
    return pWnd && pdfium::ContainsValue(m_aKeyboardPath, pWnd);
  }

  void SetFocus(CPWL_Wnd* pWnd) {
    m_aKeyboardPath.clear();
    if (pWnd) {
      m_pMainKeyboardWnd = pWnd;
      CPWL_Wnd* pParent = pWnd;
      while (pParent) {
        m_aKeyboardPath.push_back(pParent);
        pParent = pParent->GetParentWindow();
      }
      pWnd->OnSetFocus();
    }
  }

  void KillFocus() {
    if (!m_aKeyboardPath.empty())
      if (CPWL_Wnd* pWnd = m_aKeyboardPath[0])
        pWnd->OnKillFocus();

    m_pMainKeyboardWnd = nullptr;
    m_aKeyboardPath.clear();
  }

  void SetCapture(CPWL_Wnd* pWnd) {
    m_aMousePath.clear();
    if (pWnd) {
      m_pMainMouseWnd = pWnd;
      CPWL_Wnd* pParent = pWnd;
      while (pParent) {
        m_aMousePath.push_back(pParent);
        pParent = pParent->GetParentWindow();
      }
    }
  }

  void ReleaseCapture() {
    m_pMainMouseWnd = nullptr;
    m_aMousePath.clear();
  }

 private:
  std::vector<CPWL_Wnd*> m_aMousePath;
  std::vector<CPWL_Wnd*> m_aKeyboardPath;
  CPWL_Wnd* m_pCreatedWnd;
  CPWL_Wnd* m_pMainMouseWnd;
  CPWL_Wnd* m_pMainKeyboardWnd;
};

CPWL_Wnd::CPWL_Wnd()
    : m_pVScrollBar(nullptr),
      m_rcWindow(),
      m_rcClip(),
      m_bCreated(false),
      m_bVisible(false),
      m_bNotifying(false),
      m_bEnabled(true) {}

CPWL_Wnd::~CPWL_Wnd() {
  ASSERT(m_bCreated == false);
}

CFX_ByteString CPWL_Wnd::GetClassName() const {
  return "CPWL_Wnd";
}

void CPWL_Wnd::Create(const PWL_CREATEPARAM& cp) {
  if (!IsValid()) {
    m_sPrivateParam = cp;

    OnCreate(m_sPrivateParam);

    m_sPrivateParam.rcRectWnd.Normalize();
    m_rcWindow = m_sPrivateParam.rcRectWnd;
    m_rcClip = CPWL_Utils::InflateRect(m_rcWindow, 1.0f);

    CreateMsgControl();

    if (m_sPrivateParam.pParentWnd)
      m_sPrivateParam.pParentWnd->OnNotify(this, PNM_ADDCHILD);

    PWL_CREATEPARAM ccp = m_sPrivateParam;

    ccp.dwFlags &= 0xFFFF0000L;  // remove sub styles
    ccp.mtChild = CFX_Matrix(1, 0, 0, 1, 0, 0);

    CreateScrollBar(ccp);
    CreateChildWnd(ccp);

    m_bVisible = HasFlag(PWS_VISIBLE);

    OnCreated();

    RePosChildWnd();
    m_bCreated = true;
  }
}

void CPWL_Wnd::OnCreate(PWL_CREATEPARAM& cp) {}

void CPWL_Wnd::OnCreated() {}

void CPWL_Wnd::OnDestroy() {}

void CPWL_Wnd::InvalidateFocusHandler(IPWL_FocusHandler* handler) {
  if (m_sPrivateParam.pFocusHandler == handler)
    m_sPrivateParam.pFocusHandler = nullptr;
}

void CPWL_Wnd::InvalidateProvider(IPWL_Provider* provider) {
  if (m_sPrivateParam.pProvider.Get() == provider)
    m_sPrivateParam.pProvider.Reset();
}

void CPWL_Wnd::Destroy() {
  KillFocus();
  OnDestroy();
  if (m_bCreated) {
    for (auto it = m_Children.rbegin(); it != m_Children.rend(); ++it) {
      if (CPWL_Wnd* pChild = *it) {
        *it = nullptr;
        pChild->Destroy();
        delete pChild;
      }
    }
    if (m_sPrivateParam.pParentWnd)
      m_sPrivateParam.pParentWnd->OnNotify(this, PNM_REMOVECHILD);

    m_bCreated = false;
  }
  DestroyMsgControl();
  m_sPrivateParam.Reset();
  m_Children.clear();
  m_pVScrollBar = nullptr;
}

void CPWL_Wnd::Move(const CFX_FloatRect& rcNew, bool bReset, bool bRefresh) {
  if (IsValid()) {
    CFX_FloatRect rcOld = GetWindowRect();

    m_rcWindow = rcNew;
    m_rcWindow.Normalize();

    if (rcOld.left != rcNew.left || rcOld.right != rcNew.right ||
        rcOld.top != rcNew.top || rcOld.bottom != rcNew.bottom) {
      if (bReset) {
        RePosChildWnd();
      }
    }
    if (bRefresh) {
      InvalidateRectMove(rcOld, rcNew);
    }

    m_sPrivateParam.rcRectWnd = m_rcWindow;
  }
}

void CPWL_Wnd::InvalidateRectMove(const CFX_FloatRect& rcOld,
                                  const CFX_FloatRect& rcNew) {
  CFX_FloatRect rcUnion = rcOld;
  rcUnion.Union(rcNew);

  InvalidateRect(&rcUnion);
}

void CPWL_Wnd::GetAppearanceStream(CFX_ByteTextBuf& sAppStream) {
  if (IsValid() && IsVisible()) {
    GetThisAppearanceStream(sAppStream);
    GetChildAppearanceStream(sAppStream);
  }
}

// if don't set,Get default apperance stream
void CPWL_Wnd::GetThisAppearanceStream(CFX_ByteTextBuf& sAppStream) {
  CFX_FloatRect rectWnd = GetWindowRect();
  if (!rectWnd.IsEmpty()) {
    CFX_ByteTextBuf sThis;

    if (HasFlag(PWS_BACKGROUND))
      sThis << CPWL_Utils::GetRectFillAppStream(rectWnd, GetBackgroundColor());

    if (HasFlag(PWS_BORDER)) {
      sThis << CPWL_Utils::GetBorderAppStream(
          rectWnd, (FX_FLOAT)GetBorderWidth(), GetBorderColor(),
          GetBorderLeftTopColor(GetBorderStyle()),
          GetBorderRightBottomColor(GetBorderStyle()), GetBorderStyle(),
          GetBorderDash());
    }

    sAppStream << sThis;
  }
}

void CPWL_Wnd::GetChildAppearanceStream(CFX_ByteTextBuf& sAppStream) {
  for (CPWL_Wnd* pChild : m_Children) {
    if (pChild)
      pChild->GetAppearanceStream(sAppStream);
  }
}

void CPWL_Wnd::DrawAppearance(CFX_RenderDevice* pDevice,
                              CFX_Matrix* pUser2Device) {
  if (IsValid() && IsVisible()) {
    DrawThisAppearance(pDevice, pUser2Device);
    DrawChildAppearance(pDevice, pUser2Device);
  }
}

void CPWL_Wnd::DrawThisAppearance(CFX_RenderDevice* pDevice,
                                  CFX_Matrix* pUser2Device) {
  CFX_FloatRect rectWnd = GetWindowRect();
  if (!rectWnd.IsEmpty()) {
    if (HasFlag(PWS_BACKGROUND)) {
      CFX_FloatRect rcClient = CPWL_Utils::DeflateRect(
          rectWnd, (FX_FLOAT)(GetBorderWidth() + GetInnerBorderWidth()));
      CPWL_Utils::DrawFillRect(pDevice, pUser2Device, rcClient,
                               GetBackgroundColor(), GetTransparency());
    }

    if (HasFlag(PWS_BORDER))
      CPWL_Utils::DrawBorder(pDevice, pUser2Device, rectWnd,
                             (FX_FLOAT)GetBorderWidth(), GetBorderColor(),
                             GetBorderLeftTopColor(GetBorderStyle()),
                             GetBorderRightBottomColor(GetBorderStyle()),
                             GetBorderStyle(), GetTransparency());
  }
}

void CPWL_Wnd::DrawChildAppearance(CFX_RenderDevice* pDevice,
                                   CFX_Matrix* pUser2Device) {
  for (CPWL_Wnd* pChild : m_Children) {
    if (!pChild)
      continue;

    CFX_Matrix mt = pChild->GetChildMatrix();
    if (mt.IsIdentity()) {
      pChild->DrawAppearance(pDevice, pUser2Device);
    } else {
      mt.Concat(*pUser2Device);
      pChild->DrawAppearance(pDevice, &mt);
    }
  }
}

void CPWL_Wnd::InvalidateRect(CFX_FloatRect* pRect) {
  if (IsValid()) {
    CFX_FloatRect rcRefresh = pRect ? *pRect : GetWindowRect();

    if (!HasFlag(PWS_NOREFRESHCLIP)) {
      CFX_FloatRect rcClip = GetClipRect();
      if (!rcClip.IsEmpty()) {
        rcRefresh.Intersect(rcClip);
      }
    }

    FX_RECT rcWin = PWLtoWnd(rcRefresh);
    rcWin.left -= PWL_INVALIDATE_INFLATE;
    rcWin.top -= PWL_INVALIDATE_INFLATE;
    rcWin.right += PWL_INVALIDATE_INFLATE;
    rcWin.bottom += PWL_INVALIDATE_INFLATE;

    if (CFX_SystemHandler* pSH = GetSystemHandler()) {
      if (CPDFSDK_Widget* widget = static_cast<CPDFSDK_Widget*>(
              m_sPrivateParam.pAttachedWidget.Get())) {
        pSH->InvalidateRect(widget, rcWin);
      }
    }
  }
}

#define PWL_IMPLEMENT_KEY_METHOD(key_method_name)                  \
  bool CPWL_Wnd::key_method_name(uint16_t nChar, uint32_t nFlag) { \
    if (!IsValid() || !IsVisible() || !IsEnabled())                \
      return false;                                                \
    if (!IsWndCaptureKeyboard(this))                               \
      return false;                                                \
    for (const auto& pChild : m_Children) {                        \
      if (pChild && IsWndCaptureKeyboard(pChild))                  \
        return pChild->key_method_name(nChar, nFlag);              \
    }                                                              \
    return false;                                                  \
  }

PWL_IMPLEMENT_KEY_METHOD(OnKeyDown)
PWL_IMPLEMENT_KEY_METHOD(OnChar)
#undef PWL_IMPLEMENT_KEY_METHOD

#define PWL_IMPLEMENT_MOUSE_METHOD(mouse_method_name)                          \
  bool CPWL_Wnd::mouse_method_name(const CFX_PointF& point, uint32_t nFlag) {  \
    if (!IsValid() || !IsVisible() || !IsEnabled())                            \
      return false;                                                            \
    if (IsWndCaptureMouse(this)) {                                             \
      for (const auto& pChild : m_Children) {                                  \
        if (pChild && IsWndCaptureMouse(pChild)) {                             \
          return pChild->mouse_method_name(pChild->ParentToChild(point),       \
                                           nFlag);                             \
        }                                                                      \
      }                                                                        \
      SetCursor();                                                             \
      return false;                                                            \
    }                                                                          \
    for (const auto& pChild : m_Children) {                                    \
      if (pChild && pChild->WndHitTest(pChild->ParentToChild(point))) {        \
        return pChild->mouse_method_name(pChild->ParentToChild(point), nFlag); \
      }                                                                        \
    }                                                                          \
    if (WndHitTest(point))                                                     \
      SetCursor();                                                             \
    return false;                                                              \
  }

PWL_IMPLEMENT_MOUSE_METHOD(OnLButtonDblClk)
PWL_IMPLEMENT_MOUSE_METHOD(OnLButtonDown)
PWL_IMPLEMENT_MOUSE_METHOD(OnLButtonUp)
PWL_IMPLEMENT_MOUSE_METHOD(OnRButtonDown)
PWL_IMPLEMENT_MOUSE_METHOD(OnRButtonUp)
PWL_IMPLEMENT_MOUSE_METHOD(OnMouseMove)
#undef PWL_IMPLEMENT_MOUSE_METHOD

bool CPWL_Wnd::OnMouseWheel(short zDelta,
                            const CFX_PointF& point,
                            uint32_t nFlag) {
  if (!IsValid() || !IsVisible() || !IsEnabled())
    return false;

  SetCursor();
  if (!IsWndCaptureKeyboard(this))
    return false;

  for (const auto& pChild : m_Children) {
    if (pChild && IsWndCaptureKeyboard(pChild))
      return pChild->OnMouseWheel(zDelta, pChild->ParentToChild(point), nFlag);
  }
  return false;
}

void CPWL_Wnd::AddChild(CPWL_Wnd* pWnd) {
  m_Children.push_back(pWnd);
}

void CPWL_Wnd::RemoveChild(CPWL_Wnd* pWnd) {
  for (auto it = m_Children.rbegin(); it != m_Children.rend(); ++it) {
    if (*it && *it == pWnd) {
      m_Children.erase(std::next(it).base());
      break;
    }
  }
}

void CPWL_Wnd::OnNotify(CPWL_Wnd* pWnd,
                        uint32_t msg,
                        intptr_t wParam,
                        intptr_t lParam) {
  switch (msg) {
    case PNM_ADDCHILD:
      AddChild(pWnd);
      break;
    case PNM_REMOVECHILD:
      RemoveChild(pWnd);
      break;
    default:
      break;
  }
}

bool CPWL_Wnd::IsValid() const {
  return m_bCreated;
}

const PWL_CREATEPARAM& CPWL_Wnd::GetCreationParam() const {
  return m_sPrivateParam;
}

CPWL_Wnd* CPWL_Wnd::GetParentWindow() const {
  return m_sPrivateParam.pParentWnd;
}

CFX_FloatRect CPWL_Wnd::GetWindowRect() const {
  return m_rcWindow;
}

CFX_FloatRect CPWL_Wnd::GetClientRect() const {
  CFX_FloatRect rcWindow = GetWindowRect();
  CFX_FloatRect rcClient = CPWL_Utils::DeflateRect(
      rcWindow, (FX_FLOAT)(GetBorderWidth() + GetInnerBorderWidth()));
  if (CPWL_ScrollBar* pVSB = GetVScrollBar())
    rcClient.right -= pVSB->GetScrollBarWidth();

  rcClient.Normalize();
  return rcWindow.Contains(rcClient) ? rcClient : CFX_FloatRect();
}

CFX_PointF CPWL_Wnd::GetCenterPoint() const {
  CFX_FloatRect rcClient = GetClientRect();
  return CFX_PointF((rcClient.left + rcClient.right) * 0.5f,
                    (rcClient.top + rcClient.bottom) * 0.5f);
}

bool CPWL_Wnd::HasFlag(uint32_t dwFlags) const {
  return (m_sPrivateParam.dwFlags & dwFlags) != 0;
}

void CPWL_Wnd::RemoveFlag(uint32_t dwFlags) {
  m_sPrivateParam.dwFlags &= ~dwFlags;
}

void CPWL_Wnd::AddFlag(uint32_t dwFlags) {
  m_sPrivateParam.dwFlags |= dwFlags;
}

CPWL_Color CPWL_Wnd::GetBackgroundColor() const {
  return m_sPrivateParam.sBackgroundColor;
}

void CPWL_Wnd::SetBackgroundColor(const CPWL_Color& color) {
  m_sPrivateParam.sBackgroundColor = color;
}

CPWL_Color CPWL_Wnd::GetTextColor() const {
  return m_sPrivateParam.sTextColor;
}

BorderStyle CPWL_Wnd::GetBorderStyle() const {
  return m_sPrivateParam.nBorderStyle;
}

void CPWL_Wnd::SetBorderStyle(BorderStyle nBorderStyle) {
  if (HasFlag(PWS_BORDER))
    m_sPrivateParam.nBorderStyle = nBorderStyle;
}

int32_t CPWL_Wnd::GetBorderWidth() const {
  if (HasFlag(PWS_BORDER))
    return m_sPrivateParam.dwBorderWidth;

  return 0;
}

int32_t CPWL_Wnd::GetInnerBorderWidth() const {
  return 0;
}

CPWL_Color CPWL_Wnd::GetBorderColor() const {
  if (HasFlag(PWS_BORDER))
    return m_sPrivateParam.sBorderColor;

  return CPWL_Color();
}

const CPWL_Dash& CPWL_Wnd::GetBorderDash() const {
  return m_sPrivateParam.sDash;
}

void* CPWL_Wnd::GetAttachedData() const {
  return m_sPrivateParam.pAttachedData;
}

CPWL_ScrollBar* CPWL_Wnd::GetVScrollBar() const {
  if (HasFlag(PWS_VSCROLL))
    return m_pVScrollBar;

  return nullptr;
}

void CPWL_Wnd::CreateScrollBar(const PWL_CREATEPARAM& cp) {
  CreateVScrollBar(cp);
}

void CPWL_Wnd::CreateVScrollBar(const PWL_CREATEPARAM& cp) {
  if (!m_pVScrollBar && HasFlag(PWS_VSCROLL)) {
    PWL_CREATEPARAM scp = cp;

    // flags
    scp.dwFlags =
        PWS_CHILD | PWS_BACKGROUND | PWS_AUTOTRANSPARENT | PWS_NOREFRESHCLIP;

    scp.pParentWnd = this;
    scp.sBackgroundColor = PWL_DEFAULT_WHITECOLOR;
    scp.eCursorType = FXCT_ARROW;
    scp.nTransparency = PWL_SCROLLBAR_TRANSPARENCY;

    m_pVScrollBar = new CPWL_ScrollBar(SBT_VSCROLL);
    m_pVScrollBar->Create(scp);
  }
}

void CPWL_Wnd::SetCapture() {
  if (CPWL_MsgControl* pMsgCtrl = GetMsgControl())
    pMsgCtrl->SetCapture(this);
}

void CPWL_Wnd::ReleaseCapture() {
  for (const auto& pChild : m_Children) {
    if (pChild)
      pChild->ReleaseCapture();
  }
  if (CPWL_MsgControl* pMsgCtrl = GetMsgControl())
    pMsgCtrl->ReleaseCapture();
}

void CPWL_Wnd::SetFocus() {
  if (CPWL_MsgControl* pMsgCtrl = GetMsgControl()) {
    if (!pMsgCtrl->IsMainCaptureKeyboard(this))
      pMsgCtrl->KillFocus();
    pMsgCtrl->SetFocus(this);
  }
}

void CPWL_Wnd::KillFocus() {
  if (CPWL_MsgControl* pMsgCtrl = GetMsgControl()) {
    if (pMsgCtrl->IsWndCaptureKeyboard(this))
      pMsgCtrl->KillFocus();
  }
}

void CPWL_Wnd::OnSetFocus() {}

void CPWL_Wnd::OnKillFocus() {}

bool CPWL_Wnd::WndHitTest(const CFX_PointF& point) const {
  return IsValid() && IsVisible() && GetWindowRect().Contains(point);
}

bool CPWL_Wnd::ClientHitTest(const CFX_PointF& point) const {
  return IsValid() && IsVisible() && GetClientRect().Contains(point);
}

const CPWL_Wnd* CPWL_Wnd::GetRootWnd() const {
  if (m_sPrivateParam.pParentWnd)
    return m_sPrivateParam.pParentWnd->GetRootWnd();

  return this;
}

void CPWL_Wnd::SetVisible(bool bVisible) {
  if (!IsValid())
    return;

  for (const auto& pChild : m_Children) {
    if (pChild)
      pChild->SetVisible(bVisible);
  }
  if (bVisible != m_bVisible) {
    m_bVisible = bVisible;
    RePosChildWnd();
    InvalidateRect();
  }
}

void CPWL_Wnd::SetClipRect(const CFX_FloatRect& rect) {
  m_rcClip = rect;
  m_rcClip.Normalize();
}

const CFX_FloatRect& CPWL_Wnd::GetClipRect() const {
  return m_rcClip;
}

bool CPWL_Wnd::IsReadOnly() const {
  return HasFlag(PWS_READONLY);
}

void CPWL_Wnd::RePosChildWnd() {
  CFX_FloatRect rcContent = CPWL_Utils::DeflateRect(
      GetWindowRect(), (FX_FLOAT)(GetBorderWidth() + GetInnerBorderWidth()));

  CPWL_ScrollBar* pVSB = GetVScrollBar();

  CFX_FloatRect rcVScroll =
      CFX_FloatRect(rcContent.right - PWL_SCROLLBAR_WIDTH, rcContent.bottom,
                    rcContent.right - 1.0f, rcContent.top);

  if (pVSB)
    pVSB->Move(rcVScroll, true, false);
}

void CPWL_Wnd::CreateChildWnd(const PWL_CREATEPARAM& cp) {}

void CPWL_Wnd::SetCursor() {
  if (IsValid()) {
    if (CFX_SystemHandler* pSH = GetSystemHandler()) {
      int32_t nCursorType = GetCreationParam().eCursorType;
      pSH->SetCursor(nCursorType);
    }
  }
}

void CPWL_Wnd::CreateMsgControl() {
  if (!m_sPrivateParam.pMsgControl)
    m_sPrivateParam.pMsgControl = new CPWL_MsgControl(this);
}

void CPWL_Wnd::DestroyMsgControl() {
  if (CPWL_MsgControl* pMsgControl = GetMsgControl())
    if (pMsgControl->IsWndCreated(this))
      delete pMsgControl;
}

CPWL_MsgControl* CPWL_Wnd::GetMsgControl() const {
  return m_sPrivateParam.pMsgControl;
}

bool CPWL_Wnd::IsCaptureMouse() const {
  return IsWndCaptureMouse(this);
}

bool CPWL_Wnd::IsWndCaptureMouse(const CPWL_Wnd* pWnd) const {
  if (CPWL_MsgControl* pCtrl = GetMsgControl())
    return pCtrl->IsWndCaptureMouse(pWnd);

  return false;
}

bool CPWL_Wnd::IsWndCaptureKeyboard(const CPWL_Wnd* pWnd) const {
  if (CPWL_MsgControl* pCtrl = GetMsgControl())
    return pCtrl->IsWndCaptureKeyboard(pWnd);

  return false;
}

bool CPWL_Wnd::IsFocused() const {
  if (CPWL_MsgControl* pCtrl = GetMsgControl())
    return pCtrl->IsMainCaptureKeyboard(this);

  return false;
}

CFX_FloatRect CPWL_Wnd::GetFocusRect() const {
  return CPWL_Utils::InflateRect(GetWindowRect(), 1);
}

FX_FLOAT CPWL_Wnd::GetFontSize() const {
  return m_sPrivateParam.fFontSize;
}

void CPWL_Wnd::SetFontSize(FX_FLOAT fFontSize) {
  m_sPrivateParam.fFontSize = fFontSize;
}

CFX_SystemHandler* CPWL_Wnd::GetSystemHandler() const {
  return m_sPrivateParam.pSystemHandler;
}

IPWL_FocusHandler* CPWL_Wnd::GetFocusHandler() const {
  return m_sPrivateParam.pFocusHandler;
}

IPWL_Provider* CPWL_Wnd::GetProvider() const {
  return m_sPrivateParam.pProvider.Get();
}

IPVT_FontMap* CPWL_Wnd::GetFontMap() const {
  return m_sPrivateParam.pFontMap;
}

CPWL_Color CPWL_Wnd::GetBorderLeftTopColor(BorderStyle nBorderStyle) const {
  switch (nBorderStyle) {
    case BorderStyle::BEVELED:
      return CPWL_Color(COLORTYPE_GRAY, 1);
    case BorderStyle::INSET:
      return CPWL_Color(COLORTYPE_GRAY, 0.5f);
    default:
      return CPWL_Color();
  }
}

CPWL_Color CPWL_Wnd::GetBorderRightBottomColor(BorderStyle nBorderStyle) const {
  switch (nBorderStyle) {
    case BorderStyle::BEVELED:
      return GetBackgroundColor() / 2.0f;
    case BorderStyle::INSET:
      return CPWL_Color(COLORTYPE_GRAY, 0.75f);
    default:
      return CPWL_Color();
  }
}

int32_t CPWL_Wnd::GetTransparency() {
  return m_sPrivateParam.nTransparency;
}

void CPWL_Wnd::SetTransparency(int32_t nTransparency) {
  for (const auto& pChild : m_Children) {
    if (pChild)
      pChild->SetTransparency(nTransparency);
  }
  m_sPrivateParam.nTransparency = nTransparency;
}

CFX_Matrix CPWL_Wnd::GetWindowMatrix() const {
  CFX_Matrix mt = GetChildToRoot();
  if (IPWL_Provider* pProvider = GetProvider())
    mt.Concat(pProvider->GetWindowMatrix(GetAttachedData()));
  return mt;
}

FX_RECT CPWL_Wnd::PWLtoWnd(const CFX_FloatRect& rect) const {
  CFX_FloatRect rcTemp = rect;
  CFX_Matrix mt = GetWindowMatrix();
  mt.TransformRect(rcTemp);
  return FX_RECT((int32_t)(rcTemp.left + 0.5), (int32_t)(rcTemp.bottom + 0.5),
                 (int32_t)(rcTemp.right + 0.5), (int32_t)(rcTemp.top + 0.5));
}

CFX_PointF CPWL_Wnd::ParentToChild(const CFX_PointF& point) const {
  CFX_Matrix mt = GetChildMatrix();
  if (mt.IsIdentity())
    return point;

  mt.SetReverse(mt);
  return mt.Transform(point);
}

CFX_FloatRect CPWL_Wnd::ParentToChild(const CFX_FloatRect& rect) const {
  CFX_Matrix mt = GetChildMatrix();
  if (mt.IsIdentity())
    return rect;

  mt.SetReverse(mt);
  CFX_FloatRect rc = rect;
  mt.TransformRect(rc);
  return rc;
}

CFX_Matrix CPWL_Wnd::GetChildToRoot() const {
  CFX_Matrix mt(1, 0, 0, 1, 0, 0);
  if (HasFlag(PWS_CHILD)) {
    const CPWL_Wnd* pParent = this;
    while (pParent) {
      mt.Concat(pParent->GetChildMatrix());
      pParent = pParent->GetParentWindow();
    }
  }
  return mt;
}

CFX_Matrix CPWL_Wnd::GetChildMatrix() const {
  if (HasFlag(PWS_CHILD))
    return m_sPrivateParam.mtChild;

  return CFX_Matrix(1, 0, 0, 1, 0, 0);
}

void CPWL_Wnd::SetChildMatrix(const CFX_Matrix& mt) {
  m_sPrivateParam.mtChild = mt;
}

const CPWL_Wnd* CPWL_Wnd::GetFocused() const {
  CPWL_MsgControl* pMsgCtrl = GetMsgControl();
  return pMsgCtrl ? pMsgCtrl->m_pMainKeyboardWnd : nullptr;
}

void CPWL_Wnd::EnableWindow(bool bEnable) {
  if (m_bEnabled == bEnable)
    return;

  for (const auto& pChild : m_Children) {
    if (pChild)
      pChild->EnableWindow(bEnable);
  }
  m_bEnabled = bEnable;
}

bool CPWL_Wnd::IsCTRLpressed(uint32_t nFlag) const {
  CFX_SystemHandler* pSystemHandler = GetSystemHandler();
  return pSystemHandler && pSystemHandler->IsCTRLKeyDown(nFlag);
}

bool CPWL_Wnd::IsSHIFTpressed(uint32_t nFlag) const {
  CFX_SystemHandler* pSystemHandler = GetSystemHandler();
  return pSystemHandler && pSystemHandler->IsSHIFTKeyDown(nFlag);
}

bool CPWL_Wnd::IsALTpressed(uint32_t nFlag) const {
  CFX_SystemHandler* pSystemHandler = GetSystemHandler();
  return pSystemHandler && pSystemHandler->IsALTKeyDown(nFlag);
}
