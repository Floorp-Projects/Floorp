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

#include "ImageCache.h"

#include "nsXPIDLString.h"
#include "nsCOMPtr.h"

#include "nsHashtable.h"

class nsIURIKey : public nsHashKey {
protected:
  nsCOMPtr<nsIURI> mKey;

public:
  nsIURIKey(nsIURI* key) : mKey(key) {}
  ~nsIURIKey(void) {}

  PRUint32 HashCode(void) const {
    nsXPIDLCString spec;
    mKey->GetSpec(getter_Copies(spec));
    return (PRUint32) PL_HashString(spec);
  }

  PRBool Equals(const nsHashKey *aKey) const {
    PRBool eq;
    mKey->Equals( ((nsIURIKey*) aKey)->mKey, &eq );
    return eq;
  }

  nsHashKey *Clone(void) const {
    return new nsIURIKey(mKey);
  }
};


ImageCache::ImageCache()
{
  /* member initializers and constructor code */
}

nsSupportsHashtable mCache;


ImageCache::~ImageCache()
{
  /* destructor code */
}

PRBool ImageCache::Put(nsIURI *aKey, nsImageRequest *request)
{
  nsIURIKey key(aKey);
  return mCache.Put(&key, NS_STATIC_CAST(nsIImageRequest*, request));
}

PRBool ImageCache::Get(nsIURI *aKey, nsImageRequest **request)
{
  nsIURIKey key(aKey);
  nsImageRequest *sup = NS_REINTERPRET_CAST(nsImageRequest*, NS_STATIC_CAST(nsIImageRequest*, mCache.Get(&key))); // this addrefs
  
  if (sup) {
    *request = sup;
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

PRBool ImageCache::Remove(nsIURI *aKey)
{
  nsIURIKey key(aKey);
  return mCache.Remove(&key);
}
