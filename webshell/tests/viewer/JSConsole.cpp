/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//is case this is defined from the outside... MMP
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif

#include "JSConsole.h"
#include "jsconsres.h"
#include "nsIScriptContext.h"
#include <stdio.h>
#include "jsapi.h"

HINSTANCE JSConsole::sAppInstance = 0;
HACCEL JSConsole::sAccelTable = 0;
CHAR JSConsole::sDefaultCaption[] = "JavaScript Console";
BOOL JSConsole::mRegistered = FALSE;


// display an error string along with the error returned from
// the GetLastError functions
#define MESSAGE_LENGTH 256
void DisplayError(LPSTR lpMessage)
{
    CHAR lpMsgBuf[MESSAGE_LENGTH * 2];
    CHAR lpLastError[MESSAGE_LENGTH];

    ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL,
                    GetLastError(),
                    0,
                    (LPTSTR)&lpLastError,
                    MESSAGE_LENGTH,
                    NULL);
    
    strcpy(lpMsgBuf, lpMessage);
    strcat(lpMsgBuf, "\nThe WWS (Worthless Windows System) reports:\n");
    strcat(lpMsgBuf, lpLastError);

    // Display the string.
    ::MessageBox(NULL, lpMsgBuf, "JSConsole Error", MB_OK | MB_ICONSTOP);
}

#if defined(_DEBUG)
  #define VERIFY(value, errorCondition, message)   \
                  if((value) == (errorCondition)) DisplayError(message);
#else // !_DEBUG
  #define VERIFY(value, errorCondition, message)   (value)
#endif // _DEBUG

//
// Register the window class
//
BOOL JSConsole::RegisterWidget()
{
    WNDCLASS wc;
   
    wc.style = 0;
    wc.lpfnWndProc = JSConsole::WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = JSConsole::sAppInstance;
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.lpszMenuName = MAKEINTRESOURCE(JSCONSOLE_MENU);
    wc.lpszClassName = "JavaScript Console";

    return (BOOL)::RegisterClass(&wc);
}

//
// Create the main application window
//
JSConsole* JSConsole::CreateConsole()
{
    if (!JSConsole::mRegistered){
        JSConsole::mRegistered = RegisterWidget();
    }

    HWND hWnd = ::CreateWindowEx(WS_EX_ACCEPTFILES | 
                                            WS_EX_CLIENTEDGE | 
                                            WS_EX_CONTROLPARENT,
                                "JavaScript Console",
                                JSConsole::sDefaultCaption,
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                450,
                                500,
                                NULL,
                                NULL,
                                JSConsole::sAppInstance,
                                NULL);
    if (hWnd) {
        ::ShowWindow(hWnd, SW_SHOW);
        ::UpdateWindow(hWnd);

        JSConsole *console = (JSConsole*)::GetWindowLong(hWnd, GWL_USERDATA);
        return console;
    }

    return NULL;
}

//
// Window Procedure
//
LRESULT CALLBACK JSConsole::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    JSConsole *console = (JSConsole*)::GetWindowLong(hWnd, GWL_USERDATA);
    
    // just ignore the message unless is WM_NCCREATE
    if (!console) {
        if (uMsg == WM_NCCREATE) {
            HWND hWndEdit = ::CreateWindow("EDIT",
                                            NULL,
                                            WS_CHILDWINDOW | WS_VISIBLE | ES_MULTILINE |
                                                        ES_AUTOHSCROLL | ES_AUTOVSCROLL |
                                                        WS_HSCROLL | WS_VSCROLL,
                                            0, 0, 0, 0,
                                            hWnd,
                                            NULL,
                                            JSConsole::sAppInstance,
                                            NULL);
            if (!hWndEdit) {
                ::DisplayError("Cannot Create Edit Window");
                return FALSE;
            }
            ::SendMessage(hWndEdit, EM_SETLIMITTEXT, (WPARAM)0, (LPARAM)0);

            console = new JSConsole(hWnd, hWndEdit);
            ::SetWindowLong(hWnd, GWL_USERDATA, (DWORD)console);

            ::SetFocus(hWndEdit);
        }

#if defined(STRICT)
        return ::CallWindowProc((WNDPROC)::DefWindowProc, hWnd, uMsg, 
                                wParam, lParam);
#else
        return ::CallWindowProc((FARPROC)::DefWindowProc, hWnd, uMsg, 
                                wParam, lParam);
#endif /* STRICT */
    }

    switch(uMsg) {

        // make sure the edit window covers the whole client area
        case WM_SIZE:
            return console->OnSize(wParam, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam));

        // exit the application
        case WM_DESTROY:
            console->OnDestroy();

#if defined(STRICT)
            return ::CallWindowProc((WNDPROC)::DefWindowProc, hWnd, uMsg, 
                                    wParam, lParam);
#else
            return ::CallWindowProc((FARPROC)::DefWindowProc, hWnd, uMsg, 
                                    wParam, lParam);
#endif /* STRICT */

        // enable/disable menu items
        case WM_INITMENUPOPUP:
            return console->OnInitMenu((HMENU)wParam, (UINT)LOWORD(lParam), (BOOL)HIWORD(lParam));

        case WM_COMMAND:
            // menu or accelerator
            if (HIWORD(wParam) == 0 || HIWORD(wParam) == 1) {
                switch(LOWORD(wParam)) {

                    case ID_FILENEW:
                        console->OnFileNew();
                        break;

                    case ID_FILEOPEN:
                        if (console->OpenFileDialog(OPEN_DIALOG)) {
                            console->LoadFile();
                        }

                        break;

                    case ID_FILESAVE:
                        if (console->CanSave()) {
                            console->SaveFile();
                            break;
                        }
                        // fall through so it can "Save As..."

                    case ID_FILESAVEAS:
                        if (console->OpenFileDialog(SAVE_DIALOG)) {
                            console->SaveFile();
                        }
                        break;

                    case ID_FILEEXIT:
                        ::DestroyWindow(hWnd);
                        break;

                    case ID_EDITUNDO:
                        console->OnEditUndo();
                        break;

                    case ID_EDITCUT:
                        console->OnEditCut();
                        break;

                    case ID_EDITCOPY:
                        console->OnEditCopy();
                        break;

                    case ID_EDITPASTE:
                        console->OnEditPaste();
                        break;

                    case ID_EDITDELETE:
                        console->OnEditDelete();
                        break;

                    case ID_EDITSELECTALL:
                        console->OnEditSelectAll();
                        break;

                    case ID_COMMANDSEVALALL:
                        console->OnCommandEvaluateAll();
                        break;

                    case ID_COMMANDSEVALSEL:
                        console->OnCommandEvaluateSelection();
                        break;

                    case ID_COMMANDSINSPECTOR:
                        console->OnCommandInspector();
                        break;

                }
            }

            break;

        case WM_DROPFILES:
        {
            HDROP hDropInfo = (HDROP)wParam;
            if (::DragQueryFile(hDropInfo, (UINT)-1L, NULL, 0) != 1) {
                ::MessageBox(hWnd, "Just One File Please...", "JSConsole Error", MB_OK | MB_ICONINFORMATION);
            }
            else {
                CHAR fileName[MAX_PATH];
                ::DragQueryFile(hDropInfo, 0, fileName, MAX_PATH);
                console->SetFileName(fileName);
                console->LoadFile();
            }
            break;
        }

        case WM_SETFOCUS:
            return console->OnSetFocus((HWND)wParam);

        default:
#if defined(STRICT)
            return ::CallWindowProc((WNDPROC)::DefWindowProc, hWnd, uMsg, 
                                    wParam, lParam);
#else
            return ::CallWindowProc((FARPROC)::DefWindowProc, hWnd, uMsg, 
                                    wParam, lParam);
#endif /* STRICT */
    }

    return 0;
}

//
// Constructor
// The main window and the edit control must have been created already
//
JSConsole::JSConsole(HWND aMainWindow, HWND aEditControl) : 
                                    mMainWindow(aMainWindow), 
                                    mEditWindow(aEditControl),
                                    mContext(NULL)
{
    mFileInfo.Init();
}

//
// Destructor
//
JSConsole::~JSConsole()
{
}

//
// Load a file into the edit field
//
BOOL JSConsole::LoadFile()
{
    BOOL result = FALSE;

    if (mMainWindow) {
        // open the file
        HANDLE file = ::CreateFile(mFileInfo.mCurrentFileName,
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    NULL,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                    NULL);
        if (file != INVALID_HANDLE_VALUE) {
            // check the file size. Max is 64k
            DWORD sizeHiWord;
            DWORD sizeLoWord = ::GetFileSize(file, &sizeHiWord);
            if (sizeLoWord < 0x10000 && sizeHiWord == 0) {
                // alloc a buffer big enough to contain the file (account for '\0'
                CHAR *buffer = new CHAR[sizeLoWord + 1]; 
                if (buffer) {
                    // read the file in memory
                    if (::ReadFile(file,
                                    buffer,
                                    sizeLoWord,
                                    &sizeHiWord,
                                    NULL)) {
                        NS_ASSERTION(sizeLoWord == sizeHiWord, "ReadFile inconsistency");
                        buffer[sizeLoWord] = '\0'; // terminate the buffer 
                        // write the file to the edit field
                        ::SendMessage(mEditWindow, WM_SETTEXT, (WPARAM)0, (LPARAM)buffer);

                        // update the caption
                        CHAR caption[80];
                        ::wsprintf(caption, 
                                    "%s - %s", 
                                    mFileInfo.mCurrentFileName + mFileInfo.mFileOffset,
                                    sDefaultCaption);
                        ::SendMessage(mMainWindow, WM_SETTEXT, (WPARAM)0, (LPARAM)caption);

                        result = TRUE;
                    }
                    else {
                        ::DisplayError("Error Reading the File");
                    }

                    // free the allocated buffer
                    delete[] buffer;
                }
                else {
                    ::MessageBox(mMainWindow, 
                                "Cannot Allocate Enough Memory to Copy the File in Memory", 
                                "JSConsole Error", 
                                MB_OK | MB_ICONSTOP);
                }
            }
            else {
                ::MessageBox(mMainWindow, 
                            "File too big. Max is 64k", 
                            "JSConsole Error", 
                            MB_OK | MB_ICONSTOP);
            }

            // close the file handle
            ::CloseHandle(file);
        }
#ifdef _DEBUG
        else {
            CHAR message[MAX_PATH + 20];
            wsprintf(message, "Cannot Open File: %s", mFileInfo.mCurrentFileName); 
            ::DisplayError(message);
        }
#endif
    }

    return result;
}

//
// Save the current text into a file
//
BOOL JSConsole::SaveFile()
{
    BOOL result = FALSE;

    if (mMainWindow && mFileInfo.mCurrentFileName[0] != '\0') {
        // create the new file
        HANDLE file = ::CreateFile(mFileInfo.mCurrentFileName,
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ,
                                    NULL,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                    NULL);
        if (file != INVALID_HANDLE_VALUE) {
            DWORD size;
            // get the text size
            size = ::SendMessage(mEditWindow, WM_GETTEXTLENGTH, (WPARAM)0, (LPARAM)0);

            // alloc a buffer big enough to contain the file
            CHAR *buffer = new CHAR[++size];
            if (buffer) {
                DWORD byteRead;
                // read the text area content
                ::SendMessage(mEditWindow, WM_GETTEXT, (WPARAM)size, (LPARAM)buffer);

                // write the buffer to disk
                if (::WriteFile(file,
                                buffer,
                                size,
                                &byteRead,
                                NULL)) {
                    NS_ASSERTION(byteRead == size, "WriteFile inconsistency");
                    // update the caption
                    CHAR caption[80];
                    ::wsprintf(caption, 
                                "%s - %s", 
                                mFileInfo.mCurrentFileName + mFileInfo.mFileOffset,
                                sDefaultCaption);
                    ::SendMessage(mMainWindow, WM_SETTEXT, (WPARAM)0, (LPARAM)caption);

                    result = TRUE;
                }
                else {
                    ::DisplayError("Error Writing the File");
                }

                // free the allocated buffer
                delete[] buffer;
            }
            else {
                ::MessageBox(mMainWindow, 
                            "Cannot Allocate Enough Memory to Copy the Edit Text in Memory", 
                            "JSConsole Error", 
                            MB_OK | MB_ICONSTOP);
            }

            // close the file handle
            ::CloseHandle(file);
        }
#ifdef _DEBUG
        else {
            CHAR message[MAX_PATH + 20];
            wsprintf(message, "Cannot Open File: %s", mFileInfo.mCurrentFileName); 
            ::DisplayError(message);
        }
#endif
    }

    return result;
}

//
// Open a FileOpen or FileSave dialog
//
BOOL JSConsole::OpenFileDialog(UINT aWhichDialog)
{
    BOOL result = FALSE;
    OPENFILENAME ofn;

    if (mMainWindow) {

        // *.js is the standard File Name on the Save/Open Dialog
        if (mFileInfo.mCurrentFileName[0] == '\0') 
            ::strcpy(mFileInfo.mCurrentFileName, "*.js");

        // fill the OPENFILENAME sruct
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = mMainWindow;
        ofn.hInstance = JSConsole::sAppInstance;
        ofn.lpstrFilter = "JavaScript Files (*.js)\0*.js\0Text Files (*.txt)\0*.txt\0All Files\0*.*\0\0";
        ofn.lpstrCustomFilter = NULL; 
        ofn.nMaxCustFilter = 0;
        ofn.nFilterIndex = 1; // the first one in lpstrFilter
        ofn.lpstrFile = mFileInfo.mCurrentFileName; // contains the file path name on return
        ofn.nMaxFile = sizeof(mFileInfo.mCurrentFileName);
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL; // use default
        ofn.lpstrTitle = NULL; // use default
        ofn.Flags = OFN_CREATEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
        ofn.nFileOffset = 0; 
        ofn.nFileExtension = 0;
        ofn.lpstrDefExt = "js"; // default extension is .js
        ofn.lCustData = NULL; 
        ofn.lpfnHook = NULL;
        ofn.lpTemplateName = NULL;

        // call the open file dialog or the save file dialog according to aIsOpenDialog
        if (aWhichDialog == OPEN_DIALOG) {
            result = ::GetOpenFileName(&ofn);
        }
        else if (aWhichDialog == SAVE_DIALOG) {
            result = ::GetSaveFileName(&ofn);
        }
    
        if (!result) {
            mFileInfo.mCurrentFileName[0] = '\0';
            ::CommDlgExtendedError();
        }
        else {
            mFileInfo.mFileOffset = ofn.nFileOffset;
            mFileInfo.mFileExtension = ofn.nFileExtension;
        }
    }

    return result;
}

//
// set the mFileInfo structure with the proper value given a generic path
//
void JSConsole::SetFileName(LPSTR aFileName)
{
    strcpy(mFileInfo.mCurrentFileName, aFileName);
    for (int i = strlen(aFileName); i >= 0; i--) {
        if (mFileInfo.mCurrentFileName[i] == '.') {
            mFileInfo.mFileExtension = i;
        }

        if (mFileInfo.mCurrentFileName[i] == '\\') {
            mFileInfo.mFileOffset = i + 1;
            break;
        }
    }
}

//
// Move the edit window to cover the all client area
//
LRESULT JSConsole::OnSize(DWORD aResizeFlags, UINT aWidth, UINT aHeight)
{
    ::MoveWindow(mEditWindow, 0, 0, aWidth, aHeight, TRUE);
    RECT textArea;
    textArea.left = 3;
    textArea.top = 1;
    textArea.right = aWidth - 20;
    textArea.bottom = aHeight - 17;
    ::SendMessage(mEditWindow, EM_SETRECTNP, (WPARAM)0, (LPARAM)&textArea);
    return 0L;
}

//
// Initialize properly menu items
//
LRESULT JSConsole::OnInitMenu(HMENU aMenu, UINT aPos, BOOL aIsSystem)
{
    if (!aIsSystem) {
        if (aPos == EDITMENUPOS) {
            InitEditMenu(aMenu);
        }
        else if (aPos == COMMANDSMENUPOS) {
            InitCommandMenu(aMenu);
        }
    }

    return 0L;
}

//
// Pass the focus to the edit window
//
LRESULT JSConsole::OnSetFocus(HWND aWnd)
{
    ::SetFocus(mEditWindow);
    return 0L;
}

//
// Destroy message
//
void JSConsole::OnDestroy()
{
    if (mDestroyNotification)
        (*mDestroyNotification)();
}

//
// File/New. Reset caption, text area and file info
//
void JSConsole::OnFileNew()
{
    SendMessage(mEditWindow, WM_SETTEXT, (WPARAM)0, (LPARAM)0);
    SendMessage(mMainWindow, WM_SETTEXT, (WPARAM)0, (LPARAM)JSConsole::sDefaultCaption);
    mFileInfo.Init();
}

//
// Edit/Undo. Undo the last operation on the edit field
//
void JSConsole::OnEditUndo()
{
    SendMessage(mEditWindow, WM_UNDO, (WPARAM)0, (LPARAM)0);
}

//
// Edit/Cut. Cut the current selection
//
void JSConsole::OnEditCut()
{
    SendMessage(mEditWindow, WM_CUT, (WPARAM)0, (LPARAM)0);
}

//
// Edit/Copy. Copy the current selection
//
void JSConsole::OnEditCopy()
{
    SendMessage(mEditWindow, WM_COPY, (WPARAM)0, (LPARAM)0);
}

//
// Edit/Paste. Paste from the clipboard
//
void JSConsole::OnEditPaste()
{
    SendMessage(mEditWindow, WM_PASTE, (WPARAM)0, (LPARAM)0);
}

//
// Edit/Delete. Delete the current selection
//
void JSConsole::OnEditDelete()
{
    SendMessage(mEditWindow, WM_CLEAR, (WPARAM)0, (LPARAM)0);
}

//
// Edit/Select All. Select the whole text in the text area
//
void JSConsole::OnEditSelectAll()
{
    //SendMessage(mEditWindow, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
}

//
// Command/Evaluate All. Take the text area content and evaluate in the js context
//
void JSConsole::OnCommandEvaluateAll()
{
    EvaluateText(0, (UINT)-1);
}

//
// Command/Evaluate Selection. Take the current text area selection and evaluate in the js context
//
void JSConsole::OnCommandEvaluateSelection()
{
    //
    // get the selection and evaluate it
    //
    DWORD startSel, endSel;

    // get selection range
    ::SendMessage(mEditWindow, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);

    EvaluateText(startSel, endSel);
}

//
// Command/Inspector. Run the js inspector on the global object
//
void JSConsole::OnCommandInspector()
{
    ::MessageBox(mMainWindow, "Inspector not yet available", "JSConsole Error", MB_OK | MB_ICONINFORMATION);
}

//
// Help
//
void JSConsole::OnHelp()
{
}

//
// private method. Deal with the "Edit" menu
//
void JSConsole::InitEditMenu(HMENU aMenu)
{
    CHAR someText[2] = {'\0', '\0'}; // some buffer

    // set flags to "disable"
    UINT undoFlags = MF_BYPOSITION | MF_DISABLED | MF_GRAYED;
    UINT cutFlags = MF_BYPOSITION | MF_DISABLED | MF_GRAYED;
    UINT copyFlags = MF_BYPOSITION | MF_DISABLED | MF_GRAYED;
    UINT pasteFlags = MF_BYPOSITION | MF_DISABLED | MF_GRAYED;
    UINT deleteFlags = MF_BYPOSITION | MF_DISABLED | MF_GRAYED;
    UINT selectAllFlags = MF_BYPOSITION | MF_DISABLED | MF_GRAYED;

    // check if the edit area has any text
    SendMessage(mEditWindow, WM_GETTEXT, (WPARAM)2, (LPARAM)someText);
    if (someText[0] != '\0') {
        // enable the "Select All"
        selectAllFlags = MF_BYPOSITION | MF_ENABLED;

        // enable "Copy/Cut/Paste" if there is any selection
        UINT startPos, endPos;
        SendMessage(mEditWindow, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
        if (startPos != endPos) {
            cutFlags = MF_BYPOSITION | MF_ENABLED;
            copyFlags = MF_BYPOSITION | MF_ENABLED;
            deleteFlags = MF_BYPOSITION | MF_ENABLED;
        }
    }

    // undo is available if the edit control says so
    if (SendMessage(mEditWindow, EM_CANUNDO, (WPARAM)0, (LPARAM)0)) {
        undoFlags = MF_BYPOSITION | MF_ENABLED;
    }

    // check whether or not the clipboard contains text data
    if (IsClipboardFormatAvailable(CF_TEXT)) {
        pasteFlags = MF_BYPOSITION | MF_ENABLED;
    }

    // do enable/disable
    VERIFY(EnableMenuItem(aMenu, 
                            ID_EDITUNDO - ID_EDITMENU - 1, 
                            undoFlags),
            -1L,
            "Disable/Enable \"Undo\" Failed");
    VERIFY(EnableMenuItem(aMenu, 
                            ID_EDITCUT - ID_EDITMENU - 1, 
                            cutFlags),
            -1L,
            "Disable/Enable \"Cut\" Failed");
    VERIFY(EnableMenuItem(aMenu, 
                            ID_EDITCOPY - ID_EDITMENU - 1, 
                            copyFlags),
            -1L,
            "Disable/Enable \"Copy\" Failed");
    VERIFY(EnableMenuItem(aMenu, 
                            ID_EDITPASTE - ID_EDITMENU - 1, 
                            pasteFlags),
            -1L,
            "Disable/Enable \"Paste\" Failed");
    VERIFY(EnableMenuItem(aMenu, 
                            ID_EDITDELETE - ID_EDITMENU - 1, 
                            deleteFlags),
            -1L,
            "Disable/Enable \"Delete\" Failed");
    VERIFY(EnableMenuItem(aMenu, 
                            ID_EDITSELECTALL - ID_EDITMENU - 1, 
                            selectAllFlags),
            -1L,
            "Disable/Enable \"Select All\" Failed");
}

//
// private method. Deal with the "Command" menu
//
void JSConsole::InitCommandMenu(HMENU aMenu)
{
    CHAR someText[2] = {'\0', '\0'}; // some buffer

    // set flags to "disable"
    UINT evaluateAllFlags = MF_BYPOSITION | MF_DISABLED | MF_GRAYED;
    UINT evaluateSelectionFlags = MF_BYPOSITION | MF_DISABLED | MF_GRAYED;

    // check if the edit area has any text
    SendMessage(mEditWindow, WM_GETTEXT, (WPARAM)2, (LPARAM)someText);
    if (someText[0] != 0) {
        // if there is some text enable "Evaluate All"
        evaluateAllFlags = MF_BYPOSITION | MF_ENABLED;

        // enable "Evaluate Selection" if there is any selection
        UINT startPos, endPos;
        SendMessage(mEditWindow, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
        if (startPos != endPos) {
            evaluateSelectionFlags = MF_BYPOSITION | MF_ENABLED;
        }
    }

    // disable/enable commands
    VERIFY(EnableMenuItem(aMenu, 
                            ID_COMMANDSEVALALL - ID_COMMANDSMENU - 1, 
                            evaluateAllFlags),
            -1L,
            "Disable/Enable \"Evaluate All\" Failed");
    VERIFY(EnableMenuItem(aMenu, 
                            ID_COMMANDSEVALSEL - ID_COMMANDSMENU - 1, 
                            evaluateSelectionFlags),
            -1L,
            "Disable/Enable \"Evaluate Selection\" Failed");
}

//
// normailize a buffer of char coming from a text area. 
// Basically get rid of the 0x0D char
//
LPSTR NormalizeBuffer(LPSTR aBuffer)
{
    // trim all the 0x0D at the beginning (should be 1 at most, but hey...)
    while (*aBuffer == 0x0D) {
        aBuffer++;
    }

    LPSTR readPointer = aBuffer;
    LPSTR writePointer = aBuffer;
    
    do {
        // compact the buffer if needed
        *writePointer = *readPointer;

        // skip the 0x0D
        if (*readPointer != 0x0D) {
            writePointer++;
        }

    } while (*readPointer++ != '\0');

    return aBuffer;
}

LPSTR PrepareForTextArea(LPSTR aBuffer, PRInt32 aSize)
{
  PRInt32 count = 0;
  LPSTR newBuffer = aBuffer;
  LPSTR readPointer = aBuffer;

  // count the '\n'
  while (*readPointer != '\0' && (*readPointer++ != '\n' || ++count));

  if (0 != count) {
    readPointer = aBuffer;
    newBuffer = new CHAR[aSize + count + 1];
    LPSTR writePointer = newBuffer;
    while (*readPointer != '\0') {
      if (*readPointer == '\n') {
        *writePointer++ = 0x0D;
      }
      *writePointer++ = *readPointer++;
    }
    *writePointer = '\0';
  }

  return newBuffer;
}

//
// Evaluate the text enclosed between startSel and endSel
//
void JSConsole::EvaluateText(UINT aStartSel, UINT aEndSel)
{
    if (mContext) {
        // get the text size
        UINT size = ::SendMessage(mEditWindow, WM_GETTEXTLENGTH, (WPARAM)0, (LPARAM)0);

        // alloc a buffer big enough to contain the file
        CHAR *buffer = new CHAR[++size];
        if (buffer) {

            // get the whole text
            ::SendMessage(mEditWindow, WM_GETTEXT, (WPARAM)size, (LPARAM)buffer);

            // get the portion of the text to be evaluated
            if (aEndSel != (UINT)-1) {
                size = aEndSel - aStartSel;
                strncpy(buffer, buffer + aStartSel, size);
                buffer[size] = '\0';
            }
            else {
                aEndSel = size;
            }
        

            // change the 0x0D0x0A couple in 0x0A ('\n')
            // no new buffer allocation, the pointer may be in the middle 
            // of the old buffer though, so keep buffer to call delete
            LPSTR cleanBuffer = ::NormalizeBuffer(buffer);

            // evaluate the string
            nsAutoString returnValue;
            PRBool isUndefined;
            if (NS_SUCCEEDED(mContext->EvaluateString(NS_ConvertASCIItoUCS2(cleanBuffer), 
                                                      nsnull,
                                                      nsnull,
                                                      nsnull,
                                                      0,
                                                      nsnull,
                                                      returnValue,
                                                      &isUndefined))) {
                // output the result on the console and on the edit area
                CHAR result[128];
                LPSTR res = result;
                int bDelete = 0;

                JSContext *cx = (JSContext *)mContext->GetNativeContext();
                char *str = returnValue.ToNewCString();

                ::printf("The return value is %s\n", str);

                // make a string with 0xA changed to 0xD0xA
                res = PrepareForTextArea(str, returnValue.Length());
                if (res != str) {
                  bDelete = 1; // if the buffer was new'ed
                }

                // set the position at the end of the selection
                ::SendMessage(mEditWindow, EM_SETSEL, (WPARAM)aEndSel, (LPARAM)aEndSel);
                // write the result
                ::SendMessage(mEditWindow, EM_REPLACESEL, (WPARAM)TRUE, (LPARAM)res);
                // highlight the result
                ::SendMessage(mEditWindow, EM_SETSEL, (WPARAM)aEndSel - 1, (LPARAM)(aEndSel + strlen(res)));

                // deal with the "big string" case
                if (bDelete > 0) {
                  delete[] res;
                }
                delete[] str;

                // clean up a bit
                JS_GC((JSContext *)mContext->GetNativeContext());
            }
            else {
                ::MessageBox(mMainWindow, 
                                "Error evaluating the Script", 
                                "JSConsole Error", 
                                MB_OK | MB_ICONERROR);
            }

            delete[] buffer;
        }
        else {
            ::MessageBox(mMainWindow, 
                        "Not Enough Memory to Allocate a Buffer to Evaluate the Script",
                        "JSConsole Error",
                        MB_OK | MB_ICONSTOP);
        }
    }
    else {
        ::MessageBox(mMainWindow, 
                    "Java Script Context not initialized",
                    "JSConsole Error",
                    MB_OK | MB_ICONSTOP);
    }
}


