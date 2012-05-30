/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5Highlighter.h"
#include "nsDebug.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5AttributeName.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsHtml5ViewSourceUtils.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

// The old code had a limit of 16 tokens. 1300 is a number picked my measuring
// the size of 16 tokens on cnn.com.
#define NS_HTML5_HIGHLIGHTER_PRE_BREAK_THRESHOLD 1300

PRUnichar nsHtml5Highlighter::sComment[] =
  { 'c', 'o', 'm', 'm', 'e', 'n', 't', 0 };

PRUnichar nsHtml5Highlighter::sCdata[] =
  { 'c', 'd', 'a', 't', 'a', 0 };

PRUnichar nsHtml5Highlighter::sEntity[] =
  { 'e', 'n', 't', 'i', 't', 'y', 0 };

PRUnichar nsHtml5Highlighter::sEndTag[] =
  { 'e', 'n', 'd', '-', 't', 'a', 'g', 0 };

PRUnichar nsHtml5Highlighter::sStartTag[] =
  { 's', 't', 'a', 'r', 't', '-', 't', 'a', 'g', 0 };

PRUnichar nsHtml5Highlighter::sAttributeName[] =
  { 'a', 't', 't', 'r', 'i', 'b', 'u', 't', 'e', '-', 'n', 'a', 'm', 'e', 0 };

PRUnichar nsHtml5Highlighter::sAttributeValue[] =
  { 'a', 't', 't', 'r', 'i', 'b', 'u', 't', 'e', '-',
    'v', 'a', 'l', 'u', 'e', 0 };

PRUnichar nsHtml5Highlighter::sDoctype[] =
  { 'd', 'o', 'c', 't', 'y', 'p', 'e', 0 };

PRUnichar nsHtml5Highlighter::sPi[] =
  { 'p', 'i', 0 };

nsHtml5Highlighter::nsHtml5Highlighter(nsAHtml5TreeOpSink* aOpSink)
 : mState(NS_HTML5TOKENIZER_DATA)
 , mCStart(PR_INT32_MAX)
 , mPos(0)
 , mLineNumber(1)
 , mInlinesOpen(0)
 , mInCharacters(false)
 , mBuffer(nsnull)
 , mSyntaxHighlight(Preferences::GetBool("view_source.syntax_highlight",
                                         true))
 , mOpSink(aOpSink)
 , mCurrentRun(nsnull)
 , mAmpersand(nsnull)
 , mSlash(nsnull)
 , mHandles(new nsIContent*[NS_HTML5_HIGHLIGHTER_HANDLE_ARRAY_LENGTH])
 , mHandlesUsed(0)
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

  nsIContent** root = CreateElement(nsHtml5Atoms::html, nsnull);
  mOpQueue.AppendElement()->Init(eTreeOpAppendToDocument, root);
  mStack.AppendElement(root);

  Push(nsGkAtoms::head, nsnull);

  Push(nsGkAtoms::title, nsnull);
  // XUL will add the "Source of: " prefix.
  PRUint32 length = aTitle.Length();
  if (length > PR_INT32_MAX) {
    length = PR_INT32_MAX;
  }
  AppendCharacters(aTitle.get(), 0, (PRInt32)length);
  Pop(); // title

  Push(nsGkAtoms::link, nsHtml5ViewSourceUtils::NewLinkAttributes());

  mOpQueue.AppendElement()->Init(eTreeOpUpdateStyleSheet, CurrentNode());

  Pop(); // link

  Pop(); // head

  Push(nsGkAtoms::body, nsHtml5ViewSourceUtils::NewBodyAttributes());

  nsHtml5HtmlAttributes* preAttrs = new nsHtml5HtmlAttributes(0);
  nsString* preId = new nsString(NS_LITERAL_STRING("line1"));
  preAttrs->addAttribute(nsHtml5AttributeName::ATTR_ID, preId);
  Push(nsGkAtoms::pre, preAttrs);

  StartCharacters();

  mOpQueue.AppendElement()->Init(eTreeOpStartLayout);
}

PRInt32
nsHtml5Highlighter::Transition(PRInt32 aState, bool aReconsume, PRInt32 aPos)
{
  mPos = aPos;
  switch (mState) {
    case NS_HTML5TOKENIZER_SCRIPT_DATA:
    case NS_HTML5TOKENIZER_RAWTEXT:
    case NS_HTML5TOKENIZER_RCDATA:
    case NS_HTML5TOKENIZER_DATA:
      // We can transition on < and on &. Either way, we don't yet know the
      // role of the token, so open a span without class.
      if (aState == NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE) {
        StartSpan();
        // Start another span for highlighting the ampersand
        StartSpan();
        mAmpersand = CurrentNode();
      } else {
        EndCharactersAndStartMarkupRun();
      }
      break;
    case NS_HTML5TOKENIZER_TAG_OPEN:
      switch (aState) {
        case NS_HTML5TOKENIZER_TAG_NAME:
          StartSpan(sStartTag);
          break;
        case NS_HTML5TOKENIZER_DATA:
          FinishTag(); // DATA
          break;
        case NS_HTML5TOKENIZER_PROCESSING_INSTRUCTION:
          AddClass(sPi);
          break;
      }
      break;
    case NS_HTML5TOKENIZER_TAG_NAME:
      switch (aState) {
        case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME:
          EndSpanOrA(); // NS_HTML5TOKENIZER_TAG_NAME
          break;
        case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG:
          EndSpanOrA(); // NS_HTML5TOKENIZER_TAG_NAME
          StartSpan(); // for highlighting the slash
          mSlash = CurrentNode();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME:
      switch (aState) {
        case NS_HTML5TOKENIZER_ATTRIBUTE_NAME:
          StartSpan(sAttributeName);
          break;
        case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG:
          StartSpan(); // for highlighting the slash
          mSlash = CurrentNode();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case NS_HTML5TOKENIZER_ATTRIBUTE_NAME:
      switch (aState) {
        case NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME:
        case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE:
          EndSpanOrA(); // NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME
          break;
        case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG:
          EndSpanOrA(); // NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME
          StartSpan(); // for highlighting the slash
          mSlash = CurrentNode();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE:
      switch (aState) {
        case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_DOUBLE_QUOTED:
        case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_SINGLE_QUOTED:
          FlushCurrent();
          StartA();
          break;
        case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED:
          StartA();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_DOUBLE_QUOTED:
    case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_SINGLE_QUOTED:
      switch (aState) {
        case NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED:
          EndSpanOrA();
          break;
        case NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE:
          StartSpan();
          StartSpan(); // for ampersand itself
          mAmpersand = CurrentNode();
          break;
        default:
          NS_NOTREACHED("Impossible transition.");
          break;
      }
      break;
    case NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED:
      switch (aState) {
        case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME:
          break;
        case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG:
          StartSpan(); // for highlighting the slash
          mSlash = CurrentNode();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG:
      EndSpanOrA(); // end the slash highlight
      switch (aState) {
        case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME:
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED:
      switch (aState) {
        case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME:
          EndSpanOrA();
          break;
        case NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE:
          StartSpan();
          StartSpan(); // for ampersand itself
          mAmpersand = CurrentNode();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME:
      switch (aState) {
        case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG:
          StartSpan(); // for highlighting the slash
          mSlash = CurrentNode();
          break;
        case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE:
          break;
        case NS_HTML5TOKENIZER_ATTRIBUTE_NAME:
          StartSpan(sAttributeName);
          break;
        default:
          FinishTag();
          break;
      }
      break;
      // most comment states are omitted, because they don't matter to
      // highlighting
    case NS_HTML5TOKENIZER_COMMENT_START:
    case NS_HTML5TOKENIZER_COMMENT_END:
    case NS_HTML5TOKENIZER_COMMENT_END_BANG:
    case NS_HTML5TOKENIZER_COMMENT_START_DASH:
    case NS_HTML5TOKENIZER_BOGUS_COMMENT:
    case NS_HTML5TOKENIZER_BOGUS_COMMENT_HYPHEN:
      if (aState == NS_HTML5TOKENIZER_DATA) {
        AddClass(sComment);
        FinishTag();
      }
      break;
      // most cdata states are omitted, because they don't matter to
      // highlighting
    case NS_HTML5TOKENIZER_CDATA_RSQB_RSQB:
      if (aState == NS_HTML5TOKENIZER_DATA) {
        AddClass(sCdata);
        FinishTag();
      }
      break;
    case NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE:
      EndSpanOrA(); // the span for the ampersand
      switch (aState) {
        case NS_HTML5TOKENIZER_CONSUME_NCR:
        case NS_HTML5TOKENIZER_CHARACTER_REFERENCE_HILO_LOOKUP:
          break;
        default:
          // not actually a character reference
          EndSpanOrA();
          break;
      }
      break;
    case NS_HTML5TOKENIZER_CHARACTER_REFERENCE_HILO_LOOKUP:
      if (aState == NS_HTML5TOKENIZER_CHARACTER_REFERENCE_TAIL) {
        break;
      }
      // not actually a character reference
      EndSpanOrA();
      break;
    case NS_HTML5TOKENIZER_CHARACTER_REFERENCE_TAIL:
      if (!aReconsume) {
        FlushCurrent();
      }
      EndSpanOrA();
      break;
    case NS_HTML5TOKENIZER_DECIMAL_NRC_LOOP:
    case NS_HTML5TOKENIZER_HEX_NCR_LOOP:
      switch (aState) {
        case NS_HTML5TOKENIZER_HANDLE_NCR_VALUE:
          AddClass(sEntity);
          FlushCurrent();
          break;
        case NS_HTML5TOKENIZER_HANDLE_NCR_VALUE_RECONSUME:
          AddClass(sEntity);
          break;
      }
      EndSpanOrA();
      break;
    case NS_HTML5TOKENIZER_CLOSE_TAG_OPEN:
      switch (aState) {
        case NS_HTML5TOKENIZER_DATA:
          FinishTag();
          break;
        case NS_HTML5TOKENIZER_TAG_NAME:
          StartSpan(sEndTag);
          break;
      }
      break;
    case NS_HTML5TOKENIZER_RAWTEXT_RCDATA_LESS_THAN_SIGN:
      if (aState == NS_HTML5TOKENIZER_NON_DATA_END_TAG_NAME) {
        FlushCurrent();
        StartSpan(); // don't know if it is "end-tag" yet :-(
        break;
      }
      EndSpanOrA();
      StartCharacters();
      break;
    case NS_HTML5TOKENIZER_NON_DATA_END_TAG_NAME:
      switch (aState) {
        case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME:
          AddClass(sEndTag);
          EndSpanOrA();
          break;
        case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG:
          AddClass(sEndTag);
          EndSpanOrA();
          StartSpan(); // for highlighting the slash
          mSlash = CurrentNode();
          break;
        case NS_HTML5TOKENIZER_DATA: // yes, as a result of emitting the token
          AddClass(sEndTag);
          FinishTag();
          break;
        default:
          FinishTag();
          break;
      }
      break;
    case NS_HTML5TOKENIZER_SCRIPT_DATA_LESS_THAN_SIGN:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN:
      if (aState == NS_HTML5TOKENIZER_NON_DATA_END_TAG_NAME) {
        FlushCurrent();
        StartSpan(); // don't know if it is "end-tag" yet :-(
        break;
      }
      FinishTag();
      break;
    case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH_DASH:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH:
      if (aState == NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN) {
        EndCharactersAndStartMarkupRun();
      }
      break;
      // Lots of double escape states omitted, because they don't highlight.
      // Likewise, only doctype states that can emit the doctype are of
      // interest. Otherwise, the transition out of bogus comment deals.
    case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME:
    case NS_HTML5TOKENIZER_DOCTYPE_NAME:
    case NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME:
    case NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_KEYWORD:
    case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER:
    case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED:
    case NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER:
    case NS_HTML5TOKENIZER_BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
    case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED:
    case NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER:
    case NS_HTML5TOKENIZER_BOGUS_DOCTYPE:
    case NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_KEYWORD:
    case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER:
    case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED:
    case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED:
      if (aState == NS_HTML5TOKENIZER_DATA) {
        AddClass(sDoctype);
        FinishTag();
      }
      break;
    case NS_HTML5TOKENIZER_PROCESSING_INSTRUCTION_QUESTION_MARK:
      if (aState == NS_HTML5TOKENIZER_DATA) {
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
    case NS_HTML5TOKENIZER_COMMENT_END:
    case NS_HTML5TOKENIZER_COMMENT_END_BANG:
    case NS_HTML5TOKENIZER_COMMENT_START_DASH:
    case NS_HTML5TOKENIZER_BOGUS_COMMENT:
    case NS_HTML5TOKENIZER_BOGUS_COMMENT_HYPHEN:
      AddClass(sComment);
      break;
    case NS_HTML5TOKENIZER_CDATA_RSQB_RSQB:
      AddClass(sCdata);
      break;
    case NS_HTML5TOKENIZER_DECIMAL_NRC_LOOP:
    case NS_HTML5TOKENIZER_HEX_NCR_LOOP:
      // XXX need tokenizer help here
      break;
    case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME:
    case NS_HTML5TOKENIZER_DOCTYPE_NAME:
    case NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME:
    case NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_KEYWORD:
    case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER:
    case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED:
    case NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER:
    case NS_HTML5TOKENIZER_BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS:
    case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED:
    case NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER:
    case NS_HTML5TOKENIZER_BOGUS_DOCTYPE:
    case NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_KEYWORD:
    case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER:
    case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED:
    case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED:
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
  NS_PRECONDITION(!mBuffer, "Old buffer still here!");
  mBuffer = aBuffer;
  mCStart = aBuffer->getStart();
}

void
nsHtml5Highlighter::DropBuffer(PRInt32 aPos)
{
  NS_PRECONDITION(mBuffer, "No buffer to drop!");
  mPos = aPos;
  FlushChars();
  mBuffer = nsnull;
}

void
nsHtml5Highlighter::StartSpan()
{
  FlushChars();
  Push(nsGkAtoms::span, nsnull);
  ++mInlinesOpen;
}

void
nsHtml5Highlighter::StartSpan(const PRUnichar* aClass)
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
  NS_PRECONDITION(!mInCharacters, "Already in characters!");
  FlushChars();
  Push(nsGkAtoms::span, nsnull);
  mCurrentRun = CurrentNode();
  mInCharacters = true;
}

void
nsHtml5Highlighter::EndCharactersAndStartMarkupRun()
{
  NS_PRECONDITION(mInCharacters, "Not in characters!");
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
  Push(nsGkAtoms::a, nsnull);
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
  EndSpanOrA(); // DATA
  NS_ASSERTION(!mInlinesOpen, "mInlinesOpen got out of sync!");
  StartCharacters();
}

void
nsHtml5Highlighter::FlushChars()
{
  if (mCStart < mPos) {
    PRUnichar* buf = mBuffer->getBuffer();
    PRInt32 i = mCStart;
    while (i < mPos) {
      PRUnichar c = buf[i];
      switch (c) {
        case '\r':
          // The input this code sees has been normalized so that there are
          // CR breaks and LF breaks but no CRLF breaks. Overwrite CR with LF
          // to show consistent LF line breaks to layout. It is OK to mutate
          // the input data, because there are no reparses in the View Source
          // case, so we won't need the original data in the buffer anymore.
          buf[i] = '\n';
          // fall through
        case '\n': {
          ++i;
          if (mCStart < i) {
            PRInt32 len = i - mCStart;
            AppendCharacters(buf, mCStart, len);
            mCStart = i;
          }
          ++mLineNumber;
          Push(nsGkAtoms::span, nsnull);
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
      PRInt32 len = mPos - mCStart;
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
                                               nsString* aValue)
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
  AddViewSourceHref(*aValue);
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
    mOldHandles.AppendElement(mHandles.forget());
    mHandles = new nsIContent*[NS_HTML5_HIGHLIGHTER_HANDLE_ARRAY_LENGTH];
    mHandlesUsed = 0;
  }
#ifdef DEBUG
  mHandles[mHandlesUsed] = (nsIContent*)0xC0DEDBAD;
#endif
  return &mHandles[mHandlesUsed++];
}

nsIContent**
nsHtml5Highlighter::CreateElement(nsIAtom* aName,
                                  nsHtml5HtmlAttributes* aAttributes)
{
  NS_PRECONDITION(aName, "Got null name.");
  nsIContent** content = AllocateContentHandle();
  mOpQueue.AppendElement()->Init(kNameSpaceID_XHTML,
                                 aName,
                                 aAttributes,
                                 content,
                                 true);
  return content;
}

nsIContent**
nsHtml5Highlighter::CurrentNode()
{
  NS_PRECONDITION(mStack.Length() >= 1, "Must have something on stack.");
  return mStack[mStack.Length() - 1];
}

void
nsHtml5Highlighter::Push(nsIAtom* aName,
                         nsHtml5HtmlAttributes* aAttributes)
{
  NS_PRECONDITION(mStack.Length() >= 1, "Pushing without root.");
  nsIContent** elt = CreateElement(aName, aAttributes); // Don't inline below!
  mOpQueue.AppendElement()->Init(eTreeOpAppend, elt, CurrentNode());
  mStack.AppendElement(elt);
}

void
nsHtml5Highlighter::Pop()
{
  NS_PRECONDITION(mStack.Length() >= 2, "Popping when stack too short.");
  mStack.RemoveElementAt(mStack.Length() - 1);
}

void
nsHtml5Highlighter::AppendCharacters(const PRUnichar* aBuffer,
                                     PRInt32 aStart,
                                     PRInt32 aLength)
{
  NS_PRECONDITION(aBuffer, "Null buffer");

  PRUnichar* bufferCopy = new PRUnichar[aLength];
  memcpy(bufferCopy, aBuffer + aStart, aLength * sizeof(PRUnichar));

  mOpQueue.AppendElement()->Init(eTreeOpAppendText,
                                 bufferCopy,
                                 aLength,
                                 CurrentNode());
}


void
nsHtml5Highlighter::AddClass(const PRUnichar* aClass)
{
  if (!mSyntaxHighlight) {
    return;
  }
  mOpQueue.AppendElement()->InitAddClass(CurrentNode(), aClass);
}

void
nsHtml5Highlighter::AddViewSourceHref(const nsString& aValue)
{
  PRUnichar* bufferCopy = new PRUnichar[aValue.Length() + 1];
  memcpy(bufferCopy, aValue.get(), aValue.Length() * sizeof(PRUnichar));
  bufferCopy[aValue.Length()] = 0;

  mOpQueue.AppendElement()->Init(eTreeOpAddViewSourceHref,
                                 bufferCopy,
                                 aValue.Length(),
                                 CurrentNode());
}

void
nsHtml5Highlighter::AddErrorToCurrentNode(const char* aMsgId)
{
  if (!mSyntaxHighlight) {
    return;
  }
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(CurrentNode(), aMsgId);
}

void
nsHtml5Highlighter::AddErrorToCurrentRun(const char* aMsgId)
{
  if (!mSyntaxHighlight) {
    return;
  }
  NS_PRECONDITION(mCurrentRun, "Adding error to run without one!");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mCurrentRun, aMsgId);
}

void
nsHtml5Highlighter::AddErrorToCurrentRun(const char* aMsgId,
                                         nsIAtom* aName)
{
  if (!mSyntaxHighlight) {
    return;
  }
  NS_PRECONDITION(mCurrentRun, "Adding error to run without one!");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mCurrentRun, aMsgId, aName);
}

void
nsHtml5Highlighter::AddErrorToCurrentRun(const char* aMsgId,
                                         nsIAtom* aName,
                                         nsIAtom* aOther)
{
  if (!mSyntaxHighlight) {
    return;
  }
  NS_PRECONDITION(mCurrentRun, "Adding error to run without one!");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mCurrentRun, aMsgId, aName, aOther);
}

void
nsHtml5Highlighter::AddErrorToCurrentAmpersand(const char* aMsgId)
{
  if (!mSyntaxHighlight) {
    return;
  }
  NS_PRECONDITION(mAmpersand, "Adding error to ampersand without one!");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mAmpersand, aMsgId);
}

void
nsHtml5Highlighter::AddErrorToCurrentSlash(const char* aMsgId)
{
  if (!mSyntaxHighlight) {
    return;
  }
  NS_PRECONDITION(mSlash, "Adding error to slash without one!");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mSlash, aMsgId);
}
