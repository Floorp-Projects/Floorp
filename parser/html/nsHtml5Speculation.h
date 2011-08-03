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

#ifndef nsHtml5Speculation_h__
#define nsHtml5Speculation_h__

#include "nsHtml5UTF16Buffer.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5TreeOperation.h"
#include "nsAHtml5TreeOpSink.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"

class nsHtml5Speculation : public nsAHtml5TreeOpSink
{
  public:
    nsHtml5Speculation(nsHtml5UTF16Buffer* aBuffer, 
                       PRInt32 aStart, 
                       PRInt32 aStartLineNumber, 
                       nsAHtml5TreeBuilderState* aSnapshot);
    
    ~nsHtml5Speculation();

    nsHtml5UTF16Buffer* GetBuffer() {
      return mBuffer;
    }
    
    PRInt32 GetStart() {
      return mStart;
    }

    PRInt32 GetStartLineNumber() {
      return mStartLineNumber;
    }
    
    nsAHtml5TreeBuilderState* GetSnapshot() {
      return mSnapshot;
    }

    /**
     * Flush the operations from the tree operations from the argument
     * queue unconditionally.
     */
    virtual void MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue);
    
    void FlushToSink(nsAHtml5TreeOpSink* aSink);

  private:
    /**
     * The first buffer in the pending UTF-16 buffer queue
     */
    nsRefPtr<nsHtml5UTF16Buffer>        mBuffer;
    
    /**
     * The start index of this speculation in the first buffer
     */
    PRInt32                             mStart;

    /**
     * The current line number at the start of the speculation
     */
    PRInt32                             mStartLineNumber;
    
    nsAutoPtr<nsAHtml5TreeBuilderState> mSnapshot;

    nsTArray<nsHtml5TreeOperation>      mOpQueue;
};

#endif // nsHtml5Speculation_h__
