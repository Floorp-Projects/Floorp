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

#include "nsMIMEBasicBodyPart.h"
#include "nsxpfcCIID.h"
#include "nspr.h"

static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kMIMEBodyPartIID,  NS_IMIME_BODY_PART_IID);

nsMIMEBasicBodyPart :: nsMIMEBasicBodyPart() : nsMIMEBodyPart()
{
  NS_INIT_REFCNT();
  mMimeBasicPart = nsnull;
}

nsMIMEBasicBodyPart :: ~nsMIMEBasicBodyPart()  
{
  if (nsnull != mMimeBasicPart)
  {
		mime_basicPart_free_all(mMimeBasicPart);
		PR_Free (mMimeBasicPart);
  }
}

NS_IMPL_ADDREF(nsMIMEBasicBodyPart)
NS_IMPL_RELEASE(nsMIMEBasicBodyPart)
NS_IMPL_QUERY_INTERFACE(nsMIMEBasicBodyPart, kMIMEBodyPartIID)

nsresult nsMIMEBasicBodyPart::Init()
{
  return (nsMIMEBodyPart::Init());
}


nsresult nsMIMEBasicBodyPart::SetBody(nsString& aBody)
{
  char * body = aBody.ToNewCString();

	mMimeBasicPart = (mime_basicPart_t *) PR_Malloc (sizeof (mime_basicPart_t));

	if (mMimeBasicPart == NULL)
		return MIME_ERR_OUTOFMEMORY; // XXX convert
	else   
		memset (mMimeBasicPart, 0, sizeof (mime_basicPart_t));

	mMimeBasicPart->content_type = MIME_CONTENT_TEXT;
	mMimeBasicPart->content_subtype = strdup ("plain");
	mMimeBasicPart->content_type_params = strdup ("charset=us-ascii");
	mMimeBasicPart->encoding_type = MIME_ENCODING_7BIT;

	int ret = mime_basicPart_setDataBuf (mMimeBasicPart, strlen (body), body, TRUE);

	if (MIME_OK != ret)
	{
    delete body;
		mime_basicPart_free_all(mMimeBasicPart);
		free (mMimeBasicPart);
		return ret;
	}

  delete body;

  return NS_OK;
}

