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
#include "imgRequest.h"
#include "prtypes.h"

#ifdef MOZ_NEW_CACHE
#include "nsICacheEntryDescriptor.h"
#else
class nsICacheEntryDescriptor;
#endif


class ImageCache
{
public:
#ifdef MOZ_NEW_CACHE
  ImageCache();
  ~ImageCache();

  static void Shutdown(); // for use by the factory

  /* additional members */
  static PRBool Put(nsIURI *aKey, imgRequest *request, nsICacheEntryDescriptor **aEntry);
  static PRBool Get(nsIURI *aKey, imgRequest **aRequest, nsICacheEntryDescriptor **aEntry);
  static PRBool Remove(nsIURI *aKey);

#else

  ImageCache()  { }
  ~ImageCache() { }

  static void Shutdown() { }

  /* additional members */
  static PRBool Put(nsIURI *aKey, imgRequest *request, nsICacheEntryDescriptor **aEntry) {
    return PR_FALSE;
  }
  static PRBool Get(nsIURI *aKey, imgRequest **aRequest, nsICacheEntryDescriptor **aEntry) {
    return PR_FALSE;
  }
  static PRBool Remove(nsIURI *aKey) {
    return PR_FALSE;
  }
#endif /* MOZ_NEW_CACHE */
};

#endif
