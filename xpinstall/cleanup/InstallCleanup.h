/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Navigator.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape Communications Corp. are
 * Copyright (C) 1998, 1999, 2000, 2001 Netscape Communications Corp.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Don Bragg <dbragg@netscape.com>
 */
#ifndef INSTALL_CLEANUP_H
#define INSTALL_CLEANUP_H

#include <stdlib.h>
#include <stdio.h>

#include "prtypes.h"
#include "VerReg.h"

#define DONE 0
#define TRY_LATER -1

int PerformScheduledTasks(HREG);
int DeleteScheduledFiles(HREG);
int ReplaceScheduledFiles(HREG);
int NativeReplaceFile(const char* replacementFile, const char* doomedFile );
int NativeDeleteFile(const char* aFileToDelete);

#endif //INSTALL_CLEANUP_H

