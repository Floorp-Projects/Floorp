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

#include "nsIXPINotifier.h"
#include "nsSimpleNotifier.h"

#include <stdlib.h>
#include <stdio.h>


nsSimpleNotifier::nsSimpleNotifier()
{}

nsSimpleNotifier::~nsSimpleNotifier()
{}

NS_IMETHODIMP
nsSimpleNotifier::BeforeJavascriptEvaluation(void)
{
    printf("Prior to evaluation of Install Script\n");
}

NS_IMETHODIMP
nsSimpleNotifier::AfterJavascriptEvaluation(void)
{
    printf("After evaluation of Install Script\n");
}

NS_IMETHODIMP
nsSimpleNotifier::InstallStarted(const char* UIPackageName)
{
    printf("Installing: %s\n", UIPackageName);
}

NS_IMETHODIMP
nsSimpleNotifier::ItemScheduled(const char* message )
{
    printf("Scheduled Item:\t%s\n", message);
    return 0;
}

NS_IMETHODIMP
nsSimpleNotifier::InstallFinalization(const char* message, long itemNum, long totNum )
{
    printf("Item: %s\t(%ld of %ld) Installed.\n", message, itemNum, totNum );
}

NS_IMETHODIMP
nsSimpleNotifier::InstallAborted(void)
{
    printf("Installation aborted.\n");
}


