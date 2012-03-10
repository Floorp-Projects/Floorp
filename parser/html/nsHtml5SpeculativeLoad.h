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
 
#ifndef nsHtml5SpeculativeLoad_h_
#define nsHtml5SpeculativeLoad_h_

#include "nsString.h"

class nsHtml5TreeOpExecutor;

enum eHtml5SpeculativeLoad {
#ifdef DEBUG
  eSpeculativeLoadUninitialized,
#endif
  eSpeculativeLoadBase,
  eSpeculativeLoadImage,
  eSpeculativeLoadScript,
  eSpeculativeLoadStyle,
  eSpeculativeLoadManifest,
  eSpeculativeLoadSetDocumentCharset
};

class nsHtml5SpeculativeLoad {
  public:
    nsHtml5SpeculativeLoad();
    ~nsHtml5SpeculativeLoad();
    
    inline void InitBase(const nsAString& aUrl) {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadBase;
      mUrl.Assign(aUrl);
    }

    inline void InitImage(const nsAString& aUrl,
                          const nsAString& aCrossOrigin) {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadImage;
      mUrl.Assign(aUrl);
      mCrossOrigin.Assign(aCrossOrigin);
    }

    inline void InitScript(const nsAString& aUrl,
                           const nsAString& aCharset,
                           const nsAString& aType,
                           const nsAString& aCrossOrigin) {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadScript;
      mUrl.Assign(aUrl);
      mCharset.Assign(aCharset);
      mTypeOrCharsetSource.Assign(aType);
      mCrossOrigin.Assign(aCrossOrigin);
    }
    
    inline void InitStyle(const nsAString& aUrl, const nsAString& aCharset) {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadStyle;
      mUrl.Assign(aUrl);
      mCharset.Assign(aCharset);
    }

    /**
     * "Speculative" manifest loads aren't truly speculative--if a manifest
     * gets loaded, we are committed to it. There can never be a <script>
     * before the manifest, so the situation of having to undo a manifest due
     * to document.write() never arises. The reason why a parser
     * thread-discovered manifest gets loaded via the speculative load queue
     * as opposed to tree operation queue is that the manifest must get
     * processed before any actual speculative loads such as scripts. Thus,
     * manifests seen by the parser thread have to maintain the queue order
     * relative to true speculative loads. See bug 541079.
     */
    inline void InitManifest(const nsAString& aUrl) {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadManifest;
      mUrl.Assign(aUrl);
    }

    /**
     * "Speculative" charset setting isn't truly speculative. If the charset
     * is set via this operation, we are committed to it unless chardet or
     * a late meta cause a reload. The reason why a parser
     * thread-discovered charset gets communicated via the speculative load
     * queue as opposed to tree operation queue is that the charset change
     * must get processed before any actual speculative loads such as style
     * sheets. Thus, encoding decisions by the parser thread have to maintain
     * the queue order relative to true speculative loads. See bug 675499.
     */
    inline void InitSetDocumentCharset(nsACString& aCharset,
                                       PRInt32 aCharsetSource) {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadSetDocumentCharset;
      CopyUTF8toUTF16(aCharset, mCharset);
      mTypeOrCharsetSource.Assign((PRUnichar)aCharsetSource);
    }

    void Perform(nsHtml5TreeOpExecutor* aExecutor);
    
  private:
    eHtml5SpeculativeLoad mOpCode;
    nsString mUrl;
    /**
     * If mOpCode is eSpeculativeLoadStyle or eSpeculativeLoadScript
     * then this is the value of the "charset" attribute. For
     * eSpeculativeLoadSetDocumentCharset it is the charset that the
     * document's charset is being set to. Otherwise it's empty.
     */
    nsString mCharset;
    /**
     * If mOpCode is eSpeculativeLoadSetDocumentCharset, this is a
     * one-character string whose single character's code point is to be
     * interpreted as a charset source integer. Otherwise, it is empty or
     * the value of the type attribute.
     */
    nsString mTypeOrCharsetSource;
    /**
     * If mOpCode is eSpeculativeLoadImage or eSpeculativeLoadScript,
     * this is the value of the "crossorigin" attribute.  If the
     * attribute is not set, this will be a void string.
     */
    nsString mCrossOrigin;
};

#endif // nsHtml5SpeculativeLoad_h_
