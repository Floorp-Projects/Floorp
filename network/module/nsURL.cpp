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
#include "nsIHttpUrl.h"     /* NS_NewHttpUrl(...) */
#include "nsString.h"
#include <stdlib.h>

#include <stdio.h>/* XXX */
#include "plstr.h"
#include "prprf.h"  /* PR_snprintf(...) */
#include "prmem.h"  /* PR_Malloc(...) / PR_Free(...) */

#ifdef XP_PC
#include <windows.h>
#endif

char *mangleResourceIntoFileURL(const char* aResourceFileName);

class URLImpl : public nsIURL {
public:
  URLImpl(const nsString& aSpec);
  URLImpl(const nsIURL* aURL, const nsString& aSpec);
  ~URLImpl();

  NS_DECL_ISUPPORTS

  virtual nsIInputStream* Open(PRInt32* aErrorCode);
  virtual nsresult Open(nsIStreamListener *aListener);

  virtual PRBool operator==(const nsIURL& aURL) const;
  virtual nsresult Set(const char *aNewSpec);

  virtual const char* GetProtocol() const;
  virtual const char* GetHost() const;
  virtual const char* GetFile() const;
  virtual const char* GetRef() const;
  virtual const char* GetSpec() const;
  virtual PRInt32 GetPort() const;

  virtual void ToString(nsString& aString) const;

  char* mSpec;
  char* mProtocol;
  char* mHost;
  char* mFile;
  char* mRef;
  PRInt32 mPort;
  PRBool mOK;

  nsISupports* mProtocolUrl;

  nsresult ParseURL(const nsIURL* aURL, const nsString& aSpec);
  void CreateProtocolURL();
};

URLImpl::URLImpl(const nsString& aSpec)
{
  NS_INIT_REFCNT();
  mProtocolUrl = nsnull;

  mProtocol = nsnull;
  mHost = nsnull;
  mFile = nsnull;
  mRef = nsnull;
  mPort = -1;
  mSpec = nsnull;

  ParseURL(nsnull, aSpec);
}

URLImpl::URLImpl(const nsIURL* aURL, const nsString& aSpec)
{
  NS_INIT_REFCNT();
  mProtocolUrl = nsnull;

  mProtocol = nsnull;
  mHost = nsnull;
  mFile = nsnull;
  mRef = nsnull;
  mPort = -1;
  mSpec = nsnull;

  ParseURL(aURL, aSpec);
}

NS_IMPL_ADDREF(URLImpl)
NS_IMPL_RELEASE(URLImpl)

NS_DEFINE_IID(kURLIID, NS_IURL_IID);

nsresult URLImpl::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
    if (aIID.Equals(kURLIID)) {
        *aInstancePtr = (void*) ((nsIURL*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports *)this);
        AddRef();
        return NS_OK;
    }
    if (nsnull == mProtocolUrl) {
        CreateProtocolURL();
    }
    if (nsnull != mProtocolUrl) {
        return mProtocolUrl->QueryInterface(aIID, aInstancePtr);
    }
    return NS_NOINTERFACE;
}


URLImpl::~URLImpl()
{
  NS_IF_RELEASE(mProtocolUrl);

  PR_FREEIF(mSpec);
  PR_FREEIF(mProtocol);
  PR_FREEIF(mHost);
  PR_FREEIF(mFile);
  PR_FREEIF(mRef);
}

nsresult URLImpl::Set(const char *aNewSpec)
{
    return ParseURL(nsnull, aNewSpec);
}


PRBool URLImpl::operator==(const nsIURL& aURL) const
{
  URLImpl&  other = (URLImpl&)aURL; // XXX ?
  return PRBool((0 == PL_strcmp(mProtocol, other.mProtocol)) && 
                (0 == PL_strcasecmp(mHost, other.mHost)) &&
                (0 == PL_strcmp(mFile, other.mFile)));
}

const char* URLImpl::GetProtocol() const
{
  return mProtocol;
}

const char* URLImpl::GetHost() const
{
  return mHost;
}

const char* URLImpl::GetFile() const
{
  return mFile;
}

const char* URLImpl::GetSpec() const
{
  return mSpec;
}

const char* URLImpl::GetRef() const
{
  return mRef;
}

PRInt32 URLImpl::GetPort() const
{
  return mPort;
}


void URLImpl::ToString(nsString& aString) const
{
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

  PR_FREEIF(mProtocol);
  PR_FREEIF(mHost);
  PR_FREEIF(mFile);
  PR_FREEIF(mRef);
  mPort = -1;
  PR_FREEIF(mSpec);

  if (nsnull == cSpec) {
    if (nsnull == aURL) {
      return NS_ERROR_ILLEGAL_VALUE;
    }
    mProtocol = (nsnull != uProtocol) ? PL_strdup(uProtocol) : nsnull;
    mHost = (nsnull != uHost) ? PL_strdup(uHost) : nsnull;
    mPort = uPort;
    mFile = (nsnull != uFile) ? PL_strdup(uFile) : nsnull;
    return NS_OK;
  }

  mSpec = PL_strdup(cSpec);
  const char* cp = PL_strchr(cSpec, ':');
  if (nsnull == cp) {
    // relative spec
    if (nsnull == aURL) {
      delete cSpec;
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
      char* dp = PL_strrchr(uFile, '/');
      PRInt32 dirlen = (dp + 1) - uFile;
      mFile = (char*) PR_Malloc(dirlen + len);
      PL_strncpy(mFile, uFile, dirlen);
      PL_strcpy(mFile + dirlen, cSpec);
    }
  } else {
    // absolute spec

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
      return NS_ERROR_ILLEGAL_VALUE;
    }

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
#endif
    } else {
      // Host name follows protocol for http style urls
      cp = PL_strchr(cp, '/');
      if (nsnull == cp) {
        // There is no file name, only a host name
        PRInt32 hlen = PL_strlen(cp0);
        mHost = (char*) PR_Malloc(hlen + 1);
        PL_strcpy(mHost, cp0);

        // Set filename to "/"
        mFile = (char*) PR_Malloc(2);
        mFile[0] = '/';
        mFile[1] = 0;
      }
      else {
        PRInt32 hlen = cp - cp0;
        mHost = (char*) PR_Malloc(hlen + 1);
        PL_strncpy(mHost, cp0, hlen);
        mHost[hlen] = 0;

        // The rest is the file name
        PRInt32 flen = PL_strlen(cp);
        mFile = (char*) PR_Malloc(flen + 1);
        PL_strcpy(mFile, cp);
      }
    }
  }

//printf("protocol='%s' host='%s' file='%s'\n", mProtocol, mHost, mFile);
  delete cSpec;
  return NS_OK;
}

nsIInputStream* URLImpl::Open(PRInt32* aErrorCode)
{
  nsresult rv;
  nsIInputStream* in = nsnull;
  nsINetService *inet;

  // XXX: Rewrite the resource: URL into a file: URL
  if (PL_strcmp(mProtocol, "resource") == 0) {
    char* fileName;

    fileName = mangleResourceIntoFileURL(mFile);
    Set(fileName);
    PR_Free(fileName);
  } 

  rv = NS_NewINetService(&inet, nsnull);
  if (NS_OK == rv) {
    rv = inet->OpenBlockingStream(this, NULL, &in);
  }
  // XXX:  The INetService should be released...

  *aErrorCode = rv;
  return in;
}

nsresult URLImpl::Open(nsIStreamListener *aListener)
{
  nsINetService *inet;
  nsresult rv;

  // XXX: Rewrite the resource: URL into a file: URL
  if (PL_strcmp(mProtocol, "resource") == 0) {
    char *fileName;

    fileName = mangleResourceIntoFileURL(mFile);
    Set(fileName);
    PR_Free(fileName);
  } 

  rv = NS_NewINetService(&inet, nsnull);
  if (NS_OK == rv) {
    rv = inet->OpenStream(this, aListener);
  }
  // XXX:  The INetService should be released...
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
  URLImpl* it = new URLImpl(aSpec);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kURLIID, (void **) aInstancePtrResult);
}

NS_NET nsresult NS_NewURL(nsIURL** aInstancePtrResult,
                          const nsIURL* aURL,
                          const nsString& aSpec)
{
  URLImpl* it = new URLImpl(aURL, aSpec);
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
    URLImpl base(aBaseURL);
    URLImpl url(&base, aSpec);
    url.ToString(aResult);
  } else {
    URLImpl url(aURL, aSpec);
    url.ToString(aResult);
  }
  return NS_OK;
}

char *mangleResourceIntoFileURL(const char* aResourceFileName) 
{
  // XXX For now, resources are not in jar files 
  // Find base path name to the resource file
  char* resourceBase;
  char* cp;

#ifdef XP_PC
  // XXX For now, all resources are relative to the .exe file
  resourceBase = (char *)PR_Malloc(_MAX_PATH);;
  DWORD mfnLen = GetModuleFileName(NULL, resourceBase, _MAX_PATH);
  // Truncate the executable name from the rest of the path...
  cp = strrchr(resourceBase, '\\');
  if (nsnull != cp) {
    *cp = '\0';
  }
  // Change the first ':' into a '|'
  cp = PL_strchr(resourceBase, ':');
  if (nsnull != cp) {
      *cp = '|';
  }
#endif

#ifdef XP_UNIX
  // XXX For now, all resources are relative to the current working directory

    FILE *pp;

#define MAXPATHLEN 2000

    resourceBase = (char *)PR_Malloc(MAXPATHLEN);;

    if (!(pp = popen("pwd", "r"))) {
      printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 1\n");
      return(nsnull);
    }
    else {
      if (fgets(resourceBase, MAXPATHLEN, pp)) {
        printf("[%s] %d\n", resourceBase, PL_strlen(resourceBase));
        resourceBase[PL_strlen(resourceBase)-1] = 0;
      }
      else {
       printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 2\n");
       return(nsnull);
      }
   }

   printf("RESOURCE name %s\n", resourceBase);
#endif

  // Join base path to resource name
  if (aResourceFileName[0] == '/') {
    aResourceFileName++;
  }
  PRInt32 baseLen = PL_strlen(resourceBase);
  PRInt32 resLen = PL_strlen(aResourceFileName);
  PRInt32 totalLen = 8 + baseLen + 1 + resLen + 1;
  char* fileName = (char *)PR_Malloc(totalLen);
  PR_snprintf(fileName, totalLen, "file:///%s/%s", resourceBase, aResourceFileName);

#ifdef XP_PC
  // Change any backslashes into foreward slashes...
  while ((cp = PL_strchr(fileName, '\\')) != 0) {
    *cp = '/';
    cp++;
  }
#endif

  PR_Free(resourceBase);

  return fileName;
}
