/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Samir Gehani <sgehani@netscape.com>
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

#include "nsLegacyCheck.h"
#include "assert.h"

nsLegacyCheck::nsLegacyCheck() :
    mFilename(NULL),
    mMessage(NULL),
    mNext(NULL)
{
}

nsLegacyCheck::~nsLegacyCheck()
{
#ifdef DEBUG
    if (mFilename)
        printf("%s %d: Freeing %s\n", __FILE__, __LINE__, mFilename);
#endif
    XI_IF_FREE(mFilename);
    XI_IF_FREE(mMessage);
}

int
nsLegacyCheck::SetFilename(char *aFilename)
{
    if (!aFilename)
        return E_PARAM;
    
    mFilename = aFilename;

    return OK;
}

char *
nsLegacyCheck::GetFilename()
{
    return mFilename;
}

int
nsLegacyCheck::SetMessage(char *aMessage)
{
    if (!aMessage)
        return E_PARAM;
    
    mMessage = aMessage;

    return OK;
}

char *
nsLegacyCheck::GetMessage()
{
    return mMessage;
}

int
nsLegacyCheck::SetNext(nsLegacyCheck *aNext)
{
    if (!aNext)
        return E_PARAM;

    mNext = aNext;

    return OK;
}

nsLegacyCheck *
nsLegacyCheck::GetNext()
{
    return mNext;
}

int
nsLegacyCheck::InitNext()
{
    XI_ASSERT((mNext==NULL), "Leaking nsLegacyCheck::mNext!\n");
    mNext = NULL;

    return OK;
}
