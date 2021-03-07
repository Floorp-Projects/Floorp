/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define DEBUG_FIND 1

#include "nsFind.h"
#include "mozilla/Likely.h"
#include "nsContentCID.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsIFrame.h"
#include "nsITextControlFrame.h"
#include "nsIFormControl.h"
#include "nsTextFragment.h"
#include "nsString.h"
#include "nsAtom.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"
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
#include "mozilla/StaticPrefs_browser.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::unicode;

// Yikes!  Casting a char to unichar can fill with ones!
#define CHAR_TO_UNICHAR(c) ((char16_t)(unsigned char)c)

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
    : mFindBackward(false),
      mCaseSensitive(false),
      mMatchDiacritics(false),
      mWordBreaker(nullptr) {}

nsFind::~nsFind() = default;

#ifdef DEBUG_FIND
#  define DEBUG_FIND_PRINTF(...) printf(__VA_ARGS__)
#else
#  define DEBUG_FIND_PRINTF(...) /* nothing */
#endif

static nsIContent& AnonymousSubtreeRootParent(const nsINode& aNode) {
  MOZ_ASSERT(aNode.IsInNativeAnonymousSubtree());
  return *aNode.GetClosestNativeAnonymousSubtreeRootParent();
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

static bool ShouldFindAnonymousContent(const nsIContent& aContent) {
  MOZ_ASSERT(aContent.IsInNativeAnonymousSubtree());

  nsIContent& parent = AnonymousSubtreeRootParent(aContent);
  if (nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(&parent)) {
    if (formControl->IsTextControl(/* aExcludePassword = */ true)) {
      // Only editable NAC in textfields should be findable. That is, we want to
      // find "bar" in `<input value="bar">`, but not in `<input
      // placeholder="bar">`.
      //
      // TODO(emilio): Ideally we could lift this restriction, but we hide the
      // placeholder text at paint-time instead of with CSS visibility, which
      // means that we won't skip it even if invisible. We should probably fix
      // that.
      return aContent.IsEditable();
    }

    // We want to avoid finding in password inputs anyway, as it is confusing.
    if (formControl->ControlType() == NS_FORM_INPUT_PASSWORD) {
      return false;
    }
  }

  return true;
}

static bool SkipNode(const nsIContent* aContent) {
  const nsIContent* content = aContent;
  while (content) {
    if (!IsDisplayedNode(content) || content->IsComment() ||
        content->IsAnyOfHTMLElements(nsGkAtoms::select)) {
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

    if (content->IsInNativeAnonymousSubtree() &&
        !ShouldFindAnonymousContent(*content)) {
      DEBUG_FIND_PRINTF("Skipping node: ");
      DumpNode(content);
      return true;
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

static bool NonTextNodeForcesBreak(const nsINode& aNode) {
  nsIFrame* frame =
      aNode.IsContent() ? aNode.AsContent()->GetPrimaryFrame() : nullptr;
  // TODO(emilio): Maybe we should treat <br> more like a space instead of a
  // forced break? Unclear...
  return frame && frame->IsBrFrame();
}

static bool ForceBreakBetweenText(const Text& aPrevious, const Text& aNext) {
  return GetBlockParent(aPrevious) != GetBlockParent(aNext);
}

struct nsFind::State final {
  State(bool aFindBackward, nsIContent& aRoot, const nsRange& aStartPoint)
      : mFindBackward(aFindBackward),
        mInitialized(false),
        mFoundBreak(false),
        mIterOffset(-1),
        mIterator(aRoot),
        mStartPoint(aStartPoint) {}

  void PositionAt(Text& aNode) { mIterator.Seek(aNode); }

  bool ForcedBreak() const { return mFoundBreak; }

  Text* GetCurrentNode() const {
    if (MOZ_UNLIKELY(!mInitialized)) {
      return nullptr;
    }
    nsINode* node = mIterator.GetCurrent();
    MOZ_ASSERT(!node || node->IsText());
    return node ? node->GetAsText() : nullptr;
  }

  Text* GetNextNode(bool aAlreadyMatching) {
    if (MOZ_UNLIKELY(!mInitialized)) {
      MOZ_ASSERT(!aAlreadyMatching);
      Initialize();
    } else {
      Advance(Initializing::No, aAlreadyMatching);
      mIterOffset = -1;  // mIterOffset only really applies to the first node.
    }
    return GetCurrentNode();
  }

 private:
  enum class Initializing { No, Yes };

  // Advance to the next visible text-node.
  void Advance(Initializing, bool aAlreadyMatching);
  // Sets up the first node position and offset.
  void Initialize();

  // Returns whether the node should be used (true) or skipped over (false)
  static bool AnalyzeNode(const nsINode& aNode, const Text* aPrev,
                          bool aAlreadyMatching, bool* aForcedBreak) {
    if (!aNode.IsText()) {
      *aForcedBreak = *aForcedBreak || NonTextNodeForcesBreak(aNode);
      return false;
    }
    if (SkipNode(aNode.AsText())) {
      return false;
    }
    *aForcedBreak = *aForcedBreak ||
                    (aPrev && ForceBreakBetweenText(*aPrev, *aNode.AsText()));
    if (*aForcedBreak) {
      // If we've already found a break, we can stop searching and just use this
      // node, regardless of the subtree we're on. There's no point to continue
      // a match across different blocks, regardless of which subtree you're
      // looking into.
      return true;
    }

    // TODO(emilio): We can't represent ranges that span native anonymous /
    // shadow tree boundaries, but if we did the following check could / should
    // be removed.
    if (aAlreadyMatching && aPrev &&
        !nsContentUtils::IsInSameAnonymousTree(&aNode, aPrev)) {
      // As an optimization, if we were finding inside an native-anonymous
      // subtree (like a pseudo-element), we know those trees are "atomic" and
      // can't have any other subtrees in between, so we can just break the
      // match here.
      if (aPrev->IsInNativeAnonymousSubtree()) {
        *aForcedBreak = true;
        return true;
      }
      // Otherwise we can skip the node and keep looking past this subtree.
      return false;
    }

    return true;
  }

  const bool mFindBackward;

  // Whether we've called GetNextNode() at least once.
  bool mInitialized;

 public:
  // Whether we've found a forced break from the last node to the current one.
  bool mFoundBreak;
  // An offset into the text of the first node we're starting to search at.
  int mIterOffset;
  TreeIterator<StyleChildrenIterator> mIterator;

  // These are only needed for the first GetNextNode() call.
  const nsRange& mStartPoint;
};

void nsFind::State::Advance(Initializing aInitializing, bool aAlreadyMatching) {
  MOZ_ASSERT(mInitialized);

  // The Advance() call during Initialize() calls us in a partial state, where
  // mIterator may not be pointing to a text node yet. aInitializing prevents
  // tripping the invariants of GetCurrentNode().
  const Text* prev =
      aInitializing == Initializing::Yes ? nullptr : GetCurrentNode();
  mFoundBreak = false;

  while (true) {
    nsIContent* current =
        mFindBackward ? mIterator.GetPrev() : mIterator.GetNext();
    if (!current) {
      return;
    }
    if (AnalyzeNode(*current, prev, aAlreadyMatching, &mFoundBreak)) {
      break;
    }
  }
}

void nsFind::State::Initialize() {
  MOZ_ASSERT(!mInitialized);
  mInitialized = true;
  mIterOffset = mFindBackward ? -1 : 0;

  nsINode* container = mFindBackward ? mStartPoint.GetStartContainer()
                                     : mStartPoint.GetEndContainer();

  // Set up ourselves at the first node we want to start searching at.
  nsIContent* beginning = mFindBackward ? mStartPoint.GetChildAtStartOffset()
                                        : mStartPoint.GetChildAtEndOffset();
  if (beginning) {
    mIterator.Seek(*beginning);
    // If the start point is pointing to a node, when looking backwards we'd
    // start looking at the children of that node, and we don't really want
    // that. When looking forwards, we look at the next sibling afterwards.
    if (mFindBackward) {
      mIterator.GetPrevSkippingChildren();
    }
  } else if (container && container->IsContent()) {
    // Text-only range, or pointing to past the end of the node, for example.
    mIterator.Seek(*container->AsContent());
  }

  nsINode* current = mIterator.GetCurrent();
  if (!current) {
    return;
  }

  const bool kAlreadyMatching = false;
  if (!AnalyzeNode(*current, nullptr, kAlreadyMatching, &mFoundBreak)) {
    Advance(Initializing::Yes, kAlreadyMatching);
    current = mIterator.GetCurrent();
    if (!current) {
      return;
    }
  }

  if (current != container) {
    return;
  }

  mIterOffset =
      mFindBackward ? mStartPoint.StartOffset() : mStartPoint.EndOffset();
}

class MOZ_STACK_CLASS nsFind::StateRestorer final {
 public:
  explicit StateRestorer(State& aState)
      : mState(aState),
        mIterOffset(aState.mIterOffset),
        mFoundBreak(aState.mFoundBreak),
        mCurrNode(aState.GetCurrentNode()) {}

  ~StateRestorer() {
    mState.mFoundBreak = mFoundBreak;
    mState.mIterOffset = mIterOffset;
    if (mCurrNode) {
      mState.PositionAt(*mCurrNode);
    }
  }

 private:
  State& mState;

  int32_t mIterOffset;
  bool mFoundBreak;
  Text* mCurrNode;
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

NS_IMETHODIMP
nsFind::GetMatchDiacritics(bool* aMatchDiacritics) {
  if (!aMatchDiacritics) {
    return NS_ERROR_NULL_POINTER;
  }

  *aMatchDiacritics = mMatchDiacritics;
  return NS_OK;
}

NS_IMETHODIMP
nsFind::SetMatchDiacritics(bool aMatchDiacritics) {
  mMatchDiacritics = aMatchDiacritics;
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

char32_t nsFind::DecodeChar(const char16_t* t2b, int32_t* index) const {
  char32_t c = t2b[*index];
  if (mFindBackward) {
    if (*index >= 1 && NS_IS_SURROGATE_PAIR(t2b[*index - 1], t2b[*index])) {
      c = SURROGATE_TO_UCS4(t2b[*index - 1], t2b[*index]);
      (*index)--;
    }
  } else {
    if (NS_IS_SURROGATE_PAIR(t2b[*index], t2b[*index + 1])) {
      c = SURROGATE_TO_UCS4(t2b[*index], t2b[*index + 1]);
      (*index)++;
    }
  }
  return c;
}

bool nsFind::BreakInBetween(char32_t x, char32_t y) const {
  char16_t x16[2], y16[2];
  int32_t x16len, y16len;
  if (IS_IN_BMP(x)) {
    x16[0] = (char16_t)x;
    x16len = 1;
  } else {
    x16[0] = H_SURROGATE(x);
    x16[1] = L_SURROGATE(x);
    x16len = 2;
  }
  if (IS_IN_BMP(y)) {
    y16[0] = (char16_t)y;
    y16len = 1;
  } else {
    y16[0] = H_SURROGATE(y);
    y16[1] = L_SURROGATE(y);
    y16len = 2;
  }
  return mWordBreaker->BreakInBetween(x16, x16len, y16, y16len);
}

char32_t nsFind::PeekNextChar(State& aState, bool aAlreadyMatching) const {
  // We need to restore the necessary state before this function returns.
  StateRestorer restorer(aState);

  while (true) {
    const Text* text = aState.GetNextNode(aAlreadyMatching);
    if (!text || aState.ForcedBreak()) {
      return L'\0';
    }

    const nsTextFragment& frag = text->TextFragment();
    uint32_t len = frag.GetLength();
    if (!len) {
      continue;
    }

    const char16_t* t2b = nullptr;
    const char* t1b = nullptr;

    if (frag.Is2b()) {
      t2b = frag.Get2b();
    } else {
      t1b = frag.Get1b();
    }

    int32_t index = mFindBackward ? len - 1 : 0;
    return t1b ? CHAR_TO_UNICHAR(t1b[index]) : DecodeChar(t2b, &index);
  }
}

#define NBSP_CHARCODE (CHAR_TO_UNICHAR(160))
#define IsSpace(c) (nsCRT::IsAsciiSpace(c) || (c) == NBSP_CHARCODE)
#define OVERFLOW_PINDEX (mFindBackward ? pindex < 0 : pindex > patLen)
#define DONE_WITH_PINDEX (mFindBackward ? pindex <= 0 : pindex >= patLen)

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
    ToFoldedCase(patAutoStr);
  }
  if (!mMatchDiacritics) {
    ToNaked(patAutoStr);
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

  // Place to save the range start point in case we find a match:
  Text* matchAnchorNode = nullptr;
  int32_t matchAnchorOffset = 0;
  char32_t matchAnchorChar = 0;

  // Get the end point, so we know when to end searches:
  nsINode* endNode = aEndPoint->GetEndContainer();
  uint32_t endOffset = aEndPoint->EndOffset();

  char32_t c = 0;
  char32_t patc = 0;
  char32_t prevCharInMatch = 0;

  State state(mFindBackward, *root, *aStartPoint);
  Text* current = nullptr;

  auto EndPartialMatch = [&]() -> bool {
    bool hadAnchorNode = !!matchAnchorNode;
    // If we didn't match, go back to the beginning of patStr, and set findex
    // back to the next char after we started the current match.
    if (matchAnchorNode) {  // we're ending a partial match
      findex = matchAnchorOffset;
      state.mIterOffset = matchAnchorOffset;
      c = matchAnchorChar;
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
    matchAnchorChar = 0;
    inWhitespace = false;
    prevCharInMatch = 0;
    pindex = mFindBackward ? patLen : 0;
    DEBUG_FIND_PRINTF("Setting findex back to %d, pindex to %d\n", findex,
                      pindex);
    return hadAnchorNode;
  };

  while (true) {
    DEBUG_FIND_PRINTF("Loop (pindex = %d)...\n", pindex);

    // If this is our first time on a new node, reset the pointers:
    if (!frag) {
      current = state.GetNextNode(!!matchAnchorNode);
      if (!current) {
        DEBUG_FIND_PRINTF("Reached the end, matching: %d\n", !!matchAnchorNode);
        if (EndPartialMatch()) {
          continue;
        }
        return NS_OK;
      }

      // We have a new text content. See if we need to force a break due to
      // <br>, different blocks or what not.
      if (state.ForcedBreak()) {
        DEBUG_FIND_PRINTF("Forced break!\n");
        if (EndPartialMatch()) {
          continue;
        }
        // This ensures word breaking thinks it has a new word, which is
        // effectively what we want.
        c = 0;
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
      DEBUG_FIND_PRINTF("Reached the end and not in the middle of a match\n");
      return NS_OK;
    }

    // Save the previous character for word boundary detection
    char32_t prevChar = c;
    // The two characters we'll be comparing are c and patc. If not matching
    // diacritics, don't leave c set to a combining diacritical mark. (patc is
    // already guaranteed to not be a combining diacritical mark.)
    c = (t2b ? DecodeChar(t2b, &findex) : CHAR_TO_UNICHAR(t1b[findex]));
    if (!mMatchDiacritics && IsCombiningDiacritic(c) &&
        !IsMathSymbol(prevChar)) {
      continue;
    }
    patc = DecodeChar(patStr, &pindex);

    DEBUG_FIND_PRINTF(
        "Comparing '%c'=%#x to '%c'=%#x (%d of %d), findex=%d%s\n", (char)c,
        (int)c, (char)patc, (int)patc, pindex, patLen, findex,
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
      patc = DecodeChar(patStr, &pindex);
    }
    if (!inWhitespace && IsSpace(patc)) {
      inWhitespace = true;
    } else if (!inWhitespace) {
      if (!mCaseSensitive) {
        c = ToFoldedCase(c);
      }
      if (!mMatchDiacritics) {
        c = ToNaked(c);
      }
    }

    if (c == CH_SHY) {
      // ignore soft hyphens in the document
      continue;
    }

    if (pindex != (mFindBackward ? patLen : 0) && c != patc && !inWhitespace) {
      // A non-matching '\n' between CJ characters is ignored
      if (c == '\n' && t2b && IS_CJ_CHAR(prevCharInMatch)) {
        int32_t nindex = findex + incr;
        if (mFindBackward ? (nindex >= 0) : (nindex < fragLen)) {
          if (IS_CJ_CHAR(t2b[nindex])) {
            continue;
          }
        }
      }

      // We also ignore ZWSP and other default-ignorable characters.
      if (IsDefaultIgnorable(c)) {
        continue;
      }
    }

    // Figure whether the previous char is a word-breaking one.
    bool wordBreakPrev = false;
    if (mWordBreaker) {
      if (prevChar == NBSP_CHARCODE) {
        prevChar = CHAR_TO_UNICHAR(' ');
      }
      wordBreakPrev = BreakInBetween(prevChar, c);
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
        if (!IS_IN_BMP(c)) {
          matchAnchorOffset -= incr;
        }
        matchAnchorChar = c;
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
          if (mFindBackward ? (nextfindex >= 0) : (nextfindex < fragLen)) {
            if (t2b)
              nextChar = DecodeChar(t2b, &nextfindex);
            else
              nextChar = CHAR_TO_UNICHAR(t1b[nextfindex]);
          } else {
            // Get next character from the next node.
            nextChar = PeekNextChar(state, !!matchAnchorNode);
          }

          if (nextChar == NBSP_CHARCODE) {
            nextChar = CHAR_TO_UNICHAR(' ');
          }

          // If a word break isn't there when it needs to be, reset search.
          if (!BreakInBetween(c, nextChar)) {
            matchAnchorNode = nullptr;
            continue;
          }
        }

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

        RefPtr<nsRange> range = nsRange::Create(current);
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
    EndPartialMatch();
  }

  return NS_OK;
}
