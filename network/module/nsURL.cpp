/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsIURLGroup.h"
#include "nsIHttpUrl.h"     /* NS_NewHttpUrl(...) */
#include "nsString.h"
#include <stdlib.h>

#include <stdio.h>/* XXX */
#include "plstr.h"
#include "prprf.h"  /* PR_snprintf(...) */
#include "prmem.h"  /* PR_Malloc(...) / PR_Free(...) */
#if defined(XP_MAC)
#include "xp_mcom.h"	/* XP_STRDUP() */
#endif  /* XP_MAC */

#if defined(XP_PC)
#include <windows.h>  // Needed for Interlocked APIs defined in nsISupports.h
#endif /* XP_PC */


class URLImpl : public nsIURL {
public:
  URLImpl(const nsIURL* aURL, 
          const nsString& aSpec, 
          nsISupports* container = nsnull, 
          nsIURLGroup* aGroup = nsnull);
  virtual ~URLImpl();

  NS_DECL_ISUPPORTS

  virtual nsIInputStream* Open(PRInt32* aErrorCode);
  virtual nsresult Open(nsIStreamListener *aListener);

  virtual PRBool operator==(const nsIURL& aURL) const;
  virtual nsresult Set(const char *aNewSpec);
  virtual nsresult SetLoadAttribs(nsILoadAttribs *aLoadAttrib);

  virtual const char* GetProtocol() const;
  virtual const char* GetHost() const;
  virtual const char* GetFile() const;
  virtual const char* GetRef() const;
  virtual const char* GetSearch() const;
  virtual const char* GetSpec() const;
  virtual PRInt32 GetPort() const;
  virtual nsISupports* GetContainer() const;
  virtual nsILoadAttribs* GetLoadAttribs() const;
  virtual nsIURLGroup* GetURLGroup() const;

  virtual void ToString(nsString& aString) const;

  char* mSpec;
  char* mProtocol;
  char* mHost;
  char* mFile;
  char* mRef;
  char* mSearch;
  nsISupports*    mContainer;
  nsILoadAttribs* mLoadAttribs;
  nsIURLGroup*    mURLGroup;

  PRInt32 mPort;

  nsISupports* mProtocolUrl;

  nsresult ParseURL(const nsIURL* aURL, const nsString& aSpec);
  void CreateProtocolURL();

private:
  void Init(nsISupports *aContainer, nsIURLGroup* aGroup);
};

URLImpl::URLImpl(const nsIURL* aURL, const nsString& aSpec, 
                 nsISupports* aContainer, nsIURLGroup* aGroup)
{
  Init(aContainer, aGroup);
  ParseURL(aURL, aSpec);
}

void URLImpl::Init(nsISupports* aContainer, nsIURLGroup* aGroup) 
{
  NS_INIT_REFCNT();
  mProtocolUrl = nsnull;

  mProtocol = nsnull;
  mHost = nsnull;
  mFile = nsnull;
  mRef = nsnull;
  mSearch = nsnull;
  mPort = -1;
  mSpec = nsnull;

  NS_NewLoadAttribs(&mLoadAttribs);

  mURLGroup = aGroup;
  NS_IF_ADDREF(mURLGroup);

  mContainer = aContainer;
  NS_IF_ADDREF(mContainer);
}


NS_IMPL_THREADSAFE_ADDREF(URLImpl)
NS_IMPL_THREADSAFE_RELEASE(URLImpl)

NS_DEFINE_IID(kURLIID, NS_IURL_IID);

nsresult URLImpl::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
      return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kURLIID)) {
      *aInstancePtr = (void*) ((nsIURL*)this);
      NS_ADDREF_THIS();
      return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
      *aInstancePtr = (void*) ((nsISupports *)this);
      NS_ADDREF_THIS();
      return NS_OK;
  }
  /*
   * Check for aggregated interfaces...
   */
  NS_LOCK_INSTANCE();
  if (nsnull == mProtocolUrl) {
      CreateProtocolURL();
  }
  if (nsnull != mProtocolUrl) {
      rv = mProtocolUrl->QueryInterface(aIID, aInstancePtr);
  }
  NS_UNLOCK_INSTANCE();

#if defined(NS_DEBUG) 
  /*
   * Check for the debug-only interface indicating thread-safety
   */
  static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);
  if (aIID.Equals(kIsThreadsafeIID)) {
    return NS_OK;
  }
#endif /* NS_DEBUG */

  return rv;
}


URLImpl::~URLImpl()
{
  NS_IF_RELEASE(mProtocolUrl);
  NS_IF_RELEASE(mContainer);
  NS_IF_RELEASE(mLoadAttribs);
  NS_IF_RELEASE(mURLGroup);

  PR_FREEIF(mSpec);
  PR_FREEIF(mProtocol);
  PR_FREEIF(mHost);
  PR_FREEIF(mFile);
  PR_FREEIF(mRef);
  PR_FREEIF(mSearch);
}

nsresult URLImpl::Set(const char *aNewSpec)
{
    return ParseURL(nsnull, aNewSpec);
}

nsresult URLImpl::SetLoadAttribs(nsILoadAttribs *aLoadAttrib)
{
  NS_LOCK_INSTANCE();
  mLoadAttribs = aLoadAttrib;
  NS_IF_ADDREF(mLoadAttribs);
  NS_UNLOCK_INSTANCE();

  return NS_OK;
}


PRBool URLImpl::operator==(const nsIURL& aURL) const
{
  PRBool bIsEqual;
  URLImpl&  other = (URLImpl&)aURL; // XXX ?

  NS_LOCK_INSTANCE();
  bIsEqual =  PRBool((0 == PL_strcmp(mProtocol, other.mProtocol)) && 
                (0 == PL_strcasecmp(mHost, other.mHost)) &&
                (0 == PL_strcmp(mFile, other.mFile)));
  NS_UNLOCK_INSTANCE();

  return bIsEqual;
}

const char* URLImpl::GetProtocol() const
{
  const char* protocol;

  NS_LOCK_INSTANCE();
  protocol = mProtocol;
  NS_UNLOCK_INSTANCE();

  return protocol;
}

const char* URLImpl::GetHost() const
{
  const char* host;

  NS_LOCK_INSTANCE();
  host = mHost;
  NS_UNLOCK_INSTANCE();

  return host;
}

const char* URLImpl::GetFile() const
{
  const char* file;

  NS_LOCK_INSTANCE();
  file = mFile;
  NS_UNLOCK_INSTANCE();

  return file;
}

const char* URLImpl::GetSpec() const
{
  const char* spec;

  NS_LOCK_INSTANCE();
  spec = mSpec;
  NS_UNLOCK_INSTANCE();

  return spec;
}

const char* URLImpl::GetRef() const
{
  const char* ref;

  NS_LOCK_INSTANCE();
  ref = mRef;
  NS_UNLOCK_INSTANCE();

  return ref;
}

const char* URLImpl::GetSearch() const
{
  const char* search;

  NS_LOCK_INSTANCE();
  search = mSearch;
  NS_UNLOCK_INSTANCE();

  return search;
}

PRInt32 URLImpl::GetPort() const
{
  PRInt32 port;

  NS_LOCK_INSTANCE();
  port = mPort;
  NS_UNLOCK_INSTANCE();

  return port;
}

nsISupports* URLImpl::GetContainer() const
{
  nsISupports* container;

  NS_LOCK_INSTANCE();
  container = mContainer;
  NS_IF_ADDREF(container);
  NS_UNLOCK_INSTANCE();

  return container;
}

nsILoadAttribs* URLImpl::GetLoadAttribs() const
{
  nsILoadAttribs* loadAttribs;

  NS_LOCK_INSTANCE();
  loadAttribs = mLoadAttribs;
  NS_IF_ADDREF(loadAttribs);
  NS_UNLOCK_INSTANCE();

  return loadAttribs;
}

nsIURLGroup* URLImpl::GetURLGroup() const
{
  nsIURLGroup* group;

  NS_LOCK_INSTANCE();
  group = mURLGroup;
  NS_IF_ADDREF(group);
  NS_UNLOCK_INSTANCE();

  return group;
}

void URLImpl::ToString(nsString& aString) const
{
  NS_LOCK_INSTANCE();

  // XXX Special-case javascript: URLs for the moment.
  // This code will go away when we actually start doing
  // protocol-specific parsing.
  if (PL_strcmp(mProtocol, "javascript") == 0) {
    aString.SetString(mSpec);
  } else {
    aString.SetLength(0);
    aString.Append(mProtocol);
    aString.Append("://");
    if (nsnull != mHost) {
      aString.Append(mHost);
      if (0 < mPort) {
        aString.Append(':');
        aString.Append(mPort, 10);
      }
    }
    aString.Append(mFile);
    if (nsnull != mRef) {
      aString.Append('#');
      aString.Append(mRef);
    }
    if (nsnull != mSearch) {
      aString.Append('?');
      aString.Append(mSearch);
    }
  }
  NS_UNLOCK_INSTANCE();
}

// XXX recode to use nsString api's

// XXX don't bother with port numbers
// XXX don't bother with ref's
// XXX null pointer checks are incomplete
nsresult URLImpl::ParseURL(const nsIURL* aURL, const nsString& aSpec)
{
  // XXX hack!
  char* cSpec = aSpec.ToNewCString();

  const char* uProtocol = nsnull;
  const char* uHost = nsnull;
  const char* uFile = nsnull;
  PRInt32 uPort = -1;
  if (nsnull != aURL) {
    uProtocol = aURL->GetProtocol();
    uHost = aURL->GetHost();
    uFile = aURL->GetFile();
    uPort = aURL->GetPort();
  }

  NS_LOCK_INSTANCE();

  PR_FREEIF(mProtocol);
  PR_FREEIF(mHost);
  PR_FREEIF(mFile);
  PR_FREEIF(mRef);
  PR_FREEIF(mSearch);
  mPort = -1;
  PR_FREEIF(mSpec);

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
      delete cSpec;

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
          delete cSpec;
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
    PR_snprintf(mSpec, plen, "%s://%s%s%s", 
                mProtocol, ((nsnull != mHost) ? mHost : ""), portBuffer,
                mFile);

    if (mRef) {
      PL_strcat(mSpec, "#");
      PL_strcat(mSpec, mRef);
    }
    if (mSearch) {
      PL_strcat(mSpec, "?");
      PL_strcat(mSpec, mSearch);
    }
  } else {
    // absolute spec
    PRInt32 slen = aSpec.Length();
    mSpec = (char *) PR_Malloc(slen + 1);
    aSpec.ToCString(mSpec, slen+1);

    // get protocol first
    PRInt32 plen = cp - cSpec;
    mProtocol = (char*) PR_Malloc(plen + 1);
    PL_strncpy(mProtocol, cSpec, plen);
    mProtocol[plen] = 0;
    cp++;                               // eat : in protocol

    // skip over one, two or three slashes
    if (*cp == '/') {
      cp++;
      if (*cp == '/') {
        cp++;
        if (*cp == '/') {
          cp++;
        }
      }
    } else {
      delete cSpec;

      NS_UNLOCK_INSTANCE();
      return NS_ERROR_ILLEGAL_VALUE;
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
        (PL_strcmp(mProtocol, "file") == 0)) {
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
  delete cSpec;

  NS_UNLOCK_INSTANCE();
  return NS_OK;
}

static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

nsIInputStream* URLImpl::Open(PRInt32* aErrorCode)
{
  nsresult rv;
  nsIInputStream* in = nsnull;
  nsINetService *inet = nsnull;

  rv = nsServiceManager::GetService(kNetServiceCID,
                                    kINetServiceIID,
                                    (nsISupports **)&inet);
  if (NS_OK == rv) {
    rv = inet->OpenBlockingStream(this, NULL, &in);
    nsServiceManager::ReleaseService(kNetServiceCID, inet);
  }

  *aErrorCode = rv;
  return in;
}

nsresult URLImpl::Open(nsIStreamListener *aListener)
{
  nsINetService *inet = nsnull;
  nsresult rv;

  if (nsnull != mURLGroup) {
    rv = mURLGroup->OpenStream(this, aListener);
  } else {
    rv = nsServiceManager::GetService(kNetServiceCID,
                                      kINetServiceIID,
                                      (nsISupports **)&inet);
    if (NS_OK == rv) {
      rv = inet->OpenStream(this, aListener);
      nsServiceManager::ReleaseService(kNetServiceCID, inet);
    }
  }
  return rv;
}


void URLImpl::CreateProtocolURL()
{
    nsresult result;

    result = NS_NewHttpUrl(&mProtocolUrl, this);
}


NS_NET nsresult NS_NewURL(nsIURL** aInstancePtrResult,
                          const nsString& aSpec)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  URLImpl* it = new URLImpl(nsnull, aSpec);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kURLIID, (void **) aInstancePtrResult);
}

NS_NET nsresult NS_NewURL(nsIURL** aInstancePtrResult,
                          const nsIURL* aURL,
                          const nsString& aSpec,
                          nsISupports* aContainer,
                          nsIURLGroup* aGroup)
{
  URLImpl* it = new URLImpl(aURL, aSpec, aContainer, aGroup);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kURLIID, (void **) aInstancePtrResult);
}

NS_NET nsresult NS_MakeAbsoluteURL(nsIURL* aURL,
                                   const nsString& aBaseURL,
                                   const nsString& aSpec,
                                   nsString& aResult)
{
  if (0 < aBaseURL.Length()) {
    URLImpl base(nsnull, aBaseURL);
    URLImpl url(&base, aSpec);
    url.ToString(aResult);
  } else {
    URLImpl url(aURL, aSpec);
    url.ToString(aResult);
  }
  return NS_OK;
}
