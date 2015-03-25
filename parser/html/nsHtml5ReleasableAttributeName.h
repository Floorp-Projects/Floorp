/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5ReleasableAttributeName_h
#define nsHtml5ReleasableAttributeName_h

#include "nsHtml5AttributeName.h"
#include "mozilla/Attributes.h"

class nsHtml5AtomTable;

class nsHtml5ReleasableAttributeName final : public nsHtml5AttributeName
{
  public:
    nsHtml5ReleasableAttributeName(int32_t* uri, nsIAtom** local, nsIAtom** prefix);
    virtual nsHtml5AttributeName* cloneAttributeName(nsHtml5AtomTable* aInterner);
    virtual void release();
};

#endif // nsHtml5ReleasableAttributeName_h
