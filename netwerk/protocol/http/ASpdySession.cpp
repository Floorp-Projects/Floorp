/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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

#include "nsHttp.h"
#include "nsHttpHandler.h"

#include "ASpdySession.h"
#include "SpdySession2.h"
#include "SpdySession3.h"

#include "mozilla/Telemetry.h"

namespace mozilla {
namespace net {

ASpdySession *
ASpdySession::NewSpdySession(PRUint32 version,
                             nsAHttpTransaction *aTransaction,
                             nsISocketTransport *aTransport,
                             PRInt32 aPriority)
{
  // This is a necko only interface, so we can enforce version
  // requests as a precondition
  NS_ABORT_IF_FALSE(version == SpdyInformation::SPDY_VERSION_2 ||
                    version == SpdyInformation::SPDY_VERSION_3,
                    "Unsupported spdy version");

  // Don't do a runtime check of IsSpdyV?Enabled() here because pref value
  // may have changed since starting negotiation. The selected protocol comes
  // from a list provided in the SERVER HELLO filtered by our acceptable
  // versions, so there is no risk of the server ignoring our prefs.

  Telemetry::Accumulate(Telemetry::SPDY_VERSION, version);
    
  if (version == SpdyInformation::SPDY_VERSION_2)
    return new SpdySession2(aTransaction, aTransport, aPriority);

  return new SpdySession3(aTransaction, aTransport, aPriority);
}

SpdyInformation::SpdyInformation()
{
  // list the preferred version first
  Version[0] = SPDY_VERSION_3;
  VersionString[0] = NS_LITERAL_CSTRING("spdy/3");
  AlternateProtocolString[0] = NS_LITERAL_CSTRING("443:npn-spdy/3");

  Version[1] = SPDY_VERSION_2;
  VersionString[1] = NS_LITERAL_CSTRING("spdy/2");
  AlternateProtocolString[1] = NS_LITERAL_CSTRING("443:npn-spdy/2");
}

bool
SpdyInformation::ProtocolEnabled(PRUint32 index)
{
  if (index == 0)
    return gHttpHandler->IsSpdyV3Enabled();

  if (index == 1)
    return gHttpHandler->IsSpdyV2Enabled();

  NS_ABORT_IF_FALSE(false, "index out of range");
  return false;
}

nsresult
SpdyInformation::GetNPNVersionIndex(const nsACString &npnString,
                                    PRUint8 *result)
{
  if (npnString.IsEmpty())
    return NS_ERROR_FAILURE;

  if (npnString.Equals(VersionString[0]))
    *result = Version[0];
  else if (npnString.Equals(VersionString[1]))
    *result = Version[1];
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
SpdyInformation::GetAlternateProtocolVersionIndex(const char *val,
                                                  PRUint8 *result)
{
  if (!val || !val[0])
    return NS_ERROR_FAILURE;

  if (ProtocolEnabled(0) && nsHttp::FindToken(val,
                                              AlternateProtocolString[0].get(),
                                              HTTP_HEADER_VALUE_SEPS))
    *result = Version[0];
  else if (ProtocolEnabled(1) && nsHttp::FindToken(val,
                                                   AlternateProtocolString[1].get(),
                                                   HTTP_HEADER_VALUE_SEPS))
    *result = Version[1];
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}

} // namespace mozilla::net
} // namespace mozilla

