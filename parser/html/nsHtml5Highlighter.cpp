/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5Highlighter.h"
#include "nsDebug.h"
#include "nsHtml5AttributeName.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5ViewSourceUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

// The old code had a limit of 16 tokens. 1300 is a number picked my measuring
// the size of 16 tokens on cnn.com.
#define NS_HTML5_HIGHLIGHTER_PRE_BREAK_THRESHOLD 1300

char16_t nsHtml5Highlighter::sComment[] = {
  'c', 'o', 'm', 'm', 'e', 'n', 't', 0
};

char16_t nsHtml5Highlighter::sCdata[] = { 'c', 'd', 'a', 't', 'a', 0 };

char16_t nsHtml5Highlighter::sEntity[] = { 'e', 'n', 't', 'i', 't', 'y', 0 };

char16_t nsHtml5Highlighter::sEndTag[] = {
  'e', 'n', 'd', '-', 't', 'a', 'g', 0
};

char16_t nsHtml5Highlighter::sStartTag[] = { 's', 't', 'a', 'r', 't',
                                             '-', 't', 'a', 'g', 0 };

char16_t nsHtml5Highlighter::sAttributeName[] = { 'a', 't', 't', 'r', 'i',
                                                  'b', 'u', 't', 'e', '-',
                                                  'n', 'a', 'm', 'e', 0 };

char16_t nsHtml5Highlighter::sAttributeValue[] = { 'a', 't', 't', 'r', 'i', 'b',
                                                   'u', 't', 'e', '-', 'v', 'a',
                                                   'l', 'u', 'e', 0 };

char16_t nsHtml5Highlighter::sDoctype[] = {
  'd', 'o', 'c', 't', 'y', 'p', 'e', 0
};

char16_t nsHtml5Highlighter::sPi[] = { 'p', 'i', 0 };

nsHtml5Highlighter::nsHtml5Highlighter(nsAHtml5TreeOpSink* aOpSink)
  : mState(nsHtml5Tokenizer::DATA)
  , mCStart(INT32_MAX)
  , mPos(0)
  , mLineNumber(1)
  , mInlinesOpen(0)
  , mInCharacters(false)
  , mBuffer(nullptr)
  , mOpSink(aOpSink)
  , mCurrentRun(nullptr)
  , mAmpersand(nullptr)
  , mSlash(nullptr)
  , mHandles(
      MakeUnique<nsIContent* []>(NS_HTML5_HIGHLIGHTER_HANDLE_ARRAY_LENGTH))
  , mHandlesUsed(0)
  , mSeenBase(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

nsHtml5Highlighter::~nsHtml5Highlighter()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

void
nsHtml5Highlighter::Start(const nsAutoString& aTitle)
{
  // Doctype
  mOpQueue.AppendElement()->Init(nsGkAtoms::html, EmptyString(), EmptyString());

  mOpQueue.AppendElement()->Init(STANDARDS_MODE);

  // <html> uses NS_NewHTMLSharedElement creator
  nsIContent** root =
    CreateElement(nsGkAtoms::html, nullptr, nullptr, NS_NewHTMLSharedElement);
  mOpQueue.AppendElement()->Init(eTreeOpAppendToDocument, root);
  mStack.AppendElement(root);

  // <head> uses NS_NewHTMLSharedElement creator
  Push(nsGkAtoms::head, nullptr, NS_NewHTMLSharedElement);

  Push(nsGkAtoms::meta,
       nsHtml5ViewSourceUtils::NewMetaViewportAttributes(),
       NS_NewHTMLMetaElement);
  Pop(); // meta

  Push(nsGkAtoms::title, nullptr, NS_NewHTMLTitleElement);
  // XUL will add the "Source of: " prefix.
  uint32_t length = aTitle.Length();
  if (length > INT32_MAX) {
    length = INT32_MAX;
  }
  AppendCharacters(aTitle.BeginReading(), 0, (int32_t)length);
  Pop(); // title

  Push(nsGkAtoms::link,
       nsHtml5ViewSourceUtils::NewLinkAttributes(),
       NS_NewHTMLLinkElement);

  mOpQueue.AppendElement()->Init(eTreeOpUpdateStyleSheet, CurrentNode());

  Pop(); // link

  Pop(); // head

  Push(nsGkAtoms::body,
       nsHtml5ViewSourceUtils::NewBodyAttributes(),
       NS_NewHTMLBodyElement);

  nsHtml5HtmlAttributes* preAttrs = new nsHtml5HtmlAttributes(0);
  nsHtml5String preId = nsHtml5Portability::newStringFromLiteral("line1");
  preAttrs->addAttribute(nsHtml5AttributeName::ATTR_ID, preId, -1);
  Push(nsGkAtoms::pre, preAttrs, NS_NewHTMLPreElement);

  StartCharacters();

  mOpQueue.AppendElement()->Init(eTreeOpStartLayout);
}

int32_t
nsHtml5Highlighter::Transition(int32_t aState, bool aReconsume, int32_t aPos)
{
  mPos = aPos;
  switch (mState) {
    case nsHtml5Tokenizer::SCRIPT_DATA:
    case nsHtml5Tokenizer::RAWTEXT:
    case nsHtml5Tokenizer::RCDATA:
    case nsHtml5Tokenizer::DATA:
      // We can transition on < and on &. Either way, we don't yet know the
      // role of the token, so open a span without class.
      if (aState == nsHtml5Tokenizer::CONSUME_CHARACTER_REFERENCE) {
        StartSpan();
        // Start another span for highlighting the ampersand
        StartSpan();
        mAmpersand = CurrentNode();
      } else {
        EndCharactersAndStartMarkupRun();
      }
      break;
    case nsHtml5Tokenizer::TAG_OPEN:
      switch (aState) {
        case nsHtml5Tokenizer::TAG_NAME:
          StartSpan(sStartTag);
          break;
        case nsHtml5Tokenizer::DATA:
          FinishTag(); // DATA
          break;
        case nsHtml5Tokenizer::PROCESSING_INSTRUCTION:
          AddClass(sPi);
          break;
      }
      break;
    case nsHtml5Tokenizer::TAG_NAME:
      switch (aState) {
        case nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME:
          EndSpanOrA(); // nsHtml5Tokenizer::TAG_NAME
          break;
        case nsHtml5Tokenizer::SELF_CLOSING_START_TAG:
          EndSpanOrA(); // nsHtml5Tokenizer::TAG_NAME
          StartSpan();  // for highlighting the slash
          mSlash = CurrentNode();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME:
      switch (aState) {
        case nsHtml5Tokenizer::ATTRIBUTE_NAME:
          StartSpan(sAttributeName);
          break;
        case nsHtml5Tokenizer::SELF_CLOSING_START_TAG:
          StartSpan(); // for highlighting the slash
          mSlash = CurrentNode();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case nsHtml5Tokenizer::ATTRIBUTE_NAME:
      switch (aState) {
        case nsHtml5Tokenizer::AFTER_ATTRIBUTE_NAME:
        case nsHtml5Tokenizer::BEFORE_ATTRIBUTE_VALUE:
          EndSpanOrA(); // nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME
          break;
        case nsHtml5Tokenizer::SELF_CLOSING_START_TAG:
          EndSpanOrA(); // nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME
          StartSpan();  // for highlighting the slash
          mSlash = CurrentNode();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case nsHtml5Tokenizer::BEFORE_ATTRIBUTE_VALUE:
      switch (aState) {
        case nsHtml5Tokenizer::ATTRIBUTE_VALUE_DOUBLE_QUOTED:
        case nsHtml5Tokenizer::ATTRIBUTE_VALUE_SINGLE_QUOTED:
          FlushCurrent();
          StartA();
          break;
        case nsHtml5Tokenizer::ATTRIBUTE_VALUE_UNQUOTED:
          StartA();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case nsHtml5Tokenizer::ATTRIBUTE_VALUE_DOUBLE_QUOTED:
    case nsHtml5Tokenizer::ATTRIBUTE_VALUE_SINGLE_QUOTED:
      switch (aState) {
        case nsHtml5Tokenizer::AFTER_ATTRIBUTE_VALUE_QUOTED:
          EndSpanOrA();
          break;
        case nsHtml5Tokenizer::CONSUME_CHARACTER_REFERENCE:
          StartSpan();
          StartSpan(); // for ampersand itself
          mAmpersand = CurrentNode();
          break;
        default:
          NS_NOTREACHED("Impossible transition.");
          break;
      }
      break;
    case nsHtml5Tokenizer::AFTER_ATTRIBUTE_VALUE_QUOTED:
      switch (aState) {
        case nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME:
          break;
        case nsHtml5Tokenizer::SELF_CLOSING_START_TAG:
          StartSpan(); // for highlighting the slash
          mSlash = CurrentNode();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case nsHtml5Tokenizer::SELF_CLOSING_START_TAG:
      EndSpanOrA(); // end the slash highlight
      switch (aState) {
        case nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME:
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case nsHtml5Tokenizer::ATTRIBUTE_VALUE_UNQUOTED:
      switch (aState) {
        case nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME:
          EndSpanOrA();
          break;
        case nsHtml5Tokenizer::CONSUME_CHARACTER_REFERENCE:
          StartSpan();
          StartSpan(); // for ampersand itself
          mAmpersand = CurrentNode();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case nsHtml5Tokenizer::AFTER_ATTRIBUTE_NAME:
      switch (aState) {
        case nsHtml5Tokenizer::SELF_CLOSING_START_TAG:
          StartSpan(); // for highlighting the slash
          mSlash = CurrentNode();
          break;
        case nsHtml5Tokenizer::BEFORE_ATTRIBUTE_VALUE:
          break;
        case nsHtml5Tokenizer::ATTRIBUTE_NAME:
          StartSpan(sAttributeName);
          break;
        default:
          FinishTag();
          break;
      }
      break;
    // most comment states are omitted, because they don't matter to
    // highlighting
    case nsHtml5Tokenizer::COMMENT_START:
    case nsHtml5Tokenizer::COMMENT_END:
    case nsHtml5Tokenizer::COMMENT_END_BANG:
    case nsHtml5Tokenizer::COMMENT_START_DASH:
    case nsHtml5Tokenizer::BOGUS_COMMENT:
    case nsHtml5Tokenizer::BOGUS_COMMENT_HYPHEN:
      if (aState == nsHtml5Tokenizer::DATA) {
        AddClass(sComment);
        FinishTag();
      }
      break;
    // most cdata states are omitted, because they don't matter to
    // highlighting
    case nsHtml5Tokenizer::CDATA_RSQB_RSQB:
      if (aState == nsHtml5Tokenizer::DATA) {
        AddClass(sCdata);
        FinishTag();
      }
      break;
    case nsHtml5Tokenizer::CONSUME_CHARACTER_REFERENCE:
      EndSpanOrA(); // the span for the ampersand
      switch (aState) {
        case nsHtml5Tokenizer::CONSUME_NCR:
        case nsHtml5Tokenizer::CHARACTER_REFERENCE_HILO_LOOKUP:
          break;
        default:
          // not actually a character reference
          EndSpanOrA();
          break;
      }
      break;
    case nsHtml5Tokenizer::CHARACTER_REFERENCE_HILO_LOOKUP:
      if (aState == nsHtml5Tokenizer::CHARACTER_REFERENCE_TAIL) {
        break;
      }
      // not actually a character reference
      EndSpanOrA();
      break;
    case nsHtml5Tokenizer::CHARACTER_REFERENCE_TAIL:
      if (!aReconsume) {
        FlushCurrent();
      }
      EndSpanOrA();
      break;
    case nsHtml5Tokenizer::DECIMAL_NRC_LOOP:
    case nsHtml5Tokenizer::HEX_NCR_LOOP:
      switch (aState) {
        case nsHtml5Tokenizer::HANDLE_NCR_VALUE:
          AddClass(sEntity);
          FlushCurrent();
          break;
        case nsHtml5Tokenizer::HANDLE_NCR_VALUE_RECONSUME:
          AddClass(sEntity);
          break;
      }
      EndSpanOrA();
      break;
    case nsHtml5Tokenizer::CLOSE_TAG_OPEN:
      switch (aState) {
        case nsHtml5Tokenizer::DATA:
          FinishTag();
          break;
        case nsHtml5Tokenizer::TAG_NAME:
          StartSpan(sEndTag);
          break;
      }
      break;
    case nsHtml5Tokenizer::RAWTEXT_RCDATA_LESS_THAN_SIGN:
      if (aState == nsHtml5Tokenizer::NON_DATA_END_TAG_NAME) {
        FlushCurrent();
        StartSpan(); // don't know if it is "end-tag" yet :-(
        break;
      }
      EndSpanOrA();
      StartCharacters();
      break;
    case nsHtml5Tokenizer::NON_DATA_END_TAG_NAME:
      switch (aState) {
        case nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME:
          AddClass(sEndTag);
          EndSpanOrA();
          break;
        case nsHtml5Tokenizer::SELF_CLOSING_START_TAG:
          AddClass(sEndTag);
          EndSpanOrA();
          StartSpan(); // for highlighting the slash
          mSlash = CurrentNode();
          break;
        case nsHtml5Tokenizer::DATA: // yes, as a result of emitting the token
          AddClass(sEndTag);
          FinishTag();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case nsHtml5Tokenizer::SCRIPT_DATA_LESS_THAN_SIGN:
    case nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN:
      if (aState == nsHtml5Tokenizer::NON_DATA_END_TAG_NAME) {
        FlushCurrent();
        StartSpan(); // don't know if it is "end-tag" yet :-(
        break;
      }
      FinishTag();
      break;
    case nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED_DASH_DASH:
    case nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED:
    case nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED_DASH:
      if (aState == nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN) {
        EndCharactersAndStartMarkupRun();
      }
      break;
    // Lots of double escape states omitted, because they don't highlight.
    // Likewise, only doctype states that can emit the doctype are of
    // interest. Otherwise, the transition out of bogus comment deals.
    case nsHtml5Tokenizer::BEFORE_DOCTYPE_NAME:
    case nsHtml5Tokenizer::DOCTYPE_NAME:
    case nsHtml5Tokenizer::AFTER_DOCTYPE_NAME:
    case nsHtml5Tokenizer::AFTER_DOCTYPE_PUBLIC_KEYWORD:
    case nsHtml5Tokenizer::BEFORE_DOCTYPE_PUBLIC_IDENTIFIER:
    case nsHtml5Tokenizer::DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED:
    case nsHtml5Tokenizer::AFTER_DOCTYPE_PUBLIC_IDENTIFIER:
    case nsHtml5Tokenizer::BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
    case nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED:
    case nsHtml5Tokenizer::AFTER_DOCTYPE_SYSTEM_IDENTIFIER:
    case nsHtml5Tokenizer::BOGUS_DOCTYPE:
    case nsHtml5Tokenizer::AFTER_DOCTYPE_SYSTEM_KEYWORD:
    case nsHtml5Tokenizer::BEFORE_DOCTYPE_SYSTEM_IDENTIFIER:
    case nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED:
    case nsHtml5Tokenizer::DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED:
      if (aState == nsHtml5Tokenizer::DATA) {
        AddClass(sDoctype);
        FinishTag();
      }
      break;
    case nsHtml5Tokenizer::PROCESSING_INSTRUCTION_QUESTION_MARK:
      if (aState == nsHtml5Tokenizer::DATA) {
        FinishTag();
      }
      break;
    default:
      break;
  }
  mState = aState;
  return aState;
}

void
nsHtml5Highlighter::End()
{
  switch (mState) {
    case nsHtml5Tokenizer::COMMENT_END:
    case nsHtml5Tokenizer::COMMENT_END_BANG:
    case nsHtml5Tokenizer::COMMENT_START_DASH:
    case nsHtml5Tokenizer::BOGUS_COMMENT:
    case nsHtml5Tokenizer::BOGUS_COMMENT_HYPHEN:
      AddClass(sComment);
      break;
    case nsHtml5Tokenizer::CDATA_RSQB_RSQB:
      AddClass(sCdata);
      break;
    case nsHtml5Tokenizer::DECIMAL_NRC_LOOP:
    case nsHtml5Tokenizer::HEX_NCR_LOOP:
      // XXX need tokenizer help here
      break;
    case nsHtml5Tokenizer::BEFORE_DOCTYPE_NAME:
    case nsHtml5Tokenizer::DOCTYPE_NAME:
    case nsHtml5Tokenizer::AFTER_DOCTYPE_NAME:
    case nsHtml5Tokenizer::AFTER_DOCTYPE_PUBLIC_KEYWORD:
    case nsHtml5Tokenizer::BEFORE_DOCTYPE_PUBLIC_IDENTIFIER:
    case nsHtml5Tokenizer::DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED:
    case nsHtml5Tokenizer::AFTER_DOCTYPE_PUBLIC_IDENTIFIER:
    case nsHtml5Tokenizer::BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
    case nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED:
    case nsHtml5Tokenizer::AFTER_DOCTYPE_SYSTEM_IDENTIFIER:
    case nsHtml5Tokenizer::BOGUS_DOCTYPE:
    case nsHtml5Tokenizer::AFTER_DOCTYPE_SYSTEM_KEYWORD:
    case nsHtml5Tokenizer::BEFORE_DOCTYPE_SYSTEM_IDENTIFIER:
    case nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED:
    case nsHtml5Tokenizer::DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED:
      AddClass(sDoctype);
      break;
    default:
      break;
  }
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(eTreeOpStreamEnded);
  FlushOps();
}

void
nsHtml5Highlighter::SetBuffer(nsHtml5UTF16Buffer* aBuffer)
{
  MOZ_ASSERT(!mBuffer, "Old buffer still here!");
  mBuffer = aBuffer;
  mCStart = aBuffer->getStart();
}

void
nsHtml5Highlighter::DropBuffer(int32_t aPos)
{
  MOZ_ASSERT(mBuffer, "No buffer to drop!");
  mPos = aPos;
  FlushChars();
  mBuffer = nullptr;
}

void
nsHtml5Highlighter::StartSpan()
{
  FlushChars();
  Push(nsGkAtoms::span, nullptr, NS_NewHTMLSpanElement);
  ++mInlinesOpen;
}

void
nsHtml5Highlighter::StartSpan(const char16_t* aClass)
{
  StartSpan();
  AddClass(aClass);
}

void
nsHtml5Highlighter::EndSpanOrA()
{
  FlushChars();
  Pop();
  --mInlinesOpen;
}

void
nsHtml5Highlighter::StartCharacters()
{
  MOZ_ASSERT(!mInCharacters, "Already in characters!");
  FlushChars();
  Push(nsGkAtoms::span, nullptr, NS_NewHTMLSpanElement);
  mCurrentRun = CurrentNode();
  mInCharacters = true;
}

void
nsHtml5Highlighter::EndCharactersAndStartMarkupRun()
{
  MOZ_ASSERT(mInCharacters, "Not in characters!");
  FlushChars();
  Pop();
  mInCharacters = false;
  // Now start markup run
  StartSpan();
  mCurrentRun = CurrentNode();
}

void
nsHtml5Highlighter::StartA()
{
  FlushChars();
  Push(nsGkAtoms::a, nullptr, NS_NewHTMLAnchorElement);
  AddClass(sAttributeValue);
  ++mInlinesOpen;
}

void
nsHtml5Highlighter::FinishTag()
{
  while (mInlinesOpen > 1) {
    EndSpanOrA();
  }
  FlushCurrent(); // >
  EndSpanOrA();   // DATA
  NS_ASSERTION(!mInlinesOpen, "mInlinesOpen got out of sync!");
  StartCharacters();
}

void
nsHtml5Highlighter::FlushChars()
{
  if (mCStart < mPos) {
    char16_t* buf = mBuffer->getBuffer();
    int32_t i = mCStart;
    while (i < mPos) {
      char16_t c = buf[i];
      switch (c) {
        case '\r':
          // The input this code sees has been normalized so that there are
          // CR breaks and LF breaks but no CRLF breaks. Overwrite CR with LF
          // to show consistent LF line breaks to layout. It is OK to mutate
          // the input data, because there are no reparses in the View Source
          // case, so we won't need the original data in the buffer anymore.
          buf[i] = '\n';
          MOZ_FALLTHROUGH;
        case '\n': {
          ++i;
          if (mCStart < i) {
            int32_t len = i - mCStart;
            AppendCharacters(buf, mCStart, len);
            mCStart = i;
          }
          ++mLineNumber;
          Push(nsGkAtoms::span, nullptr, NS_NewHTMLSpanElement);
          nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
          NS_ASSERTION(treeOp, "Tree op allocation failed.");
          treeOp->InitAddLineNumberId(CurrentNode(), mLineNumber);
          Pop();
          break;
        }
        default:
          ++i;
          break;
      }
    }
    if (mCStart < mPos) {
      int32_t len = mPos - mCStart;
      AppendCharacters(buf, mCStart, len);
      mCStart = mPos;
    }
  }
}

void
nsHtml5Highlighter::FlushCurrent()
{
  mPos++;
  FlushChars();
}

bool
nsHtml5Highlighter::FlushOps()
{
  bool hasOps = !mOpQueue.IsEmpty();
  if (hasOps) {
    mOpSink->MoveOpsFrom(mOpQueue);
  }
  return hasOps;
}

void
nsHtml5Highlighter::MaybeLinkifyAttributeValue(nsHtml5AttributeName* aName,
                                               nsHtml5String aValue)
{
  if (!(nsHtml5AttributeName::ATTR_HREF == aName ||
        nsHtml5AttributeName::ATTR_SRC == aName ||
        nsHtml5AttributeName::ATTR_ACTION == aName ||
        nsHtml5AttributeName::ATTR_CITE == aName ||
        nsHtml5AttributeName::ATTR_BACKGROUND == aName ||
        nsHtml5AttributeName::ATTR_LONGDESC == aName ||
        nsHtml5AttributeName::ATTR_XLINK_HREF == aName ||
        nsHtml5AttributeName::ATTR_DEFINITIONURL == aName)) {
    return;
  }
  AddViewSourceHref(aValue);
}

void
nsHtml5Highlighter::CompletedNamedCharacterReference()
{
  AddClass(sEntity);
}

nsIContent**
nsHtml5Highlighter::AllocateContentHandle()
{
  if (mHandlesUsed == NS_HTML5_HIGHLIGHTER_HANDLE_ARRAY_LENGTH) {
    mOldHandles.AppendElement(std::move(mHandles));
    mHandles =
      MakeUnique<nsIContent* []>(NS_HTML5_HIGHLIGHTER_HANDLE_ARRAY_LENGTH);
    mHandlesUsed = 0;
  }
#ifdef DEBUG
  mHandles[mHandlesUsed] = reinterpret_cast<nsIContent*>(uintptr_t(0xC0DEDBAD));
#endif
  return &mHandles[mHandlesUsed++];
}

nsIContent**
nsHtml5Highlighter::CreateElement(
  nsAtom* aName,
  nsHtml5HtmlAttributes* aAttributes,
  nsIContent** aIntendedParent,
  mozilla::dom::HTMLContentCreatorFunction aCreator)
{
  MOZ_ASSERT(aName, "Got null name.");
  nsHtml5ContentCreatorFunction creator;
  creator.html = aCreator;
  nsIContent** content = AllocateContentHandle();
  mOpQueue.AppendElement()->Init(kNameSpaceID_XHTML,
                                 aName,
                                 aAttributes,
                                 content,
                                 aIntendedParent,
                                 true,
                                 creator);
  return content;
}

nsIContent**
nsHtml5Highlighter::CurrentNode()
{
  MOZ_ASSERT(mStack.Length() >= 1, "Must have something on stack.");
  return mStack[mStack.Length() - 1];
}

void
nsHtml5Highlighter::Push(nsAtom* aName,
                         nsHtml5HtmlAttributes* aAttributes,
                         mozilla::dom::HTMLContentCreatorFunction aCreator)
{
  MOZ_ASSERT(mStack.Length() >= 1, "Pushing without root.");
  nsIContent** elt = CreateElement(aName,
                                   aAttributes,
                                   CurrentNode(),
                                   aCreator); // Don't inline below!
  mOpQueue.AppendElement()->Init(eTreeOpAppend, elt, CurrentNode());
  mStack.AppendElement(elt);
}

void
nsHtml5Highlighter::Pop()
{
  MOZ_ASSERT(mStack.Length() >= 2, "Popping when stack too short.");
  mStack.RemoveLastElement();
}

void
nsHtml5Highlighter::AppendCharacters(const char16_t* aBuffer,
                                     int32_t aStart,
                                     int32_t aLength)
{
  MOZ_ASSERT(aBuffer, "Null buffer");

  char16_t* bufferCopy = new char16_t[aLength];
  memcpy(bufferCopy, aBuffer + aStart, aLength * sizeof(char16_t));

  mOpQueue.AppendElement()->Init(
    eTreeOpAppendText, bufferCopy, aLength, CurrentNode());
}

void
nsHtml5Highlighter::AddClass(const char16_t* aClass)
{
  mOpQueue.AppendElement()->InitAddClass(CurrentNode(), aClass);
}

void
nsHtml5Highlighter::AddViewSourceHref(nsHtml5String aValue)
{
  char16_t* bufferCopy = new char16_t[aValue.Length() + 1];
  aValue.CopyToBuffer(bufferCopy);
  bufferCopy[aValue.Length()] = 0;

  mOpQueue.AppendElement()->Init(
    eTreeOpAddViewSourceHref, bufferCopy, aValue.Length(), CurrentNode());
}

void
nsHtml5Highlighter::AddBase(nsHtml5String aValue)
{
  if (mSeenBase) {
    return;
  }
  mSeenBase = true;
  char16_t* bufferCopy = new char16_t[aValue.Length() + 1];
  aValue.CopyToBuffer(bufferCopy);
  bufferCopy[aValue.Length()] = 0;

  mOpQueue.AppendElement()->Init(
    eTreeOpAddViewSourceBase, bufferCopy, aValue.Length());
}

void
nsHtml5Highlighter::AddErrorToCurrentNode(const char* aMsgId)
{
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(CurrentNode(), aMsgId);
}

void
nsHtml5Highlighter::AddErrorToCurrentRun(const char* aMsgId)
{
  MOZ_ASSERT(mCurrentRun, "Adding error to run without one!");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mCurrentRun, aMsgId);
}

void
nsHtml5Highlighter::AddErrorToCurrentRun(const char* aMsgId, nsAtom* aName)
{
  MOZ_ASSERT(mCurrentRun, "Adding error to run without one!");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mCurrentRun, aMsgId, aName);
}

void
nsHtml5Highlighter::AddErrorToCurrentRun(const char* aMsgId,
                                         nsAtom* aName,
                                         nsAtom* aOther)
{
  MOZ_ASSERT(mCurrentRun, "Adding error to run without one!");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mCurrentRun, aMsgId, aName, aOther);
}

void
nsHtml5Highlighter::AddErrorToCurrentAmpersand(const char* aMsgId)
{
  MOZ_ASSERT(mAmpersand, "Adding error to ampersand without one!");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mAmpersand, aMsgId);
}

void
nsHtml5Highlighter::AddErrorToCurrentSlash(const char* aMsgId)
{
  MOZ_ASSERT(mSlash, "Adding error to slash without one!");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mSlash, aMsgId);
}
