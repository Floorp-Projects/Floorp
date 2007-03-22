/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Douglas Turner <dougt@netscape.com>
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

#include "nsSimpleNotifier.h"

#include <stdlib.h>
#include <stdio.h>


nsSimpleNotifier::nsSimpleNotifier()
{}

nsSimpleNotifier::~nsSimpleNotifier()
{}

NS_IMETHODIMP
nsSimpleNotifier::BeforeJavascriptEvaluation(const PRUnichar *URL)
{
    printf("Prior to evaluation of Install Script\n");
}

NS_IMETHODIMP
nsSimpleNotifier::AfterJavascriptEvaluation(const PRUnichar *URL)
{
    printf("After evaluation of Install Script\n");
}

NS_IMETHODIMP
nsSimpleNotifier::InstallStarted(const PRUnichar *URL, const char* UIPackageName)
{
    printf("Installing: %s\n", UIPackageName);
}

NS_IMETHODIMP
nsSimpleNotifier::ItemScheduled(const PRUnichar* message )
{
    printf("Scheduled Item:\t%s\n", message);
    return 0;
}

NS_IMETHODIMP
nsSimpleNotifier::FinalizeProgress(const PRUnichar* message, long itemNum, long totNum )
{
    printf("Item: %s\t(%ld of %ld) Installed.\n", message, itemNum, totNum );
}

NS_IMETHODIMP
nsSimpleNotifier::FinalStatus(const PRUnichar *URL, PRInt32 status)
{
    printf("Installation aborted.\n");
}

NS_IMETHODIMP
nsSimpleNotifier::LogComment(const PRUnichar *message)
{
    printf("**NOTE: %s\n",message);
}

NS_IMETHOD
nsSimpleNotifier::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    printf("Query Interface.\n");
}

NS_IMETHOD_(nsrefcnt)
nsSimpleNotifier::AddRef(void)
{
    printf("Add Reference.\n");
}

NS_IMETHOD_(nsrefcnt)
nsSimpleNotifier::Release(void)
{
    printf("Release.\n");
}
