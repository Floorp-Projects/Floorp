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
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsINetService.h"
#include "nsString.h"
#include "nsCRT.h"
#include "prprf.h"

#ifdef XP_PC
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#ifdef XP_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#endif

NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);

class StreamToFile : public nsIStreamListener {
public:
  StreamToFile(FILE* fp);

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetBindInfo(nsIURL* aURL);
  NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 Progress, PRInt32 ProgressMax);
  NS_IMETHOD OnStatus(nsIURL* aURL, const nsString& aMsg);
  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
  NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRInt32 length);
  NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 status, const nsString& aMsg);

  PRBool IsDone() const { return mDone; }
  PRBool HaveError() const { return mError; }

protected:
  ~StreamToFile();

  PRBool mDone;
  PRBool mError;
  FILE* mFile;
};

StreamToFile::StreamToFile(FILE* fp)
{
  NS_INIT_REFCNT();
  mDone = PR_FALSE;
  mError = PR_FALSE;
  mFile = fp;
}

NS_IMPL_ISUPPORTS(StreamToFile,kIStreamListenerIID)

StreamToFile::~StreamToFile()
{
  if (nsnull != mFile) {
    fclose(mFile);
  }
}

NS_IMETHODIMP
StreamToFile::GetBindInfo(nsIURL* aURL)
{
  return 0;
}

NS_IMETHODIMP
StreamToFile::OnProgress(nsIURL* aURL, PRInt32 Progress, PRInt32 ProgressMax)
{
  return 0;
}

NS_IMETHODIMP
StreamToFile::OnStatus(nsIURL* aURL, const nsString& aMsg)
{
  return 0;
}

NS_IMETHODIMP
StreamToFile::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  return 0;
}

NS_IMETHODIMP
StreamToFile::OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream,
                              PRInt32 length) 
{
  PRUint32 len;
  do {
    char buffer[4000];
    nsresult err = pIStream->Read(buffer, 0, sizeof(buffer), &len);
    if (err == NS_OK) {
      if (nsnull != mFile) {
        fwrite(buffer, 1, len, mFile);
      }
    }
  } while (len > 0);
  return 0;
}


NS_IMETHODIMP
StreamToFile::OnStopBinding(nsIURL* aURL, PRInt32 status, const nsString& aMsg)
{
  mDone = PR_TRUE;
  if (0 != status) {
    mError = PR_TRUE;
  }
  return 0;
}

//----------------------------------------------------------------------

// This could turn into a handy utility someday...

class PageGrabber {
public:
  PageGrabber();
  ~PageGrabber();

  void* operator new(size_t size) {
    void* rv = ::operator new(size);
    nsCRT::zero(rv, size);
    return (void*) rv;
  }

  nsresult Init(const nsString& aDirectory);

  nsresult Grab(const nsString& aURL);

protected:
  char* NextFile(const char* aExtension);

  nsString mDirectory;
  PRInt32 mFileNum;
};

PageGrabber::PageGrabber()
{
}

PageGrabber::~PageGrabber()
{
}

nsresult
PageGrabber::Init(const nsString& aDirectory)
{
  mDirectory = aDirectory;
  if (aDirectory.Last() != '/') {
    mDirectory.Append('/');
  }
  return NS_OK;
}

char*
PageGrabber::NextFile(const char* aExtension)
{
  char* cname = nsnull;
  nsAutoString name;
  for (;;) {
    name.Truncate();
    name.Append(mDirectory);
    char fileName[20];
    PR_snprintf(fileName, sizeof(fileName), "%08d.%s", mFileNum, aExtension);
    name.Append(fileName);

    // See if file already exists; if it does advance mFileNum by 100 and
    // try again.
    cname = name.ToNewCString();
    struct stat sb;
    int s = stat(cname, &sb);
    if (s < 0) {
      mFileNum++;
      break;
    }
    else {
      mFileNum += 100;
      delete [] cname;
    }
  }
  return cname;
}

nsresult
PageGrabber::Grab(const nsString& aURL)
{
  char* cname = NextFile("html");
  if (nsnull == cname) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  FILE* fp = fopen(cname, "wb");
  if (nsnull == fp) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  printf("Copying ");
  fputs(aURL, stdout);
  printf(" to %s\n", cname);
        
  // Create the URL object...
  nsIURL* url = NULL;
  nsresult rv = NS_NewURL(&url, aURL);
  if (NS_OK != rv) {
    return rv;
  }
        
  // Start the URL load...
  StreamToFile* copier = new StreamToFile(fp);
  NS_ADDREF(copier);
  rv = url->Open(copier);
  if (NS_OK != rv) {
    NS_RELEASE(copier);
    NS_RELEASE(url);
    return rv;
  }
        
  // Enter the message pump to allow the URL load to proceed.
#ifdef XP_PC
  MSG msg;
  while ( !copier->IsDone() ) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
#endif

  PRBool error = copier->HaveError();
  NS_RELEASE(url);
  NS_RELEASE(copier);
  return error ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//----------------------------------------------------------------------

int
main(int argc, char **argv)
{
  nsString url_address;

  if (argc != 3) {
    fprintf(stderr, "Usage: grabpage url directory\n");
    return -1;
  }
  PageGrabber* grabber = new PageGrabber();
  grabber->Init(argv[2]);
  if (NS_OK != grabber->Grab(argv[1])) {
    return -1;
  }
  return 0;
}
