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
 * Portions created by the Initial Developer are Copyright (C) 2008-2009
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

#define NS_HTML5_TREE_BUILDER_HANDLE_ARRAY_LENGTH 512

  private:

    nsTArray<nsHtml5TreeOperation>         mOpQueue;
    nsTArray<nsHtml5SpeculativeLoad>       mSpeculativeLoadQueue;
    nsAHtml5TreeOpSink*                    mOpSink;
    nsAutoArrayPtr<nsIContent*>            mHandles;
    PRInt32                                mHandlesUsed;
    nsTArray<nsAutoArrayPtr<nsIContent*> > mOldHandles;
    nsHtml5TreeOpStage*                    mSpeculativeLoadStage;
    nsIContent**                           mDeepTreeSurrogateParent;
    bool                                   mCurrentHtmlScriptIsAsyncOrDefer;
#ifdef DEBUG
    bool                                   mActive;
#endif

    // DocumentModeHandler
    /**
     * Tree builder uses this to report quirkiness of the document
     */
    void documentMode(nsHtml5DocumentMode m);

    nsIContent** AllocateContentHandle();
    
    void accumulateCharactersForced(const PRUnichar* aBuf, PRInt32 aStart, PRInt32 aLength)
    {
      accumulateCharacters(aBuf, aStart, aLength);
    }

  public:

    nsHtml5TreeBuilder(nsAHtml5TreeOpSink* aOpSink,
                       nsHtml5TreeOpStage* aStage);

    ~nsHtml5TreeBuilder();
    
    bool HasScript();
    
    void SetOpSink(nsAHtml5TreeOpSink* aOpSink) {
      mOpSink = aOpSink;
    }

    void ClearOps() {
      mOpQueue.Clear();
    }
    
    bool Flush(bool aDiscretionary = false);
    
    void FlushLoads();

    void SetDocumentCharset(nsACString& aCharset, PRInt32 aCharsetSource);

    void StreamEnded();

    void NeedsCharsetSwitchTo(const nsACString& aEncoding, PRInt32 aSource);

    void AddSnapshotToScript(nsAHtml5TreeBuilderState* aSnapshot, PRInt32 aLine);

    void DropHandles();
