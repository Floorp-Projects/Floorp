/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is frightening to behold.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
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

/**
 * The MIME stream separates headers and a datastream. It also allows
 * automatic creation of the content-length header.
 */

#ifndef _nsMIMEInputStream_h_
#define _nsMIMEInputStream_h_

#include "nsIMIMEInputStream.h"

#define NS_MIMEINPUTSTREAM_CLASSNAME  "nsMIMEInputStream"
#define NS_MIMEINPUTSTREAM_CONTRACTID "@mozilla.org/network/mime-input-stream;1"
#define NS_MIMEINPUTSTREAM_CID                       \
{ /* 58a1c31c-1dd2-11b2-a3f6-d36949d48268 */         \
    0x58a1c31c,                                      \
    0x1dd2,                                          \
    0x11b2,                                          \
    {0xa3, 0xf6, 0xd3, 0x69, 0x49, 0xd4, 0x82, 0x68} \
}

extern NS_METHOD nsMIMEInputStreamConstructor(nsISupports *outer,
                                              REFNSIID iid,
                                              void **result);

#endif // _nsMIMEInputStream_h_
