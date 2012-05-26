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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick McManus <mcmanus@ducksong.com>
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

#ifndef mozilla_net_ASpdySession_h
#define mozilla_net_ASpdySession_h

#include "nsAHttpTransaction.h"
#include "nsAHttpConnection.h"
#include "prinrval.h"
#include "nsString.h"

class nsISocketTransport;

namespace mozilla { namespace net {

// This is designed to handle up to 2 concrete protocol levels
// simultaneously
//
// Currently supported are v3 (preferred), and v2
// network.protocol.http.spdy.enabled.v2 (and v3) prefs can enable/disable
// them.

class ASpdySession : public nsAHttpTransaction
{
public:
  virtual bool AddStream(nsAHttpTransaction *, PRInt32) = 0;
  virtual bool CanReuse() = 0;
  virtual bool RoomForMoreStreams() = 0;
  virtual PRIntervalTime IdleTime() = 0;
  virtual void ReadTimeoutTick(PRIntervalTime now) = 0;
  virtual void DontReuse() = 0;

  static ASpdySession *NewSpdySession(PRUint32 version,
                                      nsAHttpTransaction *,
                                      nsISocketTransport *,
                                      PRInt32);

  const static PRUint32 kSendingChunkSize = 4096;
};

// this is essentially a single instantiation as a member of nsHttpHandler.
// It could be all static except using static ctors of XPCOM objects is a
// bad idea.
class SpdyInformation
{
public:
  SpdyInformation();
  ~SpdyInformation() {}

  // determine if a version of the protocol is enabled. The primary
  // version is index 0, the secondary version is index 1.
  bool ProtocolEnabled(PRUint32 index);

  // lookup a version enum based on an npn string. returns NS_OK if
  // string was known.
  nsresult GetNPNVersionIndex(const nsACString &npnString, PRUint8 *result);

  // lookup a version enum based on an alternate protocol string. returns NS_OK
  // if string was known and corresponding protocol is enabled.
  nsresult GetAlternateProtocolVersionIndex(const char *val,
                                            PRUint8 *result);

  enum {
    SPDY_VERSION_2 = 2,
    SPDY_VERSION_3 = 3
  };

  PRUint8   Version[2];
  nsCString VersionString[2];
  nsCString AlternateProtocolString[2];
};

}} // namespace mozilla::net

#endif // mozilla_net_ASpdySession_h
