/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef jsconsole_h__
#define jsconsole_h__

#include <windows.h>

class nsIScriptContext;

#define OPEN_DIALOG         0
#define SAVE_DIALOG         1

typedef void (*DESTROY_NOTIFICATION) ();

class JSConsole {

private:
    HWND mMainWindow;
    HWND mEditWindow;

    DESTROY_NOTIFICATION mDestroyNotification;

    // keep info from the OPENFILENAME struct
    struct FileInfo {
        CHAR mCurrentFileName[MAX_PATH];
        WORD mFileOffset;
        WORD mFileExtension;

        void Init() {mCurrentFileName[0] = '\0'; mFileOffset = 0; mFileExtension = 0;}
    } mFileInfo;

    // one context per window
    nsIScriptContext *mContext;

public:
    static HINSTANCE sAppInstance;
    static HACCEL sAccelTable;
    static CHAR sDefaultCaption[];

    static BOOL RegisterWidget();
    static JSConsole* CreateConsole();
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    static BOOL mRegistered;

public:
    JSConsole(HWND aMainWindow, HWND aEditControl);
    ~JSConsole();

    BOOL LoadFile();
    BOOL SaveFile();

    BOOL OpenFileDialog(UINT aOpenOrSave);
    inline BOOL CanSave() { return mFileInfo.mCurrentFileName[0] != '\0'; }
    void SetFileName(LPSTR aFileName);

    HWND GetMainWindow() { return mMainWindow; }
    void SetNotification(DESTROY_NOTIFICATION aNotification) { mDestroyNotification = aNotification; }
    void SetContext(nsIScriptContext *aContext) { mContext = aContext; }

    void EvaluateText(UINT aStartSel, UINT aEndSel);

    // windows messages
    LRESULT OnSize(DWORD aResizeFlags, UINT aWidth, UINT aHeight);
    LRESULT OnInitMenu(HMENU aMenu, UINT aPos, BOOL aIsSystem);
    LRESULT OnSetFocus(HWND aWnd);

    void OnDestroy();

    // menu items
    void OnFileNew();
    void OnEditUndo();
    void OnEditCut();
    void OnEditCopy();
    void OnEditPaste();
    void OnEditDelete();
    void OnEditSelectAll();
    void OnCommandEvaluateAll();
    void OnCommandEvaluateSelection();
    void OnCommandInspector();
    void OnHelp();

private:
    void InitEditMenu(HMENU aMenu);
    void InitCommandMenu(HMENU aMenu);
};

#endif // jsconsole_h__

