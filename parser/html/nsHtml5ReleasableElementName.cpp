/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5ReleasableElementName.h"

nsHtml5ReleasableElementName::nsHtml5ReleasableElementName(nsIAtom* name)
  : nsHtml5ElementName(name)
{
}

void
nsHtml5ReleasableElementName::release()
{
  delete this;
}

nsHtml5ElementName* 
nsHtml5ReleasableElementName::cloneElementName(nsHtml5AtomTable* aInterner)
{
  nsIAtom* l = name;
  if (aInterner) {
    if (!l->IsStaticAtom()) {
      nsAutoString str;
      l->ToString(str);
      l = aInterner->GetAtom(str);
    }
  }
  return new nsHtml5ReleasableElementName(l);
}
