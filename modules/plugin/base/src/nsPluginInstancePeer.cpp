/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nscore.h"
#include "nsPluginInstancePeer.h"
#include "nsIPluginInstance.h"
#include <stdio.h>
#include "prmem.h"
#include "prthread.h"
#include "plstr.h"
#include "prprf.h"
#include "nsIFileStream.h"
#include "nsFileSpec.h"
#include "nsCOMPtr.h"
#include "nsIJVMManager.h"
#include "nsIServiceManager.h"

#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"

#if defined(XP_PC) && !defined(XP_OS2)
#include "windows.h"
#include "winbase.h"
#endif

nsPluginInstancePeerImpl::nsPluginInstancePeerImpl()
{
  NS_INIT_REFCNT();

  mInstance = nsnull;
  mOwner = nsnull;
  mMIMEType = nsnull;
}

nsPluginInstancePeerImpl::~nsPluginInstancePeerImpl()
{
  mInstance = nsnull;
  NS_IF_RELEASE(mOwner);

  if (nsnull != mMIMEType)
  {
    PR_Free((void *)mMIMEType);
    mMIMEType = nsnull;
  }
}

static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kIPluginTagInfo2IID, NS_IPLUGINTAGINFO2_IID); 
static NS_DEFINE_IID(kIJVMPluginTagInfoIID, NS_IJVMPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWindowlessPluginInstancePeerIID, NS_IWINDOWLESSPLUGININSTANCEPEER_IID);//~~~

NS_IMPL_ADDREF(nsPluginInstancePeerImpl);
NS_IMPL_RELEASE(nsPluginInstancePeerImpl);

nsresult nsPluginInstancePeerImpl::QueryInterface(const nsIID& iid, void** instance)
{
    if (instance == NULL)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(NS_GET_IID(nsIPluginInstancePeer)) || iid.Equals(NS_GET_IID(nsIPluginInstancePeer2)))
    {
        *instance = (void *)(nsIPluginInstancePeer2*)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kIWindowlessPluginInstancePeerIID))//~~~
    {
        *instance = (void *)(nsIWindowlessPluginInstancePeer*)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kIPluginTagInfoIID))
    {
        *instance = (void *)(nsIPluginTagInfo *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kIPluginTagInfo2IID))
    {
        *instance = (void *)(nsIPluginTagInfo2 *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kIJVMPluginTagInfoIID))
    {
        *instance = (void *)(nsIJVMPluginTagInfo *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kISupportsIID))
    {
        *instance = (void *)(nsISupports *)(nsIPluginTagInfo *)this;
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetValue(nsPluginInstancePeerVariable variable, void *value)
{
  if(!mOwner)
    return NS_ERROR_FAILURE;

  return mOwner->GetValue(variable, value);
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetMIMEType(nsMIMEType *result)
{
  if (nsnull == mMIMEType)
    *result = "";
  else
    *result = mMIMEType;

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetMode(nsPluginMode *result)
{
  if (nsnull != mOwner)
    return mOwner->GetMode(result);
  else
    return NS_ERROR_FAILURE;
}

// nsPluginStreamToFile
// --------------------
// Used to handle NPN_NewStream() - writes the stream as received by the plugin
// to a file and at completion (NPN_DestroyStream), tells the browser to load it into
// a plugin-specified target

static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);

class nsPluginStreamToFile : public nsIOutputStream
{
public:

	nsPluginStreamToFile(const char* target, nsIPluginInstanceOwner* owner);
	virtual ~nsPluginStreamToFile();

	NS_DECL_ISUPPORTS
	NS_DECL_NSIOUTPUTSTREAM
 
protected:
	char* mTarget;
	nsFileURL mFileURL;
	nsFileSpec mFileSpec;
	nsCOMPtr<nsIFileSpecOutputStream> mFileThing;
	nsIPluginInstanceOwner* mOwner;
};

NS_IMPL_ADDREF(nsPluginStreamToFile);
NS_IMPL_RELEASE(nsPluginStreamToFile);

nsPluginStreamToFile::nsPluginStreamToFile(const char* target, nsIPluginInstanceOwner* owner)
:	mTarget(PL_strdup(target))
,	mFileURL(nsnull)
,	mOwner(owner)
{
	NS_INIT_REFCNT();

	// open the file and prepare it for writing
	char buf[400], tpath[300];
#ifdef XP_PC
#ifdef XP_OS2
    PL_strcpy(tpath, getenv("TEMP"));
#else
	::GetTempPath(sizeof(tpath), tpath);
#endif
	PRInt32 len = PL_strlen(tpath);

	if ((len > 0) && (tpath[len-1] != '\\'))
	{
		tpath[len] = '\\';
		tpath[len+1] = 0;
	}
#elif defined (XP_UNIX) || defined (XP_BEOS)
	PL_strcpy(tpath, "/tmp/");
#else
	tpath[0] = 0;
#endif // XP_PC

	PR_snprintf(buf, sizeof(buf), "%s%08X.html", tpath, this);


	// Create and validate the file spec object. (When we have a constructor for the temp
	// directory, we should use this instead of the per-platform hack above).
	mFileSpec = PL_strdup(buf); 
	if (mFileSpec.Error())
		return;

	// create the file
	nsISupports* ourStream;
	if (NS_FAILED(NS_NewTypicalOutputFileStream(&ourStream, mFileSpec)))
		return;
	mFileThing = do_QueryInterface(ourStream);
	NS_RELEASE(ourStream);
	
	mFileThing->Close();

	// construct the URL we'll use later in calls to GetURL()
	mFileURL = mFileSpec;	

#ifdef NS_DEBUG
	printf("File URL = %s\n", mFileURL.GetAsString());
#endif
}

nsPluginStreamToFile::~nsPluginStreamToFile()
{
	if (nsnull != mTarget)
		PL_strfree(mTarget);

}

nsresult nsPluginStreamToFile::QueryInterface(const nsIID& aIID,
                                              void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIOutputStreamIID))
  {
    *aInstancePtrResult = (void *)((nsIOutputStream *)this);
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsPluginStreamToFile::Flush()
{
    return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamToFile::Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount)
{
	// write the data to the file and update the target
	nsCOMPtr<nsIOpenFile> thing;
	thing = do_QueryInterface(mFileThing);
	thing->Open(mFileSpec, (PR_RDWR|PR_APPEND), 0700);
	PRUint32 actualCount;
	mFileThing->Write(aBuf, aCount, &actualCount);
	mFileThing->Close();
	mOwner->GetURL(mFileURL.GetAsString(), mTarget, nsnull, 0, nsnull, 0);

	return NS_OK;
}
    
NS_IMETHODIMP
nsPluginStreamToFile::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("WriteFrom");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginStreamToFile::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("WriteSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginStreamToFile::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_NOTREACHED("GetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginStreamToFile::SetNonBlocking(PRBool aNonBlocking)
{
    NS_NOTREACHED("SetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginStreamToFile::GetObserver(nsIOutputStreamObserver * *aObserver)
{
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginStreamToFile::SetObserver(nsIOutputStreamObserver * aObserver)
{
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginStreamToFile::Close(void)
{
	mOwner->GetURL(mFileURL.GetAsString(), mTarget, nsnull, 0, nsnull, 0);

	return NS_OK;
}

// end of nsPluginStreamToFile

NS_IMETHODIMP nsPluginInstancePeerImpl::NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result)
{
  nsresult rv;
  nsPluginStreamToFile*  stream = new nsPluginStreamToFile(target, mOwner);
  if(stream == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = stream->QueryInterface(kIOutputStreamIID, (void **)result);

  return rv;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::ShowStatus(const char* message)
{
  if (nsnull != mOwner)
    return mOwner->ShowStatus(message);
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo  *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetAttributes(n, names, values);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    n = 0;
    names = nsnull;
    values = nsnull;

    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetAttribute(const char* name, const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo  *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetAttribute(name, result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetDOMElement(nsIDOMElement* *result)
{
  if (mOwner == nsnull)
  {
    *result = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsIPluginTagInfo2  *tinfo;
  nsresult          rv;

  rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

  if (NS_OK == rv) 
  {
    rv = tinfo->GetDOMElement(result);
    NS_RELEASE(tinfo);
  }

  return rv;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetTagType(nsPluginTagType *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetTagType(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = nsPluginTagType_Unknown;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetTagText(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetTagText(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetParameters(PRUint16& n, const char*const*& names, const char*const*& values)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetParameters(n, names, values);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    n = 0;
    names = nsnull;
    values = nsnull;

    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetParameter(const char* name, const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetParameter(name, result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetDocumentBase(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetDocumentBase(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetDocumentEncoding(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetDocumentEncoding(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetAlignment(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetAlignment(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetWidth(PRUint32 *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetWidth(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetHeight(PRUint32 *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetHeight(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetBorderVertSpace(PRUint32 *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetBorderVertSpace(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetBorderHorizSpace(PRUint32 *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetBorderHorizSpace(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetUniqueID(PRUint32 *result)
{
  if (nsnull != mOwner)
  {
    nsIPluginTagInfo2 *tinfo;
    nsresult          rv;

    rv = mOwner->QueryInterface(kIPluginTagInfo2IID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetUniqueID(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetCode(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIJVMPluginTagInfo *tinfo;
    nsresult            rv;

    rv = mOwner->QueryInterface(kIJVMPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetCode(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetCodeBase(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIJVMPluginTagInfo *tinfo;
    nsresult            rv;

    rv = mOwner->QueryInterface(kIJVMPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetCodeBase(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetArchive(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIJVMPluginTagInfo *tinfo;
    nsresult            rv;

    rv = mOwner->QueryInterface(kIJVMPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetArchive(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetName(const char* *result)
{
  if (nsnull != mOwner)
  {
    nsIJVMPluginTagInfo *tinfo;
    nsresult            rv;

    rv = mOwner->QueryInterface(kIJVMPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetName(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetMayScript(PRBool *result)
{
  if (nsnull != mOwner)
  {
    nsIJVMPluginTagInfo *tinfo;
    nsresult            rv;

    rv = mOwner->QueryInterface(kIJVMPluginTagInfoIID, (void **)&tinfo);

    if (NS_OK == rv) 
    {
      rv = tinfo->GetMayScript(result);
      NS_RELEASE(tinfo);
    }

    return rv;
  }
  else
  {
    *result = 0;
    return NS_ERROR_FAILURE;
  }
}

NS_IMETHODIMP nsPluginInstancePeerImpl::SetWindowSize(PRUint32 width, PRUint32 height)
{
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetJSWindow(JSObject* *outJSWindow)
{
  *outJSWindow = NULL;
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIDocument> document;   

  rv = mOwner->GetDocument(getter_AddRefs(document));

  if (NS_SUCCEEDED(rv) && document) {
    nsCOMPtr<nsIScriptGlobalObject> global;
    document->GetScriptGlobalObject(getter_AddRefs(global));

    if(global) {
      *outJSWindow = global->GetGlobalJSObject();
    }
  } 

  return rv;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetJSThread(PRUint32 *outThreadID)
{
	*outThreadID = mThreadID;
	return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetJSContext(JSContext* *outContext)
{
  *outContext = NULL;
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIDocument> document;

  rv = mOwner->GetDocument(getter_AddRefs(document));

  if (NS_SUCCEEDED(rv) && document) {
    nsCOMPtr<nsIScriptGlobalObject> global;
    document->GetScriptGlobalObject(getter_AddRefs(global));

    if (global) {
      nsCOMPtr<nsIScriptContext> context;

      if (global->GetContext(getter_AddRefs(context)) == NS_OK) {
        if (context) {
            *outContext = (JSContext*) context->GetNativeContext();
            rv = NS_OK;
        }
      }
    }
  }

  return rv;
}

nsresult nsPluginInstancePeerImpl::Initialize(nsIPluginInstanceOwner *aOwner,
                                                const nsMIMEType aMIMEType)
{
  mOwner = aOwner;
  NS_IF_ADDREF(mOwner);

  aOwner->GetInstance(mInstance);
  NS_IF_RELEASE(mInstance);

  if (nsnull != aMIMEType)
  {
    mMIMEType = (nsMIMEType)PR_Malloc(PL_strlen(aMIMEType) + 1);

    if (nsnull != mMIMEType)
      PL_strcpy((char *)mMIMEType, aMIMEType);
  }
  
  // record the thread we were created in.
  mThreadID = NS_PTR_TO_INT32(PR_GetCurrentThread());

  return NS_OK;
}

nsresult nsPluginInstancePeerImpl::SetOwner(nsIPluginInstanceOwner *aOwner)
{
  // get rid of the previous owner
  NS_IF_RELEASE(mOwner);

  mOwner = aOwner;
  NS_IF_ADDREF(mOwner);

  aOwner->GetInstance(mInstance);
  NS_IF_RELEASE(mInstance);
  return NS_OK;
}

nsresult nsPluginInstancePeerImpl::GetOwner(nsIPluginInstanceOwner *&aOwner)
{
  aOwner = mOwner;
  NS_IF_ADDREF(mOwner);

  if (nsnull != mOwner)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsPluginInstancePeerImpl::InvalidateRect(nsPluginRect *invalidRect)
{
  if(!mOwner)
    return NS_ERROR_FAILURE;

  return mOwner->InvalidateRect(invalidRect);
}

NS_IMETHODIMP nsPluginInstancePeerImpl::InvalidateRegion(nsPluginRegion invalidRegion)
{
  if(!mOwner)
    return NS_ERROR_FAILURE;

  return mOwner->InvalidateRegion(invalidRegion);
}

NS_IMETHODIMP nsPluginInstancePeerImpl::ForceRedraw(void)
{
  if(!mOwner)
    return NS_ERROR_FAILURE;

  return mOwner->ForceRedraw();
}

/*void 
nsPluginInstancePeerImpl::SetStopped()
{
  mStopped = PR_TRUE;
}

void
nsPluginInstancePeerImpl::SetStarted()
{
  mStopped = PR_FALSE;
}

void
nsPluginInstancePeerImpl::IsStopped(PRBool *result)
{
  *result = mStopped;
}*/
