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

#include "nsHttpRequestHead.h"

//-----------------------------------------------------------------------------
// nsHttpRequestHead
//-----------------------------------------------------------------------------

void
nsHttpRequestHead::Flatten(nsACString &buf, PRBool pruneProxyHeaders)
{
    // note: the first append is intentional.
 
    buf.Append(mMethod.get());
    buf.Append(' ');
    buf.Append(mRequestURI);
    buf.Append(" HTTP/");

    switch (mVersion) {
    case NS_HTTP_VERSION_1_1:
        buf.Append("1.1");
        break;
    case NS_HTTP_VERSION_0_9:
        buf.Append("0.9");
        break;
    default:
        buf.Append("1.0");
    }

    buf.Append("\r\n");

    mHeaders.Flatten(buf, pruneProxyHeaders);
}
