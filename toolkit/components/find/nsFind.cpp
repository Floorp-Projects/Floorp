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
#include "mozilla/dom/Text.h"

using namespace mozilla;
using namespace mozilla::dom;

// Yikes!  Casting a char to unichar can fill with ones!
#define CHAR_TO_UNICHAR(c) ((char16_t)(unsigned char)c)

static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kCPreContentIteratorCID, NS_PRECONTENTITERATOR_CID);

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

// nsFindContentIterator is a special iterator that also goes through any
// existing <textarea>'s or text <input>'s editor to lookup the anonymous DOM
// content there.
//
// Details:
// 1) We use two iterators: The "outer-iterator" goes through the normal DOM.
// The "inner-iterator" goes through the anonymous DOM inside the editor.
//
// 2) [MaybeSetupInnerIterator] As soon as the outer-iterator's current node is
// changed, a check is made to see if the node is a <textarea> or a text <input>
// node. If so, an inner-iterator is created to lookup the anynomous contents of
// the editor underneath the text control.
//
// 3) When the inner-iterator is created, we position the outer-iterator 'after'
// (or 'before' in backward search) the text control to avoid revisiting that
// control.
//
// 4) As a consequence of searching through text controls, we can be called via
// FindNext with the current selection inside a <textarea> or a text <input>.
// This means that we can be given an initial search range that stretches across
// the anonymous DOM and the normal DOM. To cater for this situation, we split
// the anonymous part into the inner-iterator and then reposition the outer-
// iterator outside.
//
// 5) The implementation assumes that First() and Next() are only called in
// find-forward mode, while Last() and Prev() are used in find-backward.

class nsFindContentIterator final : public nsIContentIterator
{
public:
  explicit nsFindContentIterator(bool aFindBackward)
    : mStartOffset(0)
    , mEndOffset(0)
    , mFindBackward(aFindBackward)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsFindContentIterator)

  // nsIContentIterator
  virtual nsresult Init(nsINode* aRoot) override
  {
    MOZ_ASSERT_UNREACHABLE("internal error");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  virtual nsresult Init(nsRange* aRange) override
  {
    MOZ_ASSERT_UNREACHABLE("internal error");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  virtual nsresult Init(nsINode* aStartContainer, uint32_t aStartOffset,
                        nsINode* aEndContainer, uint32_t aEndOffset) override
  {
    MOZ_ASSERT_UNREACHABLE("internal error");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  virtual nsresult Init(const RawRangeBoundary& aStart,
                        const RawRangeBoundary& aEnd) override
  {
    MOZ_ASSERT_UNREACHABLE("internal error");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  // Not a range because one of the endpoints may be anonymous.
  nsresult Init(nsINode* aStartNode, int32_t aStartOffset,
                nsINode* aEndNode, int32_t aEndOffset);
  virtual void First() override;
  virtual void Last() override;
  virtual void Next() override;
  virtual void Prev() override;
  virtual nsINode* GetCurrentNode() override;
  virtual bool IsDone() override;
  virtual nsresult PositionAt(nsINode* aCurNode) override;

  void Reset();
protected:
  virtual ~nsFindContentIterator() {}

private:
  static already_AddRefed<nsRange> CreateRange(nsINode* aNode)
  {
    RefPtr<nsRange> range = new nsRange(aNode);
    range->SetMaySpanAnonymousSubtrees(true);
    return range.forget();
  }

  nsCOMPtr<nsIContentIterator> mOuterIterator;
  nsCOMPtr<nsIContentIterator> mInnerIterator;
  // Can't use a range here, since we want to represent part of the flattened
  // tree, including native anonymous content.
  nsCOMPtr<nsINode> mStartNode;
  int32_t mStartOffset;
  nsCOMPtr<nsINode> mEndNode;
  int32_t mEndOffset;

  nsCOMPtr<nsIContent> mStartOuterContent;
  nsCOMPtr<nsIContent> mEndOuterContent;
  bool mFindBackward;

  void MaybeSetupInnerIterator();
  void SetupInnerIterator(nsIContent* aContent);
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFindContentIterator)
  NS_INTERFACE_MAP_ENTRY(nsIContentIterator)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFindContentIterator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFindContentIterator)

NS_IMPL_CYCLE_COLLECTION(nsFindContentIterator, mOuterIterator, mInnerIterator,
                         mStartOuterContent, mEndOuterContent, mEndNode,
                         mStartNode)

nsresult
nsFindContentIterator::Init(nsINode* aStartNode, int32_t aStartOffset,
                            nsINode* aEndNode, int32_t aEndOffset)
{
  NS_ENSURE_ARG_POINTER(aStartNode);
  NS_ENSURE_ARG_POINTER(aEndNode);
  if (!mOuterIterator) {
    if (mFindBackward) {
      // Use post-order in the reverse case, so we get parents before children
      // in case we want to prevent descending into a node.
      mOuterIterator = do_CreateInstance(kCContentIteratorCID);
    } else {
      // Use pre-order in the forward case, so we get parents before children in
      // case we want to prevent descending into a node.
      mOuterIterator = do_CreateInstance(kCPreContentIteratorCID);
    }
    NS_ENSURE_ARG_POINTER(mOuterIterator);
  }

  // Set up the search "range" that we will examine
  mStartNode = aStartNode;
  mStartOffset = aStartOffset;
  mEndNode = aEndNode;
  mEndOffset = aEndOffset;

  return NS_OK;
}

void
nsFindContentIterator::First()
{
  Reset();
}

void
nsFindContentIterator::Last()
{
  Reset();
}

void
nsFindContentIterator::Next()
{
  if (mInnerIterator) {
    mInnerIterator->Next();
    if (!mInnerIterator->IsDone()) {
      return;
    }

    // by construction, mOuterIterator is already on the next node
  } else {
    mOuterIterator->Next();
  }
  MaybeSetupInnerIterator();
}

void
nsFindContentIterator::Prev()
{
  if (mInnerIterator) {
    mInnerIterator->Prev();
    if (!mInnerIterator->IsDone()) {
      return;
    }

    // by construction, mOuterIterator is already on the previous node
  } else {
    mOuterIterator->Prev();
  }
  MaybeSetupInnerIterator();
}

nsINode*
nsFindContentIterator::GetCurrentNode()
{
  if (mInnerIterator && !mInnerIterator->IsDone()) {
    return mInnerIterator->GetCurrentNode();
  }
  return mOuterIterator->GetCurrentNode();
}

bool
nsFindContentIterator::IsDone()
{
  if (mInnerIterator && !mInnerIterator->IsDone()) {
    return false;
  }
  return mOuterIterator->IsDone();
}

static nsIContent&
AnonymousSubtreeRootParent(nsINode& aNode)
{
  MOZ_ASSERT(aNode.IsInNativeAnonymousSubtree());

  nsIContent* current = aNode.GetParent();
  while (current->IsInNativeAnonymousSubtree()) {
    current = current->GetParent();
    MOZ_ASSERT(current, "huh?");
  }
  return *current;
}

nsresult
nsFindContentIterator::PositionAt(nsINode* aCurNode)
{
  nsresult rv = mOuterIterator->PositionAt(aCurNode);
  if (NS_SUCCEEDED(rv)) {
    MaybeSetupInnerIterator();
    return rv;
  }

  // If this failed, it means that aCurNode is necessarily anonymous.
  nsIContent& nonAnonNode = AnonymousSubtreeRootParent(*aCurNode);
  SetupInnerIterator(&nonAnonNode);
  MOZ_ASSERT(mInnerIterator, "How did we have an anonymous node otherwise?");
  rv = mInnerIterator->PositionAt(aCurNode);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (!mOuterIterator->IsDone()) {
    if (mFindBackward) {
      mOuterIterator->Last();
    } else {
      mOuterIterator->First();
    }
  }
  return rv;
}

void
nsFindContentIterator::Reset()
{
  mInnerIterator = nullptr;
  mStartOuterContent = nullptr;
  mEndOuterContent = nullptr;

  // As a consequence of searching through text controls, we may have been
  // initialized with a selection inside a <textarea> or a text <input>.

  // see if the start node is an anonymous text node inside a text control
  nsCOMPtr<nsIContent> startContent(do_QueryInterface(mStartNode));
  if (startContent) {
    mStartOuterContent = startContent->FindFirstNonChromeOnlyAccessContent();
  }

  // see if the end node is an anonymous text node inside a text control
  nsCOMPtr<nsIContent> endContent(do_QueryInterface(mEndNode));
  if (endContent) {
    mEndOuterContent = endContent->FindFirstNonChromeOnlyAccessContent();
  }

  // Note: OK to just set up the outer iterator here; if our range has a native
  // anonymous endpoint we'll end up setting up an inner iterator, and reset the
  // outer one in the process.
  nsCOMPtr<nsINode> node = mStartNode;
  NS_ENSURE_TRUE_VOID(node);

  RefPtr<nsRange> range = CreateRange(node);
  range->SetStart(*mStartNode, mStartOffset, IgnoreErrors());
  range->SetEnd(*mEndNode, mEndOffset, IgnoreErrors());
  mOuterIterator->Init(range);

  if (!mFindBackward) {
    if (mStartOuterContent != startContent) {
      // the start node was an anonymous text node
      SetupInnerIterator(mStartOuterContent);
      if (mInnerIterator) {
        mInnerIterator->First();
      }
    }
    if (!mOuterIterator->IsDone()) {
      mOuterIterator->First();
    }
  } else {
    if (mEndOuterContent != endContent) {
      // the end node was an anonymous text node
      SetupInnerIterator(mEndOuterContent);
      if (mInnerIterator) {
        mInnerIterator->Last();
      }
    }
    if (!mOuterIterator->IsDone()) {
      mOuterIterator->Last();
    }
  }

  // if we didn't create an inner-iterator, the boundary node could still be
  // a text control, in which case we also need an inner-iterator straightaway
  if (!mInnerIterator) {
    MaybeSetupInnerIterator();
  }
}

void
nsFindContentIterator::MaybeSetupInnerIterator()
{
  mInnerIterator = nullptr;

  nsCOMPtr<nsIContent> content =
    do_QueryInterface(mOuterIterator->GetCurrentNode());
  if (!content || !content->IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) {
    return;
  }

  nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(content));
  if (!formControl->IsTextControl(true)) {
    return;
  }

  SetupInnerIterator(content);
  if (mInnerIterator) {
    if (!mFindBackward) {
      mInnerIterator->First();
      // finish setup: position mOuterIterator on the actual "next" node (this
      // completes its re-init, @see SetupInnerIterator)
      if (!mOuterIterator->IsDone()) {
        mOuterIterator->First();
      }
    } else {
      mInnerIterator->Last();
      // finish setup: position mOuterIterator on the actual "previous" node
      // (this completes its re-init, @see SetupInnerIterator)
      if (!mOuterIterator->IsDone()) {
        mOuterIterator->Last();
      }
    }
  }
}

void
nsFindContentIterator::SetupInnerIterator(nsIContent* aContent)
{
  if (!aContent) {
    return;
  }
  NS_ASSERTION(!aContent->IsRootOfNativeAnonymousSubtree(), "invalid call");

  nsITextControlFrame* tcFrame = do_QueryFrame(aContent->GetPrimaryFrame());
  if (!tcFrame) {
    return;
  }

  // don't mess with disabled input fields
  RefPtr<TextEditor> textEditor = tcFrame->GetTextEditor();
  if (!textEditor || textEditor->IsDisabled()) {
    return;
  }

  RefPtr<dom::Element> rootElement = textEditor->GetRoot();

  if (!rootElement) {
    return;
  }

  RefPtr<nsRange> innerRange = CreateRange(aContent);
  RefPtr<nsRange> outerRange = CreateRange(aContent);
  if (!innerRange || !outerRange) {
    return;
  }

  // now create the inner-iterator
  mInnerIterator = do_CreateInstance(kCPreContentIteratorCID);

  if (mInnerIterator) {
    innerRange->SelectNodeContents(*rootElement, IgnoreErrors());

    // fix up the inner bounds, we may have to only lookup a portion
    // of the text control if the current node is a boundary point
    if (aContent == mStartOuterContent) {
      innerRange->SetStart(*mStartNode, mStartOffset, IgnoreErrors());
    }
    if (aContent == mEndOuterContent) {
      innerRange->SetEnd(*mEndNode, mEndOffset, IgnoreErrors());
    }
    // Note: we just init here. We do First() or Last() later.
    mInnerIterator->Init(innerRange);

    // make sure to place the outer-iterator outside the text control so that we
    // don't go there again.
    IgnoredErrorResult res1, res2;
    if (!mFindBackward) { // find forward
      // cut the outer-iterator after the current node
      outerRange->SetEnd(*mEndNode, mEndOffset, res1);
      outerRange->SetStartAfter(*aContent, res2);
    } else { // find backward
      // cut the outer-iterator before the current node
      outerRange->SetStart(*mStartNode, mStartOffset, res1);
      outerRange->SetEndBefore(*aContent, res2);
    }
    if (res1.Failed() || res2.Failed()) {
      // we are done with the outer-iterator, the inner-iterator will traverse
      // what we want
      outerRange->Collapse(true);
    }

    // Note: we just re-init here, using the segment of our search range that
    // is yet to be visited. Thus when we later do mOuterIterator->First() [or
    // mOuterIterator->Last()], we will effectively be on the next node [or
    // the previous node] _with respect to_ the search range.
    mOuterIterator->Init(outerRange);
  }
}

nsresult
NS_NewFindContentIterator(bool aFindBackward, nsIContentIterator** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsFindContentIterator* it = new nsFindContentIterator(aFindBackward);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContentIterator), (void**)aResult);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFind)
  NS_INTERFACE_MAP_ENTRY(nsIFind)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFind)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFind)

NS_IMPL_CYCLE_COLLECTION(nsFind)

nsFind::nsFind()
  : mFindBackward(false)
  , mCaseSensitive(false)
  , mWordBreaker(nullptr)
{
}

nsFind::~nsFind() = default;

#ifdef DEBUG_FIND
#define DEBUG_FIND_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_FIND_PRINTF(...) /* nothing */
#endif

static void
DumpNode(const nsINode* aNode)
{
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

static bool
IsBlockNode(const nsIContent* aContent)
{
  if (aContent->IsElement() && aContent->AsElement()->IsDisplayContents()) {
    return false;
  }

  // FIXME(emilio): This is dubious...
  if (aContent->IsAnyOfHTMLElements(nsGkAtoms::img,
                                    nsGkAtoms::hr,
                                    nsGkAtoms::th,
                                    nsGkAtoms::td)) {
    return true;
  }

  nsIFrame* frame = aContent->GetPrimaryFrame();
  return frame && frame->StyleDisplay()->IsBlockOutsideStyle();
}

static bool
IsDisplayedNode(const nsINode* aNode)
{
  if (!aNode->IsContent()) {
    return false;
  }

  if (aNode->AsContent()->GetPrimaryFrame()) {
    return true;
  }

  // If there's no frame, it's not displayed, unless it's display: contents.
  return aNode->IsElement() && aNode->AsElement()->IsDisplayContents();
}

static bool
IsVisibleNode(const nsINode* aNode)
{
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

static bool
IsTextFormControl(nsIContent& aContent)
{
  if (!aContent.IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) {
    return false;
  }

  nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(&aContent);
  return formControl->IsTextControl(true);
}

static bool
SkipNode(const nsIContent* aContent)
{
  const nsIContent* content = aContent;
  while (content) {
    if (!IsDisplayedNode(content) ||
        content->IsComment() ||
        content->IsAnyOfHTMLElements(nsGkAtoms::script,
                                     nsGkAtoms::noframes,
                                     nsGkAtoms::select)) {
      DEBUG_FIND_PRINTF("Skipping node: ");
      DumpNode(content);
      return true;
    }

    // Skip NAC in non-form-control.
    if (content->IsInNativeAnonymousSubtree() &&
        !IsTextFormControl(AnonymousSubtreeRootParent(*content))) {
      return true;
    }

    // Only climb to the nearest block node
    if (IsBlockNode(content)) {
      return false;
    }

    content = content->GetParent();
  }

  return false;
}

static const nsIContent*
GetBlockParent(const Text* aNode)
{
  // FIXME(emilio): This should use GetFlattenedTreeParent instead to properly
  // handle Shadow DOM.
  for (const nsIContent* current = aNode->GetParent(); current;
       current = current->GetParent()) {
    if (IsBlockNode(current)) {
      return current;
    }
  }
  return nullptr;
}

struct nsFind::State final
{
  State(bool aFindBackward, nsIContent& aRoot, const nsRange& aStartPoint)
    : mFindBackward(aFindBackward)
    , mInitialized(false)
    , mIterOffset(-1)
    , mLastBlockParent(nullptr)
    , mIterator(aRoot)
    , mStartPoint(aStartPoint)
  {
  }

  void PositionAt(Text& aNode)
  {
    mIterator.Seek(aNode);
  }

  Text* GetCurrentNode() const
  {
    MOZ_ASSERT(mInitialized);
    nsINode* node = mIterator.GetCurrent();
    MOZ_ASSERT(!node || node->IsText());
    return node ? node->GetAsText() : nullptr;
  }

  Text* GetNextNode()
  {
    if (MOZ_UNLIKELY(!mInitialized)) {
      Initialize();
    } else {
      Advance();
      mIterOffset = -1; // mIterOffset only really applies to the first node.
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

void
nsFind::State::Advance()
{
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

void
nsFind::State::Initialize()
{
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

  mLastBlockParent = GetBlockParent(current->AsText());

  if (current != beginning) {
    return;
  }

  mIterOffset = mFindBackward ? mStartPoint.EndOffset()
                              : mStartPoint.StartOffset();
}

const nsTextFragment*
nsFind::State::GetNextNonEmptyTextFragmentInSameBlock()
{
  while (true) {
    const Text* current = GetNextNode();
    if (!current) {
      return nullptr;
    }

    const nsIContent* blockParent = GetBlockParent(current);
    if (!blockParent || blockParent != mLastBlockParent) {
      return nullptr;
    }

    const nsTextFragment& frag = current->TextFragment();
    if (frag.GetLength()) {
      return &frag;
    }
  }
}

class MOZ_STACK_CLASS nsFind::StateRestorer final
{
public:
  explicit StateRestorer(State& aState)
    : mState(aState)
    , mIterOffset(aState.mIterOffset)
    , mCurrNode(aState.GetCurrentNode())
    , mLastBlockParent(aState.mLastBlockParent)
  {
  }

  ~StateRestorer()
  {
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
nsFind::GetFindBackwards(bool* aFindBackward)
{
  if (!aFindBackward) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFindBackward = mFindBackward;
  return NS_OK;
}

NS_IMETHODIMP
nsFind::SetFindBackwards(bool aFindBackward)
{
  mFindBackward = aFindBackward;
  return NS_OK;
}

NS_IMETHODIMP
nsFind::GetCaseSensitive(bool* aCaseSensitive)
{
  if (!aCaseSensitive) {
    return NS_ERROR_NULL_POINTER;
  }

  *aCaseSensitive = mCaseSensitive;
  return NS_OK;
}

NS_IMETHODIMP
nsFind::SetCaseSensitive(bool aCaseSensitive)
{
  mCaseSensitive = aCaseSensitive;
  return NS_OK;
}

/* attribute boolean entireWord; */
NS_IMETHODIMP
nsFind::GetEntireWord(bool *aEntireWord)
{
  if (!aEntireWord)
    return NS_ERROR_NULL_POINTER;

  *aEntireWord = !!mWordBreaker;
  return NS_OK;
}

NS_IMETHODIMP
nsFind::SetEntireWord(bool aEntireWord)
{
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

char16_t
nsFind::PeekNextChar(State& aState) const
{
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
#define ALMOST_DONE_WITH_PINDEX (mFindBackward ? pindex <= 0 : pindex >= patLen - 1)

// Take nodes out of the tree with NextNode, until null (NextNode will return 0
// at the end of our range).
NS_IMETHODIMP
nsFind::Find(const char16_t* aPatText, nsRange* aSearchRange,
             nsRange* aStartPoint, nsRange* aEndPoint,
             nsRange** aRangeRet)
{
  DEBUG_FIND_PRINTF("============== nsFind::Find('%s'%s, %p, %p, %p)\n",
                    NS_LossyConvertUTF16toASCII(aPatText).get(),
                    mFindBackward ? " (backward)" : " (forward)",
                    (void*)aSearchRange, (void*)aStartPoint, (void*)aEndPoint);

  NS_ENSURE_ARG(aSearchRange);
  NS_ENSURE_ARG(aStartPoint);
  NS_ENSURE_ARG(aEndPoint);
  NS_ENSURE_ARG_POINTER(aRangeRet);

  nsIDocument* document =
    aStartPoint->GetRoot() ? aStartPoint->GetRoot()->OwnerDoc() : nullptr;
  NS_ENSURE_ARG(document);

  Element* root = document->GetRootElement();
  NS_ENSURE_ARG(root);

  *aRangeRet = 0;

  if (!aPatText) {
    return NS_ERROR_NULL_POINTER;
  }

  nsAutoString patAutoStr(aPatText);
  if (!mCaseSensitive) {
    ToLowerCase(patAutoStr);
  }

  // Ignore soft hyphens in the pattern
  static const char kShy[] = { char(CH_SHY), 0 };
  patAutoStr.StripChars(kShy);

  const char16_t* patStr = patAutoStr.get();
  int32_t patLen = patAutoStr.Length() - 1;

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
      const nsIContent* blockParent = GetBlockParent(current);
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
        DEBUG_FIND_PRINTF("At the end of a text node -- skipping to the next\n");
        frag = nullptr;
        continue;
      }

      if (frag->Is2b()) {
        t2b = frag->Get2b();
        t1b = nullptr;
#ifdef DEBUG_FIND
        nsAutoString str2(t2b, fragLen);
        DEBUG_FIND_PRINTF("2 byte, '%s'\n", NS_LossyConvertUTF16toASCII(str2).get());
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
        DEBUG_FIND_PRINTF("Will need to pull a new node: mAO = %d, frag len=%d\n",
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
      if (prevChar == NBSP_CHARCODE)
        prevChar = CHAR_TO_UNICHAR(' ');
      wordBreakPrev = mWordBreaker->BreakInBetween(&prevChar, 1, &c, 1);
    }

    // Compare. Match if we're in whitespace and c is whitespace, or if the
    // characters match and at least one of the following is true:
    // a) we're not matching the entire word
    // b) a match has already been stored
    // c) the previous character is a different "class" than the current character.
    if ((c == patc && (!mWordBreaker || matchAnchorNode || wordBreakPrev)) ||
        (inWhitespace && IsSpace(c)))
    {
      prevCharInMatch = c;
      if (inWhitespace) {
        DEBUG_FIND_PRINTF("YES (whitespace)(%d of %d)\n", pindex, patLen);
      } else {
        DEBUG_FIND_PRINTF("YES! '%c' == '%c' (%d of %d)\n", c, patc, pindex, patLen);
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
            nextChar = (t2b ? t2b[nextfindex] : CHAR_TO_UNICHAR(t1b[nextfindex]));
          // Get next character from the next node.
          else
            nextChar = PeekNextChar(state);

          if (nextChar == NBSP_CHARCODE)
            nextChar = CHAR_TO_UNICHAR(' ');

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
        if (startParent && endParent &&
            IsVisibleNode(startParent) && IsVisibleNode(endParent)) {
          range->SetStart(*startParent, matchStartOffset, IgnoreErrors());
          range->SetEnd(*endParent, matchEndOffset, IgnoreErrors());
          *aRangeRet = range.get();
          NS_ADDREF(*aRangeRet);
        } else {
          // This match is no good -- invisible or bad range
          startParent = nullptr;
        }

        if (startParent) {
          return NS_OK;
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
    if (matchAnchorNode) { // we're ending a partial match
      findex = matchAnchorOffset;
      state.mIterOffset = matchAnchorOffset;
      // +incr will be added to findex when we continue

      // Are we going back to a previous node?
      if (matchAnchorNode != state.GetCurrentNode()) {
        frag = nullptr;
        state.PositionAt(*matchAnchorNode);
        DEBUG_FIND_PRINTF("Repositioned anchor node\n");
      }
      DEBUG_FIND_PRINTF("Ending a partial match; findex -> %d, mIterOffset -> %d\n",
                        findex, state.mIterOffset);
    }
    matchAnchorNode = nullptr;
    matchAnchorOffset = 0;
    inWhitespace = false;
    pindex = mFindBackward ? patLen : 0;
    DEBUG_FIND_PRINTF("Setting findex back to %d, pindex to %d\n", findex, pindex);
  }

  return NS_OK;
}
