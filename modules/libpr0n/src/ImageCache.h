/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef ImageCache_h__
#define ImageCache_h__

#include "nsIURI.h"
#include "nsImageRequest.h"


#define IMAGE_CACHE_CID \
{ /* 70058a20-1dd2-11b2-9d22-db0a9d82e8bd */         \
     0x70058a20,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x9d, 0x22, 0xdb, 0x0a, 0x9d, 0x82, 0xe8, 0xbd} \
}

class ImageCache
{
public:
  ImageCache();
  ~ImageCache();

  /* additional members */
  static PRBool Put(nsIURI *aKey, nsImageRequest *request);
  static PRBool Get(nsIURI *aKey, nsImageRequest **request);
  static PRBool Remove(nsIURI *aKey);

private:
};

#endif
