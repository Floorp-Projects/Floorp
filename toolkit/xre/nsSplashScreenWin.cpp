/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 *   mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSplashScreen.h"

#include <windows.h>

static ATOM gSplashScreenClass = 0;

class nsSplashScreenWin;

static nsSplashScreenWin *gSplashScreen = NULL;

class nsSplashScreenWin :
    public nsSplashScreen
{
public:
    nsSplashScreenWin();

    virtual void Open();
    virtual void Close();
    virtual void Update(PRInt32 progress);

protected:
    HWND mDialog;
    HBITMAP mSplashBitmap, mOldBitmap;
    HDC mSplashBitmapDC;
    int mWidth, mHeight;

    static DWORD WINAPI ThreadProc(LPVOID splashScreen);

    static LRESULT CALLBACK DialogProc (HWND hWnd,
                                        UINT uMsg,
                                        WPARAM wParam,
                                        LPARAM lParam);

    void OnPaint(HDC dc, const PAINTSTRUCT *ps);

    PRInt32 mProgress;
};

nsSplashScreen *
nsSplashScreen::GetOrCreate()
{
    if (!gSplashScreen)
        gSplashScreen = new nsSplashScreenWin();

    return gSplashScreen;
}

nsSplashScreen *
nsSplashScreen::Get()
{
    return gSplashScreen;
}

nsSplashScreenWin::nsSplashScreenWin()
    : mDialog(NULL), mSplashBitmap(NULL), mOldBitmap(NULL),
      mSplashBitmapDC(NULL),
      mWidth(200), mHeight(100),
      mProgress(-1)
{
}

void
nsSplashScreenWin::Open()
{
    if (mIsOpen || mDialog)
        return;

    mIsOpen = PR_TRUE;

    if (gSplashScreenClass == 0) {
        WNDCLASS wc;
        memset(&wc, 0, sizeof(WNDCLASS));

        wc.style = CS_NOCLOSE;
        wc.lpfnWndProc = (WNDPROC) DialogProc;
        wc.hInstance = GetModuleHandle(0);
        wc.hIcon = NULL;
        wc.hCursor = NULL;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        wc.lpszMenuName = NULL;
        wc.lpszClassName = TEXT("MozillaSplashScreen");

        gSplashScreenClass = RegisterClass(&wc);
    }

    if (mSplashBitmap == NULL) {
        wchar_t path[_MAX_PATH];
        if (::GetModuleFileNameW(0, path, _MAX_PATH)) {
            wchar_t *slash = wcsrchr(path, '\\');
            if (slash != NULL)
                slash[1] = 0;

            wcscat(path, L"splash.bmp");

#ifdef WINCE
            mSplashBitmap = ::SHLoadDIBitmap(path);
#else
#warning splashscreen needs some code to load bitmaps on non-WinCE
            mSplashBitmap = nsnull;
#endif

            if (mSplashBitmap) {
                BITMAP bmo;
                if (GetObject(mSplashBitmap, sizeof(BITMAP), &bmo) > 0) {
                    mWidth = bmo.bmWidth;
                    mHeight = bmo.bmHeight;

                    mSplashBitmapDC = CreateCompatibleDC(NULL);
                    mOldBitmap = (HBITMAP) SelectObject(mSplashBitmapDC, mSplashBitmap);
                } else {
                    DeleteObject(mSplashBitmap);
                    mSplashBitmap = NULL;
                }
            }
        }
    }

    DWORD threadID = 0;
    CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ThreadProc, this, 0, &threadID);
}

DWORD WINAPI
nsSplashScreenWin::ThreadProc(LPVOID splashScreen)
{
    nsSplashScreenWin *sp = (nsSplashScreenWin*) splashScreen;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND dlg = CreateWindowEx
        (WS_EX_NOACTIVATE /* | WS_EX_TOPMOST | WS_EX_TOOLWINDOW*/,
         TEXT("MozillaSplashScreen"),
         TEXT("Starting up..."),
         WS_POPUP | WS_BORDER,
         (screenWidth - sp->mWidth) / 2,
         (screenHeight - sp->mHeight) / 2,
         sp->mWidth, sp->mHeight,
         HWND_DESKTOP,
         NULL,
         GetModuleHandle(0),
         NULL);

    sp->mDialog = dlg;

    ShowWindow(dlg, SW_SHOW);
    SetForegroundWindow(dlg);

    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0)) {
        DispatchMessage(&msg);
    }

    // window was destroyed, nothing to do
    return 0;
}

void
nsSplashScreenWin::Close()
{
    if (mDialog) {
        ShowWindow(mDialog, SW_HIDE);
        PostMessage(mDialog, WM_QUIT, 0, 0);
        mDialog = NULL;
    }

    if (mSplashBitmap) {
        SelectObject(mSplashBitmapDC, mOldBitmap);
        DeleteObject(mSplashBitmapDC);
        DeleteObject(mSplashBitmap);
        mOldBitmap = NULL;
        mSplashBitmapDC = NULL;
        mSplashBitmap = NULL;
    }

    mIsOpen = PR_FALSE;
}

void
nsSplashScreenWin::Update(PRInt32 progress)
{
    if (progress >= 0)
        mProgress = progress > 100 ? 100 : progress;

    InvalidateRect(mDialog, NULL, FALSE);
}

void
nsSplashScreenWin::OnPaint(HDC dc, const PAINTSTRUCT *ps)
{
    if (mSplashBitmapDC) {
        BitBlt(dc,
               0, 0, gSplashScreen->mWidth, gSplashScreen->mHeight,
               gSplashScreen->mSplashBitmapDC,
               0, 0,
               SRCCOPY);
    } else {
        HBRUSH bkgr = CreateSolidBrush(RGB(200,200,200));
        RECT r = { 0, 0, mWidth, mHeight };
        FillRect(dc, &r, bkgr);
        DeleteObject(bkgr);
    }

#define BOTTOM_OFFSET 10
#define PROGRESS_HEIGHT 11

    if (mProgress != -1) {
        HBRUSH border = CreateSolidBrush(RGB(0x33,0x33,0x99));
        HBRUSH fill = CreateSolidBrush(RGB(0x33,0x66,0xFF));

        // the progress bar goes 15 pixels above the bottom of the window,
        // and is 2/3 of the width.  The outer border is 11 px in height,
        // which will be inset by 2 to draw the fill.
        int leftBorder = (mWidth * 2 / 3) / 2;

        // left, top, right, bottom
        RECT outerRect = { leftBorder, mHeight - BOTTOM_OFFSET - PROGRESS_HEIGHT,
                           mWidth - leftBorder, mHeight - BOTTOM_OFFSET };


	HBRUSH oldBrush = (HBRUSH)SelectObject(dc, border);
        MoveToEx(dc, outerRect.left, outerRect.top, (LPPOINT) NULL);
        LineTo(dc, outerRect.right, outerRect.top);
        LineTo(dc, outerRect.right, outerRect.bottom);
        LineTo(dc, outerRect.left, outerRect.bottom);
        LineTo(dc, outerRect.left, outerRect.top);
        SelectObject(dc, oldBrush);

        outerRect.top += 2;
        outerRect.left += 2;
        outerRect.right -= 2;
        outerRect.bottom -= 1;

        outerRect.right = outerRect.left + (outerRect.right - outerRect.left) * mProgress / 100;
        FillRect(dc, &outerRect, fill);

        DeleteObject(border);
        DeleteObject(fill);
    }
}

LRESULT CALLBACK
nsSplashScreenWin::DialogProc (HWND hWnd,
                               UINT uMsg,
                               WPARAM wParam,
                               LPARAM lParam)
{
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hWnd, &ps);

            if (gSplashScreen)
                gSplashScreen->OnPaint(dc, &ps);

            EndPaint(hWnd, &ps);
            return TRUE;
        }
            break;

        case WM_DESTROY:
            return TRUE;

        case WM_QUIT:
            DestroyWindow(hWnd);
            return TRUE;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    return FALSE;
}
