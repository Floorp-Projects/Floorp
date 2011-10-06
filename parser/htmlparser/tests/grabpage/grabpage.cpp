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
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsServiceManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"

#include "nsNetCID.h"
#include "nsCOMPtr.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsILocalFile.h"

#include "nsStringAPI.h"
#include "nsCRT.h"
#include "prprf.h"

#ifdef XP_WIN
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#ifdef XP_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#endif
#ifdef XP_OS2
#include <os2.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

class StreamToFile : public nsIStreamListener {
public:
  StreamToFile(FILE* fp);

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetBindInfo(nsIURI* aURL);
  NS_IMETHOD OnProgress(nsIURI* aURL, PRInt32 Progress, PRInt32 ProgressMax);
  NS_IMETHOD OnStatus(nsIURI* aURL, const nsString& aMsg);
  NS_IMETHOD OnStartRequest(nsIRequest* aRequest, nsISupports *);
  NS_IMETHOD OnDataAvailable(nsIRequest* aRequest, nsISupports *, nsIInputStream *pIStream, PRUint32 aOffset, PRUint32 aCount);
  NS_IMETHOD OnStopRequest(nsIRequest* aRequest, nsISupports *, PRUint32 status);

  bool IsDone() const { return mDone; }
  bool HaveError() const { return mError; }

protected:
  virtual ~StreamToFile();

  bool mDone;
  bool mError;
  FILE* mFile;
};

StreamToFile::StreamToFile(FILE* fp)
{
  mDone = PR_FALSE;
  mError = PR_FALSE;
  mFile = fp;
}

NS_IMPL_ISUPPORTS2(StreamToFile,
                   nsIStreamListener,
                   nsIRequestObserver)

StreamToFile::~StreamToFile()
{
  if (nsnull != mFile) {
    fclose(mFile);
  }
}

NS_IMETHODIMP
StreamToFile::GetBindInfo(nsIURI* aURL)
{
  return 0;
}

NS_IMETHODIMP
StreamToFile::OnProgress(nsIURI* aURL, PRInt32 Progress, PRInt32 ProgressMax)
{
  return 0;
}

NS_IMETHODIMP
StreamToFile::OnStatus(nsIURI* aURL, const nsString& aMsg)
{
  return 0;
}

NS_IMETHODIMP
StreamToFile::OnStartRequest(nsIRequest *aRequest, nsISupports *aSomething)
{
  return 0;
}

NS_IMETHODIMP
StreamToFile::OnDataAvailable(
  nsIRequest* aRequest,
  nsISupports *,
  nsIInputStream *pIStream,
  PRUint32 aOffset,
  PRUint32 aCount)
{
  PRUint32 len;
  do {
    char buffer[4000];
    nsresult err = pIStream->Read(buffer, sizeof(buffer), &len);
    if (NS_SUCCEEDED(err)) {
      if (nsnull != mFile) {
        fwrite(buffer, 1, len, mFile);
      }
    }
  } while (len > 0);
  return 0;
}


NS_IMETHODIMP
StreamToFile::OnStopRequest(nsIRequest *aRequest, nsISupports *aSomething, PRUint32 status)
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

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsresult Init(nsILocalFile *aDirectory);

  nsresult Grab(const nsCString& aURL);

protected:
  nsILocalFile* NextFile(const char* aExtension);

  nsILocalFile *mDirectory;
  PRInt32 mFileNum;
};

PageGrabber::PageGrabber()
{
}

PageGrabber::~PageGrabber()
{
}

nsresult
PageGrabber::Init(nsILocalFile *aDirectory)
{
  mDirectory = aDirectory;
  return NS_OK;
}

nsILocalFile*
PageGrabber::NextFile(const char* aExtension)
{
  nsCAutoString name(NS_LITERAL_CSTRING("grab."));
  name += nsDependentCString(aExtension);
  nsIFile *cfile;
  mDirectory->Clone(&cfile);
  nsCOMPtr<nsILocalFile> file = do_QueryInterface(cfile);
  file->AppendRelativeNativePath(name);
  file->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0660);
  return file;
}

nsresult
PageGrabber::Grab(const nsCString& aURL)
{
  nsresult rv;
  // Unix needs this
  nsCOMPtr<nsILocalFile> file = NextFile("html");
  if (!file) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  FILE* fp;
  rv = file->OpenANSIFileDesc("wb", &fp);
  if (NS_FAILED(rv)) {
    return rv;
  }
  printf("Copying ");
  fputs(aURL.get(), stdout);
  nsAutoString path;
  file->GetPath(path);
  NS_ConvertUTF16toUTF8 cpath(path);
  printf(" to %s\n", cpath.get());

  // Create the URL object...
  nsCOMPtr<nsIURI> url;

  nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;

  rv = ioService->NewURI(aURL, nsnull, nsnull, getter_AddRefs(url));
  nsIChannel *channel = nsnull;
  rv = ioService->NewChannelFromURI(url, &channel);
  if (NS_FAILED(rv)) return rv;

  // Start the URL load...
  StreamToFile* copier = new StreamToFile(fp);
  if (!copier)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(copier);

  rv = channel->AsyncOpen(copier, nsnull);

  if (NS_FAILED(rv)) {
    NS_RELEASE(copier);
    return rv;
  }
    
  // Enter the message pump to allow the URL load to proceed.
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  while ( !copier->IsDone() ) {
    if (!NS_ProcessNextEvent(thread))
      break;
  }

  rv = copier->HaveError() ? NS_ERROR_FAILURE : NS_OK;
  NS_RELEASE(copier);

  return rv;
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
  if(grabber) {
    nsCOMPtr <nsILocalFile> directory(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));;
    if (NS_FAILED(directory->InitWithNativePath(nsDependentCString(argv[2])))) {
      fprintf(stderr, "InitWithNativePath failed\n");
      return -2;
    }
    grabber->Init(directory);
    if (NS_OK != grabber->Grab(nsDependentCString(argv[1]))) {
      return -1;
    }
  }
  return 0;
}
