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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

#ifndef nsHtml5SpeculativeLoader_h__
#define nsHtml5SpeculativeLoader_h__

#include "mozilla/Mutex.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHashSets.h"

class nsHtml5SpeculativeLoader
{
  public:
    nsHtml5SpeculativeLoader(nsHtml5TreeOpExecutor* aExecutor);
    ~nsHtml5SpeculativeLoader();

    NS_IMETHOD_(nsrefcnt) AddRef(void);
    NS_IMETHOD_(nsrefcnt) Release(void);

    void PreloadScript(const nsAString& aURL,
                       const nsAString& aCharset,
                       const nsAString& aType);

    void PreloadStyle(const nsAString& aURL, const nsAString& aCharset);

    void PreloadImage(const nsAString& aURL);

    void ProcessManifest(const nsAString& aURL);

  private:
    
    /**
     * Get a nsIURI for an nsString if the URL hasn't been preloaded yet.
     */
    already_AddRefed<nsIURI> ConvertIfNotPreloadedYet(const nsAString& aURL);
  
    nsAutoRefCnt   mRefCnt;
    
    /**
     * The executor to use as the context for preloading.
     */
    nsRefPtr<nsHtml5TreeOpExecutor> mExecutor;
    
    /**
     * URLs already preloaded/preloading.
     */
    nsCStringHashSet mPreloadedURLs;
};

#endif // nsHtml5SpeculativeLoader_h__