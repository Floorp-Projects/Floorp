/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nscore.h"
#include "nsPluginInstancePeer.h"
#include "nsIPluginInstance.h"
#include <stdio.h>
#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsIFileStream.h"
#include "nsFileSpec.h"
#include "nsCOMPtr.h"

#if defined(OJI) && defined(XP_MAC)
#include "nsIDocument.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#endif

#ifdef XP_PC
#include "windows.h"
#include "winbase.h"
#endif

nsPluginInstancePeerImpl :: nsPluginInstancePeerImpl()
{
  NS_INIT_REFCNT();

  mInstance = nsnull;
  mOwner = nsnull;
  mMIMEType = nsnull;
}

nsPluginInstancePeerImpl :: ~nsPluginInstancePeerImpl()
{
  mInstance = nsnull;
  mOwner = nsnull;

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

NS_IMPL_ADDREF(nsPluginInstancePeerImpl);
NS_IMPL_RELEASE(nsPluginInstancePeerImpl);

nsresult nsPluginInstancePeerImpl :: QueryInterface(const nsIID& iid, void** instance)
{
    if (instance == NULL)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(nsIPluginInstancePeer::GetIID()) || iid.Equals(nsIPluginInstancePeer2::GetIID()))
    {
        *instance = (void *)(nsIPluginInstancePeer2*)this;
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetValue(nsPluginInstancePeerVariable variable, void *value)
{
printf("instance peer getvalue %d called\n", variable);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetMIMEType(nsMIMEType *result)
{
  if (nsnull == mMIMEType)
    *result = "";
  else
    *result = mMIMEType;

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetMode(nsPluginMode *result)
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

	// nsIOutputStream interface
 
	NS_IMETHOD
	Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount);

	// nsIBaseStream interface

    NS_IMETHOD
	Close(void);

protected:
	char* mTarget;
	nsFileURL mFileURL;
	nsFileSpec mFileSpec;
	nsCOMPtr<nsIFileOutputStream> mFileThing;
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
	::GetTempPath(sizeof(tpath), tpath);
	PRInt32 len = PL_strlen(tpath);

	if ((len > 0) && (tpath[len-1] != '\\'))
	{
		tpath[len] = '\\';
		tpath[len+1] = 0;
	}
#elif defined (XP_UNIX)
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

	printf("File URL = %s\n", mFileURL.GetAsString());
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
nsPluginStreamToFile::Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount)
{
	// write the data to the file and update the target
	nsCOMPtr<nsIFile> thing;
	thing = do_QueryInterface(mFileThing);
	thing->Open(mFileSpec, (PR_RDWR|PR_APPEND), 0700);
	PRUint32 actualCount;
	mFileThing->Write(aBuf, aCount, &actualCount);
	mFileThing->Close();
	mOwner->GetURL(mFileURL.GetAsString(), mTarget, nsnull);

	return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamToFile::Close(void)
{
	mOwner->GetURL(mFileURL.GetAsString(), mTarget, nsnull);

	return NS_OK;
}

// end of nsPluginStreamToFile

NS_IMETHODIMP nsPluginInstancePeerImpl :: NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result)
{
  nsresult rv;
  nsPluginStreamToFile*  stream = new nsPluginStreamToFile(target, mOwner);

  rv = stream->QueryInterface(kIOutputStreamIID, (void **)result);

  return rv;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: ShowStatus(const char* message)
{
  if (nsnull != mOwner)
    return mOwner->ShowStatus(message);
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetAttribute(const char* name, const char* *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetTagType(nsPluginTagType *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetTagText(const char* *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetParameters(PRUint16& n, const char*const*& names, const char*const*& values)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetParameter(const char* name, const char* *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetDocumentBase(const char* *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetDocumentEncoding(const char* *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetAlignment(const char* *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetWidth(PRUint32 *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetHeight(PRUint32 *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetBorderVertSpace(PRUint32 *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetBorderHorizSpace(PRUint32 *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetUniqueID(PRUint32 *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetCode(const char* *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetCodeBase(const char* *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetArchive(const char* *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetName(const char* *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: GetMayScript(PRBool *result)
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

NS_IMETHODIMP nsPluginInstancePeerImpl :: SetWindowSize(PRUint32 width, PRUint32 height)
{
printf("instance peer setwindowsize called\n");
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::GetJSWindow(JSObject* *outJSWindow)
{
	*outJSWindow = NULL;
	nsresult rv = NS_ERROR_FAILURE;
#if defined(OJI) && defined(XP_MAC)
	nsIDocument* document = nsnull;
	if (mOwner->GetDocument(&document) == NS_OK) {
		nsIScriptContextOwner* contextOwner = document->GetScriptContextOwner();
		if (nsnull != contextOwner) {
			nsIScriptGlobalObject *global = nsnull;
			contextOwner->GetScriptGlobalObject(&global);
			nsIScriptContext* context = nsnull;
			contextOwner->GetScriptContext(&context);
			if (nsnull != global && nsnull != context) {
				static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
				nsIScriptObjectOwner* window = nsnull;
				if (global->QueryInterface(kIScriptObjectOwnerIID, (void **)&window) == NS_OK) {
					rv = window->GetScriptObject(context, (void**)outJSWindow);
					NS_RELEASE(window);
				}
			}
			NS_IF_RELEASE(global);
			NS_IF_RELEASE(context);
			NS_RELEASE(contextOwner);
		}
		NS_RELEASE(document);	
	}
#endif
	return rv;
}

nsresult nsPluginInstancePeerImpl :: Initialize(nsIPluginInstanceOwner *aOwner,
                                                const nsMIMEType aMIMEType)
{
  //don't add a ref to prevent circular references... MMP
  mOwner = aOwner;

  aOwner->GetInstance(mInstance);
  //release this one too... MMP
  NS_IF_RELEASE(mInstance);

  if (nsnull != aMIMEType)
  {
    mMIMEType = (nsMIMEType)PR_Malloc(PL_strlen(aMIMEType) + 1);

    if (nsnull != mMIMEType)
      PL_strcpy((char *)mMIMEType, aMIMEType);
  }

  return NS_OK;
}

nsresult nsPluginInstancePeerImpl :: GetOwner(nsIPluginInstanceOwner *&aOwner)
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
	return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::InvalidateRegion(nsPluginRegion invalidRegion)
{
	return NS_OK;
}

NS_IMETHODIMP nsPluginInstancePeerImpl::ForceRedraw(void)
{
	return NS_OK;
}

