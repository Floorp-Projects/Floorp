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
#include "nsIOutputStream.h"

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
static NS_DEFINE_IID(kIPluginInstancePeerIID, NS_IPLUGININSTANCEPEER_IID); 
static NS_DEFINE_IID(kIJVMPluginTagInfoIID, NS_IJVMPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ADDREF(nsPluginInstancePeerImpl);
NS_IMPL_RELEASE(nsPluginInstancePeerImpl);

nsresult nsPluginInstancePeerImpl :: QueryInterface(const nsIID& iid, void** instance)
{
    if (instance == NULL)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIPluginInstancePeerIID))
    {
        *instance = (void *)(nsIPluginInstancePeer *)this;
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
	~nsPluginStreamToFile();

	NS_DECL_ISUPPORTS

	// nsIOutputStream interface
 
	NS_IMETHOD
	Write(const char* aBuf, PRUint32 aOffset, PRUint32 aCount, PRUint32 *aWriteCount);

	// nsIBaseStream interface

    NS_IMETHOD
	Close(void);

protected:
	
	char* mTarget;
	char* mFileURL;
	char* mFilename;
	FILE* mStreamFile;
	nsIPluginInstanceOwner* mOwner;
};

nsPluginStreamToFile::nsPluginStreamToFile(const char* target, nsIPluginInstanceOwner* owner)
{
	mTarget = PL_strdup(target);
	mOwner = owner;

	// open the file and prepare it for writing
	char buf[400], tpath[300];
#ifdef XP_PC
	::GetTempPath(sizeof(tpath), tpath);
	PRInt32 len = PL_strlen(tpath);

	if((len > 0) && (tpath[len-1] != '\\'))
	{
		tpath[len] = '\\';
		tpath[len+1] = 0;
	}
#elif defined (XP_UNIX)
	PL_strcpy(tpath, "/tmp/");
#else
	tpath[0] = 0;
#endif // XP_PC

	// create the file
	PR_snprintf(buf, sizeof(buf), "%s%08X.html", tpath, this);
	mStreamFile = fopen(buf, "w");
	fclose(mStreamFile);

	mFilename = PL_strdup(buf);

	// construct the URL we'll use later in calls to GetURL()
	mFileURL = (char*)PR_Malloc((PL_strlen(buf)+PL_strlen("file://")+1) * sizeof(char));	
	if(mFileURL == nsnull)
		return;

	PL_strcpy(mFileURL, "file://");
	PL_strcat(mFileURL, buf);

	// swap \ with / for the file URL
	PRInt32 i = 0;
	while(mFileURL[i] != 0)
	{
		if(mFileURL[i] == '\\')
			mFileURL[i] = '/';
		++i;
	}

	printf("File URL = %s\n", mFileURL);
}

nsPluginStreamToFile::~nsPluginStreamToFile()
{
	if(nsnull != mTarget)
		PL_strfree(mTarget);

	if(nsnull != mFileURL)
		PL_strfree(mFileURL);

	if(nsnull != mFilename)
		PL_strfree(mFilename);
}


NS_IMPL_ADDREF(nsPluginStreamToFile)
NS_IMPL_RELEASE(nsPluginStreamToFile)

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
nsPluginStreamToFile::Write(const char* aBuf, PRUint32 aOffset, PRUint32 aCount, PRUint32 *aWriteCount)
{
	// write the data to the file and update the target
	if(nsnull != mFilename)
	{
		mStreamFile = fopen(mFilename, "a");
		fwrite(aBuf, 1, aCount, mStreamFile);
		fclose(mStreamFile);
		mOwner->GetURL(mFileURL, mTarget, nsnull);
	}

	return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamToFile::Close(void)
{
	mOwner->GetURL(mFileURL, mTarget, nsnull);

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

nsresult nsPluginInstancePeerImpl :: Initialize(nsIPluginInstanceOwner *aOwner,
                                                const nsMIMEType aMIMEType)
{
  //don't add a ref to precent circular references... MMP
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

