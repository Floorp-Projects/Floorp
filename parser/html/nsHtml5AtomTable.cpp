/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5AtomTable.h"
#include "nsThreadUtils.h"

nsHtml5AtomTable::nsHtml5AtomTable() : mRecentlyUsedParserAtoms{} {
#ifdef DEBUG
  mPermittedLookupEventTarget = mozilla::GetCurrentSerialEventTarget();
#endif
}

nsHtml5AtomTable::~nsHtml5AtomTable() {}

nsAtom* nsHtml5AtomTable::GetAtom(const nsAString& aKey) {
#ifdef DEBUG
  MOZ_ASSERT(mPermittedLookupEventTarget->IsOnCurrentThread());
#endif

  uint32_t index = mozilla::HashString(aKey) % RECENTLY_USED_PARSER_ATOMS_SIZE;
  if (nsAtom* atom = mRecentlyUsedParserAtoms[index]) {
    if (atom->Equals(aKey)) {
      return atom;
    }
  }

  RefPtr<nsAtom> atom = NS_Atomize(aKey);
  nsAtom* ret = atom.get();
  mRecentlyUsedParserAtoms[index] = std::move(atom);
  return ret;
}
