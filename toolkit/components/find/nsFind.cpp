/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define DEBUG_FIND 1

#include "nsFind.h"
#include "nsContentCID.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsISelectionController.h"
#include "nsIFrame.h"
#include "nsITextControlFrame.h"
#include "nsIFormControl.h"
#include "nsTextFragment.h"
#include "nsString.h"
#include "nsAtom.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsRange.h"
#include "nsContentUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/TextEditor.h"
#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/TreeIterator.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/Text.h"

using namespace mozilla;
using namespace mozilla::dom;

// Yikes!  Casting a char to unichar can fill with ones!
#define CHAR_TO_UNICHAR(c) ((char16_t)(unsigned char)c)

#define CH_QUOTE ((char16_t)0x22)
#define CH_APOSTROPHE ((char16_t)0x27)
#define CH_LEFT_SINGLE_QUOTE ((char16_t)0x2018)
#define CH_RIGHT_SINGLE_QUOTE ((char16_t)0x2019)
#define CH_LEFT_DOUBLE_QUOTE ((char16_t)0x201C)
#define CH_RIGHT_DOUBLE_QUOTE ((char16_t)0x201D)

#define CH_SHY ((char16_t)0xAD)

// nsFind::Find casts CH_SHY to char before calling StripChars
// This works correctly if and only if CH_SHY <= 255
static_assert(CH_SHY <= 255, "CH_SHY is not an ascii character");

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFind)
  NS_INTERFACE_MAP_ENTRY(nsIFind)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFind)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFind)

NS_IMPL_CYCLE_COLLECTION(nsFind)

nsFind::nsFind()
    : mFindBackward(false), mCaseSensitive(false), mWordBreaker(nullptr) {}

nsFind::~nsFind() = default;

#ifdef DEBUG_FIND
#  define DEBUG_FIND_PRINTF(...) printf(__VA_ARGS__)
#else
#  define DEBUG_FIND_PRINTF(...) /* nothing */
#endif

static nsIContent& AnonymousSubtreeRootParent(const nsINode& aNode) {
  MOZ_ASSERT(aNode.IsInNativeAnonymousSubtree());

  nsIContent* current = aNode.GetParent();
  while (current->IsInNativeAnonymousSubtree()) {
    current = current->GetParent();
    MOZ_ASSERT(current, "huh?");
  }
  return *current;
}

static void DumpNode(const nsINode* aNode) {
#ifdef DEBUG_FIND
  if (!aNode) {
    printf(">>>> Node: NULL\n");
    return;
  }
  nsString nodeName = aNode->NodeName();
  if (aNode->IsText()) {
    nsAutoString newText;
    aNode->AsText()->AppendTextTo(newText);
    printf(">>>> Text node (node name %s): '%s'\n",
           NS_LossyConvertUTF16toASCII(nodeName).get(),
           NS_LossyConvertUTF16toASCII(newText).get());
  } else {
    printf(">>>> Node: %s\n", NS_LossyConvertUTF16toASCII(nodeName).get());
  }
#endif
}

static bool IsBlockNode(const nsIContent* aContent) {
  if (aContent->IsElement() && aContent->AsElement()->IsDisplayContents()) {
    return false;
  }

  // FIXME(emilio): This is dubious...
  if (aContent->IsAnyOfHTMLElements(nsGkAtoms::img, nsGkAtoms::hr,
                                    nsGkAtoms::th, nsGkAtoms::td)) {
    return true;
  }

  nsIFrame* frame = aContent->GetPrimaryFrame();
  return frame && frame->StyleDisplay()->IsBlockOutsideStyle();
}

static bool IsDisplayedNode(const nsINode* aNode) {
  if (!aNode->IsContent()) {
    return false;
  }

  if (aNode->AsContent()->GetPrimaryFrame()) {
    return true;
  }

  // If there's no frame, it's not displayed, unless it's display: contents.
  return aNode->IsElement() && aNode->AsElement()->IsDisplayContents();
}

static bool IsVisibleNode(const nsINode* aNode) {
  if (!IsDisplayedNode(aNode)) {
    return false;
  }

  nsIFrame* frame = aNode->AsContent()->GetPrimaryFrame();
  if (!frame) {
    // display: contents
    return true;
  }

  return frame->StyleVisibility()->IsVisible();
}

static bool IsTextFormControl(nsIContent& aContent) {
  if (!aContent.IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) {
    return false;
  }

  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(&aContent);
  return formControl->IsTextControl(true);
}

static bool SkipNode(const nsIContent* aContent) {
  const nsIContent* content = aContent;
  while (content) {
    if (!IsDisplayedNode(content) || content->IsComment() ||
        content->IsAnyOfHTMLElements(nsGkAtoms::script, nsGkAtoms::noframes,
                                     nsGkAtoms::select)) {
      DEBUG_FIND_PRINTF("Skipping node: ");
      DumpNode(content);
      return true;
    }

    // Skip option nodes if their select is a combo box, or if they
    // have no select (somehow).
    if (const auto* option = HTMLOptionElement::FromNode(content)) {
      auto* select = HTMLSelectElement::FromNodeOrNull(option->GetParent());
      if (!select || select->IsCombobox()) {
        DEBUG_FIND_PRINTF("Skipping node: ");
        DumpNode(content);
        return true;
      }
    }

    if (content->IsInNativeAnonymousSubtree()) {
      // We don't want to use almost any NAC: Only editable NAC in textfields
      // should be findable. That is, we want to find "bar" in
      // `<input value="bar">`, but not in `<input placeholder="bar">`.
      if (!content->IsEditable() ||
          !IsTextFormControl(AnonymousSubtreeRootParent(*content))) {
        DEBUG_FIND_PRINTF("Skipping node: ");
        DumpNode(content);
        return true;
      }
    }

    // Only climb to the nearest block node
    if (IsBlockNode(content)) {
      return false;
    }

    content = content->GetFlattenedTreeParent();
  }

  return false;
}

static const nsIContent* GetBlockParent(const Text& aNode) {
  for (const nsIContent* current = aNode.GetFlattenedTreeParent(); current;
       current = current->GetFlattenedTreeParent()) {
    if (IsBlockNode(current)) {
      return current;
    }
  }
  return nullptr;
}

struct nsFind::State final {
  State(bool aFindBackward, nsIContent& aRoot, const nsRange& aStartPoint)
      : mFindBackward(aFindBackward),
        mInitialized(false),
        mIterOffset(-1),
        mLastBlockParent(nullptr),
        mIterator(aRoot),
        mStartPoint(aStartPoint) {}

  void PositionAt(Text& aNode) { mIterator.Seek(aNode); }

  Text* GetCurrentNode() const {
    MOZ_ASSERT(mInitialized);
    nsINode* node = mIterator.GetCurrent();
    MOZ_ASSERT(!node || node->IsText());
    return node ? node->GetAsText() : nullptr;
  }

  Text* GetNextNode() {
    if (MOZ_UNLIKELY(!mInitialized)) {
      Initialize();
    } else {
      Advance();
      mIterOffset = -1;  // mIterOffset only really applies to the first node.
    }
    return GetCurrentNode();
  }

  // Gets the next non-empty text fragment in the same block, starting by the
  // _next_ node.
  const nsTextFragment* GetNextNonEmptyTextFragmentInSameBlock();

 private:
  // Advance to the next visible text-node.
  void Advance();
  // Sets up the first node position and offset.
  void Initialize();

  const bool mFindBackward;

  // Whether we've called GetNextNode() at least once.
  bool mInitialized;

 public:
  // An offset into the text of the first node we're starting to search at.
  int mIterOffset;
  const nsIContent* mLastBlockParent;
  TreeIterator<StyleChildrenIterator> mIterator;

  // These are only needed for the first GetNextNode() call.
  const nsRange& mStartPoint;
};

void nsFind::State::Advance() {
  MOZ_ASSERT(mInitialized);

  while (true) {
    nsIContent* current =
        mFindBackward ? mIterator.GetPrev() : mIterator.GetNext();

    if (!current) {
      return;
    }

    if (!current->IsContent() || SkipNode(current->AsContent())) {
      continue;
    }

    if (current->IsText()) {
      return;
    }
  }
}

void nsFind::State::Initialize() {
  MOZ_ASSERT(!mInitialized);
  mInitialized = true;
  mIterOffset = mFindBackward ? -1 : 0;

  // Set up ourselves at the first node we want to start searching at.
  nsINode* beginning = mFindBackward ? mStartPoint.GetEndContainer()
                                     : mStartPoint.GetStartContainer();
  if (beginning && beginning->IsContent()) {
    mIterator.Seek(*beginning->AsContent());
  }

  nsINode* current = mIterator.GetCurrent();
  if (!current) {
    return;
  }

  if (!current->IsText() || SkipNode(current->AsText())) {
    Advance();
    return;
  }

  mLastBlockParent = GetBlockParent(*current->AsText());

  if (current != beginning) {
    return;
  }

  mIterOffset =
      mFindBackward ? mStartPoint.EndOffset() : mStartPoint.StartOffset();
}

const nsTextFragment* nsFind::State::GetNextNonEmptyTextFragmentInSameBlock() {
  while (true) {
    const Text* current = GetNextNode();
    if (!current) {
      return nullptr;
    }

    const nsIContent* blockParent = GetBlockParent(*current);
    if (!blockParent || blockParent != mLastBlockParent) {
      return nullptr;
    }

    const nsTextFragment& frag = current->TextFragment();
    if (frag.GetLength()) {
      return &frag;
    }
  }
}

class MOZ_STACK_CLASS nsFind::StateRestorer final {
 public:
  explicit StateRestorer(State& aState)
      : mState(aState),
        mIterOffset(aState.mIterOffset),
        mCurrNode(aState.GetCurrentNode()),
        mLastBlockParent(aState.mLastBlockParent) {}

  ~StateRestorer() {
    mState.mIterOffset = mIterOffset;
    if (mCurrNode) {
      mState.PositionAt(*mCurrNode);
    }
    mState.mLastBlockParent = mLastBlockParent;
  }

 private:
  State& mState;

  int32_t mIterOffset;
  Text* mCurrNode;
  const nsIContent* mLastBlockParent;
};

NS_IMETHODIMP
nsFind::GetFindBackwards(bool* aFindBackward) {
  if (!aFindBackward) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFindBackward = mFindBackward;
  return NS_OK;
}

NS_IMETHODIMP
nsFind::SetFindBackwards(bool aFindBackward) {
  mFindBackward = aFindBackward;
  return NS_OK;
}

NS_IMETHODIMP
nsFind::GetCaseSensitive(bool* aCaseSensitive) {
  if (!aCaseSensitive) {
    return NS_ERROR_NULL_POINTER;
  }

  *aCaseSensitive = mCaseSensitive;
  return NS_OK;
}

NS_IMETHODIMP
nsFind::SetCaseSensitive(bool aCaseSensitive) {
  mCaseSensitive = aCaseSensitive;
  return NS_OK;
}

/* attribute boolean entireWord; */
NS_IMETHODIMP
nsFind::GetEntireWord(bool* aEntireWord) {
  if (!aEntireWord) return NS_ERROR_NULL_POINTER;

  *aEntireWord = !!mWordBreaker;
  return NS_OK;
}

NS_IMETHODIMP
nsFind::SetEntireWord(bool aEntireWord) {
  mWordBreaker = aEntireWord ? nsContentUtils::WordBreaker() : nullptr;
  return NS_OK;
}

// Here begins the find code. A ten-thousand-foot view of how it works: Find
// needs to be able to compare across inline (but not block) nodes, e.g. find
// for "abc" should match a<b>b</b>c. So after we've searched a node, we're not
// done with it; in the case of a partial match we may need to reset the
// iterator to go back to a previously visited node, so we always save the
// "match anchor" node and offset.
//
// Text nodes store their text in an nsTextFragment, which is effectively a
// union of a one-byte string or a two-byte string. Single and double strings
// are intermixed in the dom. We don't have string classes which can deal with
// intermixed strings, so all the handling is done explicitly here.

char16_t nsFind::PeekNextChar(State& aState) const {
  // We need to restore the necessary state before this function returns.
  StateRestorer restorer(aState);

  const nsTextFragment* frag = aState.GetNextNonEmptyTextFragmentInSameBlock();
  if (!frag) {
    return L'\0';
  }

  const char16_t* t2b = nullptr;
  const char* t1b = nullptr;

  if (frag->Is2b()) {
    t2b = frag->Get2b();
  } else {
    t1b = frag->Get1b();
  }

  uint32_t len = frag->GetLength();
  MOZ_ASSERT(len);

  int32_t index = mFindBackward ? len - 1 : 0;
  return t1b ? CHAR_TO_UNICHAR(t1b[index]) : t2b[index];
}

#define NBSP_CHARCODE (CHAR_TO_UNICHAR(160))
#define IsSpace(c) (nsCRT::IsAsciiSpace(c) || (c) == NBSP_CHARCODE)
#define OVERFLOW_PINDEX (mFindBackward ? pindex < 0 : pindex > patLen)
#define DONE_WITH_PINDEX (mFindBackward ? pindex <= 0 : pindex >= patLen)
#define ALMOST_DONE_WITH_PINDEX \
  (mFindBackward ? pindex <= 0 : pindex >= patLen - 1)

// Take nodes out of the tree with NextNode, until null (NextNode will return 0
// at the end of our range).
NS_IMETHODIMP
nsFind::Find(const nsAString& aPatText, nsRange* aSearchRange,
             nsRange* aStartPoint, nsRange* aEndPoint, nsRange** aRangeRet) {
  DEBUG_FIND_PRINTF("============== nsFind::Find('%s'%s, %p, %p, %p)\n",
                    NS_LossyConvertUTF16toASCII(aPatText).get(),
                    mFindBackward ? " (backward)" : " (forward)",
                    (void*)aSearchRange, (void*)aStartPoint, (void*)aEndPoint);

  NS_ENSURE_ARG(aSearchRange);
  NS_ENSURE_ARG(aStartPoint);
  NS_ENSURE_ARG(aEndPoint);
  NS_ENSURE_ARG_POINTER(aRangeRet);

  Document* document =
      aStartPoint->GetRoot() ? aStartPoint->GetRoot()->OwnerDoc() : nullptr;
  NS_ENSURE_ARG(document);

  Element* root = document->GetRootElement();
  NS_ENSURE_ARG(root);

  *aRangeRet = 0;

  nsAutoString patAutoStr(aPatText);
  if (!mCaseSensitive) {
    ToLowerCase(patAutoStr);
  }

  // Ignore soft hyphens in the pattern
  static const char kShy[] = {char(CH_SHY), 0};
  patAutoStr.StripChars(kShy);

  const char16_t* patStr = patAutoStr.get();
  int32_t patLen = patAutoStr.Length() - 1;

  // If this function is called with an empty string, we should early exit.
  if (patLen < 0) {
    return NS_OK;
  }

  // current offset into the pattern -- reset to beginning/end:
  int32_t pindex = (mFindBackward ? patLen : 0);

  // Current offset into the fragment
  int32_t findex = 0;

  // Direction to move pindex and ptr*
  int incr = (mFindBackward ? -1 : 1);

  const nsTextFragment* frag = nullptr;
  int32_t fragLen = 0;

  // Pointers into the current fragment:
  const char16_t* t2b = nullptr;
  const char* t1b = nullptr;

  // Keep track of when we're in whitespace:
  // (only matters when we're matching)
  bool inWhitespace = false;
  // Keep track of whether the previous char was a word-breaking one.
  bool wordBreakPrev = false;

  // Place to save the range start point in case we find a match:
  Text* matchAnchorNode = nullptr;
  int32_t matchAnchorOffset = 0;

  // Get the end point, so we know when to end searches:
  nsINode* endNode = aEndPoint->GetEndContainer();
  uint32_t endOffset = aEndPoint->EndOffset();

  char16_t c = 0;
  char16_t patc = 0;
  char16_t prevChar = 0;
  char16_t prevCharInMatch = 0;

  State state(mFindBackward, *root, *aStartPoint);
  Text* current = nullptr;

  while (true) {
    DEBUG_FIND_PRINTF("Loop ...\n");

    // If this is our first time on a new node, reset the pointers:
    if (!frag) {
      current = state.GetNextNode();
      if (!current) {
        return NS_OK;
      }

      // We have a new text content. If its block parent is different from the
      // block parent of the last text content, then we need to clear the match
      // since we don't want to find across block boundaries.
      const nsIContent* blockParent = GetBlockParent(*current);
      DEBUG_FIND_PRINTF("New node: old blockparent = %p, new = %p\n",
                        (void*)state.mLastBlockParent, (void*)blockParent);
      if (blockParent != state.mLastBlockParent) {
        DEBUG_FIND_PRINTF("Different block parent!\n");
        state.mLastBlockParent = blockParent;
        // End any pending match:
        matchAnchorNode = nullptr;
        matchAnchorOffset = 0;
        c = 0;
        prevChar = 0;
        prevCharInMatch = 0;
        pindex = (mFindBackward ? patLen : 0);
        inWhitespace = false;
      }

      frag = &current->TextFragment();
      fragLen = frag->GetLength();

      // Set our starting point in this node. If we're going back to the anchor
      // node, which means that we just ended a partial match, use the saved
      // offset:
      //
      // FIXME(emilio): How could current ever be the anchor node, if we had not
      // seen current so far?
      if (current == matchAnchorNode) {
        findex = matchAnchorOffset + (mFindBackward ? 1 : 0);
      } else if (state.mIterOffset >= 0) {
        findex = state.mIterOffset - (mFindBackward ? 1 : 0);
      } else {
        findex = mFindBackward ? (fragLen - 1) : 0;
      }

      // Offset can only apply to the first node:
      state.mIterOffset = -1;

      DEBUG_FIND_PRINTF("Starting from offset %d of %d\n", findex, fragLen);

      // If this is outside the bounds of the string, then skip this node:
      if (findex < 0 || findex > fragLen - 1) {
        DEBUG_FIND_PRINTF(
            "At the end of a text node -- skipping to the next\n");
        frag = nullptr;
        continue;
      }

      if (frag->Is2b()) {
        t2b = frag->Get2b();
        t1b = nullptr;
#ifdef DEBUG_FIND
        nsAutoString str2(t2b, fragLen);
        DEBUG_FIND_PRINTF("2 byte, '%s'\n",
                          NS_LossyConvertUTF16toASCII(str2).get());
#endif
      } else {
        t1b = frag->Get1b();
        t2b = nullptr;
#ifdef DEBUG_FIND
        nsAutoCString str1(t1b, fragLen);
        DEBUG_FIND_PRINTF("1 byte, '%s'\n", str1.get());
#endif
      }
    } else {
      // Still on the old node. Advance the pointers, then see if we need to
      // pull a new node.
      findex += incr;
      DEBUG_FIND_PRINTF("Same node -- (%d, %d)\n", pindex, findex);
      if (mFindBackward ? (findex < 0) : (findex >= fragLen)) {
        DEBUG_FIND_PRINTF(
            "Will need to pull a new node: mAO = %d, frag len=%d\n",
            matchAnchorOffset, fragLen);
        // Done with this node.  Pull a new one.
        frag = nullptr;
        continue;
      }
    }

    // Have we gone past the endpoint yet? If we have, and we're not in the
    // middle of a match, return.
    if (state.GetCurrentNode() == endNode &&
        ((mFindBackward && findex < static_cast<int32_t>(endOffset)) ||
         (!mFindBackward && findex > static_cast<int32_t>(endOffset)))) {
      return NS_OK;
    }

    // Save the previous character for word boundary detection
    prevChar = c;
    // The two characters we'll be comparing:
    c = (t2b ? t2b[findex] : CHAR_TO_UNICHAR(t1b[findex]));
    patc = patStr[pindex];

    DEBUG_FIND_PRINTF("Comparing '%c'=%x to '%c' (%d of %d), findex=%d%s\n",
                      (char)c, (int)c, patc, pindex, patLen, findex,
                      inWhitespace ? " (inWhitespace)" : "");

    // Do we need to go back to non-whitespace mode? If inWhitespace, then this
    // space in the pat str has already matched at least one space in the
    // document.
    if (inWhitespace && !IsSpace(c)) {
      inWhitespace = false;
      pindex += incr;
#ifdef DEBUG
      // This shouldn't happen -- if we were still matching, and we were at the
      // end of the pat string, then we should have caught it in the last
      // iteration and returned success.
      if (OVERFLOW_PINDEX) {
        NS_ASSERTION(false, "Missed a whitespace match");
      }
#endif
      patc = patStr[pindex];
    }
    if (!inWhitespace && IsSpace(patc)) {
      inWhitespace = true;
    } else if (!inWhitespace && !mCaseSensitive && IsUpperCase(c)) {
      c = ToLowerCase(c);
    }

    if (c == CH_SHY) {
      // ignore soft hyphens in the document
      continue;
    }

    if (!mCaseSensitive) {
      switch (c) {
        // treat curly and straight quotes as identical
        case CH_LEFT_SINGLE_QUOTE:
        case CH_RIGHT_SINGLE_QUOTE:
          c = CH_APOSTROPHE;
          break;
        case CH_LEFT_DOUBLE_QUOTE:
        case CH_RIGHT_DOUBLE_QUOTE:
          c = CH_QUOTE;
          break;
      }

      switch (patc) {
        // treat curly and straight quotes as identical
        case CH_LEFT_SINGLE_QUOTE:
        case CH_RIGHT_SINGLE_QUOTE:
          patc = CH_APOSTROPHE;
          break;
        case CH_LEFT_DOUBLE_QUOTE:
        case CH_RIGHT_DOUBLE_QUOTE:
          patc = CH_QUOTE;
          break;
      }
    }

    // a '\n' between CJ characters is ignored
    if (pindex != (mFindBackward ? patLen : 0) && c != patc && !inWhitespace) {
      if (c == '\n' && t2b && IS_CJ_CHAR(prevCharInMatch)) {
        int32_t nindex = findex + incr;
        if (mFindBackward ? (nindex >= 0) : (nindex < fragLen)) {
          if (IS_CJ_CHAR(t2b[nindex])) {
            continue;
          }
        }
      }
    }

    wordBreakPrev = false;
    if (mWordBreaker) {
      if (prevChar == NBSP_CHARCODE) prevChar = CHAR_TO_UNICHAR(' ');
      wordBreakPrev = mWordBreaker->BreakInBetween(&prevChar, 1, &c, 1);
    }

    // Compare. Match if we're in whitespace and c is whitespace, or if the
    // characters match and at least one of the following is true:
    // a) we're not matching the entire word
    // b) a match has already been stored
    // c) the previous character is a different "class" than the current
    // character.
    if ((c == patc && (!mWordBreaker || matchAnchorNode || wordBreakPrev)) ||
        (inWhitespace && IsSpace(c))) {
      prevCharInMatch = c;
      if (inWhitespace) {
        DEBUG_FIND_PRINTF("YES (whitespace)(%d of %d)\n", pindex, patLen);
      } else {
        DEBUG_FIND_PRINTF("YES! '%c' == '%c' (%d of %d)\n", c, patc, pindex,
                          patLen);
      }

      // Save the range anchors if we haven't already:
      if (!matchAnchorNode) {
        matchAnchorNode = state.GetCurrentNode();
        matchAnchorOffset = findex;
      }

      // Are we done?
      if (DONE_WITH_PINDEX) {
        // Matched the whole string!
        DEBUG_FIND_PRINTF("Found a match!\n");

        // Make the range:
        // Check for word break (if necessary)
        if (mWordBreaker) {
          int32_t nextfindex = findex + incr;

          char16_t nextChar;
          // If still in array boundaries, get nextChar.
          if (mFindBackward ? (nextfindex >= 0) : (nextfindex < fragLen))
            nextChar =
                (t2b ? t2b[nextfindex] : CHAR_TO_UNICHAR(t1b[nextfindex]));
          // Get next character from the next node.
          else
            nextChar = PeekNextChar(state);

          if (nextChar == NBSP_CHARCODE) nextChar = CHAR_TO_UNICHAR(' ');

          // If a word break isn't there when it needs to be, reset search.
          if (!mWordBreaker->BreakInBetween(&c, 1, &nextChar, 1)) {
            matchAnchorNode = nullptr;
            continue;
          }
        }

        RefPtr<nsRange> range = new nsRange(current);

        int32_t matchStartOffset;
        int32_t matchEndOffset;
        // convert char index to range point:
        int32_t mao = matchAnchorOffset + (mFindBackward ? 1 : 0);
        Text* startParent;
        Text* endParent;
        if (mFindBackward) {
          startParent = current;
          endParent = matchAnchorNode;
          matchStartOffset = findex;
          matchEndOffset = mao;
        } else {
          startParent = matchAnchorNode;
          endParent = current;
          matchStartOffset = mao;
          matchEndOffset = findex + 1;
        }

        if (startParent && endParent && IsVisibleNode(startParent) &&
            IsVisibleNode(endParent)) {
          IgnoredErrorResult rv;
          range->SetStart(*startParent, matchStartOffset, rv);
          if (!rv.Failed()) {
            range->SetEnd(*endParent, matchEndOffset, rv);
          }
          if (!rv.Failed()) {
            range.forget(aRangeRet);
            return NS_OK;
          }
        }

        // This match is no good, continue on in document
        matchAnchorNode = nullptr;
      }

      if (matchAnchorNode) {
        // Not done, but still matching. Advance and loop around for the next
        // characters. But don't advance from a space to a non-space:
        if (!inWhitespace || DONE_WITH_PINDEX ||
            IsSpace(patStr[pindex + incr])) {
          pindex += incr;
          inWhitespace = false;
          DEBUG_FIND_PRINTF("Advancing pindex to %d\n", pindex);
        }

        continue;
      }
    }

    DEBUG_FIND_PRINTF("NOT: %c == %c\n", c, patc);

    // If we didn't match, go back to the beginning of patStr, and set findex
    // back to the next char after we started the current match.
    if (matchAnchorNode) {  // we're ending a partial match
      findex = matchAnchorOffset;
      state.mIterOffset = matchAnchorOffset;
      // +incr will be added to findex when we continue

      // Are we going back to a previous node?
      if (matchAnchorNode != state.GetCurrentNode()) {
        frag = nullptr;
        state.PositionAt(*matchAnchorNode);
        DEBUG_FIND_PRINTF("Repositioned anchor node\n");
      }
      DEBUG_FIND_PRINTF(
          "Ending a partial match; findex -> %d, mIterOffset -> %d\n", findex,
          state.mIterOffset);
    }
    matchAnchorNode = nullptr;
    matchAnchorOffset = 0;
    inWhitespace = false;
    pindex = mFindBackward ? patLen : 0;
    DEBUG_FIND_PRINTF("Setting findex back to %d, pindex to %d\n", findex,
                      pindex);
  }

  return NS_OK;
}
