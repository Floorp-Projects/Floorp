/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_CMS_H__
#define __NS_CMS_H__

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIInterfaceRequestor.h"
#include "nsICMSMessage.h"
#include "nsICMSMessage2.h"
#include "nsIX509Cert3.h"
#include "nsVerificationJob.h"
#include "nsICMSEncoder.h"
#include "nsICMSDecoder.h"
#include "sechash.h"
#include "cms.h"
#include "nsNSSShutDown.h"

#define NS_CMSMESSAGE_CID \
  { 0xa4557478, 0xae16, 0x11d5, { 0xba,0x4b,0x00,0x10,0x83,0x03,0xb1,0x17 } }

class nsCMSMessage : public nsICMSMessage,
                     public nsICMSMessage2,
                     public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICMSMESSAGE
  NS_DECL_NSICMSMESSAGE2

  nsCMSMessage();
  nsCMSMessage(NSSCMSMessage* aCMSMsg);
  virtual ~nsCMSMessage();
  
  void referenceContext(nsIInterfaceRequestor* aContext) {m_ctx = aContext;}
  NSSCMSMessage* getCMS() {return m_cmsMsg;}
private:
  nsCOMPtr<nsIInterfaceRequestor> m_ctx;
  NSSCMSMessage * m_cmsMsg;
  NSSCMSSignerInfo* GetTopLevelSignerInfo();
  nsresult CommonVerifySignature(unsigned char* aDigestData, uint32_t aDigestDataLen);

  nsresult CommonAsyncVerifySignature(nsISMimeVerificationListener *aListener,
                                      unsigned char* aDigestData, uint32_t aDigestDataLen);

  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();

friend class nsSMimeVerificationJob;
};

// ===============================================
// nsCMSDecoder - implementation of nsICMSDecoder
// ===============================================

#define NS_CMSDECODER_CID \
  { 0x9dcef3a4, 0xa3bc, 0x11d5, { 0xba, 0x47, 0x00, 0x10, 0x83, 0x03, 0xb1, 0x17 } }

class nsCMSDecoder : public nsICMSDecoder,
                     public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICMSDECODER

  nsCMSDecoder();
  virtual ~nsCMSDecoder();

private:
  nsCOMPtr<nsIInterfaceRequestor> m_ctx;
  NSSCMSDecoderContext *m_dcx;
  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

// ===============================================
// nsCMSEncoder - implementation of nsICMSEncoder
// ===============================================

#define NS_CMSENCODER_CID \
  { 0xa15789aa, 0x8903, 0x462b, { 0x81, 0xe9, 0x4a, 0xa2, 0xcf, 0xf4, 0xd5, 0xcb } }
class nsCMSEncoder : public nsICMSEncoder,
                     public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICMSENCODER

  nsCMSEncoder();
  virtual ~nsCMSEncoder();

private:
  nsCOMPtr<nsIInterfaceRequestor> m_ctx;
  NSSCMSEncoderContext *m_ecx;
  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

#endif
