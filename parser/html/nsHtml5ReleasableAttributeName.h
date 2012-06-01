/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5ReleasableAttributeName_h__
#define nsHtml5ReleasableAttributeName_h__

#include "nsHtml5AttributeName.h"

class nsHtml5AtomTable;

class nsHtml5ReleasableAttributeName : public nsHtml5AttributeName
{
  public:
    nsHtml5ReleasableAttributeName(PRInt32* uri, nsIAtom** local, nsIAtom** prefix);
    virtual nsHtml5AttributeName* cloneAttributeName(nsHtml5AtomTable* aInterner);
    virtual void release();
};

#endif // nsHtml5ReleasableAttributeName_h__
