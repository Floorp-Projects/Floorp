/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5TreeOperation_h
#define nsHtml5TreeOperation_h

#include "nsHtml5DocumentMode.h"
#include "nsHtml5HtmlAttributes.h"
#include "mozilla/dom/FromParser.h"
#include "mozilla/dom/ShadowRootBinding.h"
#include "mozilla/NotNull.h"
#include "mozilla/Variant.h"
#include "nsCharsetSource.h"

class nsIContent;
class nsHtml5TreeOpExecutor;
class nsHtml5DocumentBuilder;
namespace mozilla {
class Encoding;

namespace dom {
class Text;
}  // namespace dom
}  // namespace mozilla

struct uninitialized {};

// main HTML5 ops
struct opDetach {
  nsIContent** mElement;

  explicit opDetach(nsIContentHandle* aElement) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opAppend {
  nsIContent** mChild;
  nsIContent** mParent;
  mozilla::dom::FromParser mFromNetwork;

  explicit opAppend(nsIContentHandle* aChild, nsIContentHandle* aParent,
                    mozilla::dom::FromParser aFromNetwork)
      : mFromNetwork(aFromNetwork) {
    mChild = static_cast<nsIContent**>(aChild);
    mParent = static_cast<nsIContent**>(aParent);
  };
};

struct opAppendChildrenToNewParent {
  nsIContent** mOldParent;
  nsIContent** mNewParent;

  explicit opAppendChildrenToNewParent(nsIContentHandle* aOldParent,
                                       nsIContentHandle* aNewParent) {
    mOldParent = static_cast<nsIContent**>(aOldParent);
    mNewParent = static_cast<nsIContent**>(aNewParent);
  };
};

struct opFosterParent {
  nsIContent** mChild;
  nsIContent** mStackParent;
  nsIContent** mTable;

  explicit opFosterParent(nsIContentHandle* aChild,
                          nsIContentHandle* aStackParent,
                          nsIContentHandle* aTable) {
    mChild = static_cast<nsIContent**>(aChild);
    mStackParent = static_cast<nsIContent**>(aStackParent);
    mTable = static_cast<nsIContent**>(aTable);
  };
};

struct opAppendToDocument {
  nsIContent** mContent;

  explicit opAppendToDocument(nsIContentHandle* aContent) {
    mContent = static_cast<nsIContent**>(aContent);
  };
};

struct opAddAttributes {
  nsIContent** mElement;
  nsHtml5HtmlAttributes* mAttributes;

  explicit opAddAttributes(nsIContentHandle* aElement,
                           nsHtml5HtmlAttributes* aAttributes)
      : mAttributes(aAttributes) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opCreateHTMLElement {
  nsIContent** mContent;
  nsAtom* mName;
  nsHtml5HtmlAttributes* mAttributes;
  mozilla::dom::HTMLContentCreatorFunction mCreator;
  nsIContent** mIntendedParent;
  mozilla::dom::FromParser mFromNetwork;

  explicit opCreateHTMLElement(
      nsIContentHandle* aContent, nsAtom* aName,
      nsHtml5HtmlAttributes* aAttributes,
      mozilla::dom::HTMLContentCreatorFunction aCreator,
      nsIContentHandle* aIntendedParent, mozilla::dom::FromParser mFromNetwork)
      : mName(aName),
        mAttributes(aAttributes),
        mCreator(aCreator),
        mFromNetwork(mFromNetwork) {
    mContent = static_cast<nsIContent**>(aContent);
    mIntendedParent = static_cast<nsIContent**>(aIntendedParent);
    aName->AddRef();
    if (mAttributes == nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES) {
      mAttributes = nullptr;
    }
  };
};

struct opCreateSVGElement {
  nsIContent** mContent;
  nsAtom* mName;
  nsHtml5HtmlAttributes* mAttributes;
  mozilla::dom::SVGContentCreatorFunction mCreator;
  nsIContent** mIntendedParent;
  mozilla::dom::FromParser mFromNetwork;

  explicit opCreateSVGElement(nsIContentHandle* aContent, nsAtom* aName,
                              nsHtml5HtmlAttributes* aAttributes,
                              mozilla::dom::SVGContentCreatorFunction aCreator,
                              nsIContentHandle* aIntendedParent,
                              mozilla::dom::FromParser mFromNetwork)
      : mName(aName),
        mAttributes(aAttributes),
        mCreator(aCreator),
        mFromNetwork(mFromNetwork) {
    mContent = static_cast<nsIContent**>(aContent);
    mIntendedParent = static_cast<nsIContent**>(aIntendedParent);
    aName->AddRef();
    if (mAttributes == nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES) {
      mAttributes = nullptr;
    }
  };
};

struct opCreateMathMLElement {
  nsIContent** mContent;
  nsAtom* mName;
  nsHtml5HtmlAttributes* mAttributes;
  nsIContent** mIntendedParent;

  explicit opCreateMathMLElement(nsIContentHandle* aContent, nsAtom* aName,
                                 nsHtml5HtmlAttributes* aAttributes,
                                 nsIContentHandle* aIntendedParent)
      : mName(aName), mAttributes(aAttributes) {
    mContent = static_cast<nsIContent**>(aContent);
    mIntendedParent = static_cast<nsIContent**>(aIntendedParent);
    aName->AddRef();
    if (mAttributes == nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES) {
      mAttributes = nullptr;
    }
  };
};

struct opSetFormElement {
  nsIContent** mContent;
  nsIContent** mFormElement;

  explicit opSetFormElement(nsIContentHandle* aContent,
                            nsIContentHandle* aFormElement) {
    mContent = static_cast<nsIContent**>(aContent);
    mFormElement = static_cast<nsIContent**>(aFormElement);
  };
};

struct opAppendText {
  nsIContent** mParent;
  char16_t* mBuffer;
  int32_t mLength;

  explicit opAppendText(nsIContentHandle* aParent, char16_t* aBuffer,
                        int32_t aLength)
      : mBuffer(aBuffer), mLength(aLength) {
    mParent = static_cast<nsIContent**>(aParent);
  };
};

struct opFosterParentText {
  nsIContent** mStackParent;
  char16_t* mBuffer;
  nsIContent** mTable;
  int32_t mLength;

  explicit opFosterParentText(nsIContentHandle* aStackParent, char16_t* aBuffer,
                              nsIContentHandle* aTable, int32_t aLength)
      : mBuffer(aBuffer), mLength(aLength) {
    mStackParent = static_cast<nsIContent**>(aStackParent);
    mTable = static_cast<nsIContent**>(aTable);
  };
};

struct opAppendComment {
  nsIContent** mParent;
  char16_t* mBuffer;
  int32_t mLength;

  explicit opAppendComment(nsIContentHandle* aParent, char16_t* aBuffer,
                           int32_t aLength)
      : mBuffer(aBuffer), mLength(aLength) {
    mParent = static_cast<nsIContent**>(aParent);
  };
};

struct opAppendCommentToDocument {
  char16_t* mBuffer;
  int32_t mLength;

  explicit opAppendCommentToDocument(char16_t* aBuffer, int32_t aLength)
      : mBuffer(aBuffer), mLength(aLength){};
};

class nsHtml5TreeOperationStringPair {
 private:
  nsString mPublicId;
  nsString mSystemId;

 public:
  nsHtml5TreeOperationStringPair(const nsAString& aPublicId,
                                 const nsAString& aSystemId)
      : mPublicId(aPublicId), mSystemId(aSystemId) {
    MOZ_COUNT_CTOR(nsHtml5TreeOperationStringPair);
  }

  ~nsHtml5TreeOperationStringPair() {
    MOZ_COUNT_DTOR(nsHtml5TreeOperationStringPair);
  }

  inline void Get(nsAString& aPublicId, nsAString& aSystemId) {
    aPublicId.Assign(mPublicId);
    aSystemId.Assign(mSystemId);
  }
};

struct opAppendDoctypeToDocument {
  nsAtom* mName;
  nsHtml5TreeOperationStringPair* mStringPair;

  explicit opAppendDoctypeToDocument(nsAtom* aName, const nsAString& aPublicId,
                                     const nsAString& aSystemId) {
    mName = aName;
    aName->AddRef();
    mStringPair = new nsHtml5TreeOperationStringPair(aPublicId, aSystemId);
  }
};

struct opGetDocumentFragmentForTemplate {
  nsIContent** mTemplate;
  nsIContent** mFragHandle;

  explicit opGetDocumentFragmentForTemplate(nsIContentHandle* aTemplate,
                                            nsIContentHandle* aFragHandle) {
    mTemplate = static_cast<nsIContent**>(aTemplate);
    mFragHandle = static_cast<nsIContent**>(aFragHandle);
  }
};

struct opSetDocumentFragmentForTemplate {
  nsIContent** mTemplate;
  nsIContent** mFragment;

  explicit opSetDocumentFragmentForTemplate(nsIContentHandle* aTemplate,
                                            nsIContentHandle* aFragment) {
    mTemplate = static_cast<nsIContent**>(aTemplate);
    mFragment = static_cast<nsIContent**>(aFragment);
  }
};

struct opGetShadowRootFromHost {
  nsIContent** mHost;
  nsIContent** mFragHandle;
  nsIContent** mTemplateNode;
  mozilla::dom::ShadowRootMode mShadowRootMode;
  bool mShadowRootDelegatesFocus;

  explicit opGetShadowRootFromHost(nsIContentHandle* aHost,
                                   nsIContentHandle* aFragHandle,
                                   nsIContentHandle* aTemplateNode,
                                   mozilla::dom::ShadowRootMode aShadowRootMode,
                                   bool aShadowRootDelegatesFocus) {
    mHost = static_cast<nsIContent**>(aHost);
    mFragHandle = static_cast<nsIContent**>(aFragHandle);
    mTemplateNode = static_cast<nsIContent**>(aTemplateNode);
    mShadowRootMode = aShadowRootMode;
    mShadowRootDelegatesFocus = aShadowRootDelegatesFocus;
  }
};

struct opGetFosterParent {
  nsIContent** mTable;
  nsIContent** mStackParent;
  nsIContent** mParentHandle;

  explicit opGetFosterParent(nsIContentHandle* aTable,
                             nsIContentHandle* aStackParent,
                             nsIContentHandle* aParentHandle) {
    mTable = static_cast<nsIContent**>(aTable);
    mStackParent = static_cast<nsIContent**>(aStackParent);
    mParentHandle = static_cast<nsIContent**>(aParentHandle);
  };
};

// Gecko-specific on-pop ops
struct opMarkAsBroken {
  nsresult mResult;

  explicit opMarkAsBroken(nsresult aResult) : mResult(aResult){};
};

struct opRunScriptThatMayDocumentWriteOrBlock {
  nsIContent** mElement;
  nsAHtml5TreeBuilderState* mBuilderState;
  int32_t mLineNumber;

  explicit opRunScriptThatMayDocumentWriteOrBlock(
      nsIContentHandle* aElement, nsAHtml5TreeBuilderState* aBuilderState)
      : mBuilderState(aBuilderState), mLineNumber(0) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opRunScriptThatCannotDocumentWriteOrBlock {
  nsIContent** mElement;

  explicit opRunScriptThatCannotDocumentWriteOrBlock(
      nsIContentHandle* aElement) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opPreventScriptExecution {
  nsIContent** mElement;

  explicit opPreventScriptExecution(nsIContentHandle* aElement) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opDoneAddingChildren {
  nsIContent** mElement;

  explicit opDoneAddingChildren(nsIContentHandle* aElement) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opDoneCreatingElement {
  nsIContent** mElement;

  explicit opDoneCreatingElement(nsIContentHandle* aElement) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opUpdateCharsetSource {
  nsCharsetSource mCharsetSource;

  explicit opUpdateCharsetSource(nsCharsetSource aCharsetSource)
      : mCharsetSource(aCharsetSource){};
};

struct opCharsetSwitchTo {
  const mozilla::Encoding* mEncoding;
  int32_t mCharsetSource;
  int32_t mLineNumber;

  explicit opCharsetSwitchTo(const mozilla::Encoding* aEncoding,
                             int32_t aCharsetSource, int32_t aLineNumber)
      : mEncoding(aEncoding),
        mCharsetSource(aCharsetSource),
        mLineNumber(aLineNumber){};
};

struct opUpdateStyleSheet {
  nsIContent** mElement;

  explicit opUpdateStyleSheet(nsIContentHandle* aElement) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opProcessOfflineManifest {
  char16_t* mUrl;

  explicit opProcessOfflineManifest(char16_t* aUrl) : mUrl(aUrl){};
};

struct opMarkMalformedIfScript {
  nsIContent** mElement;

  explicit opMarkMalformedIfScript(nsIContentHandle* aElement) {
    mElement = static_cast<nsIContent**>(aElement);
  }
};

struct opStreamEnded {};

struct opSetStyleLineNumber {
  nsIContent** mContent;
  int32_t mLineNumber;

  explicit opSetStyleLineNumber(nsIContentHandle* aContent, int32_t aLineNumber)
      : mLineNumber(aLineNumber) {
    mContent = static_cast<nsIContent**>(aContent);
  };
};

struct opSetScriptLineAndColumnNumberAndFreeze {
  nsIContent** mContent;
  int32_t mLineNumber;
  int32_t mColumnNumber;

  explicit opSetScriptLineAndColumnNumberAndFreeze(nsIContentHandle* aContent,
                                                   int32_t aLineNumber,
                                                   int32_t aColumnNumber)
      : mLineNumber(aLineNumber), mColumnNumber(aColumnNumber) {
    mContent = static_cast<nsIContent**>(aContent);
  };
};

struct opSvgLoad {
  nsIContent** mElement;

  explicit opSvgLoad(nsIContentHandle* aElement) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opMaybeComplainAboutCharset {
  char* mMsgId;
  bool mError;
  int32_t mLineNumber;

  explicit opMaybeComplainAboutCharset(char* aMsgId, bool aError,
                                       int32_t aLineNumber)
      : mMsgId(aMsgId), mError(aError), mLineNumber(aLineNumber){};
};

struct opMaybeComplainAboutDeepTree {
  int32_t mLineNumber;

  explicit opMaybeComplainAboutDeepTree(int32_t aLineNumber)
      : mLineNumber(aLineNumber){};
};

struct opAddClass {
  nsIContent** mElement;
  char16_t* mClass;

  explicit opAddClass(nsIContentHandle* aElement, char16_t* aClass)
      : mClass(aClass) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opAddViewSourceHref {
  nsIContent** mElement;
  char16_t* mBuffer;
  int32_t mLength;

  explicit opAddViewSourceHref(nsIContentHandle* aElement, char16_t* aBuffer,
                               int32_t aLength)
      : mBuffer(aBuffer), mLength(aLength) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opAddViewSourceBase {
  char16_t* mBuffer;
  int32_t mLength;

  explicit opAddViewSourceBase(char16_t* aBuffer, int32_t aLength)
      : mBuffer(aBuffer), mLength(aLength){};
};

struct opAddErrorType {
  nsIContent** mElement;
  char* mMsgId;
  nsAtom* mName;
  nsAtom* mOther;

  explicit opAddErrorType(nsIContentHandle* aElement, char* aMsgId,
                          nsAtom* aName = nullptr, nsAtom* aOther = nullptr)
      : mMsgId(aMsgId), mName(aName), mOther(aOther) {
    mElement = static_cast<nsIContent**>(aElement);
    if (aName) {
      aName->AddRef();
    }
    if (aOther) {
      aOther->AddRef();
    }
  };
};

struct opAddLineNumberId {
  nsIContent** mElement;
  int32_t mLineNumber;

  explicit opAddLineNumberId(nsIContentHandle* aElement, int32_t aLineNumber)
      : mLineNumber(aLineNumber) {
    mElement = static_cast<nsIContent**>(aElement);
  };
};

struct opStartLayout {};

struct opEnableEncodingMenu {};

typedef mozilla::Variant<
    uninitialized,
    // main HTML5 ops
    opAppend, opDetach, opAppendChildrenToNewParent, opFosterParent,
    opAppendToDocument, opAddAttributes, nsHtml5DocumentMode,
    opCreateHTMLElement, opCreateSVGElement, opCreateMathMLElement,
    opSetFormElement, opAppendText, opFosterParentText, opAppendComment,
    opAppendCommentToDocument, opAppendDoctypeToDocument,
    opGetDocumentFragmentForTemplate, opSetDocumentFragmentForTemplate,
    opGetShadowRootFromHost, opGetFosterParent,
    // Gecko-specific on-pop ops
    opMarkAsBroken, opRunScriptThatMayDocumentWriteOrBlock,
    opRunScriptThatCannotDocumentWriteOrBlock, opPreventScriptExecution,
    opDoneAddingChildren, opDoneCreatingElement, opUpdateCharsetSource,
    opCharsetSwitchTo, opUpdateStyleSheet, opProcessOfflineManifest,
    opMarkMalformedIfScript, opStreamEnded, opSetStyleLineNumber,
    opSetScriptLineAndColumnNumberAndFreeze, opSvgLoad,
    opMaybeComplainAboutCharset, opMaybeComplainAboutDeepTree, opAddClass,
    opAddViewSourceHref, opAddViewSourceBase, opAddErrorType, opAddLineNumberId,
    opStartLayout, opEnableEncodingMenu>
    treeOperation;

class nsHtml5TreeOperation final {
  template <typename T>
  using NotNull = mozilla::NotNull<T>;
  using Encoding = mozilla::Encoding;

 public:
  static nsresult AppendTextToTextNode(const char16_t* aBuffer,
                                       uint32_t aLength,
                                       mozilla::dom::Text* aTextNode,
                                       nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendText(const char16_t* aBuffer, uint32_t aLength,
                             nsIContent* aParent,
                             nsHtml5DocumentBuilder* aBuilder);

  static nsresult Append(nsIContent* aNode, nsIContent* aParent,
                         nsHtml5DocumentBuilder* aBuilder);

  static nsresult Append(nsIContent* aNode, nsIContent* aParent,
                         mozilla::dom::FromParser aFromParser,
                         nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendToDocument(nsIContent* aNode,
                                   nsHtml5DocumentBuilder* aBuilder);

  static void Detach(nsIContent* aNode, nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendChildrenToNewParent(nsIContent* aNode,
                                            nsIContent* aParent,
                                            nsHtml5DocumentBuilder* aBuilder);

  static nsresult FosterParent(nsIContent* aNode, nsIContent* aParent,
                               nsIContent* aTable,
                               nsHtml5DocumentBuilder* aBuilder);

  static nsresult AddAttributes(nsIContent* aNode,
                                nsHtml5HtmlAttributes* aAttributes,
                                nsHtml5DocumentBuilder* aBuilder);

  static void SetHTMLElementAttributes(mozilla::dom::Element* aElement,
                                       nsAtom* aName,
                                       nsHtml5HtmlAttributes* aAttributes);

  static nsIContent* CreateHTMLElement(
      nsAtom* aName, nsHtml5HtmlAttributes* aAttributes,
      mozilla::dom::FromParser aFromParser, nsNodeInfoManager* aNodeInfoManager,
      nsHtml5DocumentBuilder* aBuilder,
      mozilla::dom::HTMLContentCreatorFunction aCreator);

  static nsIContent* CreateSVGElement(
      nsAtom* aName, nsHtml5HtmlAttributes* aAttributes,
      mozilla::dom::FromParser aFromParser, nsNodeInfoManager* aNodeInfoManager,
      nsHtml5DocumentBuilder* aBuilder,
      mozilla::dom::SVGContentCreatorFunction aCreator);

  static nsIContent* CreateMathMLElement(nsAtom* aName,
                                         nsHtml5HtmlAttributes* aAttributes,
                                         nsNodeInfoManager* aNodeInfoManager,
                                         nsHtml5DocumentBuilder* aBuilder);

  static void SetFormElement(nsIContent* aNode, nsIContent* aParent);

  static nsresult AppendIsindexPrompt(nsIContent* parent,
                                      nsHtml5DocumentBuilder* aBuilder);

  static nsresult FosterParentText(nsIContent* aStackParent, char16_t* aBuffer,
                                   uint32_t aLength, nsIContent* aTable,
                                   nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendComment(nsIContent* aParent, char16_t* aBuffer,
                                int32_t aLength,
                                nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendCommentToDocument(char16_t* aBuffer, int32_t aLength,
                                          nsHtml5DocumentBuilder* aBuilder);

  static nsresult AppendDoctypeToDocument(nsAtom* aName,
                                          const nsAString& aPublicId,
                                          const nsAString& aSystemId,
                                          nsHtml5DocumentBuilder* aBuilder);

  static nsIContent* GetDocumentFragmentForTemplate(nsIContent* aNode);
  static void SetDocumentFragmentForTemplate(nsIContent* aNode,
                                             nsIContent* aDocumentFragment);

  static nsIContent* GetFosterParent(nsIContent* aTable,
                                     nsIContent* aStackParent);

  static void PreventScriptExecution(nsIContent* aNode);

  static void DoneAddingChildren(nsIContent* aNode);

  static void DoneCreatingElement(nsIContent* aNode);

  static void SvgLoad(nsIContent* aNode);

  static void MarkMalformedIfScript(nsIContent* aNode);

  nsHtml5TreeOperation();

  ~nsHtml5TreeOperation();

  inline void Init(const treeOperation& aOperation) {
    NS_ASSERTION(mOperation.is<uninitialized>(),
                 "Op code must be uninitialized when initializing.");
    mOperation = aOperation;
  }

  inline bool IsRunScriptThatMayDocumentWriteOrBlock() {
    return mOperation.is<opRunScriptThatMayDocumentWriteOrBlock>();
  }

  inline bool IsMarkAsBroken() { return mOperation.is<opMarkAsBroken>(); }

  inline void SetSnapshot(nsAHtml5TreeBuilderState* aSnapshot, int32_t aLine) {
    MOZ_ASSERT(
        IsRunScriptThatMayDocumentWriteOrBlock(),
        "Setting a snapshot for a tree operation other than eTreeOpRunScript!");
    MOZ_ASSERT(aSnapshot, "Initialized tree op with null snapshot.");
    opRunScriptThatMayDocumentWriteOrBlock data =
        mOperation.as<opRunScriptThatMayDocumentWriteOrBlock>();
    data.mBuilderState = aSnapshot;
    data.mLineNumber = aLine;
    mOperation = mozilla::AsVariant(data);
  }

  nsresult Perform(nsHtml5TreeOpExecutor* aBuilder, nsIContent** aScriptElement,
                   bool* aInterrupted, bool* aStreamEnded);

 private:
  nsHtml5TreeOperation(const nsHtml5TreeOperation&) = delete;
  nsHtml5TreeOperation& operator=(const nsHtml5TreeOperation&) = delete;

  treeOperation mOperation;
};

#endif  // nsHtml5TreeOperation_h
