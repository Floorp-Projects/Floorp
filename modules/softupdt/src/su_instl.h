/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
/* su_instl.h
 * 
 * created by atotic, 1/22/97
 */

#include "xp_mcom.h"

#define SU_REBOOT_NEEDED 999
#define SU_SUCCESS    0

XP_BEGIN_PROTOS

/* Immediately executes the file 
 * returns errors if encountered
 */
int FE_ExecuteFile( const char * filename, const char * cmdline );
#ifdef XP_PC
void    PR_CALLBACK SU_AlertCallback(void *dummy);
void FE_ScheduleRenameUtility(void);
#endif

#ifdef XP_UNIX
int FE_CopyFile (const char *in, const char *out);
#endif

int32 su_DeleteOldFileLater(char * filename);
int32 su_ReplaceOldFileLater(char *tmpfile, char *target );


XP_END_PROTOS

