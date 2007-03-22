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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
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

#include "imgICache.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include "prtypes.h"

class imgRequest;
class nsIURI;
class nsICacheEntryDescriptor;

#define NS_IMGCACHE_CID \
{ /* fb4fd28a-1dd1-11b2-8391-e14242c59a41 */         \
     0xfb4fd28a,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0x83, 0x91, 0xe1, 0x42, 0x42, 0xc5, 0x9a, 0x41} \
}

class imgCache : public imgICache, 
                 public nsIObserver,
                 public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICACHE
  NS_DECL_NSIOBSERVER

  imgCache();
  virtual ~imgCache();

  static nsresult Init();

  static void Shutdown(); // for use by the factory

  /* additional members */
  static PRBool Put(nsIURI *aKey, imgRequest *request, nsICacheEntryDescriptor **aEntry);
  static PRBool Get(nsIURI *aKey, PRBool *aHasExpired, imgRequest **aRequest, nsICacheEntryDescriptor **aEntry);
  static PRBool Remove(nsIURI *aKey);

  static nsresult ClearChromeImageCache();
  static nsresult ClearImageCache();
};

