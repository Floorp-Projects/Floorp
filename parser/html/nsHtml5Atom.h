/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5Atom_h_
#define nsHtml5Atom_h_

#include "nsIAtom.h"

/**
 * A dynamic atom implementation meant for use within the nsHtml5Tokenizer and 
 * nsHtml5TreeBuilder owned by one nsHtml5Parser or nsHtml5StreamParser 
 * instance.
 *
 * Usage is documented in nsHtml5AtomTable and nsIAtom.
 */
class nsHtml5Atom : public nsIAtom
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIATOM

    nsHtml5Atom(const nsAString& aString);
    ~nsHtml5Atom();
};

#endif // nsHtml5Atom_h_
