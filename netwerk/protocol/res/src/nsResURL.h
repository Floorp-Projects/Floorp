/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsResURL_h__
#define nsResURL_h__

#include "nsIFileChannel.h"
#include "nsCOMPtr.h"

//-----------------------------------------------------------------------------
// nsResURL : overrides nsStdURL::nsIFileURL to provide nsIFile resolution
//-----------------------------------------------------------------------------

class nsResURL : nsIFileURL
{
public:
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURI(mStdURL)
    NS_FORWARD_SAFE_NSIURL(mStdURL)
    NS_DECL_NSIFILEURL

    nsResURL() { NS_INIT_ISUPPORTS(); }
    virtual ~nsResURL() {}

    nsresult Init(nsIURI *);

private:
    nsCOMPtr<nsIURL> mStdURL;
};

#endif // nsResURL_h__
