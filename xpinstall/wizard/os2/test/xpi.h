/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *     Sean Su <ssu@netscape.com>
 *     IBM Corp.
 */
#include <os2.h>

#ifndef _XPI_H_
#define _XPI_H_

APIRET         InitializeXPIStub(void);
APIRET         DeInitializeXPIStub(void);
APIRET         SmartUpdateJars(PSZ szAppName, PSZ szAppPath, PSZ listArchive[]);
void            cbXPIStart(const char *, const char *UIName);
void            cbXPIProgress(const char* msg, PRInt32 val, PRInt32 max);
void            cbXPIFinal(const char *, PRInt32 finalStatus);
void            InitProgressDlg(void);
void            DeInitProgressDlg(void);
int             GetTotalArchivesToInstall(PSZ listArchive[]);
void            ProcessWindowsMessages();

#endif /* _XPI_H_ */

