/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Douglas Turner <dougt@netscape.com>
 */

#include "nsIXPInstallProgressNotifier.h"
#include "nsSimpleConsoleProgressNotifier.h"

#include <stdlib.h>
#include <stdio.h>


nsSimpleConsoleProgressNotifier::nsSimpleConsoleProgressNotifier()
{}

nsSimpleConsoleProgressNotifier::~nsSimpleConsoleProgressNotifier()
{}

void
nsSimpleConsoleProgressNotifier::BeforeJavascriptEvaluation(void)
{
    printf("Prior to evaluation of Install Script\n");
}

void
nsSimpleConsoleProgressNotifier::AfterJavascriptEvaluation(void)
{
    printf("After evaluation of Install Script\n");
}

void
nsSimpleConsoleProgressNotifier::InstallStarted(const char* UIPackageName)
{
    printf("Installing: %s\n", UIPackageName);
}

long
nsSimpleConsoleProgressNotifier::ItemScheduled(const char* message )
{
    printf("Scheduled Item:\t%s\n", message);
    return 0;
}

void
nsSimpleConsoleProgressNotifier::InstallFinalization(const char* message, long itemNum, long totNum )
{
    printf("Item: %s\t(%ld of %ld) Installed.\n", message, itemNum, totNum );
}

void
nsSimpleConsoleProgressNotifier::InstallAborted(void)
{
    printf("Installation aborted.\n");
}


