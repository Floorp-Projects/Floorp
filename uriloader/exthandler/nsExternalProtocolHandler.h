/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsExternalProtocolHandler_h___
#define nsExternalProtocolHandler_h___

#include "nsIExternalProtocolHandler.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "nsIExternalProtocolService.h"
#include "mozilla/Attributes.h"

class nsIURI;

// protocol handlers need to support weak references if we want the netlib nsIOService to cache them.
class nsExternalProtocolHandler final : public nsIExternalProtocolHandler, public nsSupportsWeakReference
{
public:
	NS_DECL_THREADSAFE_ISUPPORTS
	NS_DECL_NSIPROTOCOLHANDLER
	NS_DECL_NSIEXTERNALPROTOCOLHANDLER

	nsExternalProtocolHandler();

protected:
  ~nsExternalProtocolHandler();

  // helper function
  bool HaveExternalProtocolHandler(nsIURI * aURI);
	nsCString	m_schemeName;
};

#endif // nsExternalProtocolHandler_h___

