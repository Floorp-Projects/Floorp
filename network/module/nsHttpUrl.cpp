/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif
#include "nsHttpUrl.h"
#include "nsIProtocolConnection.h"
#include "nsIURLGroup.h"
#include "nsString.h"

#include "nsINetService.h"  /* XXX: NS_FALSE */
#include "nsNetStream.h"

#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsStubContext.h"

static NS_DEFINE_IID(kIOutputStreamIID,  NS_IOUTPUTSTREAM_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIHttpURLIID, NS_IHTTPURL_IID);
static NS_DEFINE_IID(kIURLIID, NS_IURL_IID);
static NS_DEFINE_IID(kIPostToServerIID, NS_IPOSTTOSERVER_IID);
static NS_DEFINE_IID(kIProtocolConnectionIID, NS_IPROTOCOLCONNECTION_IID);

nsHttpUrlImpl::nsHttpUrlImpl(nsISupports* aContainer, nsIURLGroup* aGroup)
{
    NS_INIT_REFCNT();
 
    m_PostType = Send_None;
    m_PostBuffer = nsnull;
    m_PostBufferLength = 0;
    m_URL_s = nsnull;
 
    mProtocol = nsnull;
    mHost = nsnull;
    mFile = nsnull;
    mRef = nsnull;
    mPort = -1;
    mSpec = nsnull;
    mSearch = nsnull;
    mPostData = nsnull;
    mContainer = nsnull;
    mLoadAttribs = nsnull;
    mURLGroup = aGroup;
  
    NS_NewLoadAttribs(&mLoadAttribs);

    NS_IF_ADDREF(mURLGroup);

    mContainer = aContainer;
    NS_IF_ADDREF(mContainer);
    //  ParseURL(aSpec, aURL);      // XXX whh
}
 
nsHttpUrlImpl::~nsHttpUrlImpl()
{
    NS_IF_RELEASE(mContainer);
    NS_IF_RELEASE(mLoadAttribs);
    NS_IF_RELEASE(mURLGroup);
    NS_IF_RELEASE(mPostData);

    PR_FREEIF(mSpec);
    PR_FREEIF(mProtocol);
    PR_FREEIF(mHost);
    PR_FREEIF(mFile);
    PR_FREEIF(mRef);
    PR_FREEIF(mSearch);
    PR_FREEIF(m_PostBuffer);
    if (nsnull != m_URL_s) {
        NET_DropURLStruct(m_URL_s);
    }
}
  
NS_IMPL_THREADSAFE_ADDREF(nsHttpUrlImpl);
NS_IMPL_THREADSAFE_RELEASE(nsHttpUrlImpl);

nsresult nsHttpUrlImpl::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
 
    static NS_DEFINE_IID(kINetlibURLIID, NS_INETLIBURL_IID);
    static NS_DEFINE_IID(kIPostToServerIID, NS_IPOSTTOSERVER_IID);
    if (aIID.Equals(kIHttpURLIID) || aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsIHttpURL*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIURLIID)) {
        *aInstancePtr = (void*) ((nsIURL*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kINetlibURLIID)) {
        *aInstancePtr = (void*) ((nsINetlibURL*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIPostToServerIID)) {
        *aInstancePtr = (void*) ((nsIPostToServer*)this);
        AddRef();
        return NS_OK;
    }

#if defined(NS_DEBUG)
    /*
     * Check for the debug-only interface indicating thread-safety
     */
    static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);
    if (aIID.Equals(kIsThreadsafeIID)) {
        return NS_OK;
    }
#endif
 
    return NS_NOINTERFACE;
}


NS_METHOD nsHttpUrlImpl::SetURLInfo(URL_Struct *URL_s)
{
    nsresult result = NS_OK;
  
    /* Hook us up with the world. */
    m_URL_s = URL_s;
    NET_HoldURLStruct(URL_s);

    if (Send_None != m_PostType) {
        /* Free any existing POST data hanging off the URL_Struct */
        if (nsnull != URL_s->post_data) {
            PR_Free(URL_s->post_data);
        }
  
        /*
         * Store the new POST data into the URL_Struct
         */
        URL_s->post_data         = m_PostBuffer;
        URL_s->post_data_size    = m_PostBufferLength;
        URL_s->post_data_is_file = (Send_Data == m_PostType) ? FALSE : TRUE;
        /*
         * Is the request a POST or PUT
         */
        URL_s->method = (Send_File == m_PostType) ? URL_PUT_METHOD : URL_POST_METHOD;
  
        /* Reset the local post state... */
        m_PostType = Send_None;
        m_PostBuffer = nsnull;  /* The URL_Struct owns the memory now... */
        m_PostBufferLength = 0;
    }
  
    return result;
}
  
NS_METHOD nsHttpUrlImpl::GetURLInfo(URL_Struct_** aResult) const
{
  nsresult rv;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    /* XXX: Should the URL be reference counted here?? */
    *aResult = m_URL_s;
    rv = NS_OK;
  }

  return rv;
}

NS_METHOD  nsHttpUrlImpl::SendFile(const char *aFile)
{
    nsresult result;

    result = PostFile(aFile);
    if (NS_OK == result) {
        m_PostType = Send_File;
    }
    return result;
}


NS_METHOD  nsHttpUrlImpl::SendDataFromFile(const char *aFile)
{
    nsresult result;

    result = PostFile(aFile);
    if (NS_OK == result) {
        m_PostType = Send_DataFromFile;
    }
    return result;
}


NS_METHOD  nsHttpUrlImpl::SendData(const char *aBuffer, PRUint32 aLength)
{
    nsresult result = NS_OK;

    /* Deal with error conditions... */
    if (nsnull == aBuffer) {
        result = NS_ERROR_ILLEGAL_VALUE;
        goto done;
    } 
    else if (Send_None != m_PostType) {
        result = NS_IPOSTTOSERVER_ALREADY_POSTING;
        goto done;
    }

    /* Copy the post data... */
    m_PostBuffer = (char *)PR_Malloc(aLength+1);
    if (nsnull == m_PostBuffer) {
        result = NS_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    memcpy(m_PostBuffer, aBuffer, aLength);
    m_PostBuffer[aLength] = '\0';
    m_PostBufferLength = aLength;
    m_PostType = Send_Data;

done:
    return result;
}

NS_METHOD nsHttpUrlImpl::AddMimeHeader(const char *name, const char *value)
{
    MWContext *stubContext=NULL;
    char *aName=NULL;
    char *aVal=NULL;
    PRBool addColon=TRUE;
    PRInt32 len=0;

    NS_PRECONDITION((name != nsnull) && ((*name) != nsnull), "Bad name");
    NS_PRECONDITION((value != nsnull) && ((*value) != nsnull), "Bad value");
    
    if(!name
       || !*name
       || !value
       || !*value)
        return NS_FALSE;

    // Make sure we've got a colon on the end of the header name
    if(PL_strchr(name, ':'))
        addColon=FALSE;

    /* Bring in our own copies of the data. */
    if(addColon)
        aName = (char*)PR_Malloc(PL_strlen(name)+2); // add extra byte for colon
    else
        aName = (char*)PR_Malloc(PL_strlen(name)+1);
    if(!aName)
        return NS_ERROR_OUT_OF_MEMORY;
    aVal  = (char*)PR_Malloc(PL_strlen(value)+1);
    if(!aVal)
        return NS_ERROR_OUT_OF_MEMORY;

    PL_strcpy(aName, name);
    if(addColon) {
        PL_strcat(aName, ":");
    }
        
    PL_strcpy(aVal, value);

    stubContext = new_stub_context(m_URL_s);

    /* Make the real call to NET_ParseMimeHeader, passing in the dummy bam context. */


    NET_ParseMimeHeader(FO_CACHE_AND_NGLAYOUT,
				stubContext, 
				m_URL_s, 
				aName, 
				aVal,
				TRUE);
    PR_Free(aName);
    PR_Free(aVal);
    return NS_OK;
}

nsresult nsHttpUrlImpl::PostFile(const char *aFile)
{
    nsresult result = NS_OK;

    /* Deal with error conditions... */
    if (nsnull == aFile) {
        result = NS_ERROR_ILLEGAL_VALUE;
        goto done;
    } 
    else if (Send_None != m_PostType) {
        result = NS_IPOSTTOSERVER_ALREADY_POSTING;
        goto done;
    }

    /* Copy the post data... */
    m_PostBuffer = PL_strdup(aFile);

    if (nsnull == m_PostBuffer) {
        result = NS_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    m_PostBufferLength = PL_strlen(aFile);

done:
    return result;
}

////////////////////////////////////////////////////////////////////////////////

// XXX recode to use nsString api's

// XXX don't bother with port numbers
// XXX don't bother with ref's
// XXX null pointer checks are incomplete
nsresult nsHttpUrlImpl::ParseURL(const nsString& aSpec, const nsIURL* aURL)
{
    // XXX hack!
    char* cSpec = aSpec.ToNewCString();

    const char* uProtocol = nsnull;
    const char* uHost = nsnull;
    const char* uFile = nsnull;
    PRUint32 uPort;
    if (nsnull != aURL) {
        nsresult rslt = aURL->GetProtocol(&uProtocol);
        if (rslt != NS_OK) return rslt;
        rslt = aURL->GetHost(&uHost);
        if (rslt != NS_OK) return rslt;
        rslt = aURL->GetFile(&uFile);
        if (rslt != NS_OK) return rslt;
        rslt = aURL->GetHostPort(&uPort);
        if (rslt != NS_OK) return rslt;
    }

    NS_LOCK_INSTANCE();

    PR_FREEIF(mProtocol);
    PR_FREEIF(mHost);
    PR_FREEIF(mFile);
    PR_FREEIF(mRef);
    PR_FREEIF(mSearch);
    mPort = -1;

    if (nsnull == cSpec) {
        if (nsnull == aURL) {
            NS_UNLOCK_INSTANCE();
            return NS_ERROR_ILLEGAL_VALUE;
        }
        mProtocol = (nsnull != uProtocol) ? PL_strdup(uProtocol) : nsnull;
        mHost = (nsnull != uHost) ? PL_strdup(uHost) : nsnull;
        mPort = uPort;
        mFile = (nsnull != uFile) ? PL_strdup(uFile) : nsnull;

        NS_UNLOCK_INSTANCE();
        return NS_OK;
    }

    // Strip out reference and search info
    char* ref = strpbrk(cSpec, "#?");
    if (nsnull != ref) {
        char* search = nsnull;
        if ('#' == *ref) {
            search = PL_strchr(ref + 1, '?');
            if (nsnull != search) {
                *search++ = '\0';
            }

            PRIntn hashLen = PL_strlen(ref + 1);
            if (0 != hashLen) {
                mRef = (char*) PR_Malloc(hashLen + 1);
                PL_strcpy(mRef, ref + 1);
            }      
        }
        else {
            search = ref + 1;
        }

        if (nsnull != search) {
            // The rest is the search
            PRIntn searchLen = PL_strlen(search);
            if (0 != searchLen) {
                mSearch = (char*) PR_Malloc(searchLen + 1);
                PL_strcpy(mSearch, search);
            }      
        }

        // XXX Terminate string at start of reference or search
        *ref = '\0';
    }

    // The URL is considered absolute if and only if it begins with a
    // protocol spec. A protocol spec is an alphanumeric string of 1 or
    // more characters that is terminated with a colon.
    PRBool isAbsolute = PR_FALSE;
    char* cp;
    char* ap = cSpec;
    char ch;
    while (0 != (ch = *ap)) {
        if (((ch >= 'a') && (ch <= 'z')) ||
            ((ch >= 'A') && (ch <= 'Z')) ||
            ((ch >= '0') && (ch <= '9'))) {
            ap++;
            continue;
        }
        if ((ch == ':') && (ap - cSpec >= 2)) {
            isAbsolute = PR_TRUE;
            cp = ap;
            break;
        }
        break;
    }

    if (!isAbsolute) {
        // relative spec
        if (nsnull == aURL) {
            delete[] cSpec;

            NS_UNLOCK_INSTANCE();
            return NS_ERROR_ILLEGAL_VALUE;
        }

        // keep protocol and host
        mProtocol = (nsnull != uProtocol) ? PL_strdup(uProtocol) : nsnull;
        mHost = (nsnull != uHost) ? PL_strdup(uHost) : nsnull;
        mPort = uPort;

        // figure out file name
        PRInt32 len = PL_strlen(cSpec) + 1;
        if ((len > 1) && (cSpec[0] == '/')) {
            // Relative spec is absolute to the server
            mFile = PL_strdup(cSpec);
        } else {
            if (cSpec[0] != '\0') {
                // Strip out old tail component and put in the new one
                char* dp = PL_strrchr(uFile, '/');
                if (!dp) {
                    delete[] cSpec;
                    NS_UNLOCK_INSTANCE();
                    return NS_ERROR_ILLEGAL_VALUE;
                }
                PRInt32 dirlen = (dp + 1) - uFile;
                mFile = (char*) PR_Malloc(dirlen + len);
                PL_strncpy(mFile, uFile, dirlen);
                PL_strcpy(mFile + dirlen, cSpec);
            }
            else {
                mFile = PL_strdup(uFile);
            }
        }

        /* Stolen from netlib's mkparse.c.
         *
         * modifies a url of the form   /foo/../foo1  ->  /foo1
         *                       and    /foo/./foo1   ->  /foo/foo1
         */
        char *fwdPtr = mFile;
        char *urlPtr = mFile;
    
        for(; *fwdPtr != '\0'; fwdPtr++)
        {
    
            if(*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '/')
            {
                /* remove ./ 
                 */	
                fwdPtr += 1;
            }
            else if(*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '.' && 
                    (*(fwdPtr+3) == '/' || *(fwdPtr+3) == '\0'))
            {
                /* remove foo/.. 
                 */	
                /* reverse the urlPtr to the previous slash
                 */
                if(urlPtr != mFile) 
                    urlPtr--; /* we must be going back at least one */
                for(;*urlPtr != '/' && urlPtr != mFile; urlPtr--)
                    ;  /* null body */
        
                /* forward the fwd_prt past the ../
                 */
                fwdPtr += 2;
            }
            else
            {
                /* copy the url incrementaly 
                 */
                *urlPtr++ = *fwdPtr;
            }
        }
    
        *urlPtr = '\0';  /* terminate the url */

        // Now that we've resolved the relative URL, we need to reconstruct
        // a URL spec from the components.
        ReconstructSpec();
    } else {
        // absolute spec

        PR_FREEIF(mSpec);
        PRInt32 slen = aSpec.Length();
        mSpec = (char *) PR_Malloc(slen + 1);
        aSpec.ToCString(mSpec, slen+1);

        // get protocol first
        PRInt32 plen = cp - cSpec;
        mProtocol = (char*) PR_Malloc(plen + 1);
        PL_strncpy(mProtocol, cSpec, plen);
        mProtocol[plen] = 0;
        cp++;                               // eat : in protocol

        // skip over one, two or three slashes if it isn't about:
        if (PL_strcmp(mProtocol, "about") != 0) {
            if (*cp == '/') {
                cp++;
                if (*cp == '/') {
                    cp++;
                    if (*cp == '/') {
                        cp++;
                    }
                }
            } else {
                delete[] cSpec;
                
                NS_UNLOCK_INSTANCE();
                return NS_ERROR_ILLEGAL_VALUE;
            }
        }


#if defined(XP_UNIX) || defined (XP_MAC)
        // Always leave the top level slash for absolute file paths under Mac and UNIX.
        // The code above sometimes results in stripping all of slashes
        // off. This only happens when a previously stripped url is asked to be
        // parsed again. Under Win32 this is not a problem since file urls begin
        // with a drive letter not a slash. This problem show's itself when 
        // nested documents such as iframes within iframes are parsed.

        if (PL_strcmp(mProtocol, "file") == 0) {
            if (*cp != '/') {
                cp--;
            }
        }
#endif /* XP_UNIX */

        const char* cp0 = cp;
        if ((PL_strcmp(mProtocol, "resource") == 0) ||
            (PL_strcmp(mProtocol, "file") == 0) ||
            (PL_strcmp(mProtocol, "about") == 0)) {
            // resource/file url's do not have host names.
            // The remainder of the string is the file name
            PRInt32 flen = PL_strlen(cp);
            mFile = (char*) PR_Malloc(flen + 1);
            PL_strcpy(mFile, cp);
      
#ifdef NS_WIN32
            if (PL_strcmp(mProtocol, "file") == 0) {
                // If the filename starts with a "x|" where is an single
                // character then we assume it's a drive name and change the
                // vertical bar back to a ":"
                if ((flen >= 2) && (mFile[1] == '|')) {
                    mFile[1] = ':';
                }
            }
#endif /* NS_WIN32 */
        } else {
            // Host name follows protocol for http style urls
            cp = PL_strpbrk(cp, "/:");
      
            if (nsnull == cp) {
                // There is only a host name
                PRInt32 hlen = PL_strlen(cp0);
                mHost = (char*) PR_Malloc(hlen + 1);
                PL_strcpy(mHost, cp0);
            }
            else {
                PRInt32 hlen = cp - cp0;
                mHost = (char*) PR_Malloc(hlen + 1);
                PL_strncpy(mHost, cp0, hlen);        
                mHost[hlen] = 0;

                if (':' == *cp) {
                    // We have a port number
                    cp0 = cp+1;
                    cp = PL_strchr(cp, '/');
                    mPort = strtol(cp0, (char **)nsnull, 10);
                }
            }

            if (nsnull == cp) {
                // There is no file name
                // Set filename to "/"
                mFile = (char*) PR_Malloc(2);
                mFile[0] = '/';
                mFile[1] = 0;
            }
            else {
                // The rest is the file name
                PRInt32 flen = PL_strlen(cp);
                mFile = (char*) PR_Malloc(flen + 1);
                PL_strcpy(mFile, cp);
            }
        }
    }

//printf("protocol='%s' host='%s' file='%s'\n", mProtocol, mHost, mFile);
    delete[] cSpec;

    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

void
nsHttpUrlImpl::ReconstructSpec(void)
{
    PR_FREEIF(mSpec);

    char portBuffer[10];
    if (-1 != mPort) {
        PR_snprintf(portBuffer, 10, ":%d", mPort);
    }
    else {
        portBuffer[0] = '\0';
    }

    PRInt32 plen = PL_strlen(mProtocol) + PL_strlen(mHost) +
        PL_strlen(portBuffer) + PL_strlen(mFile) + 4;
    if (mRef) {
        plen += 1 + PL_strlen(mRef);
    }
    if (mSearch) {
        plen += 1 + PL_strlen(mSearch);
    }

    mSpec = (char *) PR_Malloc(plen + 1);
    if (PL_strcmp(mProtocol, "about") == 0) {
        PR_snprintf(mSpec, plen, "%s:%s", mProtocol, mFile);
    } else {
        PR_snprintf(mSpec, plen, "%s://%s%s%s", 
                    mProtocol, ((nsnull != mHost) ? mHost : ""), portBuffer,
                    mFile);
    }

    if (mRef) {
        PL_strcat(mSpec, "#");
        PL_strcat(mSpec, mRef);
    }
    if (mSearch) {
        PL_strcat(mSpec, "?");
        PL_strcat(mSpec, mSearch);
    }
}

////////////////////////////////////////////////////////////////////////////////

PRBool nsHttpUrlImpl::Equals(const nsIURL* aURL) const 
{
    PRBool bIsEqual(PR_FALSE);
    if (aURL)
    {
        NS_LOCK_INSTANCE();
        nsIHttpURL* otherURL;
        if (NS_SUCCEEDED(((nsIURL*)aURL)->QueryInterface(kIHttpURLIID, (void**)&otherURL))) {
            nsHttpUrlImpl* other = (nsHttpUrlImpl*)otherURL;
            bIsEqual = PRBool((0 == PL_strcmp(mProtocol, other->mProtocol)) && 
                              (0 == PL_strcasecmp(mHost, other->mHost)) &&
                              (0 == PL_strcmp(mFile, other->mFile)));
            NS_RELEASE(otherURL);
        }
        NS_UNLOCK_INSTANCE();
    }
    return bIsEqual;
}

nsresult nsHttpUrlImpl::GetProtocol(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = mProtocol;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::SetProtocol(const char *aNewProtocol)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    mProtocol = nsCRT::strdup(aNewProtocol);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::GetHost(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = mHost;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::SetHost(const char *aNewHost)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    mHost = nsCRT::strdup(aNewHost);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::GetFile(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = mFile;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::SetFile(const char *aNewFile)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    mFile = nsCRT::strdup(aNewFile);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::GetSpec(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = mSpec;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::SetSpec(const char *aNewSpec)
{
    // XXX is this right, or should we call ParseURL?
    nsresult rv = NS_OK;
//    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    rv = ParseURL(aNewSpec);
    PR_FREEIF(mSpec);
    mSpec = nsCRT::strdup(aNewSpec);
    NS_UNLOCK_INSTANCE();
    return rv;
}

nsresult nsHttpUrlImpl::GetRef(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = mRef;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::SetRef(const char *aNewRef)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    mRef = nsCRT::strdup(aNewRef);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::GetHostPort(PRUint32 *result) const
{
    NS_LOCK_INSTANCE();
    *result = mPort;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::SetHostPort(PRUint32 aNewPort)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    mPort = aNewPort;
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::GetSearch(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = mSearch;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::SetSearch(const char *aNewSearch)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    mSearch = nsCRT::strdup(aNewSearch);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::GetContainer(nsISupports* *result) const
{
    NS_LOCK_INSTANCE();
    *result = mContainer;
    NS_IF_ADDREF(mContainer);
    NS_UNLOCK_INSTANCE();
    if (mContainer)
    	return NS_OK;
    else
    	return NS_ERROR_UNEXPECTED;  // Indicate an error if no container
}
  
nsresult nsHttpUrlImpl::SetContainer(nsISupports* container)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    NS_IF_RELEASE(mContainer);
    mContainer = container;
    NS_IF_ADDREF(mContainer);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::GetLoadAttribs(nsILoadAttribs* *result) const
{
    NS_LOCK_INSTANCE();
    *result = mLoadAttribs;
    NS_IF_ADDREF(mLoadAttribs);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}
  
nsresult nsHttpUrlImpl::SetLoadAttribs(nsILoadAttribs* aLoadAttribs)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    NS_IF_RELEASE(mLoadAttribs);
    mLoadAttribs = aLoadAttribs;
    NS_IF_ADDREF(mLoadAttribs);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}
  
nsresult nsHttpUrlImpl::GetURLGroup(nsIURLGroup* *result) const
{
    NS_LOCK_INSTANCE();
    *result = mURLGroup;
    NS_IF_ADDREF(mURLGroup);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}
  
nsresult nsHttpUrlImpl::SetURLGroup(nsIURLGroup* group)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    NS_IF_RELEASE(mURLGroup);
    mURLGroup = group;
    NS_IF_ADDREF(mURLGroup);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}
  
nsresult nsHttpUrlImpl::SetPostHeader(const char* name, const char* value)
{
    NS_LOCK_INSTANCE();
    // XXX
    PR_ASSERT(0);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::SetPostData(nsIInputStream* input)
{
    NS_LOCK_INSTANCE();
    mPostData = input;
    input->AddRef();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsHttpUrlImpl::GetContentLength(PRInt32 *len)
{
    NS_LOCK_INSTANCE();
    *len = m_URL_s->content_length;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}



nsresult nsHttpUrlImpl::GetServerStatus(PRInt32 *status)
{
    NS_LOCK_INSTANCE();
    *status = m_URL_s->server_status;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}


nsresult nsHttpUrlImpl::ToString(PRUnichar* *unichars) const
{
    nsAutoString string;
    NS_LOCK_INSTANCE();

    // XXX Special-case javascript: URLs for the moment.
    // This code will go away when we actually start doing
    // protocol-specific parsing.
    if (PL_strcmp(mProtocol, "javascript") == 0) {
        string.SetString(mSpec);
    } else if (PL_strcmp(mProtocol, "about") == 0) { 
        string.SetString(mProtocol);
        string.Append(':');
        string.Append(mFile);
    } else {
        string.SetLength(0);
        string.Append(mProtocol);
        string.Append("://");
        if (nsnull != mHost) {
            string.Append(mHost);
            if (0 < mPort) {
                string.Append(':');
                string.Append(mPort, 10);
            }
        }
        string.Append(mFile);
        if (nsnull != mRef) {
            string.Append('#');
            string.Append(mRef);
        }
        if (nsnull != mSearch) {
            string.Append('?');
            string.Append(mSearch);
        }
    }
    NS_UNLOCK_INSTANCE();
    *unichars = string.ToNewUnicode();
    return NS_OK;
}

