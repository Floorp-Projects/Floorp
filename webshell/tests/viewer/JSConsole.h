/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jsconsole_h__
#define jsconsole_h__

#include <windows.h>
#include "nsIScriptContext.h"

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
    void SetContext(nsIScriptContext *aContext) {
      NS_IF_ADDREF(aContext);
      NS_IF_RELEASE(mContext);
      mContext = aContext;
    }

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

