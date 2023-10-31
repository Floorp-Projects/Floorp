/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5AtomTable_h
#define nsHtml5AtomTable_h

#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "nsAtom.h"
#include "nsISerialEventTarget.h"

#define RECENTLY_USED_PARSER_ATOMS_SIZE 37

/*
 * nsHtml5AtomTable provides an atom cache for nsHtml5Parser and
 * nsHtml5StreamParser.
 *
 * An instance of nsHtml5AtomTable that belongs to an nsHtml5Parser is only
 * accessed from the main thread. An instance of nsHtml5AtomTable that belongs
 * to an nsHtml5StreamParser is accessed both from the main thread and from the
 * thread that executes the runnables of the nsHtml5StreamParser instance.
 * However, the threads never access the nsHtml5AtomTable instance concurrently
 * in the nsHtml5StreamParser case.
 *
 * Methods on the atoms obtained from nsHtml5AtomTable may be called on any
 * thread, although they only need to be called on the main thread or on the
 * thread working for the nsHtml5StreamParser when nsHtml5AtomTable belongs to
 * an nsHtml5StreamParser.
 *
 * Atoms cached by nsHtml5AtomTable are released when Clear() is called or when
 * the nsHtml5AtomTable itself is destructed, which happens when the owner
 * nsHtml5Parser or nsHtml5StreamParser is destructed.
 */
class nsHtml5AtomTable {
 public:
  nsHtml5AtomTable();
  ~nsHtml5AtomTable();

  // NOTE: We rely on mRecentlyUsedParserAtoms keeping alive the returned atom,
  // but the caller is responsible to take a reference before calling GetAtom
  // again.
  nsAtom* GetAtom(const nsAString& aKey);

  /**
   * Empties the table.
   */
  void Clear() {
    for (uint32_t i = 0; i < RECENTLY_USED_PARSER_ATOMS_SIZE; ++i) {
      mRecentlyUsedParserAtoms[i] = nullptr;
    }
  }

#ifdef DEBUG
  void SetPermittedLookupEventTarget(nsISerialEventTarget* aEventTarget) {
    mPermittedLookupEventTarget = aEventTarget;
  }
#endif

 private:
  RefPtr<nsAtom> mRecentlyUsedParserAtoms[RECENTLY_USED_PARSER_ATOMS_SIZE];
#ifdef DEBUG
  nsCOMPtr<nsISerialEventTarget> mPermittedLookupEventTarget;
#endif
};

#endif  // nsHtml5AtomTable_h
