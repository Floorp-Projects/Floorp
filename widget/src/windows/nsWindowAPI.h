/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Roy Yokoyama <yokoyama@netscape.com>
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
#ifndef WindowAPI_h__
#define WindowAPI_h__

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

typedef LRESULT (WINAPI *NS_DefWindowProc) (HWND, UINT, WPARAM, LPARAM);
typedef LRESULT (WINAPI *NS_CallWindowProc) (WNDPROC, HWND, UINT, WPARAM, LPARAM);
typedef LONG (WINAPI *NS_SetWindowLong) (HWND, int, LONG);
typedef LONG (WINAPI *NS_GetWindowLong) (HWND, int);
typedef LRESULT (WINAPI *NS_SendMessage) (HWND, UINT, WPARAM, LPARAM )  ;
typedef LONG (WINAPI *NS_DispatchMessage) (CONST MSG *);
typedef BOOL (WINAPI *NS_GetMessage) (LPMSG, HWND, UINT, UINT);
typedef BOOL (WINAPI *NS_PeekMessage) (LPMSG, HWND, UINT, UINT, UINT);
typedef BOOL (WINAPI *NS_GetOpenFileName) (LPOPENFILENAMEW);
typedef BOOL (WINAPI *NS_GetSaveFileName) (LPOPENFILENAMEW);
typedef int (WINAPI *NS_GetClassName) (HWND, LPWSTR, int);
typedef HWND (WINAPI *NS_CreateWindowEx) 
          (DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
typedef ATOM (WINAPI *NS_RegisterClass) (CONST WNDCLASSW *); 
typedef BOOL (WINAPI *NS_UnregisterClass) (LPCWSTR, HINSTANCE); 
typedef BOOL (WINAPI *NS_SHGetPathFromIDList) (LPCITEMIDLIST, LPWSTR);
typedef LPITEMIDLIST (WINAPI *NS_SHBrowseForFolder) (LPBROWSEINFOW);

#endif /* WindowAPI_h__ */
