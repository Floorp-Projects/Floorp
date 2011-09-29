/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Bradford <jab@atdot.org> (original author of nsDigestAuth.h)
 *   An-Cheng Huang <pach@cs.cmu.edu>
 *   Darin Fisher <darin@netscape.com>
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

#ifndef nsDigestAuth_h__
#define nsDigestAuth_h__

#include "nsIHttpAuthenticator.h"
#include "nsICryptoHash.h"
#include "nsString.h"
#include "nsCOMPtr.h"

#define ALGO_SPECIFIED 0x01
#define ALGO_MD5 0x02
#define ALGO_MD5_SESS 0x04
#define QOP_AUTH 0x01
#define QOP_AUTH_INT 0x02

#define DIGEST_LENGTH 16
#define EXPANDED_DIGEST_LENGTH 32
#define NONCE_COUNT_LENGTH 8

//-----------------------------------------------------------------------------
// nsHttpDigestAuth
//-----------------------------------------------------------------------------

class nsHttpDigestAuth : public nsIHttpAuthenticator
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHTTPAUTHENTICATOR

    nsHttpDigestAuth();
    ~nsHttpDigestAuth();

  protected:
    nsresult ExpandToHex(const char * digest, char * result);

    nsresult CalculateResponse(const char * ha1_digest,
                               const char * ha2_digest,
                               const nsAFlatCString & nonce,
                               PRUint16 qop,
                               const char * nonce_count,
                               const nsAFlatCString & cnonce,
                               char * result);

    nsresult CalculateHA1(const nsAFlatCString & username,
                          const nsAFlatCString & password,
                          const nsAFlatCString & realm,
                          PRUint16 algorithm,
                          const nsAFlatCString & nonce,
                          const nsAFlatCString & cnonce,
                          char * result);

    nsresult CalculateHA2(const nsAFlatCString & http_method,
                          const nsAFlatCString & http_uri_path,
                          PRUint16 qop,
                          const char * body_digest,
                          char * result);

    nsresult ParseChallenge(const char * challenge,
                            nsACString & realm,
                            nsACString & domain,
                            nsACString & nonce,
                            nsACString & opaque,
                            bool * stale,
                            PRUint16 * algorithm,
                            PRUint16 * qop);

    // result is in mHashBuf
    nsresult MD5Hash(const char *buf, PRUint32 len);

    nsresult GetMethodAndPath(nsIHttpAuthenticableChannel *,
                              bool, nsCString &, nsCString &);

    // append the quoted version of value to aHeaderLine
    nsresult AppendQuotedString(const nsACString & value,
                                nsACString & aHeaderLine);

  protected:
    nsCOMPtr<nsICryptoHash>        mVerifier;
    char                           mHashBuf[DIGEST_LENGTH];
};

#endif // nsHttpDigestAuth_h__
