/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _tabapi_h
#define _tabapi_h

#ifdef __cplusplus
extern "C" {
#endif

/* Call to get the class name; will register the class on first call       */
PSZ TabGetTabControlName(void);

/* messages that can be sent to the tab control */
#define TABM_BASE (WM_USER + 50)

#define TABC_END     ((USHORT)-1)
#define TABC_FAILURE ((USHORT)-1)

/* TABM_INSERTMULTTABS - append several tabs to the control                */
/*                                                                         */
/* param1  USHORT  usTabs,     number of tabs to insert.                   */
/* param2  PSZ   *apszLabels, array of tab labels                          */
/*                                                                         */
/* returns BOOL bSuccess                                                   */
#define TABM_INSERTMULTTABS (TABM_BASE + 0)

/* TABM_INSERTTAB - add a new tab to the control. If this is the first     */
/*                  tab in the control, it will be the hilighted tab.      */
/*                                                                         */
/* param1  USHORT usIndex, index to insert.  May be TABC_END.              */
/* param2  PSZ    pszStr,  label for new tab                               */
/*                                                                         */
/* returns USHORT usIndex, index of new tab or TABC_FAILURE                */
#define TABM_INSERTTAB (TABM_BASE + 1)

/* TABM_QUERYHILITTAB - get the index of the hilighted tab                 */
/*                                                                         */
/* param1  ULONG  ulReserved, reserved                                     */
/* param2  ULONG  ulReserved, reserved                                     */
/*                                                                         */
/* returns USHORT usIndex, index of active tab or TABC_FAILURE             */
#define TABM_QUERYHILITTAB (TABM_BASE + 2)

/* TABM_QUERYTABCOUNT - how many tabs are there?                           */
/*                                                                         */
/* param1  ULONG  ulReserved, reserved                                     */
/* param2  ULONG  ulReserved, reserved                                     */
/*                                                                         */
/* returns USHORT usTabs, number of tabs                                   */
#define TABM_QUERYTABCOUNT (TABM_BASE + 3)

/* TABM_QUERYTABTEXT - get a tab's label                                   */
/*                                                                         */
/* param1  USHORT usIndex,   index of tab to query text for                */
/*         USHORT usLength,  length of buffer                              */
/* param2  PSZ    pszBuffer, buffer to get text into                       */
/*                                                                         */
/* returns USHORT usChars, chars not copied or TABC_FAILURE                */
#define TABM_QUERYTABTEXT (TABM_BASE + 4)

/* TABM_QUERYTABTEXTLENGTH - get the length of a tab's label               */
/*                                                                         */
/* param1  USHORT usIndex,    index of tab to query text length for        */
/* param2  ULONG  ulReserved, reserved                                     */
/*                                                                         */
/* returns ULONG ulLength, length of text not including null terminator    */
#define TABM_QUERYTABTEXTLENGTH (TABM_BASE + 5)

/* TABM_REMOVETAB - remove a tab from the control                          */
/*                                                                         */
/* param1  USHORT usIndex,    index of tab to remove                       */
/* param2  ULONG  ulReserved, reserved                                     */
/*                                                                         */
/* returns BOOL bSuccess                                                   */
#define TABM_REMOVETAB (TABM_BASE + 6)

/* TABM_SETHILITTAB - set the index of the hilighted tab                   */
/*                                                                         */
/* param1  USHORT usIndex, index of requested tab                          */
/* param2  ULONG  ulReserved, reserved                                     */
/*                                                                         */
/* returns BOOL bSuccess                                                   */
#define TABM_SETHILITTAB (TABM_BASE + 7)

/* TABM_SETCAPTION - Change the caption of an existing tab                 */
/*                                                                         */
/* param1  USHORT usIndex, index to change.                                */
/* param2  PSZ    pszStr,  label for new tab                               */
/*                                                                         */
/* returns TRUE or TABC_FAILURE                                            */
#define TABM_SETCAPTION (TABM_BASE + 8)

#define TABM_LASTPUBLIC TABM_SETCAPTION

/* WM_CONTROL id sent to owner; mp2 is the index of the newly selected tab */
#define TTN_TABCHANGE 1

#ifdef __cplusplus
}
#endif

#endif
