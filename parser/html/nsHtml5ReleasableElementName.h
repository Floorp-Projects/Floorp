/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5ReleasableElementName_h__
#define nsHtml5ReleasableElementName_h__

#include "nsHtml5ElementName.h"

class nsHtml5ReleasableElementName : public nsHtml5ElementName
{
  public:
    nsHtml5ReleasableElementName(nsIAtom* name);
    virtual void release();
    virtual nsHtml5ElementName* cloneElementName(nsHtml5AtomTable* interner);
};

#endif // nsHtml5ReleasableElementName_h__
