/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5TreeOperation_h
#define nsHtml5TreeOperation_h

#include "nsHtml5DocumentMode.h"
#include "nsHtml5HtmlAttributes.h"
#include "mozilla/dom/FromParser.h"
#include "mozilla/NotNull.h"

class nsIContent;
class nsHtml5TreeOpExecutor;
class nsHtml5DocumentBuilder;
namespace mozilla {
class Encoding;

namespace dom {
class Text;
} // namespace dom
} // namespace mozilla

enum eHtml5TreeOperation
{
  eTreeOpUninitialized,
  // main HTML5 ops
  eTreeOpAppend,
  eTreeOpDetach,
  eTreeOpAppendChildrenToNewParent,
  eTreeOpFosterParent,
  eTreeOpAppendToDocument,
  eTreeOpAddAttributes,
  eTreeOpDocumentMode,
  eTreeOpCreateHTMLElementNetwork,
  eTreeOpCreateHTMLElementNotNetwork,
  eTreeOpCreateSVGElementNetwork,
  eTreeOpCreateSVGElementNotNetwork,
  eTreeOpCreateMathMLElement,
  eTreeOpSetFormElement,
  eTreeOpAppendText,
  eTreeOpFosterParentText,
  eTreeOpAppendComment,
  eTreeOpAppendCommentToDocument,
  eTreeOpAppendDoctypeToDocument,
  eTreeOpGetDocumentFragmentForTemplate,
  eTreeOpGetFosterParent,
  // Gecko-specific on-pop ops
  eTreeOpMarkAsBroken,
  eTreeOpRunScript,
  eTreeOpRunScriptAsyncDefer,
  eTreeOpPreventScriptExecution,
  eTreeOpDoneAddingChildren,
  eTreeOpDoneCreatingElement,
  eTreeOpSetDocumentCharset,
  eTreeOpNeedsCharsetSwitchTo,
  eTreeOpUpdateStyleSheet,
  eTreeOpProcessMeta,
  eTreeOpProcessOfflineManifest,
  eTreeOpMarkMalformedIfScript,
  eTreeOpStreamEnded,
  eTreeOpSetStyleLineNumber,
  eTreeOpSetScriptLineNumberAndFreeze,
  eTreeOpSvgLoad,
  eTreeOpMaybeComplainAboutCharset,
  eTreeOpAddClass,
  eTreeOpAddViewSourceHref,
  eTreeOpAddViewSourceBase,
  eTreeOpAddError,
  eTreeOpAddLineNumberId,
  eTreeOpStartLayout,
  eTreeOpEnableEncodingMenu
};

class nsHtml5TreeOperationStringPair
{
private:
  nsString mPublicId;
  nsString mSystemId;

public:
  nsHtml5TreeOperationStringPair(const nsAString& aPublicId,
                                 const nsAString& aSystemId)
    : mPublicId(aPublicId)
    , mSystemId(aSystemId)
  {
    MOZ_COUNT_CTOR(nsHtml5TreeOperationStringPair);
  }

  ~nsHtml5TreeOperationStringPair()
  {
    MOZ_COUNT_DTOR(nsHtml5TreeOperationStringPair);
  }

  inline void Get(nsAString& aPublicId, nsAString& aSystemId)
  {
    aPublicId.Assign(mPublicId);
    aSystemId.Assign(mSystemId);
  }
};

class nsHtml5TreeOperation final
{
  template<typename T>
  using NotNull = mozilla::NotNull<T>;
  using Encoding = mozilla::Encoding;

public:
  /**
   * Atom is used inside the parser core are either static atoms that are
   * the same as Gecko-wide static atoms or they are dynamic atoms scoped by
   * both thread and parser to a particular nsHtml5AtomTable. In order to
   * such scoped atoms coming into contact with the rest of Gecko, atoms
   * that are about to exit the parser must go through this method which
   * reobtains dynamic atoms from the Gecko-global atom table.
   *
   * @param aAtom a potentially parser-scoped atom
   * @return an nsAtom that's pointer comparable on the main thread with
   *         other not-parser atoms.
   */
  static inline already_AddRefed<nsAtom> Reget(nsAtom* aAtom)
  {
    if (!aAtom || aAtom->IsStatic()) {
      return dont_AddRef(aAtom);
    }
    nsAutoString str;
    aAtom->ToString(str);
    return NS_AtomizeMainThread(str);
  }

  static nsresult AppendTextToTextNode(const char16_t* aBuffer,
                                       uint32_t aLength,
                                       mozilla::dom::Text* aTextNode,
                                       nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendText(const char16_t* aBuffer,
                             uint32_t aLength,
                             nsIContent* aParent,
                             nsHtml5DocumentBuilder* aBuilder);

  static nsresult Append(nsIContent* aNode,
                         nsIContent* aParent,
                         nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendToDocument(nsIContent* aNode,
                                   nsHtml5DocumentBuilder* aBuilder);

  static void Detach(nsIContent* aNode, nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendChildrenToNewParent(nsIContent* aNode,
                                            nsIContent* aParent,
                                            nsHtml5DocumentBuilder* aBuilder);

  static nsresult FosterParent(nsIContent* aNode,
                               nsIContent* aParent,
                               nsIContent* aTable,
                               nsHtml5DocumentBuilder* aBuilder);

  static nsresult AddAttributes(nsIContent* aNode,
                                nsHtml5HtmlAttributes* aAttributes,
                                nsHtml5DocumentBuilder* aBuilder);

  static void SetHTMLElementAttributes(mozilla::dom::Element* aElement,
                                       nsAtom* aName,
                                       nsHtml5HtmlAttributes* aAttributes);

  static nsIContent* CreateHTMLElement(
    nsAtom* aName,
    nsHtml5HtmlAttributes* aAttributes,
    mozilla::dom::FromParser aFromParser,
    nsNodeInfoManager* aNodeInfoManager,
    nsHtml5DocumentBuilder* aBuilder,
    mozilla::dom::HTMLContentCreatorFunction aCreator);

  static nsIContent* CreateSVGElement(
    nsAtom* aName,
    nsHtml5HtmlAttributes* aAttributes,
    mozilla::dom::FromParser aFromParser,
    nsNodeInfoManager* aNodeInfoManager,
    nsHtml5DocumentBuilder* aBuilder,
    mozilla::dom::SVGContentCreatorFunction aCreator);

  static nsIContent* CreateMathMLElement(nsAtom* aName,
                                         nsHtml5HtmlAttributes* aAttributes,
                                         nsNodeInfoManager* aNodeInfoManager,
                                         nsHtml5DocumentBuilder* aBuilder);

  static void SetFormElement(nsIContent* aNode, nsIContent* aParent);

  static nsresult AppendIsindexPrompt(nsIContent* parent,
                                      nsHtml5DocumentBuilder* aBuilder);

  static nsresult FosterParentText(nsIContent* aStackParent,
                                   char16_t* aBuffer,
                                   uint32_t aLength,
                                   nsIContent* aTable,
                                   nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendComment(nsIContent* aParent,
                                char16_t* aBuffer,
                                int32_t aLength,
                                nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendCommentToDocument(char16_t* aBuffer,
                                          int32_t aLength,
                                          nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendDoctypeToDocument(nsAtom* aName,
                                          const nsAString& aPublicId,
                                          const nsAString& aSystemId,
                                          nsHtml5DocumentBuilder* aBuilder);

  static nsIContent* GetDocumentFragmentForTemplate(nsIContent* aNode);

  static nsIContent* GetFosterParent(nsIContent* aTable,
                                     nsIContent* aStackParent);

  static void PreventScriptExecution(nsIContent* aNode);

  static void DoneAddingChildren(nsIContent* aNode);

  static void DoneCreatingElement(nsIContent* aNode);

  static void SvgLoad(nsIContent* aNode);

  static void MarkMalformedIfScript(nsIContent* aNode);

  nsHtml5TreeOperation();

  ~nsHtml5TreeOperation();

  inline void Init(eHtml5TreeOperation aOpCode)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    mOpCode = aOpCode;
  }

  inline void Init(eHtml5TreeOperation aOpCode, nsIContentHandle* aNode)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aNode, "Initialized tree op with null node.");
    mOpCode = aOpCode;
    mOne.node = static_cast<nsIContent**>(aNode);
  }

  inline void Init(eHtml5TreeOperation aOpCode,
                   nsIContentHandle* aNode,
                   nsIContentHandle* aParent)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aNode, "Initialized tree op with null node.");
    MOZ_ASSERT(aParent, "Initialized tree op with null parent.");
    mOpCode = aOpCode;
    mOne.node = static_cast<nsIContent**>(aNode);
    mTwo.node = static_cast<nsIContent**>(aParent);
  }

  inline void Init(eHtml5TreeOperation aOpCode,
                   const nsACString& aString,
                   int32_t aInt32)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");

    int32_t len = aString.Length();
    char* str = new char[len + 1];
    const char* start = aString.BeginReading();
    for (int32_t i = 0; i < len; ++i) {
      str[i] = start[i];
    }
    str[len] = '\0';

    mOpCode = aOpCode;
    mOne.charPtr = str;
    mFour.integer = aInt32;
  }

  inline void Init(eHtml5TreeOperation aOpCode,
                   const nsACString& aString,
                   int32_t aInt32,
                   int32_t aLineNumber)
  {
    Init(aOpCode, aString, aInt32);
    mTwo.integer = aLineNumber;
  }

  inline void Init(eHtml5TreeOperation aOpCode,
                   NotNull<const Encoding*> aEncoding,
                   int32_t aInt32)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");

    mOpCode = aOpCode;
    mOne.encoding = aEncoding;
    mFour.integer = aInt32;
  }

  inline void Init(eHtml5TreeOperation aOpCode,
                   NotNull<const Encoding*> aEncoding,
                   int32_t aInt32,
                   int32_t aLineNumber)
  {
    Init(aOpCode, aEncoding, aInt32);
    mTwo.integer = aLineNumber;
  }

  inline void Init(eHtml5TreeOperation aOpCode,
                   nsIContentHandle* aNode,
                   nsIContentHandle* aParent,
                   nsIContentHandle* aTable)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aNode, "Initialized tree op with null node.");
    MOZ_ASSERT(aParent, "Initialized tree op with null parent.");
    MOZ_ASSERT(aTable, "Initialized tree op with null table.");
    mOpCode = aOpCode;
    mOne.node = static_cast<nsIContent**>(aNode);
    mTwo.node = static_cast<nsIContent**>(aParent);
    mThree.node = static_cast<nsIContent**>(aTable);
  }

  inline void Init(nsHtml5DocumentMode aMode)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    mOpCode = eTreeOpDocumentMode;
    mOne.mode = aMode;
  }

  inline void InitScript(nsIContentHandle* aNode)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aNode, "Initialized tree op with null node.");
    mOpCode = eTreeOpRunScript;
    mOne.node = static_cast<nsIContent**>(aNode);
    mTwo.state = nullptr;
  }

  inline void Init(int32_t aNamespace,
                   nsAtom* aName,
                   nsHtml5HtmlAttributes* aAttributes,
                   nsIContentHandle* aTarget,
                   nsIContentHandle* aIntendedParent,
                   bool aFromNetwork,
                   nsHtml5ContentCreatorFunction aCreator)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aName, "Initialized tree op with null name.");
    MOZ_ASSERT(aTarget, "Initialized tree op with null target node.");

    if (aNamespace == kNameSpaceID_XHTML) {
      mOpCode = aFromNetwork ? eTreeOpCreateHTMLElementNetwork
                             : eTreeOpCreateHTMLElementNotNetwork;
      mFour.htmlCreator = aCreator.html;
    } else if (aNamespace == kNameSpaceID_SVG) {
      mOpCode = aFromNetwork ? eTreeOpCreateSVGElementNetwork
                             : eTreeOpCreateSVGElementNotNetwork;
      mFour.svgCreator = aCreator.svg;
    } else {
      MOZ_ASSERT(aNamespace == kNameSpaceID_MathML);
      mOpCode = eTreeOpCreateMathMLElement;
    }
    mFive.node = static_cast<nsIContent**>(aIntendedParent);
    mOne.node = static_cast<nsIContent**>(aTarget);
    mTwo.atom = aName;
    if (aAttributes == nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES) {
      mThree.attributes = nullptr;
    } else {
      mThree.attributes = aAttributes;
    }
  }

  inline void Init(eHtml5TreeOperation aOpCode,
                   char16_t* aBuffer,
                   int32_t aLength,
                   nsIContentHandle* aStackParent,
                   nsIContentHandle* aTable)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aBuffer, "Initialized tree op with null buffer.");
    mOpCode = aOpCode;
    mOne.node = static_cast<nsIContent**>(aStackParent);
    mTwo.unicharPtr = aBuffer;
    mThree.node = static_cast<nsIContent**>(aTable);
    mFour.integer = aLength;
  }

  inline void Init(eHtml5TreeOperation aOpCode,
                   char16_t* aBuffer,
                   int32_t aLength,
                   nsIContentHandle* aParent)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aBuffer, "Initialized tree op with null buffer.");
    mOpCode = aOpCode;
    mOne.node = static_cast<nsIContent**>(aParent);
    mTwo.unicharPtr = aBuffer;
    mFour.integer = aLength;
  }

  inline void Init(eHtml5TreeOperation aOpCode,
                   char16_t* aBuffer,
                   int32_t aLength)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aBuffer, "Initialized tree op with null buffer.");
    mOpCode = aOpCode;
    mTwo.unicharPtr = aBuffer;
    mFour.integer = aLength;
  }

  inline void Init(nsIContentHandle* aElement,
                   nsHtml5HtmlAttributes* aAttributes)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aElement, "Initialized tree op with null element.");
    mOpCode = eTreeOpAddAttributes;
    mOne.node = static_cast<nsIContent**>(aElement);
    mTwo.attributes = aAttributes;
  }

  inline void Init(nsAtom* aName,
                   const nsAString& aPublicId,
                   const nsAString& aSystemId)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    mOpCode = eTreeOpAppendDoctypeToDocument;
    mOne.atom = aName;
    mTwo.stringPair = new nsHtml5TreeOperationStringPair(aPublicId, aSystemId);
  }

  inline void Init(nsIContentHandle* aElement,
                   const char* aMsgId,
                   nsAtom* aAtom,
                   nsAtom* aOtherAtom)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    mOpCode = eTreeOpAddError;
    mOne.node = static_cast<nsIContent**>(aElement);
    mTwo.charPtr = (char*)aMsgId;
    mThree.atom = aAtom;
    mFour.atom = aOtherAtom;
  }

  inline void Init(nsIContentHandle* aElement,
                   const char* aMsgId,
                   nsAtom* aAtom)
  {
    Init(aElement, aMsgId, aAtom, nullptr);
  }

  inline void Init(nsIContentHandle* aElement, const char* aMsgId)
  {
    Init(aElement, aMsgId, nullptr, nullptr);
  }

  inline void Init(const char* aMsgId, bool aError, int32_t aLineNumber)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    mOpCode = eTreeOpMaybeComplainAboutCharset;
    mOne.charPtr = const_cast<char*>(aMsgId);
    mTwo.integer = aError;
    mThree.integer = aLineNumber;
  }

  inline void Init(eHtml5TreeOperation aOpCode, const nsAString& aString)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");

    char16_t* str = ToNewUnicode(aString);
    mOpCode = aOpCode;
    mOne.unicharPtr = str;
  }

  inline void Init(eHtml5TreeOperation aOpCode,
                   nsIContentHandle* aNode,
                   int32_t aInt)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aNode, "Initialized tree op with null node.");
    mOpCode = aOpCode;
    mOne.node = static_cast<nsIContent**>(aNode);
    mFour.integer = aInt;
  }

  inline void Init(nsresult aRv)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(NS_FAILED(aRv), "Initialized tree op with non-failure.");
    mOpCode = eTreeOpMarkAsBroken;
    mOne.result = aRv;
  }

  inline void InitAddClass(nsIContentHandle* aNode, const char16_t* aClass)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aNode, "Initialized tree op with null node.");
    MOZ_ASSERT(aClass, "Initialized tree op with null string.");
    // aClass must be a literal string that does not need freeing
    mOpCode = eTreeOpAddClass;
    mOne.node = static_cast<nsIContent**>(aNode);
    mTwo.unicharPtr = (char16_t*)aClass;
  }

  inline void InitAddLineNumberId(nsIContentHandle* aNode,
                                  const int32_t aLineNumber)
  {
    MOZ_ASSERT(mOpCode == eTreeOpUninitialized,
               "Op code must be uninitialized when initializing.");
    MOZ_ASSERT(aNode, "Initialized tree op with null node.");
    MOZ_ASSERT(aLineNumber > 0, "Initialized tree op with line number.");
    // aClass must be a literal string that does not need freeing
    mOpCode = eTreeOpAddLineNumberId;
    mOne.node = static_cast<nsIContent**>(aNode);
    mFour.integer = aLineNumber;
  }

  inline bool IsRunScript() { return mOpCode == eTreeOpRunScript; }

  inline bool IsMarkAsBroken() { return mOpCode == eTreeOpMarkAsBroken; }

  inline void SetSnapshot(nsAHtml5TreeBuilderState* aSnapshot, int32_t aLine)
  {
    NS_ASSERTION(
      IsRunScript(),
      "Setting a snapshot for a tree operation other than eTreeOpRunScript!");
    MOZ_ASSERT(aSnapshot, "Initialized tree op with null snapshot.");
    mTwo.state = aSnapshot;
    mFour.integer = aLine;
  }

  nsresult Perform(nsHtml5TreeOpExecutor* aBuilder,
                   nsIContent** aScriptElement,
                   bool* aInterrupted,
                   bool* aStreamEnded);

private:
  nsHtml5TreeOperation(const nsHtml5TreeOperation&) = delete;
  nsHtml5TreeOperation& operator=(const nsHtml5TreeOperation&) = delete;

  // possible optimization:
  // Make the queue take items the size of pointer and make the op code
  // decide how many operands it dequeues after it.
  eHtml5TreeOperation mOpCode;
  union {
    nsIContent** node;
    nsAtom* atom;
    nsHtml5HtmlAttributes* attributes;
    nsHtml5DocumentMode mode;
    char16_t* unicharPtr;
    char* charPtr;
    nsHtml5TreeOperationStringPair* stringPair;
    nsAHtml5TreeBuilderState* state;
    int32_t integer;
    nsresult result;
    const Encoding* encoding;
    mozilla::dom::HTMLContentCreatorFunction htmlCreator;
    mozilla::dom::SVGContentCreatorFunction svgCreator;
  } mOne, mTwo, mThree, mFour, mFive;
};

#endif // nsHtml5TreeOperation_h
