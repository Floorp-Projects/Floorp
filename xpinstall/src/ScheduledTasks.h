/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 *     Douglas Turner <dougt@netscape.com>
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef __SCHEDULEDTASKS_H__
#define __SCHEDULEDTASKS_H__


#include "NSReg.h"
#include "nsIFile.h"

PR_BEGIN_EXTERN_C

PRInt32 DeleteFileNowOrSchedule(nsIFile* filename);
PRInt32 ReplaceFileNowOrSchedule(nsIFile* tmpfile, nsIFile* target, PRInt32 aMode);
PRInt32 ScheduleFileForDeletion(nsIFile* filename);
char*   GetRegFilePath();


void PerformScheduledTasks(HREG reg);

PR_END_EXTERN_C

#endif
