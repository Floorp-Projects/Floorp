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
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "nsHtml5SpeculativeLoad.h"
#include "nsHtml5TreeOpExecutor.h"

nsHtml5SpeculativeLoad::nsHtml5SpeculativeLoad()
#ifdef DEBUG
 : mOpCode(eSpeculativeLoadUninitialized)
#endif
{
  MOZ_COUNT_CTOR(nsHtml5SpeculativeLoad);
}

nsHtml5SpeculativeLoad::~nsHtml5SpeculativeLoad()
{
  MOZ_COUNT_DTOR(nsHtml5SpeculativeLoad);
  NS_ASSERTION(mOpCode != eSpeculativeLoadUninitialized,
               "Uninitialized speculative load.");
}

void
nsHtml5SpeculativeLoad::Perform(nsHtml5TreeOpExecutor* aExecutor)
{
  switch (mOpCode) {
    case eSpeculativeLoadBase:
      aExecutor->SetSpeculationBase(mUrl);
      break;
    case eSpeculativeLoadImage:
      aExecutor->PreloadImage(mUrl, mCharsetOrCrossOrigin);
      break;
    case eSpeculativeLoadScript:
      aExecutor->PreloadScript(mUrl, mCharsetOrCrossOrigin, mTypeOrCharsetSource);
      break;
    case eSpeculativeLoadStyle:
      aExecutor->PreloadStyle(mUrl, mCharsetOrCrossOrigin);
      break;
    case eSpeculativeLoadManifest:  
      aExecutor->ProcessOfflineManifest(mUrl);
      break;
    case eSpeculativeLoadSetDocumentCharset: {
        nsCAutoString narrowName;
        CopyUTF16toUTF8(mCharsetOrCrossOrigin, narrowName);
        NS_ASSERTION(mTypeOrCharsetSource.Length() == 1,
            "Unexpected charset source string");
        PRInt32 intSource = (PRInt32)mTypeOrCharsetSource.First();
        aExecutor->SetDocumentCharsetAndSource(narrowName,
                                               intSource);
      }
      break;
    default:
      NS_NOTREACHED("Bogus speculative load.");
      break;
  }
}
