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
 *   Daniel Veditz <dveditz@netscape.com>
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


#include "nsCRT.h"
#include "prmem.h"
#include "prprf.h"
#include "VerReg.h"
#include "ScheduledTasks.h"
#include "nsInstallLogComment.h"
#include "nsInstallResources.h"

#include "nsInstall.h"
#include "nsIDOMInstallVersion.h"
#include "nsNativeCharsetUtils.h"

nsInstallLogComment::nsInstallLogComment( nsInstall* inInstall,
                                          const nsAString& inFileOpCommand,
                                          const nsAString& inComment,
                                          PRInt32 *error)

: nsInstallObject(inInstall)
{
    MOZ_COUNT_CTOR(nsInstallLogComment);

    *error = nsInstall::SUCCESS;
    if (inInstall == NULL) 
    {
        *error = nsInstall::INVALID_ARGUMENTS;
        return;
    }
    
    mFileOpCommand = inFileOpCommand;
    mComment       = inComment;
}


nsInstallLogComment::~nsInstallLogComment()
{
    MOZ_COUNT_DTOR(nsInstallLogComment);
}


PRInt32 nsInstallLogComment::Prepare()
{
    // no set-up necessary
    return nsInstall::SUCCESS;
}

PRInt32 nsInstallLogComment::Complete()
{
    // nothing to complete
    return nsInstall::SUCCESS;
}

void nsInstallLogComment::Abort()
{
}

char* nsInstallLogComment::toString()
{
    if (!mInstall)
        return nsnull;

    char* buffer = new char[1024];
    char* rsrcVal = nsnull;
    
    if (!buffer)
        return nsnull;

    rsrcVal = mInstall->GetResourcedString(mFileOpCommand);
    if (rsrcVal)
    {
        nsCAutoString comment;
        if ( NS_SUCCEEDED( NS_CopyUnicodeToNative(mComment, comment) ) )
          PR_snprintf(buffer, 1024, rsrcVal, comment.get());
        nsCRT::free(rsrcVal);
    }

    return buffer;
}


PRBool
nsInstallLogComment::CanUninstall()
{
    return PR_FALSE;
}

PRBool
nsInstallLogComment::RegisterPackageNode()
{
    return PR_FALSE;
}

