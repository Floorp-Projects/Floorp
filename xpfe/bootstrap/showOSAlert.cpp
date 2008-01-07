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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Don Bragg <dbragg@netscape.com>
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

#include  <stdio.h>
#include  <string.h>
#include "nscore.h"

//defines and includes for previous installation cleanup process
#if defined (XP_WIN)
#include <windows.h>
#elif defined (XP_MAC)
#include <Dialogs.h>
#include <TextUtils.h>
#elif defined (XP_OS2)
#define INCL_DOS
#define INCL_WIN
#include <os2.h>
#endif

extern "C" void ShowOSAlert(const char* aMessage);


// The maximum allowed length of aMessage is 255 characters!
void ShowOSAlert(const char* aMessage)
{
#ifdef DEBUG_dbragg
printf("\n****Inside ShowOSAlert ***\n");	
#endif 

    const PRInt32 max_len = 255;
    char message_copy[max_len+1] = { 0 };
    PRInt32 input_len = strlen(aMessage);
    PRInt32 copy_len = (input_len > max_len) ? max_len : input_len;
    strncpy(message_copy, aMessage, copy_len);
    message_copy[copy_len] = 0;

#if defined (XP_WIN)
    MessageBoxA(NULL, message_copy, NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND );
#elif (XP_MAC)
    short buttonClicked;
    StandardAlert(kAlertStopAlert, c2pstr(message_copy), nil, nil, &buttonClicked);
#elif defined (XP_OS2)
    /* Set our app to be a PM app before attempting Win calls */
    PPIB ppib;
    PTIB ptib;
    DosGetInfoBlocks(&ptib, &ppib);
    ppib->pib_ultype = 3;
    HAB hab = WinInitialize(0);
    HMQ hmq = WinCreateMsgQueue(hmq,0);
    WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, message_copy, "", 0, MB_OK);
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
#endif
    // It can't hurt to display the message on the console in any case,
    // even if we have already tried to display it in a GUI window.
    fprintf(stdout, "%s\n", aMessage);
}
