/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
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

#ifndef _dirpicker_h
#define _dirpicker_h

#ifdef __cplusplus
extern "C" {
#endif

/* This is an interface to a 'choose directory' dialog which uses the FSTree    */
/* code as a back-end.                                                          */
/*                                                                              */
/* It could be expanded to give a 'choose file' dialog along the same lines as  */
/* the one from win32 very easily -- add another container for files, a         */
/* vertical (draggable) split bar to separate the two, add something for file   */
/* types, a little WPS integration and there you go.                            */

typedef struct _DIRPICKER
{
   CHAR szFullFile[CCHMAXPATH];   /* directory picked; may be set on init       */
   LONG lReturn;                  /* button pressed, DID_CANCEL or DID_OK       */
   BOOL bModal;                   /* should the dialog be shown modally         */
   PSZ  pszTitle;                 /* title for the dialog                       */
} DIRPICKER, *PDIRPICKER;

/* return: if bModal then return hwnd of dialog or 0 on error                   */
/*         else TRUE if successful, FALSE on error.                             */

HWND APIENTRY FS_PickDirectory( HWND       hwndParent,     /* parent for dialog */
                                HWND       hwndOwner,      /* owner for dialog  */
                                HMODULE    hModResources,  /* resource module   */
                                PDIRPICKER pDirPicker);    /* running data      */

#ifdef __cplusplus
}
#endif

#endif
