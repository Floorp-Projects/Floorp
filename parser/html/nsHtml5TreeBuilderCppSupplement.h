/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Likely.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/UniquePtr.h"

nsHtml5TreeBuilder::nsHtml5TreeBuilder(nsHtml5OplessBuilder* aBuilder)
    : mode(0),
      originalMode(0),
      framesetOk(false),
      tokenizer(nullptr),
      scriptingEnabled(false),
      needToDropLF(false),
      fragment(false),
      contextName(nullptr),
      contextNamespace(kNameSpaceID_None),
      contextNode(nullptr),
      templateModePtr(0),
      stackNodesIdx(0),
      numStackNodes(0),
      currentPtr(0),
      listPtr(0),
      formPointer(nullptr),
      headPointer(nullptr),
      charBufferLen(0),
      quirks(false),
      forceNoQuirks(false),
      mBuilder(aBuilder),
      mViewSource(nullptr),
      mOpSink(nullptr),
      mHandles(nullptr),
      mHandlesUsed(0),
      mSpeculativeLoadStage(nullptr),
      mBroken(NS_OK),
      mCurrentHtmlScriptIsAsyncOrDefer(false),
      mPreventScriptExecution(false)
#ifdef DEBUG
      ,
      mActive(false)
#endif
{
  MOZ_COUNT_CTOR(nsHtml5TreeBuilder);
}

nsHtml5TreeBuilder::nsHtml5TreeBuilder(nsAHtml5TreeOpSink* aOpSink,
                                       nsHtml5TreeOpStage* aStage)
    : mode(0),
      originalMode(0),
      framesetOk(false),
      tokenizer(nullptr),
      scriptingEnabled(false),
      needToDropLF(false),
      fragment(false),
      contextName(nullptr),
      contextNamespace(kNameSpaceID_None),
      contextNode(nullptr),
      templateModePtr(0),
      stackNodesIdx(0),
      numStackNodes(0),
      currentPtr(0),
      listPtr(0),
      formPointer(nullptr),
      headPointer(nullptr),
      charBufferLen(0),
      quirks(false),
      forceNoQuirks(false),
      mBuilder(nullptr),
      mViewSource(nullptr),
      mOpSink(aOpSink),
      mHandles(new nsIContent*[NS_HTML5_TREE_BUILDER_HANDLE_ARRAY_LENGTH]),
      mHandlesUsed(0),
      mSpeculativeLoadStage(aStage),
      mBroken(NS_OK),
      mCurrentHtmlScriptIsAsyncOrDefer(false),
      mPreventScriptExecution(false)
#ifdef DEBUG
      ,
      mActive(false)
#endif
{
  MOZ_COUNT_CTOR(nsHtml5TreeBuilder);
}

nsHtml5TreeBuilder::~nsHtml5TreeBuilder() {
  MOZ_COUNT_DTOR(nsHtml5TreeBuilder);
  NS_ASSERTION(!mActive,
               "nsHtml5TreeBuilder deleted without ever calling end() on it!");
  mOpQueue.Clear();
}

nsIContentHandle* nsHtml5TreeBuilder::createElement(
    int32_t aNamespace, nsAtom* aName, nsHtml5HtmlAttributes* aAttributes,
    nsIContentHandle* aIntendedParent, nsHtml5ContentCreatorFunction aCreator) {
  MOZ_ASSERT(aAttributes, "Got null attributes.");
  MOZ_ASSERT(aName, "Got null name.");
  MOZ_ASSERT(aNamespace == kNameSpaceID_XHTML ||
                 aNamespace == kNameSpaceID_SVG ||
                 aNamespace == kNameSpaceID_MathML,
             "Bogus namespace.");

  if (mBuilder) {
    nsIContent* intendedParent =
        aIntendedParent ? static_cast<nsIContent*>(aIntendedParent) : nullptr;

    // intendedParent == nullptr is a special case where the
    // intended parent is the document.
    nsNodeInfoManager* nodeInfoManager =
        intendedParent ? intendedParent->OwnerDoc()->NodeInfoManager()
                       : mBuilder->GetNodeInfoManager();

    nsIContent* elem;
    if (aNamespace == kNameSpaceID_XHTML) {
      elem = nsHtml5TreeOperation::CreateHTMLElement(
          aName, aAttributes, mozilla::dom::FROM_PARSER_FRAGMENT,
          nodeInfoManager, mBuilder, aCreator.html);
    } else if (aNamespace == kNameSpaceID_SVG) {
      elem = nsHtml5TreeOperation::CreateSVGElement(
          aName, aAttributes, mozilla::dom::FROM_PARSER_FRAGMENT,
          nodeInfoManager, mBuilder, aCreator.svg);
    } else {
      MOZ_ASSERT(aNamespace == kNameSpaceID_MathML);
      elem = nsHtml5TreeOperation::CreateMathMLElement(
          aName, aAttributes, nodeInfoManager, mBuilder);
    }
    if (MOZ_UNLIKELY(aAttributes != tokenizer->GetAttributes() &&
                     aAttributes != nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES)) {
      delete aAttributes;
    }
    return elem;
  }

  nsIContentHandle* content = AllocateContentHandle();
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  if (aNamespace == kNameSpaceID_XHTML) {
    opCreateHTMLElement opeation(
        content, aName, aAttributes, aCreator.html, aIntendedParent,
        (!!mSpeculativeLoadStage) ? mozilla::dom::FROM_PARSER_NETWORK
                                  : mozilla::dom::FROM_PARSER_DOCUMENT_WRITE);
    treeOp->Init(mozilla::AsVariant(opeation));
  } else if (aNamespace == kNameSpaceID_SVG) {
    opCreateSVGElement operation(
        content, aName, aAttributes, aCreator.svg, aIntendedParent,
        (!!mSpeculativeLoadStage) ? mozilla::dom::FROM_PARSER_NETWORK
                                  : mozilla::dom::FROM_PARSER_DOCUMENT_WRITE);
    treeOp->Init(mozilla::AsVariant(operation));
  } else {
    // kNameSpaceID_MathML
    opCreateMathMLElement operation(content, aName, aAttributes,
                                    aIntendedParent);
    treeOp->Init(mozilla::AsVariant(operation));
  }

  // mSpeculativeLoadStage is non-null only in the off-the-main-thread
  // tree builder, which handles the network stream

  // Start wall of code for speculative loading and line numbers

  if (mSpeculativeLoadStage && mode != IN_TEMPLATE) {
    switch (aNamespace) {
      case kNameSpaceID_XHTML:
        if (nsGkAtoms::img == aName) {
          nsHtml5String loading =
              aAttributes->getValue(nsHtml5AttributeName::ATTR_LOADING);
          if (!StaticPrefs::dom_image_lazy_loading_enabled() ||
              !loading.LowerCaseEqualsASCII("lazy")) {
            nsHtml5String url =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_SRC);
            nsHtml5String srcset =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_SRCSET);
            nsHtml5String crossOrigin =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_CROSSORIGIN);
            nsHtml5String referrerPolicy = aAttributes->getValue(
                nsHtml5AttributeName::ATTR_REFERRERPOLICY);
            nsHtml5String sizes =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_SIZES);
            mSpeculativeLoadQueue.AppendElement()->InitImage(
                url, crossOrigin, /* aMedia = */ nullptr, referrerPolicy,
                srcset, sizes, false);
          }
        } else if (nsGkAtoms::source == aName) {
          nsHtml5String srcset =
              aAttributes->getValue(nsHtml5AttributeName::ATTR_SRCSET);
          // Sources without srcset cannot be selected. The source could also be
          // for a media element, but in that context doesn't use srcset.  See
          // comments in nsHtml5SpeculativeLoad.h about <picture> preloading
          if (srcset) {
            nsHtml5String sizes =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_SIZES);
            nsHtml5String type =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_TYPE);
            nsHtml5String media =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_MEDIA);
            mSpeculativeLoadQueue.AppendElement()->InitPictureSource(
                srcset, sizes, type, media);
          }
        } else if (nsGkAtoms::script == aName) {
          nsHtml5TreeOperation* treeOp =
              mOpQueue.AppendElement(mozilla::fallible);
          if (MOZ_UNLIKELY(!treeOp)) {
            MarkAsBrokenAndRequestSuspensionWithoutBuilder(
                NS_ERROR_OUT_OF_MEMORY);
            return nullptr;
          }
          opSetScriptLineNumberAndFreeze operation(content,
                                                   tokenizer->getLineNumber());
          treeOp->Init(mozilla::AsVariant(operation));

          nsHtml5String url =
              aAttributes->getValue(nsHtml5AttributeName::ATTR_SRC);
          if (url) {
            nsHtml5String charset =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_CHARSET);
            nsHtml5String type =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_TYPE);
            nsHtml5String crossOrigin =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_CROSSORIGIN);
            nsHtml5String integrity =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_INTEGRITY);
            nsHtml5String referrerPolicy = aAttributes->getValue(
                nsHtml5AttributeName::ATTR_REFERRERPOLICY);
            bool async =
                aAttributes->contains(nsHtml5AttributeName::ATTR_ASYNC);
            bool defer =
                aAttributes->contains(nsHtml5AttributeName::ATTR_DEFER);
            bool noModule =
                aAttributes->contains(nsHtml5AttributeName::ATTR_NOMODULE);
            mSpeculativeLoadQueue.AppendElement()->InitScript(
                url, charset, type, crossOrigin, /* aMedia = */ nullptr,
                integrity, referrerPolicy, mode == nsHtml5TreeBuilder::IN_HEAD,
                async, defer, noModule, false);
            mCurrentHtmlScriptIsAsyncOrDefer = async || defer;
          }
        } else if (nsGkAtoms::link == aName) {
          nsHtml5String rel =
              aAttributes->getValue(nsHtml5AttributeName::ATTR_REL);
          // Not splitting on space here is bogus but the old parser didn't even
          // do a case-insensitive check.
          if (rel) {
            if (rel.LowerCaseEqualsASCII("stylesheet")) {
              nsHtml5String url =
                  aAttributes->getValue(nsHtml5AttributeName::ATTR_HREF);
              if (url) {
                nsHtml5String charset =
                    aAttributes->getValue(nsHtml5AttributeName::ATTR_CHARSET);
                nsHtml5String crossOrigin = aAttributes->getValue(
                    nsHtml5AttributeName::ATTR_CROSSORIGIN);
                nsHtml5String integrity =
                    aAttributes->getValue(nsHtml5AttributeName::ATTR_INTEGRITY);
                nsHtml5String referrerPolicy = aAttributes->getValue(
                    nsHtml5AttributeName::ATTR_REFERRERPOLICY);
                nsHtml5String media =
                    aAttributes->getValue(nsHtml5AttributeName::ATTR_MEDIA);
                mSpeculativeLoadQueue.AppendElement()->InitStyle(
                    url, charset, crossOrigin, media, referrerPolicy, integrity,
                    false);
              }
            } else if (rel.LowerCaseEqualsASCII("preconnect")) {
              nsHtml5String url =
                  aAttributes->getValue(nsHtml5AttributeName::ATTR_HREF);
              if (url) {
                nsHtml5String crossOrigin = aAttributes->getValue(
                    nsHtml5AttributeName::ATTR_CROSSORIGIN);
                mSpeculativeLoadQueue.AppendElement()->InitPreconnect(
                    url, crossOrigin);
              }
            } else if (StaticPrefs::network_preload() &&
                       rel.LowerCaseEqualsASCII("preload")) {
              nsHtml5String url =
                  aAttributes->getValue(nsHtml5AttributeName::ATTR_HREF);
              if (url) {
                nsHtml5String as =
                    aAttributes->getValue(nsHtml5AttributeName::ATTR_AS);
                nsHtml5String charset =
                    aAttributes->getValue(nsHtml5AttributeName::ATTR_CHARSET);
                nsHtml5String crossOrigin = aAttributes->getValue(
                    nsHtml5AttributeName::ATTR_CROSSORIGIN);
                nsHtml5String integrity =
                    aAttributes->getValue(nsHtml5AttributeName::ATTR_INTEGRITY);
                nsHtml5String referrerPolicy = aAttributes->getValue(
                    nsHtml5AttributeName::ATTR_REFERRERPOLICY);
                nsHtml5String media =
                    aAttributes->getValue(nsHtml5AttributeName::ATTR_MEDIA);

                // Note that respective speculative loaders for scripts and
                // styles check all additional attributes to be equal to use the
                // speculative load.  So, if any of them is specified and the
                // preload has to take the expected effect, those attributes
                // must also be specified on the actual tag to use the preload.
                // Omitting an attribute on both will make the values equal
                // (empty) and thus use the preload.
                if (as.LowerCaseEqualsASCII("script")) {
                  nsHtml5String type =
                      aAttributes->getValue(nsHtml5AttributeName::ATTR_TYPE);
                  mSpeculativeLoadQueue.AppendElement()->InitScript(
                      url, charset, type, crossOrigin, media, integrity,
                      referrerPolicy, mode == nsHtml5TreeBuilder::IN_HEAD,
                      false, false, false, true);
                } else if (as.LowerCaseEqualsASCII("style")) {
                  mSpeculativeLoadQueue.AppendElement()->InitStyle(
                      url, charset, crossOrigin, media, referrerPolicy,
                      integrity, true);
                } else if (as.LowerCaseEqualsASCII("image")) {
                  nsHtml5String srcset = aAttributes->getValue(
                      nsHtml5AttributeName::ATTR_IMAGESRCSET);
                  nsHtml5String sizes = aAttributes->getValue(
                      nsHtml5AttributeName::ATTR_IMAGESIZES);
                  mSpeculativeLoadQueue.AppendElement()->InitImage(
                      url, crossOrigin, media, referrerPolicy, srcset, sizes,
                      true);
                } else if (as.LowerCaseEqualsASCII("font")) {
                  mSpeculativeLoadQueue.AppendElement()->InitFont(
                      url, crossOrigin, media, referrerPolicy);
                } else if (as.LowerCaseEqualsASCII("fetch")) {
                  mSpeculativeLoadQueue.AppendElement()->InitFetch(
                      url, crossOrigin, media, referrerPolicy);
                }
                // Other "as" values will be supported later.
              }
            }
          }
        } else if (nsGkAtoms::video == aName) {
          nsHtml5String url =
              aAttributes->getValue(nsHtml5AttributeName::ATTR_POSTER);
          if (url) {
            mSpeculativeLoadQueue.AppendElement()->InitImage(
                url, nullptr, nullptr, nullptr, nullptr, nullptr, false);
          }
        } else if (nsGkAtoms::style == aName) {
          mImportScanner.Start();
          nsHtml5TreeOperation* treeOp =
              mOpQueue.AppendElement(mozilla::fallible);
          if (MOZ_UNLIKELY(!treeOp)) {
            MarkAsBrokenAndRequestSuspensionWithoutBuilder(
                NS_ERROR_OUT_OF_MEMORY);
            return nullptr;
          }
          opSetStyleLineNumber operation(content, tokenizer->getLineNumber());
          treeOp->Init(mozilla::AsVariant(operation));
        } else if (nsGkAtoms::html == aName) {
          nsHtml5String url =
              aAttributes->getValue(nsHtml5AttributeName::ATTR_MANIFEST);
          mSpeculativeLoadQueue.AppendElement()->InitManifest(url);
        } else if (nsGkAtoms::base == aName) {
          nsHtml5String url =
              aAttributes->getValue(nsHtml5AttributeName::ATTR_HREF);
          if (url) {
            mSpeculativeLoadQueue.AppendElement()->InitBase(url);
          }
        } else if (nsGkAtoms::meta == aName) {
          if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                  "content-security-policy",
                  aAttributes->getValue(
                      nsHtml5AttributeName::ATTR_HTTP_EQUIV))) {
            nsHtml5String csp =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_CONTENT);
            if (csp) {
              mSpeculativeLoadQueue.AppendElement()->InitMetaCSP(csp);
            }
          } else if (nsHtml5Portability::
                         lowerCaseLiteralEqualsIgnoreAsciiCaseString(
                             "referrer",
                             aAttributes->getValue(
                                 nsHtml5AttributeName::ATTR_NAME))) {
            nsHtml5String referrerPolicy =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_CONTENT);
            if (referrerPolicy) {
              mSpeculativeLoadQueue.AppendElement()->InitMetaReferrerPolicy(
                  referrerPolicy);
            }
          }
        }
        break;
      case kNameSpaceID_SVG:
        if (nsGkAtoms::image == aName) {
          nsHtml5String url =
              aAttributes->getValue(nsHtml5AttributeName::ATTR_HREF);
          if (!url) {
            url = aAttributes->getValue(nsHtml5AttributeName::ATTR_XLINK_HREF);
          }
          if (url) {
            mSpeculativeLoadQueue.AppendElement()->InitImage(
                url, nullptr, nullptr, nullptr, nullptr, nullptr, false);
          }
        } else if (nsGkAtoms::script == aName) {
          nsHtml5TreeOperation* treeOp =
              mOpQueue.AppendElement(mozilla::fallible);
          if (MOZ_UNLIKELY(!treeOp)) {
            MarkAsBrokenAndRequestSuspensionWithoutBuilder(
                NS_ERROR_OUT_OF_MEMORY);
            return nullptr;
          }
          opSetScriptLineNumberAndFreeze operation(content,
                                                   tokenizer->getLineNumber());
          treeOp->Init(mozilla::AsVariant(operation));

          nsHtml5String url =
              aAttributes->getValue(nsHtml5AttributeName::ATTR_HREF);
          if (!url) {
            url = aAttributes->getValue(nsHtml5AttributeName::ATTR_XLINK_HREF);
          }
          if (url) {
            nsHtml5String type =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_TYPE);
            nsHtml5String crossOrigin =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_CROSSORIGIN);
            nsHtml5String integrity =
                aAttributes->getValue(nsHtml5AttributeName::ATTR_INTEGRITY);
            nsHtml5String referrerPolicy = aAttributes->getValue(
                nsHtml5AttributeName::ATTR_REFERRERPOLICY);
            mSpeculativeLoadQueue.AppendElement()->InitScript(
                url, nullptr, type, crossOrigin, /* aMedia = */ nullptr,
                integrity, referrerPolicy, mode == nsHtml5TreeBuilder::IN_HEAD,
                false, false, false, false);
          }
        } else if (nsGkAtoms::style == aName) {
          mImportScanner.Start();
          nsHtml5TreeOperation* treeOp =
              mOpQueue.AppendElement(mozilla::fallible);
          if (MOZ_UNLIKELY(!treeOp)) {
            MarkAsBrokenAndRequestSuspensionWithoutBuilder(
                NS_ERROR_OUT_OF_MEMORY);
            return nullptr;
          }
          opSetStyleLineNumber operation(content, tokenizer->getLineNumber());
          treeOp->Init(mozilla::AsVariant(operation));
        }
        break;
    }
  } else if (aNamespace != kNameSpaceID_MathML) {
    // No speculative loader--just line numbers and defer/async check
    if (nsGkAtoms::style == aName) {
      nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
      if (MOZ_UNLIKELY(!treeOp)) {
        MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
        return nullptr;
      }
      opSetStyleLineNumber operation(content, tokenizer->getLineNumber());
      treeOp->Init(mozilla::AsVariant(operation));
    } else if (nsGkAtoms::script == aName) {
      nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
      if (MOZ_UNLIKELY(!treeOp)) {
        MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
        return nullptr;
      }
      opSetScriptLineNumberAndFreeze operation(content,
                                               tokenizer->getLineNumber());
      treeOp->Init(mozilla::AsVariant(operation));
      if (aNamespace == kNameSpaceID_XHTML) {
        mCurrentHtmlScriptIsAsyncOrDefer =
            aAttributes->contains(nsHtml5AttributeName::ATTR_SRC) &&
            (aAttributes->contains(nsHtml5AttributeName::ATTR_ASYNC) ||
             aAttributes->contains(nsHtml5AttributeName::ATTR_DEFER));
      }
    } else if (aNamespace == kNameSpaceID_XHTML) {
      if (nsGkAtoms::html == aName) {
        nsHtml5String url =
            aAttributes->getValue(nsHtml5AttributeName::ATTR_MANIFEST);
        nsHtml5TreeOperation* treeOp =
            mOpQueue.AppendElement(mozilla::fallible);
        if (MOZ_UNLIKELY(!treeOp)) {
          MarkAsBrokenAndRequestSuspensionWithoutBuilder(
              NS_ERROR_OUT_OF_MEMORY);
          return nullptr;
        }
        if (url) {
          nsString
              urlString;  // Not Auto, because using it to hold nsStringBuffer*
          url.ToString(urlString);
          opProcessOfflineManifest operation(ToNewUnicode(urlString));
          treeOp->Init(mozilla::AsVariant(operation));
        } else {
          opProcessOfflineManifest operation(ToNewUnicode(u""_ns));
          treeOp->Init(mozilla::AsVariant(operation));
        }
      } else if (nsGkAtoms::base == aName && mViewSource) {
        nsHtml5String url =
            aAttributes->getValue(nsHtml5AttributeName::ATTR_HREF);
        if (url) {
          mViewSource->AddBase(url);
        }
      }
    }
  }

  // End wall of code for speculative loading

  return content;
}

nsIContentHandle* nsHtml5TreeBuilder::createElement(
    int32_t aNamespace, nsAtom* aName, nsHtml5HtmlAttributes* aAttributes,
    nsIContentHandle* aFormElement, nsIContentHandle* aIntendedParent,
    nsHtml5ContentCreatorFunction aCreator) {
  nsIContentHandle* content =
      createElement(aNamespace, aName, aAttributes, aIntendedParent, aCreator);
  if (aFormElement) {
    if (mBuilder) {
      nsHtml5TreeOperation::SetFormElement(
          static_cast<nsIContent*>(content),
          static_cast<nsIContent*>(aFormElement));
    } else {
      nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
      if (MOZ_UNLIKELY(!treeOp)) {
        MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
        return nullptr;
      }
      opSetFormElement operation(content, aFormElement);
      treeOp->Init(mozilla::AsVariant(operation));
    }
  }
  return content;
}

nsIContentHandle* nsHtml5TreeBuilder::createHtmlElementSetAsRoot(
    nsHtml5HtmlAttributes* aAttributes) {
  nsHtml5ContentCreatorFunction creator;
  // <html> uses NS_NewHTMLSharedElement creator
  creator.html = NS_NewHTMLSharedElement;
  nsIContentHandle* content = createElement(kNameSpaceID_XHTML, nsGkAtoms::html,
                                            aAttributes, nullptr, creator);
  if (mBuilder) {
    nsresult rv = nsHtml5TreeOperation::AppendToDocument(
        static_cast<nsIContent*>(content), mBuilder);
    if (NS_FAILED(rv)) {
      MarkAsBrokenAndRequestSuspensionWithBuilder(rv);
    }
  } else {
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
    if (MOZ_UNLIKELY(!treeOp)) {
      MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
    opAppendToDocument operation(content);
    treeOp->Init(mozilla::AsVariant(operation));
  }
  return content;
}

nsIContentHandle* nsHtml5TreeBuilder::createAndInsertFosterParentedElement(
    int32_t aNamespace, nsAtom* aName, nsHtml5HtmlAttributes* aAttributes,
    nsIContentHandle* aFormElement, nsIContentHandle* aTable,
    nsIContentHandle* aStackParent, nsHtml5ContentCreatorFunction aCreator) {
  MOZ_ASSERT(aTable, "Null table");
  MOZ_ASSERT(aStackParent, "Null stack parent");

  if (mBuilder) {
    // Get the foster parent to use as the intended parent when creating
    // the child element.
    nsIContent* fosterParent = nsHtml5TreeOperation::GetFosterParent(
        static_cast<nsIContent*>(aTable),
        static_cast<nsIContent*>(aStackParent));

    nsIContentHandle* child = createElement(
        aNamespace, aName, aAttributes, aFormElement, fosterParent, aCreator);

    insertFosterParentedChild(child, aTable, aStackParent);

    return child;
  }

  // Tree op to get the foster parent that we use as the intended parent
  // when creating the child element.
  nsHtml5TreeOperation* fosterParentTreeOp = mOpQueue.AppendElement();
  NS_ASSERTION(fosterParentTreeOp, "Tree op allocation failed.");
  nsIContentHandle* fosterParentHandle = AllocateContentHandle();
  opGetFosterParent operation(aTable, aStackParent, fosterParentHandle);
  fosterParentTreeOp->Init(mozilla::AsVariant(operation));

  // Create the element with the correct intended parent.
  nsIContentHandle* child =
      createElement(aNamespace, aName, aAttributes, aFormElement,
                    fosterParentHandle, aCreator);

  // Insert the child into the foster parent.
  insertFosterParentedChild(child, aTable, aStackParent);

  return child;
}

void nsHtml5TreeBuilder::detachFromParent(nsIContentHandle* aElement) {
  MOZ_ASSERT(aElement, "Null element");

  if (mBuilder) {
    nsHtml5TreeOperation::Detach(static_cast<nsIContent*>(aElement), mBuilder);
    return;
  }

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opDetach operation(aElement);
  treeOp->Init(mozilla::AsVariant(operation));
}

void nsHtml5TreeBuilder::appendElement(nsIContentHandle* aChild,
                                       nsIContentHandle* aParent) {
  MOZ_ASSERT(aChild, "Null child");
  MOZ_ASSERT(aParent, "Null parent");

  if (mBuilder) {
    nsresult rv = nsHtml5TreeOperation::Append(
        static_cast<nsIContent*>(aChild), static_cast<nsIContent*>(aParent),
        mozilla::dom::FROM_PARSER_FRAGMENT, mBuilder);
    if (NS_FAILED(rv)) {
      MarkAsBrokenAndRequestSuspensionWithBuilder(rv);
    }
    return;
  }

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  opAppend operation(aChild, aParent,
                     (!!mSpeculativeLoadStage)
                         ? mozilla::dom::FROM_PARSER_NETWORK
                         : mozilla::dom::FROM_PARSER_DOCUMENT_WRITE);
  treeOp->Init(mozilla::AsVariant(operation));
}

void nsHtml5TreeBuilder::appendChildrenToNewParent(
    nsIContentHandle* aOldParent, nsIContentHandle* aNewParent) {
  MOZ_ASSERT(aOldParent, "Null old parent");
  MOZ_ASSERT(aNewParent, "Null new parent");

  if (mBuilder) {
    nsresult rv = nsHtml5TreeOperation::AppendChildrenToNewParent(
        static_cast<nsIContent*>(aOldParent),
        static_cast<nsIContent*>(aNewParent), mBuilder);
    if (NS_FAILED(rv)) {
      MarkAsBrokenAndRequestSuspensionWithBuilder(rv);
    }
    return;
  }

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opAppendChildrenToNewParent operation(aOldParent, aNewParent);
  treeOp->Init(mozilla::AsVariant(operation));
}

void nsHtml5TreeBuilder::insertFosterParentedCharacters(
    char16_t* aBuffer, int32_t aStart, int32_t aLength,
    nsIContentHandle* aTable, nsIContentHandle* aStackParent) {
  MOZ_ASSERT(aBuffer, "Null buffer");
  MOZ_ASSERT(aTable, "Null table");
  MOZ_ASSERT(aStackParent, "Null stack parent");
  MOZ_ASSERT(!aStart, "aStart must always be zero.");

  if (mBuilder) {
    nsresult rv = nsHtml5TreeOperation::FosterParentText(
        static_cast<nsIContent*>(aStackParent),
        aBuffer,  // XXX aStart always ignored???
        aLength, static_cast<nsIContent*>(aTable), mBuilder);
    if (NS_FAILED(rv)) {
      MarkAsBrokenAndRequestSuspensionWithBuilder(rv);
    }
    return;
  }

  auto bufferCopy = mozilla::MakeUniqueFallible<char16_t[]>(aLength);
  if (!bufferCopy) {
    // Just assigning mBroken instead of generating tree op. The caller
    // of tokenizeBuffer() will call MarkAsBroken() as appropriate.
    mBroken = NS_ERROR_OUT_OF_MEMORY;
    requestSuspension();
    return;
  }

  memcpy(bufferCopy.get(), aBuffer, aLength * sizeof(char16_t));

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opFosterParentText operation(aStackParent, bufferCopy.release(), aTable,
                               aLength);
  treeOp->Init(mozilla::AsVariant(operation));
}

void nsHtml5TreeBuilder::insertFosterParentedChild(
    nsIContentHandle* aChild, nsIContentHandle* aTable,
    nsIContentHandle* aStackParent) {
  MOZ_ASSERT(aChild, "Null child");
  MOZ_ASSERT(aTable, "Null table");
  MOZ_ASSERT(aStackParent, "Null stack parent");

  if (mBuilder) {
    nsresult rv = nsHtml5TreeOperation::FosterParent(
        static_cast<nsIContent*>(aChild),
        static_cast<nsIContent*>(aStackParent),
        static_cast<nsIContent*>(aTable), mBuilder);
    if (NS_FAILED(rv)) {
      MarkAsBrokenAndRequestSuspensionWithBuilder(rv);
    }
    return;
  }

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opFosterParent operation(aChild, aStackParent, aTable);
  treeOp->Init(mozilla::AsVariant(operation));
}

void nsHtml5TreeBuilder::appendCharacters(nsIContentHandle* aParent,
                                          char16_t* aBuffer, int32_t aStart,
                                          int32_t aLength) {
  MOZ_ASSERT(aBuffer, "Null buffer");
  MOZ_ASSERT(aParent, "Null parent");
  MOZ_ASSERT(!aStart, "aStart must always be zero.");

  if (mBuilder) {
    nsresult rv = nsHtml5TreeOperation::AppendText(
        aBuffer,  // XXX aStart always ignored???
        aLength, static_cast<nsIContent*>(aParent), mBuilder);
    if (NS_FAILED(rv)) {
      MarkAsBrokenAndRequestSuspensionWithBuilder(rv);
    }
    return;
  }

  auto bufferCopy = mozilla::MakeUniqueFallible<char16_t[]>(aLength);
  if (!bufferCopy) {
    // Just assigning mBroken instead of generating tree op. The caller
    // of tokenizeBuffer() will call MarkAsBroken() as appropriate.
    mBroken = NS_ERROR_OUT_OF_MEMORY;
    requestSuspension();
    return;
  }

  memcpy(bufferCopy.get(), aBuffer, aLength * sizeof(char16_t));

  if (mImportScanner.ShouldScan()) {
    nsTArray<nsString> imports = mImportScanner.Scan(Span(aBuffer, aLength));
    for (nsString& url : imports) {
      mSpeculativeLoadQueue.AppendElement()->InitImportStyle(std::move(url));
    }
  }

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opAppendText operation(aParent, bufferCopy.release(), aLength);
  treeOp->Init(mozilla::AsVariant(operation));
}

void nsHtml5TreeBuilder::appendComment(nsIContentHandle* aParent,
                                       char16_t* aBuffer, int32_t aStart,
                                       int32_t aLength) {
  MOZ_ASSERT(aBuffer, "Null buffer");
  MOZ_ASSERT(aParent, "Null parent");
  MOZ_ASSERT(!aStart, "aStart must always be zero.");

  if (mBuilder) {
    nsresult rv = nsHtml5TreeOperation::AppendComment(
        static_cast<nsIContent*>(aParent),
        aBuffer,  // XXX aStart always ignored???
        aLength, mBuilder);
    if (NS_FAILED(rv)) {
      MarkAsBrokenAndRequestSuspensionWithBuilder(rv);
    }
    return;
  }

  auto bufferCopy = mozilla::MakeUniqueFallible<char16_t[]>(aLength);
  if (!bufferCopy) {
    // Just assigning mBroken instead of generating tree op. The caller
    // of tokenizeBuffer() will call MarkAsBroken() as appropriate.
    mBroken = NS_ERROR_OUT_OF_MEMORY;
    requestSuspension();
    return;
  }

  memcpy(bufferCopy.get(), aBuffer, aLength * sizeof(char16_t));

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opAppendComment operation(aParent, bufferCopy.release(), aLength);
  treeOp->Init(mozilla::AsVariant(operation));
}

void nsHtml5TreeBuilder::appendCommentToDocument(char16_t* aBuffer,
                                                 int32_t aStart,
                                                 int32_t aLength) {
  MOZ_ASSERT(aBuffer, "Null buffer");
  MOZ_ASSERT(!aStart, "aStart must always be zero.");

  if (mBuilder) {
    nsresult rv = nsHtml5TreeOperation::AppendCommentToDocument(
        aBuffer,  // XXX aStart always ignored???
        aLength, mBuilder);
    if (NS_FAILED(rv)) {
      MarkAsBrokenAndRequestSuspensionWithBuilder(rv);
    }
    return;
  }

  auto bufferCopy = mozilla::MakeUniqueFallible<char16_t[]>(aLength);
  if (!bufferCopy) {
    // Just assigning mBroken instead of generating tree op. The caller
    // of tokenizeBuffer() will call MarkAsBroken() as appropriate.
    mBroken = NS_ERROR_OUT_OF_MEMORY;
    requestSuspension();
    return;
  }

  memcpy(bufferCopy.get(), aBuffer, aLength * sizeof(char16_t));

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opAppendCommentToDocument data(bufferCopy.release(), aLength);
  treeOp->Init(mozilla::AsVariant(data));
}

void nsHtml5TreeBuilder::addAttributesToElement(
    nsIContentHandle* aElement, nsHtml5HtmlAttributes* aAttributes) {
  MOZ_ASSERT(aElement, "Null element");
  MOZ_ASSERT(aAttributes, "Null attributes");

  if (aAttributes == nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES) {
    return;
  }

  if (mBuilder) {
    MOZ_ASSERT(
        aAttributes == tokenizer->GetAttributes(),
        "Using attribute other than the tokenizer's to add to body or html.");
    nsresult rv = nsHtml5TreeOperation::AddAttributes(
        static_cast<nsIContent*>(aElement), aAttributes, mBuilder);
    if (NS_FAILED(rv)) {
      MarkAsBrokenAndRequestSuspensionWithBuilder(rv);
    }
    return;
  }

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opAddAttributes opeation(aElement, aAttributes);
  treeOp->Init(mozilla::AsVariant(opeation));
}

void nsHtml5TreeBuilder::markMalformedIfScript(nsIContentHandle* aElement) {
  MOZ_ASSERT(aElement, "Null element");

  if (mBuilder) {
    nsHtml5TreeOperation::MarkMalformedIfScript(
        static_cast<nsIContent*>(aElement));
    return;
  }

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opMarkMalformedIfScript operation(aElement);
  treeOp->Init(mozilla::AsVariant(operation));
}

void nsHtml5TreeBuilder::start(bool fragment) {
  mCurrentHtmlScriptIsAsyncOrDefer = false;
#ifdef DEBUG
  mActive = true;
#endif
}

void nsHtml5TreeBuilder::end() {
  mOpQueue.Clear();
#ifdef DEBUG
  mActive = false;
#endif
}

void nsHtml5TreeBuilder::appendDoctypeToDocument(nsAtom* aName,
                                                 nsHtml5String aPublicId,
                                                 nsHtml5String aSystemId) {
  MOZ_ASSERT(aName, "Null name");
  nsString publicId;  // Not Auto, because using it to hold nsStringBuffer*
  nsString systemId;  // Not Auto, because using it to hold nsStringBuffer*
  aPublicId.ToString(publicId);
  aSystemId.ToString(systemId);
  if (mBuilder) {
    nsresult rv = nsHtml5TreeOperation::AppendDoctypeToDocument(
        aName, publicId, systemId, mBuilder);
    if (NS_FAILED(rv)) {
      MarkAsBrokenAndRequestSuspensionWithBuilder(rv);
    }
    return;
  }

  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opAppendDoctypeToDocument operation(aName, publicId, systemId);
  treeOp->Init(mozilla::AsVariant(operation));
  // nsXMLContentSink can flush here, but what's the point?
  // It can also interrupt here, but we can't.
}

void nsHtml5TreeBuilder::elementPushed(int32_t aNamespace, nsAtom* aName,
                                       nsIContentHandle* aElement) {
  NS_ASSERTION(aNamespace == kNameSpaceID_XHTML ||
                   aNamespace == kNameSpaceID_SVG ||
                   aNamespace == kNameSpaceID_MathML,
               "Element isn't HTML, SVG or MathML!");
  NS_ASSERTION(aName, "Element doesn't have local name!");
  NS_ASSERTION(aElement, "No element!");
  /*
   * The frame constructor uses recursive algorithms, so it can't deal with
   * arbitrarily deep trees. This is especially a problem on Windows where
   * the permitted depth of the runtime stack is rather small.
   *
   * The following is a protection against author incompetence--not against
   * malice. There are other ways to make the DOM deep anyway.
   *
   * The basic idea is that when the tree builder stack gets too deep,
   * append operations no longer append to the node that the HTML parsing
   * algorithm says they should but instead text nodes are append to the last
   * element that was seen before a magic tree builder stack threshold was
   * reached and element and comment nodes aren't appended to the DOM at all.
   *
   * However, for security reasons, non-child descendant text nodes inside an
   * SVG script or style element should not become children. Also, non-cell
   * table elements shouldn't be used as surrogate parents for user experience
   * reasons.
   */
  if (aNamespace != kNameSpaceID_XHTML) {
    return;
  }
  if (aName == nsGkAtoms::body || aName == nsGkAtoms::frameset) {
    if (mBuilder) {
      // InnerHTML and DOMParser shouldn't start layout anyway
      return;
    }
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
    if (MOZ_UNLIKELY(!treeOp)) {
      MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    treeOp->Init(mozilla::AsVariant(opStartLayout()));
    return;
  }
  if (nsIContent::RequiresDoneCreatingElement(kNameSpaceID_XHTML, aName)) {
    if (mBuilder) {
      nsHtml5TreeOperation::DoneCreatingElement(
          static_cast<nsIContent*>(aElement));
    } else {
      opDoneCreatingElement operation(aElement);
      mOpQueue.AppendElement()->Init(mozilla::AsVariant(operation));
    }
    return;
  }
  if (mSpeculativeLoadStage && aName == nsGkAtoms::picture) {
    // mSpeculativeLoadStage is non-null only in the off-the-main-thread
    // tree builder, which handles the network stream
    //
    // See comments in nsHtml5SpeculativeLoad.h about <picture> preloading
    mSpeculativeLoadQueue.AppendElement()->InitOpenPicture();
  }
}

void nsHtml5TreeBuilder::elementPopped(int32_t aNamespace, nsAtom* aName,
                                       nsIContentHandle* aElement) {
  NS_ASSERTION(aNamespace == kNameSpaceID_XHTML ||
                   aNamespace == kNameSpaceID_SVG ||
                   aNamespace == kNameSpaceID_MathML,
               "Element isn't HTML, SVG or MathML!");
  NS_ASSERTION(aName, "Element doesn't have local name!");
  NS_ASSERTION(aElement, "No element!");
  if (aNamespace == kNameSpaceID_MathML) {
    return;
  }
  // we now have only SVG and HTML
  if (aName == nsGkAtoms::script) {
    if (mPreventScriptExecution) {
      if (mBuilder) {
        nsHtml5TreeOperation::PreventScriptExecution(
            static_cast<nsIContent*>(aElement));
        return;
      }
      opPreventScriptExecution operation(aElement);
      mOpQueue.AppendElement()->Init(mozilla::AsVariant(operation));
      return;
    }
    if (mBuilder) {
      return;
    }
    if (mCurrentHtmlScriptIsAsyncOrDefer) {
      NS_ASSERTION(aNamespace == kNameSpaceID_XHTML,
                   "Only HTML scripts may be async/defer.");
      nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
      if (MOZ_UNLIKELY(!treeOp)) {
        MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
      opRunScriptAsyncDefer operation(aElement);
      treeOp->Init(mozilla::AsVariant(operation));
      mCurrentHtmlScriptIsAsyncOrDefer = false;
      return;
    }
    requestSuspension();
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
    if (MOZ_UNLIKELY(!treeOp)) {
      MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    opRunScript operation(aElement, nullptr);
    treeOp->Init(mozilla::AsVariant(operation));
    return;
  }
  // Some nodes need DoneAddingChildren() called to initialize
  // properly (e.g. form state restoration).
  if (nsIContent::RequiresDoneAddingChildren(aNamespace, aName)) {
    if (mBuilder) {
      nsHtml5TreeOperation::DoneAddingChildren(
          static_cast<nsIContent*>(aElement));
      return;
    }
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
    if (MOZ_UNLIKELY(!treeOp)) {
      MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    opDoneAddingChildren operation(aElement);
    treeOp->Init(mozilla::AsVariant(operation));
    return;
  }
  if (aName == nsGkAtoms::style ||
      (aNamespace == kNameSpaceID_XHTML && aName == nsGkAtoms::link)) {
    if (mBuilder) {
      MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript(),
                 "Scripts must be blocked.");
      mBuilder->UpdateStyleSheet(static_cast<nsIContent*>(aElement));
      return;
    }

    if (aName == nsGkAtoms::style) {
      nsTArray<nsString> imports = mImportScanner.Stop();
      for (nsString& url : imports) {
        mSpeculativeLoadQueue.AppendElement()->InitImportStyle(std::move(url));
      }
    }

    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
    if (MOZ_UNLIKELY(!treeOp)) {
      MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    opUpdateStyleSheet operation(aElement);
    treeOp->Init(mozilla::AsVariant(operation));
    return;
  }
  if (aNamespace == kNameSpaceID_SVG) {
    if (aName == nsGkAtoms::svg) {
      if (!scriptingEnabled || mPreventScriptExecution) {
        return;
      }
      if (mBuilder) {
        nsHtml5TreeOperation::SvgLoad(static_cast<nsIContent*>(aElement));
        return;
      }
      nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
      if (MOZ_UNLIKELY(!treeOp)) {
        MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
      opSvgLoad operation(aElement);
      treeOp->Init(mozilla::AsVariant(operation));
    }
    return;
  }

  if (mSpeculativeLoadStage && aName == nsGkAtoms::picture) {
    // mSpeculativeLoadStage is non-null only in the off-the-main-thread
    // tree builder, which handles the network stream
    //
    // See comments in nsHtml5SpeculativeLoad.h about <picture> preloading
    mSpeculativeLoadQueue.AppendElement()->InitEndPicture();
  }
}

void nsHtml5TreeBuilder::accumulateCharacters(const char16_t* aBuf,
                                              int32_t aStart, int32_t aLength) {
  MOZ_RELEASE_ASSERT(charBufferLen + aLength <= charBuffer.length,
                     "About to memcpy past the end of the buffer!");
  memcpy(charBuffer + charBufferLen, aBuf + aStart, sizeof(char16_t) * aLength);
  charBufferLen += aLength;
}

// INT32_MAX is (2^31)-1. Therefore, the highest power-of-two that fits
// is 2^30. Note that this is counting char16_t units. The underlying
// bytes will be twice that, but they fit even in 32-bit size_t even
// if a contiguous chunk of memory of that size is pretty unlikely to
// be available on a 32-bit system.
#define MAX_POWER_OF_TWO_IN_INT32 0x40000000

bool nsHtml5TreeBuilder::EnsureBufferSpace(int32_t aLength) {
  // TODO: Unify nsHtml5Tokenizer::strBuf and nsHtml5TreeBuilder::charBuffer
  // so that this method becomes unnecessary.
  mozilla::CheckedInt<int32_t> worstCase(charBufferLen);
  worstCase += aLength;
  if (!worstCase.isValid()) {
    return false;
  }
  if (worstCase.value() > MAX_POWER_OF_TWO_IN_INT32) {
    return false;
  }
  if (!charBuffer) {
    if (worstCase.value() < MAX_POWER_OF_TWO_IN_INT32) {
      // Add one to round to the next power of two to avoid immediate
      // reallocation once there are a few characters in the buffer.
      worstCase += 1;
    }
    charBuffer = jArray<char16_t, int32_t>::newFallibleJArray(
        mozilla::RoundUpPow2(worstCase.value()));
    if (!charBuffer) {
      return false;
    }
  } else if (worstCase.value() > charBuffer.length) {
    jArray<char16_t, int32_t> newBuf =
        jArray<char16_t, int32_t>::newFallibleJArray(
            mozilla::RoundUpPow2(worstCase.value()));
    if (!newBuf) {
      return false;
    }
    memcpy(newBuf, charBuffer, sizeof(char16_t) * size_t(charBufferLen));
    charBuffer = newBuf;
  }
  return true;
}

nsIContentHandle* nsHtml5TreeBuilder::AllocateContentHandle() {
  if (MOZ_UNLIKELY(mBuilder)) {
    MOZ_ASSERT_UNREACHABLE("Must never allocate a handle with builder.");
    return nullptr;
  }
  if (mHandlesUsed == NS_HTML5_TREE_BUILDER_HANDLE_ARRAY_LENGTH) {
    mOldHandles.AppendElement(std::move(mHandles));
    mHandles = mozilla::MakeUnique<nsIContent*[]>(
        NS_HTML5_TREE_BUILDER_HANDLE_ARRAY_LENGTH);
    mHandlesUsed = 0;
  }
#ifdef DEBUG
  mHandles[mHandlesUsed] = reinterpret_cast<nsIContent*>(uintptr_t(0xC0DEDBAD));
#endif
  return &mHandles[mHandlesUsed++];
}

bool nsHtml5TreeBuilder::HasScript() {
  uint32_t len = mOpQueue.Length();
  if (!len) {
    return false;
  }
  return mOpQueue.ElementAt(len - 1).IsRunScript();
}

bool nsHtml5TreeBuilder::Flush(bool aDiscretionary) {
  if (MOZ_UNLIKELY(mBuilder)) {
    MOZ_ASSERT_UNREACHABLE("Must never flush with builder.");
    return false;
  }
  if (NS_SUCCEEDED(mBroken)) {
    if (!aDiscretionary || !(charBufferLen && currentPtr >= 0 &&
                             stack[currentPtr]->isFosterParenting())) {
      // Don't flush text on discretionary flushes if the current element on
      // the stack is a foster-parenting element and there's pending text,
      // because flushing in that case would make the tree shape dependent on
      // where the flush points fall.
      flushCharacters();
    }
    FlushLoads();
  }
  if (mOpSink) {
    bool hasOps = !mOpQueue.IsEmpty();
    if (hasOps) {
      // If the builder is broken and mOpQueue is not empty, there must be
      // one op and it must be eTreeOpMarkAsBroken.
      if (NS_FAILED(mBroken)) {
        MOZ_ASSERT(mOpQueue.Length() == 1,
                   "Tree builder is broken with a non-empty op queue whose "
                   "length isn't 1.");
        MOZ_ASSERT(mOpQueue[0].IsMarkAsBroken(),
                   "Tree builder is broken but the op in queue is not marked "
                   "as broken.");
      }
      mOpSink->MoveOpsFrom(mOpQueue);
    }
    return hasOps;
  }
  // no op sink: throw away ops
  mOpQueue.Clear();
  return false;
}

void nsHtml5TreeBuilder::FlushLoads() {
  if (MOZ_UNLIKELY(mBuilder)) {
    MOZ_ASSERT_UNREACHABLE("Must never flush loads with builder.");
    return;
  }
  if (!mSpeculativeLoadQueue.IsEmpty()) {
    mSpeculativeLoadStage->MoveSpeculativeLoadsFrom(mSpeculativeLoadQueue);
  }
}

void nsHtml5TreeBuilder::SetDocumentCharset(NotNull<const Encoding*> aEncoding,
                                            int32_t aCharsetSource) {
  if (mBuilder) {
    mBuilder->SetDocumentCharsetAndSource(aEncoding, aCharsetSource);
  } else if (mSpeculativeLoadStage) {
    mSpeculativeLoadQueue.AppendElement()->InitSetDocumentCharset(
        aEncoding, aCharsetSource);
  } else {
    opSetDocumentCharset opearation(aEncoding, aCharsetSource);
    mOpQueue.AppendElement()->Init(mozilla::AsVariant(opearation));
  }
}

void nsHtml5TreeBuilder::StreamEnded() {
  MOZ_ASSERT(!mBuilder, "Must not call StreamEnded with builder.");
  MOZ_ASSERT(!fragment, "Must not parse fragments off the main thread.");
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  treeOp->Init(mozilla::AsVariant(opStreamEnded()));
}

void nsHtml5TreeBuilder::NeedsCharsetSwitchTo(
    NotNull<const Encoding*> aEncoding, int32_t aCharsetSource,
    int32_t aLineNumber) {
  if (MOZ_UNLIKELY(mBuilder)) {
    MOZ_ASSERT_UNREACHABLE("Must never switch charset with builder.");
    return;
  }
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  opCharsetSwitchTo opeation(aEncoding, aCharsetSource, aLineNumber);
  treeOp->Init(mozilla::AsVariant(opeation));
}

void nsHtml5TreeBuilder::MaybeComplainAboutCharset(const char* aMsgId,
                                                   bool aError,
                                                   int32_t aLineNumber) {
  if (MOZ_UNLIKELY(mBuilder)) {
    MOZ_ASSERT_UNREACHABLE("Must never complain about charset with builder.");
    return;
  }
  opMaybeComplainAboutCharset opeartion(const_cast<char*>(aMsgId), aError,
                                        aLineNumber);
  mOpQueue.AppendElement()->Init(mozilla::AsVariant(opeartion));
}

void nsHtml5TreeBuilder::TryToEnableEncodingMenu() {
  if (MOZ_UNLIKELY(mBuilder)) {
    MOZ_ASSERT_UNREACHABLE("Must never disable encoding menu with builder.");
    return;
  }
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
  NS_ASSERTION(treeOp, "Tree op allocation failed.");
  treeOp->Init(mozilla::AsVariant(opEnableEncodingMenu()));
}

void nsHtml5TreeBuilder::AddSnapshotToScript(
    nsAHtml5TreeBuilderState* aSnapshot, int32_t aLine) {
  if (MOZ_UNLIKELY(mBuilder)) {
    MOZ_ASSERT_UNREACHABLE("Must never use snapshots with builder.");
    return;
  }
  MOZ_ASSERT(HasScript(), "No script to add a snapshot to!");
  MOZ_ASSERT(aSnapshot, "Got null snapshot.");
  mOpQueue.ElementAt(mOpQueue.Length() - 1).SetSnapshot(aSnapshot, aLine);
}

void nsHtml5TreeBuilder::DropHandles() {
  MOZ_ASSERT(!mBuilder, "Must not drop handles with builder.");
  mOldHandles.Clear();
  mHandlesUsed = 0;
}

void nsHtml5TreeBuilder::MarkAsBroken(nsresult aRv) {
  if (MOZ_UNLIKELY(mBuilder)) {
    MOZ_ASSERT_UNREACHABLE("Must not call this with builder.");
    return;
  }
  mBroken = aRv;
  mOpQueue.Clear();  // Previous ops don't matter anymore
  opMarkAsBroken operation(aRv);
  mOpQueue.AppendElement()->Init(mozilla::AsVariant(operation));
}

void nsHtml5TreeBuilder::MarkAsBrokenFromPortability(nsresult aRv) {
  if (mBuilder) {
    MarkAsBrokenAndRequestSuspensionWithBuilder(aRv);
    return;
  }
  mBroken = aRv;
  requestSuspension();
}

void nsHtml5TreeBuilder::StartPlainTextViewSource(const nsAutoString& aTitle) {
  MOZ_ASSERT(!mBuilder, "Must not view source with builder.");

  startTag(nsHtml5ElementName::ELT_META,
           nsHtml5ViewSourceUtils::NewMetaViewportAttributes(), false);

  startTag(nsHtml5ElementName::ELT_TITLE,
           nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES, false);

  // XUL will add the "Source of: " prefix.
  uint32_t length = aTitle.Length();
  if (length > INT32_MAX) {
    length = INT32_MAX;
  }
  characters(aTitle.get(), 0, (int32_t)length);
  endTag(nsHtml5ElementName::ELT_TITLE);

  startTag(nsHtml5ElementName::ELT_LINK,
           nsHtml5ViewSourceUtils::NewLinkAttributes(), false);

  startTag(nsHtml5ElementName::ELT_BODY,
           nsHtml5ViewSourceUtils::NewBodyAttributes(), false);

  StartPlainTextBody();
}

void nsHtml5TreeBuilder::StartPlainText() {
  MOZ_ASSERT(!mBuilder, "Must not view source with builder.");
  setForceNoQuirks(true);
  startTag(nsHtml5ElementName::ELT_LINK,
           nsHtml5PlainTextUtils::NewLinkAttributes(), false);

  startTag(nsHtml5ElementName::ELT_BODY,
           nsHtml5PlainTextUtils::NewBodyAttributes(), false);

  StartPlainTextBody();
}

void nsHtml5TreeBuilder::StartPlainTextBody() {
  MOZ_ASSERT(!mBuilder, "Must not view source with builder.");
  startTag(nsHtml5ElementName::ELT_PRE, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES,
           false);
  needToDropLF = false;
}

// DocumentModeHandler
void nsHtml5TreeBuilder::documentMode(nsHtml5DocumentMode m) {
  if (mBuilder) {
    mBuilder->SetDocumentMode(m);
    return;
  }
  if (mSpeculativeLoadStage) {
    mSpeculativeLoadQueue.AppendElement()->InitSetDocumentMode(m);
    return;
  }
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  treeOp->Init(mozilla::AsVariant(m));
}

nsIContentHandle* nsHtml5TreeBuilder::getDocumentFragmentForTemplate(
    nsIContentHandle* aTemplate) {
  if (mBuilder) {
    return nsHtml5TreeOperation::GetDocumentFragmentForTemplate(
        static_cast<nsIContent*>(aTemplate));
  }
  nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement(mozilla::fallible);
  if (MOZ_UNLIKELY(!treeOp)) {
    MarkAsBrokenAndRequestSuspensionWithoutBuilder(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }
  nsIContentHandle* fragHandle = AllocateContentHandle();
  opGetDocumentFragmentForTemplate operation(aTemplate, fragHandle);
  treeOp->Init(mozilla::AsVariant(operation));
  return fragHandle;
}

nsIContentHandle* nsHtml5TreeBuilder::getFormPointerForContext(
    nsIContentHandle* aContext) {
  MOZ_ASSERT(mBuilder, "Must have builder.");
  if (!aContext) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  // aContext must always be an element that already exists
  // in the document.
  nsIContent* contextNode = static_cast<nsIContent*>(aContext);
  nsIContent* currentAncestor = contextNode;

  // We traverse the ancestors of the context node to find the nearest
  // form pointer. This traversal is why aContext must not be an emtpy handle.
  nsIContent* nearestForm = nullptr;
  while (currentAncestor) {
    if (currentAncestor->IsHTMLElement(nsGkAtoms::form)) {
      nearestForm = currentAncestor;
      break;
    }
    currentAncestor = currentAncestor->GetParent();
  }

  if (!nearestForm) {
    return nullptr;
  }

  return nearestForm;
}

// Error reporting

void nsHtml5TreeBuilder::EnableViewSource(nsHtml5Highlighter* aHighlighter) {
  MOZ_ASSERT(!mBuilder, "Must not view source with builder.");
  mViewSource = aHighlighter;
}

void nsHtml5TreeBuilder::errDeepTree() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errDeepTree");
  } else if (!mBuilder) {
    nsHtml5TreeOperation* treeOp = mOpQueue.AppendElement();
    MOZ_ASSERT(treeOp, "Tree op allocation failed.");
    opMaybeComplainAboutDeepTree operation(tokenizer->getLineNumber());
    treeOp->Init(mozilla::AsVariant(operation));
  }
}

void nsHtml5TreeBuilder::errStrayStartTag(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errStrayStartTag2", aName);
  }
}

void nsHtml5TreeBuilder::errStrayEndTag(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errStrayEndTag", aName);
  }
}

void nsHtml5TreeBuilder::errUnclosedElements(int32_t aIndex, nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errUnclosedElements", aName);
  }
}

void nsHtml5TreeBuilder::errUnclosedElementsImplied(int32_t aIndex,
                                                    nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errUnclosedElementsImplied", aName);
  }
}

void nsHtml5TreeBuilder::errUnclosedElementsCell(int32_t aIndex) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errUnclosedElementsCell");
  }
}

void nsHtml5TreeBuilder::errStrayDoctype() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errStrayDoctype");
  }
}

void nsHtml5TreeBuilder::errAlmostStandardsDoctype() {
  if (MOZ_UNLIKELY(mViewSource) && !forceNoQuirks) {
    mViewSource->AddErrorToCurrentRun("errAlmostStandardsDoctype");
  }
}

void nsHtml5TreeBuilder::errQuirkyDoctype() {
  if (MOZ_UNLIKELY(mViewSource) && !forceNoQuirks) {
    mViewSource->AddErrorToCurrentRun("errQuirkyDoctype");
  }
}

void nsHtml5TreeBuilder::errNonSpaceInTrailer() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNonSpaceInTrailer");
  }
}

void nsHtml5TreeBuilder::errNonSpaceAfterFrameset() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNonSpaceAfterFrameset");
  }
}

void nsHtml5TreeBuilder::errNonSpaceInFrameset() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNonSpaceInFrameset");
  }
}

void nsHtml5TreeBuilder::errNonSpaceAfterBody() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNonSpaceAfterBody");
  }
}

void nsHtml5TreeBuilder::errNonSpaceInColgroupInFragment() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNonSpaceInColgroupInFragment");
  }
}

void nsHtml5TreeBuilder::errNonSpaceInNoscriptInHead() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNonSpaceInNoscriptInHead");
  }
}

void nsHtml5TreeBuilder::errFooBetweenHeadAndBody(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errFooBetweenHeadAndBody", aName);
  }
}

void nsHtml5TreeBuilder::errStartTagWithoutDoctype() {
  if (MOZ_UNLIKELY(mViewSource) && !forceNoQuirks) {
    mViewSource->AddErrorToCurrentRun("errStartTagWithoutDoctype");
  }
}

void nsHtml5TreeBuilder::errNoSelectInTableScope() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNoSelectInTableScope");
  }
}

void nsHtml5TreeBuilder::errStartSelectWhereEndSelectExpected() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errStartSelectWhereEndSelectExpected");
  }
}

void nsHtml5TreeBuilder::errStartTagWithSelectOpen(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errStartTagWithSelectOpen", aName);
  }
}

void nsHtml5TreeBuilder::errBadStartTagInNoscriptInHead(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errBadStartTagInNoscriptInHead", aName);
  }
}

void nsHtml5TreeBuilder::errImage() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errImage");
  }
}

void nsHtml5TreeBuilder::errIsindex() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errIsindex");
  }
}

void nsHtml5TreeBuilder::errFooSeenWhenFooOpen(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errFooSeenWhenFooOpen2", aName);
  }
}

void nsHtml5TreeBuilder::errHeadingWhenHeadingOpen() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errHeadingWhenHeadingOpen");
  }
}

void nsHtml5TreeBuilder::errFramesetStart() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errFramesetStart");
  }
}

void nsHtml5TreeBuilder::errNoCellToClose() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNoCellToClose");
  }
}

void nsHtml5TreeBuilder::errStartTagInTable(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errStartTagInTable", aName);
  }
}

void nsHtml5TreeBuilder::errFormWhenFormOpen() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errFormWhenFormOpen");
  }
}

void nsHtml5TreeBuilder::errTableSeenWhileTableOpen() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errTableSeenWhileTableOpen");
  }
}

void nsHtml5TreeBuilder::errStartTagInTableBody(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errStartTagInTableBody", aName);
  }
}

void nsHtml5TreeBuilder::errEndTagSeenWithoutDoctype() {
  if (MOZ_UNLIKELY(mViewSource) && !forceNoQuirks) {
    mViewSource->AddErrorToCurrentRun("errEndTagSeenWithoutDoctype");
  }
}

void nsHtml5TreeBuilder::errEndTagAfterBody() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEndTagAfterBody");
  }
}

void nsHtml5TreeBuilder::errEndTagSeenWithSelectOpen(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEndTagSeenWithSelectOpen", aName);
  }
}

void nsHtml5TreeBuilder::errGarbageInColgroup() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errGarbageInColgroup");
  }
}

void nsHtml5TreeBuilder::errEndTagBr() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEndTagBr");
  }
}

void nsHtml5TreeBuilder::errNoElementToCloseButEndTagSeen(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNoElementToCloseButEndTagSeen",
                                      aName);
  }
}

void nsHtml5TreeBuilder::errHtmlStartTagInForeignContext(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errHtmlStartTagInForeignContext", aName);
  }
}

void nsHtml5TreeBuilder::errNoTableRowToClose() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNoTableRowToClose");
  }
}

void nsHtml5TreeBuilder::errNonSpaceInTable() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNonSpaceInTable");
  }
}

void nsHtml5TreeBuilder::errUnclosedChildrenInRuby() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errUnclosedChildrenInRuby");
  }
}

void nsHtml5TreeBuilder::errStartTagSeenWithoutRuby(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errStartTagSeenWithoutRuby", aName);
  }
}

void nsHtml5TreeBuilder::errSelfClosing() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentSlash("errSelfClosing");
  }
}

void nsHtml5TreeBuilder::errNoCheckUnclosedElementsOnStack() {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errNoCheckUnclosedElementsOnStack");
  }
}

void nsHtml5TreeBuilder::errEndTagDidNotMatchCurrentOpenElement(
    nsAtom* aName, nsAtom* aOther) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEndTagDidNotMatchCurrentOpenElement",
                                      aName, aOther);
  }
}

void nsHtml5TreeBuilder::errEndTagViolatesNestingRules(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEndTagViolatesNestingRules", aName);
  }
}

void nsHtml5TreeBuilder::errEndWithUnclosedElements(nsAtom* aName) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errEndWithUnclosedElements", aName);
  }
}

void nsHtml5TreeBuilder::errListUnclosedStartTags(int32_t aIgnored) {
  if (MOZ_UNLIKELY(mViewSource)) {
    mViewSource->AddErrorToCurrentRun("errListUnclosedStartTags");
  }
}
