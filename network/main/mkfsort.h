/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef MKFSORT_H
#define MKFSORT_H

#include "mksort.h"

#include <time.h>

#ifdef XP_UNIX
#include <sys/types.h>
#endif /* XP_UNIX */

extern void NET_FreeEntryInfoStruct(NET_FileEntryInfo *entry_info);
extern NET_FileEntryInfo * NET_CreateFileEntryInfoStruct (void);
extern int NET_CompareEntryInfoStructs (void *ent1, void *ent2);
extern void NET_DoFileSort (SortStruct * sort_list);

extern int
NET_PrintDirectory(SortStruct **sort_base, NET_StreamClass * stream, char * path, URL_Struct *URL_s);

#endif /* MKFSORT_H */
