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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Drinan <ddrinan@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __NS_CMS_H__
#define __NS_CMS_H__

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsICMS.h"
#include "sechash.h"
#include "cms.h"

#define NS_HASH_CLASSNAME "Hash Object"
#define NS_HASH_CID \
  { 0xa31a3028, 0xae28, 0x11d5, { 0xba, 0x4b, 0x00, 0x10, 0x83, 0x03, 0xb1, 0x17 } }

class nsHash : public nsIHash
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIHASH

  nsHash();
  virtual ~nsHash();

private:
  HASHContext * m_ctxt;
};

#define NS_CMSMESSAGE_CLASSNAME "CMS Message Object"
#define NS_CMSMESSAGE_CID \
  { 0xa4557478, 0xae16, 0x11d5, { 0xba,0x4b,0x00,0x10,0x83,0x03,0xb1,0x17 } }

class nsCMSMessage : public nsICMSMessage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICMSMESSAGE

  nsCMSMessage();
  nsCMSMessage(NSSCMSMessage* aCMSMsg);
  virtual ~nsCMSMessage();
  
  NSSCMSMessage* getCMS() {return m_cmsMsg;};
private:
  NSSCMSMessage * m_cmsMsg;
};


// ===============================================
// nsCMSDecoder - implementation of nsICMSDecoder
// ===============================================

#define NS_CMSDECODER_CLASSNAME "CMS Decoder Object"
#define NS_CMSDECODER_CID \
  { 0x9dcef3a4, 0xa3bc, 0x11d5, { 0xba, 0x47, 0x00, 0x10, 0x83, 0x03, 0xb1, 0x17 } }

class nsCMSDecoder : public nsICMSDecoder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICMSDECODER

  nsCMSDecoder();
  virtual ~nsCMSDecoder();

private:
  nsCOMPtr<nsIInterfaceRequestor> m_ctx;
  NSSCMSDecoderContext *m_dcx;
};

// ===============================================
// nsCMSEncoder - implementation of nsICMSEncoder
// ===============================================

#define NS_CMSENCODER_CLASSNAME "CMS Decoder Object"
#define NS_CMSENCODER_CID \
  { 0xa15789aa, 0x8903, 0x462b, { 0x81, 0xe9, 0x4a, 0xa2, 0xcf, 0xf4, 0xd5, 0xcb } }
class nsCMSEncoder : public nsICMSEncoder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICMSENCODER

  nsCMSEncoder();
  virtual ~nsCMSEncoder();

private:
  nsCOMPtr<nsIInterfaceRequestor> m_ctx;
  NSSCMSEncoderContext *m_ecx;
};

#endif
