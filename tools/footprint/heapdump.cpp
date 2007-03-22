/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001, 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *	Suresh Duddi <dp@netscape.com>
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

/*
 * heapdump
 *
 * Sends a message to netscape 6.3 to dump the heap.
 */

#include <windows.h>
#include <stdio.h>

static const char *kMozHeapDumpMessageString = "MOZ_HeapDump";
static const char *kMozAppClassName = "MozillaWindowClass";

BOOL IsMozilla(const char *title)
{
    if (!title || !*title)
        return FALSE;
    // Title containing " - Mozilla" is a mozilla window
    if (strstr(title, " - Mozilla"))
      return TRUE;

    // Title containing "Mozilla {" is a mozilla window
    // This form happens when there is no title
    if (strstr(title, "Mozilla {"))
      return TRUE;

    return FALSE;
}

BOOL IsNetscape(const char *title)
{
    if (!title || !*title)
        return FALSE;
    // Skip to where the <space>-<space>Netscape 6 exists
    return strstr(title, " - Netscape 6") ? TRUE : FALSE;
}

// Find a mozilla or Netscape top level window in that order
// Pattter we are looking for is
// .* - Netscape 6 [{.*}]
// .* - Mozilla [{.*}]
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    HWND * pwnd = (HWND *)lParam;
    char buf[1024];

    // Make sure we are dealing with a window of our class name
    GetClassName(hwnd, buf, sizeof(buf)-1);
    if (strcmp(buf, kMozAppClassName))
        return TRUE;

    GetWindowText(hwnd, buf, sizeof(buf)-1);

    if (IsMozilla(buf)) {
        // Yeah. Search ends.
        *pwnd = hwnd;
        return FALSE;
    }
    
    // We continue search if we find a Netscape window
    // since we might find a mozilla window next
    if (IsNetscape(buf))
        *pwnd = hwnd;
    
    return TRUE;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    UINT msgHandle = RegisterWindowMessage(kMozHeapDumpMessageString);

    // Find a window to send the message to.
    // We look for a mozilla window first and then a netscape window
    // logic being that developers run release Netscape (to read mail) and
    // mozilla (debug builds).
    
    HWND mozwindow = 0;
    EnumWindows(EnumWindowsProc, (LPARAM) &mozwindow);
    if (mozwindow == 0) {
        printf("Cannot find Mozilla or Netscape 6 window. Exit\n");
        exit(-1);
    }

    
    char buf[1024];
    GetWindowText(mozwindow, buf, sizeof(buf)-1);
    if (IsMozilla(buf))
        printf("Found Mozilla window with title : %s\n", buf); 
    else if (IsNetscape(buf))
        printf("Found Netscape window with title : %s\n", buf); 

    SendMessage(mozwindow, msgHandle, 0, 0);

    printf("Sending HeapDump Message done.  Heapdump available in c:\\heapdump.txt\n");
    return 0;
}

