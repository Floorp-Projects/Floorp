/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsSoftwareUpdate.h"

#include "nsInstallVersion.h"
#include "nsIDOMInstallVersion.h"

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptGlobalObject.h"

#include "pratom.h"
#include "prprf.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kIInstallVersion_IID, NS_IDOMINSTALLVERSION_IID);


nsInstallVersion::nsInstallVersion()
{
    mScriptObject   = nsnull;
    NS_INIT_REFCNT();
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
    
    if (nsnull == mScriptObject) 
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
    major   = aMajor;
    minor   = aMinor;
    release = aRelease;
    build   = aBuild;

    return NS_OK;
}

NS_IMETHODIMP    
nsInstallVersion::Init(const nsString& version)
{
    PRInt32 errorCode;
    PRInt32 aMajor, aMinor, aRelease, aBuild;

    major = minor = release = build = 0;
    
    errorCode = nsInstallVersion::StringToVersionNumbers(version, &aMajor, &aMinor, &aRelease, &aBuild);    
    if (NS_SUCCEEDED(errorCode)) 
    {
        Init(aMajor, aMinor, aRelease, aBuild);
    }
    
    return NS_OK;
}


NS_IMETHODIMP    
nsInstallVersion::GetMajor(PRInt32* aMajor)
{
    *aMajor = major;
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallVersion::SetMajor(PRInt32 aMajor)
{
    major = aMajor;
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallVersion::GetMinor(PRInt32* aMinor)
{
    *aMinor = minor;
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallVersion::SetMinor(PRInt32 aMinor)
{
    minor = aMinor;
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallVersion::GetRelease(PRInt32* aRelease)
{
    *aRelease = release;
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallVersion::SetRelease(PRInt32 aRelease)
{
    release = aRelease;
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallVersion::GetBuild(PRInt32* aBuild)
{
    *aBuild = build;
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallVersion::SetBuild(PRInt32 aBuild)
{
    build = aBuild;
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
nsInstallVersion::CompareTo(const nsString& aAString, PRInt32* aReturn)
{
    return NS_OK;
}

NS_IMETHODIMP    
nsInstallVersion::CompareTo(PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn)
{
    int diff;
    
    if ( major == aMajor ) 
    {
        if ( minor == aMinor ) 
        {
            if ( release == aRelease ) 
            {
                if ( build == aBuild )
                    diff = EQUAL;
                else if ( build > aBuild )
                    diff = BLD_DIFF;
                else
                    diff = BLD_DIFF_MINUS;
            }
            else if ( release > aRelease )
                diff = REL_DIFF;
            else
                diff = REL_DIFF_MINUS;
        }
        else if (  minor > aMinor )
            diff = MINOR_DIFF;
        else
            diff = MINOR_DIFF_MINUS;
    }
    else if ( major > aMajor )
        diff = MAJOR_DIFF;
    else
        diff = MAJOR_DIFF_MINUS;

    *aReturn = diff;

    return NS_OK;
}


NS_IMETHODIMP    
nsInstallVersion::ToString(nsString& aReturn)
{
    char *result=NULL;
    result = PR_sprintf_append(result, "%d.%d.%d.%d", major, minor, release, build);
    
    aReturn = result;

    return NS_OK;
}


nsresult
nsInstallVersion::StringToVersionNumbers(const nsString& version, PRInt32 *aMajor, PRInt32 *aMinor, PRInt32 *aRelease, PRInt32 *aBuild)    
{
    PRInt32 errorCode;

    int dot = version.Find('.', 0);
    
    if ( dot == -1 ) 
    {
        *aMajor = version.ToInteger(&errorCode);
    }
    else  
    {
        nsString majorStr;
        version.Mid(majorStr, 0, dot);
        *aMajor  = majorStr.ToInteger(&errorCode);

        int prev = dot+1;
        dot = version.Find('.',prev);
        if ( dot == -1 ) 
        {
            nsString minorStr;
            version.Mid(minorStr, prev, version.Length() - prev);
            *aMinor = minorStr.ToInteger(&errorCode);
        }
        else 
        {
            nsString minorStr;
            version.Mid(minorStr, prev, dot - prev);
            *aMinor = minorStr.ToInteger(&errorCode);

            prev = dot+1;
            dot = version.Find('.',prev);
            if ( dot == -1 ) 
            {
                nsString releaseStr;
                version.Mid(releaseStr, prev, version.Length() - prev);
                *aRelease = releaseStr.ToInteger(&errorCode);
            }
            else 
            {
                nsString releaseStr;
                version.Mid(releaseStr, prev, dot - prev);
                *aRelease = releaseStr.ToInteger(&errorCode);
    
                prev = dot+1;
                if ( version.Length() > dot ) 
                {
                    nsString buildStr;
                    version.Mid(buildStr, prev, version.Length() - prev);
                    *aBuild = buildStr.ToInteger(&errorCode);
               }
            }
        }
    }

    return errorCode;
}

/////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////
static PRInt32 gInstallVersionInstanceCnt = 0;
static PRInt32 gInstallVersionLock        = 0;

nsInstallVersionFactory::nsInstallVersionFactory(void)
{
    mRefCnt=0;
    PR_AtomicIncrement(&gInstallVersionInstanceCnt);
}

nsInstallVersionFactory::~nsInstallVersionFactory(void)
{
    PR_AtomicDecrement(&gInstallVersionInstanceCnt);
}

NS_IMETHODIMP 
nsInstallVersionFactory::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = (void*) this;
    }
    else if ( aIID.Equals(kIFactoryIID) )
    {
        *aInstancePtr = (void*) this;
    }

    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NO_INTERFACE;
    }

    AddRef();
    return NS_OK;
}



NS_IMETHODIMP
nsInstallVersionFactory::AddRef(void)
{
    return ++mRefCnt;
}


NS_IMETHODIMP
nsInstallVersionFactory::Release(void)
{
    if (--mRefCnt ==0)
    {
        delete this;
        return 0; // Don't access mRefCnt after deleting!
    }

    return mRefCnt;
}

NS_IMETHODIMP
nsInstallVersionFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aResult == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aResult = NULL;

    /* do I have to use iSupports? */
    nsInstallVersion *inst = new nsInstallVersion();

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult result =  inst->QueryInterface(aIID, aResult);

    if (NS_FAILED(result)) 
        delete inst;

    return result;
}

NS_IMETHODIMP
nsInstallVersionFactory::LockFactory(PRBool aLock)
{
    if (aLock)
        PR_AtomicIncrement(&gInstallVersionLock);
    else
        PR_AtomicDecrement(&gInstallVersionLock);

    return NS_OK;
}