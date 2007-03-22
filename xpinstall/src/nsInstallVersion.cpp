/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsSoftwareUpdate.h"

#include "nsInstall.h"
#include "nsInstallVersion.h"
#include "nsIDOMInstallVersion.h"

#include "nscore.h"
#include "nsString.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptGlobalObject.h"

#include "prprf.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kIInstallVersion_IID, NS_IDOMINSTALLVERSION_IID);


nsInstallVersion::nsInstallVersion()
{
    mScriptObject   = nsnull;
}

nsInstallVersion::~nsInstallVersion()
{
}

NS_IMETHODIMP
nsInstallVersion::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kIScriptObjectOwnerIID))
    {
        *aInstancePtr = (void*) ((nsIScriptObjectOwner*)this);
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kIInstallVersion_IID) )
    {
        *aInstancePtr = (void*) ((nsIDOMInstallVersion*)this);
        AddRef();
        return NS_OK;
    }
    else if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
        AddRef();
        return NS_OK;
    }

     return NS_NOINTERFACE;
}


NS_IMPL_ADDREF(nsInstallVersion)
NS_IMPL_RELEASE(nsInstallVersion)



NS_IMETHODIMP
nsInstallVersion::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;

    if (!mScriptObject)
    {
        res = NS_NewScriptInstallVersion(aContext,
                                         (nsISupports *)(nsIDOMInstallVersion*)this,
                                         nsnull,
                                         &mScriptObject);
    }


    *aScriptObject = mScriptObject;
    return res;
}

NS_IMETHODIMP
nsInstallVersion::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

//  this will go away when our constructors can have parameters.
NS_IMETHODIMP
nsInstallVersion::Init(PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild)
{
    mMajor   = aMajor;
    mMinor   = aMinor;
    mRelease = aRelease;
    mBuild   = aBuild;

    return NS_OK;
}

NS_IMETHODIMP
nsInstallVersion::Init(const nsString& version)
{
    mMajor = mMinor = mRelease = mBuild = 0;

    // nsString is a flat string
    if (PR_sscanf(NS_ConvertUTF16toUTF8(version).get(),"%d.%d.%d.%d",&mMajor,&mMinor,&mRelease,&mBuild) < 1)
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}


NS_IMETHODIMP
nsInstallVersion::GetMajor(PRInt32* aMajor)
{
    *aMajor = mMajor;
    return NS_OK;
}

NS_IMETHODIMP
nsInstallVersion::SetMajor(PRInt32 aMajor)
{
    mMajor = aMajor;
    return NS_OK;
}

NS_IMETHODIMP
nsInstallVersion::GetMinor(PRInt32* aMinor)
{
    *aMinor = mMinor;
    return NS_OK;
}

NS_IMETHODIMP
nsInstallVersion::SetMinor(PRInt32 aMinor)
{
    mMinor = aMinor;
    return NS_OK;
}

NS_IMETHODIMP
nsInstallVersion::GetRelease(PRInt32* aRelease)
{
    *aRelease = mRelease;
    return NS_OK;
}

NS_IMETHODIMP
nsInstallVersion::SetRelease(PRInt32 aRelease)
{
    mRelease = aRelease;
    return NS_OK;
}

NS_IMETHODIMP
nsInstallVersion::GetBuild(PRInt32* aBuild)
{
    *aBuild = mBuild;
    return NS_OK;
}

NS_IMETHODIMP
nsInstallVersion::SetBuild(PRInt32 aBuild)
{
    mBuild = aBuild;
    return NS_OK;
}


NS_IMETHODIMP
nsInstallVersion::CompareTo(nsIDOMInstallVersion* aVersion, PRInt32* aReturn)
{
    PRInt32 aMajor, aMinor, aRelease, aBuild;

    aVersion->GetMajor(&aMajor);
    aVersion->GetMinor(&aMinor);
    aVersion->GetRelease(&aRelease);
    aVersion->GetBuild(&aBuild);

    CompareTo(aMajor, aMinor, aRelease, aBuild, aReturn);

    return NS_OK;
}


NS_IMETHODIMP
nsInstallVersion::CompareTo(const nsString& aString, PRInt32* aReturn)
{
    nsInstallVersion inVersion;
    inVersion.Init(aString);

    return CompareTo(&inVersion, aReturn);
}

NS_IMETHODIMP
nsInstallVersion::CompareTo(PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn)
{
    int diff;

    if ( mMajor == aMajor )
    {
        if ( mMinor == aMinor )
        {
            if ( mRelease == aRelease )
            {
                if ( mBuild == aBuild )
                    diff = EQUAL;
                else if ( mBuild > aBuild )
                    diff = BLD_DIFF;
                else
                    diff = BLD_DIFF_MINUS;
            }
            else if ( mRelease > aRelease )
                diff = REL_DIFF;
            else
                diff = REL_DIFF_MINUS;
        }
        else if (  mMinor > aMinor )
            diff = MINOR_DIFF;
        else
            diff = MINOR_DIFF_MINUS;
    }
    else if ( mMajor > aMajor )
        diff = MAJOR_DIFF;
    else
        diff = MAJOR_DIFF_MINUS;

    *aReturn = diff;

    return NS_OK;
}

NS_IMETHODIMP
nsInstallVersion::ToString(nsString& aReturn)
{
    char buf[128];
    PRUint32 len;

    len=PR_snprintf(buf, sizeof(buf), "%d.%d.%d.%d", mMajor, mMinor, mRelease, mBuild);
    aReturn.AssignASCII(buf,len);

    return NS_OK;
}
