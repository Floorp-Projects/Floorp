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

nsHtml5AtomTable::~nsHtml5AtomTable() = default;

nsAtom* nsHtml5AtomTable::GetAtom(const nsAString& aKey) {
  MOZ_ASSERT(mPermittedLookupEventTarget->IsOnCurrentThread());
  uint32_t hash = mozilla::HashString(aKey);
  uint32_t index = hash % RECENTLY_USED_PARSER_ATOMS_SIZE;
  if (nsAtom* atom = mRecentlyUsedParserAtoms[index]) {
    if (atom->hash() == hash && atom->Equals(aKey)) {
      return atom;
    }
  }

  RefPtr<nsAtom> atom = NS_Atomize(aKey, hash);
  nsAtom* ret = atom.get();
  mRecentlyUsedParserAtoms[index] = std::move(atom);
  return ret;
}
