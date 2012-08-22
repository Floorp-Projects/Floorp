/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    
    inline bool HaveNotifiedIndex(uint32_t index) {
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
    uint32_t    mChildCount;
};

#endif // nsHtml5PendingNotification_h__
