/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5AtomTable.h"
#include "nsHtml5Atom.h"
#include "nsThreadUtils.h"

nsHtml5AtomEntry::nsHtml5AtomEntry(KeyTypePointer aStr)
  : nsStringHashKey(aStr)
  , mAtom(new nsHtml5Atom(*aStr))
{
}

nsHtml5AtomEntry::nsHtml5AtomEntry(const nsHtml5AtomEntry& aOther)
  : nsStringHashKey(aOther)
  , mAtom(nullptr)
{
  NS_NOTREACHED("nsHtml5AtomTable is broken and tried to copy an entry");
}

nsHtml5AtomEntry::~nsHtml5AtomEntry()
{
}

nsHtml5AtomTable::nsHtml5AtomTable()
{
#ifdef DEBUG
  NS_GetMainThread(getter_AddRefs(mPermittedLookupThread));
#endif
}

nsHtml5AtomTable::~nsHtml5AtomTable()
{
}

nsIAtom*
nsHtml5AtomTable::GetAtom(const nsAString& aKey)
{
#ifdef DEBUG
  {
    nsCOMPtr<nsIThread> currentThread;
    NS_GetCurrentThread(getter_AddRefs(currentThread));
    NS_ASSERTION(mPermittedLookupThread == currentThread, "Wrong thread!");
  }
#endif
  nsIAtom* atom = NS_GetStaticAtom(aKey);
  if (atom) {
    return atom;
  }
  nsHtml5AtomEntry* entry = mTable.PutEntry(aKey);
  if (!entry) {
    return nullptr;
  }
  return entry->GetAtom();
}
