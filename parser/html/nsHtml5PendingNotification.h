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

#ifndef nsHtml5PendingNotification_h__
#define nsHtml5PendingNotification_h__

#include "nsNodeUtils.h"

class nsHtml5TreeBuilder;

class nsHtml5PendingNotification {
  public:

    nsHtml5PendingNotification(nsIContent* aParent)
     : mParent(aParent),
       mChildCount(aParent->GetChildCount() - 1)
    {
      MOZ_COUNT_CTOR(nsHtml5PendingNotification);
    }

    ~nsHtml5PendingNotification() {
      MOZ_COUNT_DTOR(nsHtml5PendingNotification);
    }

    inline void Fire() {
      nsNodeUtils::ContentAppended(mParent, mParent->GetChildAt(mChildCount),
                                   mChildCount);
    }

    inline bool Contains(nsIContent* aNode) {
      return !!(mParent == aNode);
    }
    
    inline bool HaveNotifiedIndex(PRUint32 index) {
      return index < mChildCount;
    }

  private:
    /**
     * An element
     */
    nsIContent* mParent;

    /**
     * Child count at start of notification deferring
     */
    PRUint32    mChildCount;
};

#endif // nsHtml5PendingNotification_h__
