/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "stdafx.h"

#include "BrowserToolTip.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBrowserToolTip

CBrowserToolTip::CBrowserToolTip()
{
}

CBrowserToolTip::~CBrowserToolTip()
{
}

BEGIN_MESSAGE_MAP(CBrowserToolTip, CWnd)
	//{{AFX_MSG_MAP(CBrowserToolTip)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CBrowserToolTip::Create(CWnd *pParentWnd)
{
    return CWnd::CreateEx(WS_EX_TOOLWINDOW,
        AfxRegisterWndClass(CS_SAVEBITS, NULL, GetSysColorBrush(COLOR_INFOBK), NULL),
        _T("ToolTip"), WS_POPUP | WS_BORDER, 0, 0, 1, 1, pParentWnd->GetSafeHwnd(), NULL);
}

/////////////////////////////////////////////////////////////////////////////
// CBrowserToolTip message handlers

void CBrowserToolTip::OnPaint() 
{
	CPaintDC dc(this); // device context for painting

    CRect rcClient;
    GetClientRect(&rcClient);

    // Draw tip text
    int oldBkMode = dc.SetBkMode(TRANSPARENT);
    COLORREF oldTextColor = dc.SetTextColor(GetSysColor(COLOR_INFOTEXT));
    HGDIOBJ oldFont = dc.SelectObject(GetStockObject(DEFAULT_GUI_FONT));

    dc.DrawText(m_szTipText, -1, rcClient, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

    dc.SetBkMode(oldBkMode);
    dc.SetTextColor(oldTextColor);
    dc.SelectObject(oldFont);
}

BOOL CBrowserToolTip::PreCreateWindow(CREATESTRUCT& cs) 
{
	return CWnd::PreCreateWindow(cs);
}

void CBrowserToolTip::SetTipText(const CString &szTipText)
{
    m_szTipText = szTipText;
}

void CBrowserToolTip::Show(CWnd *pOverWnd, long left, long top)
{
    // Calculate the client window size
    CRect rcNewClient(0,0,0,0);
    CDC *pdc = GetDC();
    HGDIOBJ oldFont = pdc->SelectObject(GetStockObject(DEFAULT_GUI_FONT));
    rcNewClient.bottom = pdc->DrawText(m_szTipText, -1, rcNewClient,
        DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_CALCRECT);
    pdc->SelectObject(oldFont);
    ReleaseDC(pdc);
    rcNewClient.right += 8;
    rcNewClient.bottom += 8;
    
    // Adjust the tooltip to new size
    AdjustWindowRectEx(rcNewClient, GetWindowLong(m_hWnd, GWL_STYLE), FALSE, GetWindowLong(m_hWnd, GWL_EXSTYLE));

    // Adjust the left, top position of the tooltip
    CPoint ptTip(left, top);
    pOverWnd->ClientToScreen(&ptTip);

    // Make sure tip is below cursor
    POINT ptCursor;
    GetCursorPos(&ptCursor);
    long cyCursor = GetSystemMetrics(SM_CYCURSOR);
    if (ptTip.y < ptCursor.y + cyCursor)
        ptTip.y = ptCursor.y + cyCursor;

    // Make sure tip is fully visible
    RECT rcScreen;
    GetDesktopWindow()->GetClientRect(&rcScreen);
    if (ptTip.x < 0)
        ptTip.x = 0;
    else if (ptTip.x + rcNewClient.Width() > rcScreen.right)
        ptTip.x = rcScreen.right - rcNewClient.Width();
    if (ptTip.y < 0)
        ptTip.y = 0;
    else if (ptTip.y + rcNewClient.Height() > rcScreen.bottom)
        ptTip.y = rcScreen.bottom - rcNewClient.Height();

    // Position and show the tip
    SetWindowPos(&CWnd::wndTop, ptTip.x, ptTip.y, rcNewClient.Width(), rcNewClient.Height(),
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void CBrowserToolTip::Hide()
{
    ShowWindow(SW_HIDE);
}
