/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFind_h__
#define nsFind_h__

#include "nsIFind.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsINode.h"
#include "nsIContentIterator.h"
#include "mozilla/intl/WordBreaker.h"

class nsIContent;
class nsRange;

#define NS_FIND_CONTRACTID "@mozilla.org/embedcomp/rangefind;1"

#define NS_FIND_CID \
  {0x471f4944, 0x1dd2, 0x11b2, {0x87, 0xac, 0x90, 0xbe, 0x0a, 0x51, 0xd6, 0x09}}

class nsFindContentIterator;

class nsFind : public nsIFind
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIFIND
  NS_DECL_CYCLE_COLLECTION_CLASS(nsFind)

  nsFind();

protected:
  virtual ~nsFind();

  // Parameters set from the interface:
  bool mFindBackward;
  bool mCaseSensitive;

  // Use "find entire words" mode by setting to a word breaker or null, to
  // disable "entire words" mode.
  RefPtr<mozilla::intl::WordBreaker> mWordBreaker;

  struct State;
  class StateRestorer;
  nsIContent* GetBlockParent(nsINode* aNode);

  // Move in the right direction for our search:
  nsresult NextNode(State&,
                    nsRange* aSearchRange,
                    nsRange* aStartPoint,
                    nsRange* aEndPoint,
                    bool aContinueOk);

  // Get the first character from the next node (last if mFindBackward).
  //
  // This will mutate the state, but then restore it afterwards.
  char16_t PeekNextChar(State&,
                        nsRange* aSearchRange,
                        nsRange* aStartPoint,
                        nsRange* aEndPoint);

  // The iterator we use to move through the document:
  nsresult InitIterator(State&,
                        nsINode* aStartNode,
                        int32_t aStartOffset,
                        nsINode* aEndNode,
                        int32_t aEndOffset);
};

#endif // nsFind_h__
