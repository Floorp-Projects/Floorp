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

#include "nsMIMEMessage.h"
#include "nsxpfcCIID.h"
#include "nsMIMEBasicBodyPart.h"

static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIMessageIID,       NS_IMESSAGE_IID);
static NS_DEFINE_IID(kIMIMEMessageIID,   NS_IMIME_MESSAGE_IID);

nsMIMEMessage :: nsMIMEMessage() : nsMessage()
{
  NS_INIT_REFCNT();
  mBodyType = nsMIMEBodyType_empty;
  mBodyPart = nsnull;
  mMimeMessageT = nsnull;
  mMimeMessageStreamT = nsnull;
}

nsMIMEMessage :: ~nsMIMEMessage()  
{
  NS_IF_RELEASE(mBodyPart);
  mime_message_free_all(mMimeMessageT); 
}

NS_IMPL_ADDREF(nsMIMEMessage)
NS_IMPL_RELEASE(nsMIMEMessage)

nsresult nsMIMEMessage::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kIMIMEMessageIID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsMessage::QueryInterface(aIID,aInstancePtr));
}


nsresult nsMIMEMessage::Init()
{
  mMimeMessageT = (mime_message_t *) mime_malloc (sizeof (mime_message_t));

  memset (mMimeMessageT, 0, sizeof (mime_message_t));

  return (nsMessage::Init());
}

// XXX Implement these.  We should use a HashTable to store all the
//     headers
nsresult nsMIMEMessage::AddHeader(nsString& aHeaderName, nsString& aHeaderValue)
{
  return (NS_OK);
}
nsresult nsMIMEMessage::RemoveHeader(nsString& aHeaderName)
{
  return (NS_OK);
}
nsresult nsMIMEMessage::GetHeader(nsString& aHeaderName, nsString& aHeaderValue)
{
  return (NS_OK);
}

nsresult nsMIMEMessage::Encode()
{
  mime_message_putByteStream (mMimeMessageT, mMimeMessageStreamT); 
  return NS_OK;
}


nsresult nsMIMEMessage::AddAttachment(nsString& aAttachment, nsMIMEEncoding aMIMEEncoding)
{
  return (NS_OK);
}

nsresult nsMIMEMessage::SetBody(nsString& aBody)
{

  nsMIMEBasicBodyPart * basic = nsnull;

  /*
   * By default (for now) lets just create a MIMEBasicPart for the Body.
   *
   * This method overrides that in nsMessage since the consumer has
   * explicitly asked for a MIME message
   */
  
  NS_IF_RELEASE(mBodyPart);

  nsresult res = NS_OK;

  static NS_DEFINE_IID(kIMIMEBodyPartCID,           NS_IMIME_BODY_PART_IID);
  static NS_DEFINE_IID(kCMIMEBasicBodyPartCID,      NS_MIME_BASIC_BODY_PART_CID);

  res = nsRepository::CreateInstance(kCMIMEBasicBodyPartCID, nsnull, kIMIMEBodyPartCID, (void**)&mBodyPart);

  if (NS_OK != res)
    return res;
  
  basic = (nsMIMEBasicBodyPart *) mBodyPart;

  basic->Init();

  mMimeMessageT = (mime_message_t *) mime_malloc (sizeof (mime_message_t));

  basic->SetBody(aBody);

  mime_message_addBasicPart(mMimeMessageT, basic->mMimeBasicPart, FALSE);

  return res;
}

nsresult nsMIMEMessage::AddText(nsString& aText, nsMIMEEncoding aMIMEEncoding)
{
  return (NS_OK);
}

nsresult nsMIMEMessage::AddBodyPart(nsIMIMEBodyPart& aBodyPart)
{
  return (NS_OK);
}

nsresult nsMIMEMessage::GetBodyType(nsMIMEBodyType& aBodyType)
{
  aBodyType = mBodyType;
  return (NS_OK);
}

