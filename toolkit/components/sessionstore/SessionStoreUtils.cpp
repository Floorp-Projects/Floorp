/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Array.h"  // JS::GetArrayLength, JS::IsArrayObject
#include "js/JSON.h"
#include "jsapi.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/txIXPathContext.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/dom/XPathResult.h"
#include "mozilla/dom/XPathEvaluator.h"
#include "mozilla/dom/XPathExpression.h"
#include "mozilla/UniquePtr.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentList.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsGlobalWindowOuter.h"
#include "nsIDocShell.h"
#include "nsIFormControl.h"
#include "nsIScrollableFrame.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {

class DynamicFrameEventFilter final : public nsIDOMEventListener {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DynamicFrameEventFilter)

  explicit DynamicFrameEventFilter(EventListener* aListener)
      : mListener(aListener) {}

  NS_IMETHODIMP HandleEvent(Event* aEvent) override {
    if (mListener && TargetInNonDynamicDocShell(aEvent)) {
      mListener->HandleEvent(*aEvent);
    }

    return NS_OK;
  }

 private:
  ~DynamicFrameEventFilter() = default;

  bool TargetInNonDynamicDocShell(Event* aEvent) {
    EventTarget* target = aEvent->GetTarget();
    if (!target) {
      return false;
    }

    nsPIDOMWindowOuter* outer = target->GetOwnerGlobalForBindingsInternal();
    if (!outer) {
      return false;
    }

    nsIDocShell* docShell = outer->GetDocShell();
    if (!docShell) {
      return false;
    }

    bool isDynamic = false;
    nsresult rv = docShell->GetCreatedDynamically(&isDynamic);
    return NS_SUCCEEDED(rv) && !isDynamic;
  }

  RefPtr<EventListener> mListener;
};

NS_IMPL_CYCLE_COLLECTION(DynamicFrameEventFilter, mListener)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DynamicFrameEventFilter)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DynamicFrameEventFilter)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DynamicFrameEventFilter)

}  // anonymous namespace

/* static */
void SessionStoreUtils::ForEachNonDynamicChildFrame(
    const GlobalObject& aGlobal, WindowProxyHolder& aWindow,
    SessionStoreUtilsFrameCallback& aCallback, ErrorResult& aRv) {
  if (!aWindow.get()) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  nsCOMPtr<nsIDocShell> docShell = aWindow.get()->GetDocShell();
  if (!docShell) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  int32_t length;
  aRv = docShell->GetInProcessChildCount(&length);
  if (aRv.Failed()) {
    return;
  }

  for (int32_t i = 0; i < length; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    docShell->GetInProcessChildAt(i, getter_AddRefs(item));
    if (!item) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsCOMPtr<nsIDocShell> childDocShell(do_QueryInterface(item));
    if (!childDocShell) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    bool isDynamic = false;
    nsresult rv = childDocShell->GetCreatedDynamically(&isDynamic);
    if (NS_SUCCEEDED(rv) && isDynamic) {
      continue;
    }

    int32_t childOffset = childDocShell->GetChildOffset();
    aCallback.Call(WindowProxyHolder(item->GetWindow()->GetBrowsingContext()),
                   childOffset);
  }
}

/* static */
already_AddRefed<nsISupports>
SessionStoreUtils::AddDynamicFrameFilteredListener(
    const GlobalObject& aGlobal, EventTarget& aTarget, const nsAString& aType,
    JS::Handle<JS::Value> aListener, bool aUseCapture, bool aMozSystemGroup,
    ErrorResult& aRv) {
  if (NS_WARN_IF(!aListener.isObject())) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  JSContext* cx = aGlobal.Context();
  JS::Rooted<JSObject*> obj(cx, &aListener.toObject());
  JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
  RefPtr<EventListener> listener =
      new EventListener(cx, obj, global, GetIncumbentGlobal());

  nsCOMPtr<nsIDOMEventListener> filter(new DynamicFrameEventFilter(listener));
  if (aMozSystemGroup) {
    aRv = aTarget.AddSystemEventListener(aType, filter, aUseCapture);
  } else {
    aRv = aTarget.AddEventListener(aType, filter, aUseCapture);
  }
  if (aRv.Failed()) {
    return nullptr;
  }

  return filter.forget();
}

/* static */
void SessionStoreUtils::RemoveDynamicFrameFilteredListener(
    const GlobalObject& global, EventTarget& aTarget, const nsAString& aType,
    nsISupports* aListener, bool aUseCapture, bool aMozSystemGroup,
    ErrorResult& aRv) {
  nsCOMPtr<nsIDOMEventListener> listener = do_QueryInterface(aListener);
  if (!listener) {
    aRv.Throw(NS_ERROR_NO_INTERFACE);
    return;
  }

  if (aMozSystemGroup) {
    aTarget.RemoveSystemEventListener(aType, listener, aUseCapture);
  } else {
    aTarget.RemoveEventListener(aType, listener, aUseCapture);
  }
}

/* static */
void SessionStoreUtils::CollectDocShellCapabilities(const GlobalObject& aGlobal,
                                                    nsIDocShell* aDocShell,
                                                    nsCString& aRetVal) {
  bool allow;

#define TRY_ALLOWPROP(y)          \
  PR_BEGIN_MACRO                  \
  aDocShell->GetAllow##y(&allow); \
  if (!allow) {                   \
    if (!aRetVal.IsEmpty()) {     \
      aRetVal.Append(',');        \
    }                             \
    aRetVal.Append(#y);           \
  }                               \
  PR_END_MACRO

  TRY_ALLOWPROP(Plugins);
  // Bug 1328013 : Don't collect "AllowJavascript" property
  // TRY_ALLOWPROP(Javascript);
  TRY_ALLOWPROP(MetaRedirects);
  TRY_ALLOWPROP(Subframes);
  TRY_ALLOWPROP(Images);
  TRY_ALLOWPROP(Media);
  TRY_ALLOWPROP(DNSPrefetch);
  TRY_ALLOWPROP(WindowControl);
  TRY_ALLOWPROP(Auth);
  TRY_ALLOWPROP(ContentRetargeting);
  TRY_ALLOWPROP(ContentRetargetingOnChildren);
#undef TRY_ALLOWPROP
}

/* static */
void SessionStoreUtils::RestoreDocShellCapabilities(
    const GlobalObject& aGlobal, nsIDocShell* aDocShell,
    const nsCString& aDisallowCapabilities) {
  aDocShell->SetAllowPlugins(true);
  aDocShell->SetAllowJavascript(true);
  aDocShell->SetAllowMetaRedirects(true);
  aDocShell->SetAllowSubframes(true);
  aDocShell->SetAllowImages(true);
  aDocShell->SetAllowMedia(true);
  aDocShell->SetAllowDNSPrefetch(true);
  aDocShell->SetAllowWindowControl(true);
  aDocShell->SetAllowContentRetargeting(true);
  aDocShell->SetAllowContentRetargetingOnChildren(true);

  nsCCharSeparatedTokenizer tokenizer(aDisallowCapabilities, ',');
  while (tokenizer.hasMoreTokens()) {
    const nsACString& token = tokenizer.nextToken();
    if (token.EqualsLiteral("Plugins")) {
      aDocShell->SetAllowPlugins(false);
    } else if (token.EqualsLiteral("Javascript")) {
      aDocShell->SetAllowJavascript(false);
    } else if (token.EqualsLiteral("MetaRedirects")) {
      aDocShell->SetAllowMetaRedirects(false);
    } else if (token.EqualsLiteral("Subframes")) {
      aDocShell->SetAllowSubframes(false);
    } else if (token.EqualsLiteral("Images")) {
      aDocShell->SetAllowImages(false);
    } else if (token.EqualsLiteral("Media")) {
      aDocShell->SetAllowMedia(false);
    } else if (token.EqualsLiteral("DNSPrefetch")) {
      aDocShell->SetAllowDNSPrefetch(false);
    } else if (token.EqualsLiteral("WindowControl")) {
      aDocShell->SetAllowWindowControl(false);
    } else if (token.EqualsLiteral("ContentRetargeting")) {
      bool allow;
      aDocShell->GetAllowContentRetargetingOnChildren(&allow);
      aDocShell->SetAllowContentRetargeting(
          false);  // will also set AllowContentRetargetingOnChildren
      aDocShell->SetAllowContentRetargetingOnChildren(
          allow);  // restore the allowProp to original
    } else if (token.EqualsLiteral("ContentRetargetingOnChildren")) {
      aDocShell->SetAllowContentRetargetingOnChildren(false);
    }
  }
}

static void CollectCurrentScrollPosition(JSContext* aCx, Document& aDocument,
                                         Nullable<CollectedData>& aRetVal) {
  PresShell* presShell = aDocument.GetPresShell();
  if (!presShell) {
    return;
  }
  nsPoint scrollPos = presShell->GetVisualViewportOffset();
  int scrollX = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x);
  int scrollY = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y);

  if ((scrollX != 0) || (scrollY != 0)) {
    aRetVal.SetValue().mScroll.Construct() =
        nsPrintfCString("%d,%d", scrollX, scrollY);
  }
}

/* static */
void SessionStoreUtils::RestoreScrollPosition(const GlobalObject& aGlobal,
                                              nsGlobalWindowInner& aWindow,
                                              const CollectedData& aData) {
  if (!aData.mScroll.WasPassed()) {
    return;
  }

  nsCCharSeparatedTokenizer tokenizer(aData.mScroll.Value(), ',');
  nsAutoCString token(tokenizer.nextToken());
  int pos_X = atoi(token.get());
  token = tokenizer.nextToken();
  int pos_Y = atoi(token.get());

  aWindow.ScrollTo(pos_X, pos_Y);

  if (nsCOMPtr<Document> doc = aWindow.GetExtantDoc()) {
    if (nsPresContext* presContext = doc->GetPresContext()) {
      if (presContext->IsRootContentDocument()) {
        // Use eMainThread so this takes precedence over session history
        // (ScrollFrameHelper::ScrollToRestoredPosition()).
        presContext->PresShell()->ScrollToVisual(
            CSSPoint::ToAppUnits(CSSPoint(pos_X, pos_Y)),
            layers::FrameMetrics::eMainThread, ScrollMode::Instant);
      }
    }
  }
}

// Implements the Luhn checksum algorithm as described at
// http://wikipedia.org/wiki/Luhn_algorithm
// Number digit lengths vary with network, but should fall within 12-19 range.
// [2] More details at https://en.wikipedia.org/wiki/Payment_card_number
static bool IsValidCCNumber(nsAString& aValue) {
  uint32_t total = 0;
  uint32_t numLength = 0;
  uint32_t strLen = aValue.Length();
  for (uint32_t i = 0; i < strLen; ++i) {
    uint32_t idx = strLen - i - 1;
    // ignore whitespace and dashes)
    char16_t chr = aValue[idx];
    if (IsSpaceCharacter(chr) || chr == '-') {
      continue;
    }
    // If our number is too long, note that fact
    ++numLength;
    if (numLength > 19) {
      return false;
    }
    // Try to parse the character as a base-10 integer.
    nsresult rv = NS_OK;
    uint32_t val = Substring(aValue, idx, 1).ToInteger(&rv, 10);
    if (NS_FAILED(rv)) {
      return false;
    }
    if (i % 2 == 1) {
      val *= 2;
      if (val > 9) {
        val -= 9;
      }
    }
    total += val;
  }

  return numLength >= 12 && total % 10 == 0;
}

// Limit the number of XPath expressions for performance reasons. See bug
// 477564.
static const uint16_t kMaxTraversedXPaths = 100;

// A helper function to append a element into mId or mXpath of CollectedData
static Record<nsString, OwningStringOrBooleanOrObject>::EntryType*
AppendEntryToCollectedData(nsINode* aNode, const nsAString& aId,
                           uint16_t& aGeneratedCount,
                           Nullable<CollectedData>& aRetVal) {
  Record<nsString, OwningStringOrBooleanOrObject>::EntryType* entry;
  if (!aId.IsEmpty()) {
    if (!aRetVal.SetValue().mId.WasPassed()) {
      aRetVal.SetValue().mId.Construct();
    }
    auto& recordEntries = aRetVal.SetValue().mId.Value().Entries();
    entry = recordEntries.AppendElement();
    entry->mKey = aId;
  } else {
    if (!aRetVal.SetValue().mXpath.WasPassed()) {
      aRetVal.SetValue().mXpath.Construct();
    }
    auto& recordEntries = aRetVal.SetValue().mXpath.Value().Entries();
    entry = recordEntries.AppendElement();
    nsAutoString xpath;
    aNode->GenerateXPath(xpath);
    aGeneratedCount++;
    entry->mKey = xpath;
  }
  return entry;
}

// A helper function to append a element into aXPathVals or aIdVals
static void AppendEntryToCollectedData(
    nsINode* aNode, const nsAString& aId, CollectedInputDataValue& aEntry,
    uint16_t& aNumXPath, uint16_t& aNumId,
    nsTArray<CollectedInputDataValue>& aXPathVals,
    nsTArray<CollectedInputDataValue>& aIdVals) {
  if (!aId.IsEmpty()) {
    aEntry.id = aId;
    aIdVals.AppendElement(aEntry);
    aNumId++;
  } else {
    nsAutoString xpath;
    aNode->GenerateXPath(xpath);
    aEntry.id = xpath;
    aXPathVals.AppendElement(aEntry);
    aNumXPath++;
  }
}

/* for bool value */
static void AppendValueToCollectedData(nsINode* aNode, const nsAString& aId,
                                       const bool& aValue,
                                       uint16_t& aGeneratedCount,
                                       JSContext* aCx,
                                       Nullable<CollectedData>& aRetVal) {
  Record<nsString, OwningStringOrBooleanOrObject>::EntryType* entry =
      AppendEntryToCollectedData(aNode, aId, aGeneratedCount, aRetVal);
  entry->mValue.SetAsBoolean() = aValue;
}

/* for bool value */
static void AppendValueToCollectedData(
    nsINode* aNode, const nsAString& aId, const bool& aValue,
    uint16_t& aNumXPath, uint16_t& aNumId,
    nsTArray<CollectedInputDataValue>& aXPathVals,
    nsTArray<CollectedInputDataValue>& aIdVals) {
  CollectedInputDataValue entry;
  entry.type = NS_LITERAL_STRING("bool");
  entry.value = AsVariant(aValue);
  AppendEntryToCollectedData(aNode, aId, entry, aNumXPath, aNumId, aXPathVals,
                             aIdVals);
}

/* for nsString value */
static void AppendValueToCollectedData(nsINode* aNode, const nsAString& aId,
                                       const nsString& aValue,
                                       uint16_t& aGeneratedCount,
                                       Nullable<CollectedData>& aRetVal) {
  Record<nsString, OwningStringOrBooleanOrObject>::EntryType* entry =
      AppendEntryToCollectedData(aNode, aId, aGeneratedCount, aRetVal);
  entry->mValue.SetAsString() = aValue;
}

/* for nsString value */
static void AppendValueToCollectedData(
    nsINode* aNode, const nsAString& aId, const nsString& aValue,
    uint16_t& aNumXPath, uint16_t& aNumId,
    nsTArray<CollectedInputDataValue>& aXPathVals,
    nsTArray<CollectedInputDataValue>& aIdVals) {
  CollectedInputDataValue entry;
  entry.type = NS_LITERAL_STRING("string");
  entry.value = AsVariant(aValue);
  AppendEntryToCollectedData(aNode, aId, entry, aNumXPath, aNumId, aXPathVals,
                             aIdVals);
}

/* for single select value */
static void AppendValueToCollectedData(
    nsINode* aNode, const nsAString& aId,
    const CollectedNonMultipleSelectValue& aValue, uint16_t& aGeneratedCount,
    JSContext* aCx, Nullable<CollectedData>& aRetVal) {
  JS::Rooted<JS::Value> jsval(aCx);
  if (!ToJSValue(aCx, aValue, &jsval)) {
    JS_ClearPendingException(aCx);
    return;
  }
  Record<nsString, OwningStringOrBooleanOrObject>::EntryType* entry =
      AppendEntryToCollectedData(aNode, aId, aGeneratedCount, aRetVal);
  entry->mValue.SetAsObject() = &jsval.toObject();
}

/* for single select value */
static void AppendValueToCollectedData(
    nsINode* aNode, const nsAString& aId,
    const CollectedNonMultipleSelectValue& aValue, uint16_t& aNumXPath,
    uint16_t& aNumId, nsTArray<CollectedInputDataValue>& aXPathVals,
    nsTArray<CollectedInputDataValue>& aIdVals) {
  CollectedInputDataValue entry;
  entry.type = NS_LITERAL_STRING("singleSelect");
  entry.value = AsVariant(aValue);
  AppendEntryToCollectedData(aNode, aId, entry, aNumXPath, aNumId, aXPathVals,
                             aIdVals);
}

/* special handing for input element with string type */
static void AppendValueToCollectedData(Document& aDocument, nsINode* aNode,
                                       const nsAString& aId,
                                       const nsString& aValue,
                                       uint16_t& aGeneratedCount,
                                       JSContext* aCx,
                                       Nullable<CollectedData>& aRetVal) {
  if (!aId.IsEmpty()) {
    // We want to avoid saving data for about:sessionrestore as a string.
    // Since it's stored in the form as stringified JSON, stringifying
    // further causes an explosion of escape characters. cf. bug 467409
    if (aId.EqualsLiteral("sessionData")) {
      nsAutoCString url;
      Unused << aDocument.GetDocumentURI()->GetSpecIgnoringRef(url);
      if (url.EqualsLiteral("about:sessionrestore") ||
          url.EqualsLiteral("about:welcomeback")) {
        JS::Rooted<JS::Value> jsval(aCx);
        if (JS_ParseJSON(aCx, aValue.get(), aValue.Length(), &jsval) &&
            jsval.isObject()) {
          Record<nsString, OwningStringOrBooleanOrObject>::EntryType* entry =
              AppendEntryToCollectedData(aNode, aId, aGeneratedCount, aRetVal);
          entry->mValue.SetAsObject() = &jsval.toObject();
        } else {
          JS_ClearPendingException(aCx);
        }
        return;
      }
    }
  }
  AppendValueToCollectedData(aNode, aId, aValue, aGeneratedCount, aRetVal);
}

static void AppendValueToCollectedData(
    Document& aDocument, nsINode* aNode, const nsAString& aId,
    const nsString& aValue, uint16_t& aNumXPath, uint16_t& aNumId,
    nsTArray<CollectedInputDataValue>& aXPathVals,
    nsTArray<CollectedInputDataValue>& aIdVals) {
  CollectedInputDataValue entry;
  entry.type = NS_LITERAL_STRING("string");
  entry.value = AsVariant(aValue);
  AppendEntryToCollectedData(aNode, aId, entry, aNumXPath, aNumId, aXPathVals,
                             aIdVals);
}

/* for nsTArray<nsString>: file and multipleSelect */
static void AppendValueToCollectedData(nsINode* aNode, const nsAString& aId,
                                       const nsAString& aValueType,
                                       nsTArray<nsString>& aValue,
                                       uint16_t& aGeneratedCount,
                                       JSContext* aCx,
                                       Nullable<CollectedData>& aRetVal) {
  JS::Rooted<JS::Value> jsval(aCx);
  if (aValueType.EqualsLiteral("file")) {
    CollectedFileListValue val;
    val.mType = aValueType;
    val.mFileList.SwapElements(aValue);
    if (!ToJSValue(aCx, val, &jsval)) {
      JS_ClearPendingException(aCx);
      return;
    }
  } else {
    if (!ToJSValue(aCx, aValue, &jsval)) {
      JS_ClearPendingException(aCx);
      return;
    }
  }
  Record<nsString, OwningStringOrBooleanOrObject>::EntryType* entry =
      AppendEntryToCollectedData(aNode, aId, aGeneratedCount, aRetVal);
  entry->mValue.SetAsObject() = &jsval.toObject();
}

/* for nsTArray<nsString>: file and multipleSelect */
static void AppendValueToCollectedData(
    nsINode* aNode, const nsAString& aId, const nsAString& aValueType,
    const nsTArray<nsString>& aValue, uint16_t& aNumXPath, uint16_t& aNumId,
    nsTArray<CollectedInputDataValue>& aXPathVals,
    nsTArray<CollectedInputDataValue>& aIdVals) {
  CollectedInputDataValue entry;
  entry.type = aValueType;
  entry.value = AsVariant(aValue);
  AppendEntryToCollectedData(aNode, aId, entry, aNumXPath, aNumId, aXPathVals,
                             aIdVals);
}

/* static */
template <typename... ArgsT>
void SessionStoreUtils::CollectFromTextAreaElement(Document& aDocument,
                                                   uint16_t& aGeneratedCount,
                                                   ArgsT&&... args) {
  RefPtr<nsContentList> textlist = NS_GetContentList(
      &aDocument, kNameSpaceID_XHTML, NS_LITERAL_STRING("textarea"));
  uint32_t length = textlist->Length(true);
  for (uint32_t i = 0; i < length; ++i) {
    MOZ_ASSERT(textlist->Item(i), "null item in node list!");

    HTMLTextAreaElement* textArea =
        HTMLTextAreaElement::FromNodeOrNull(textlist->Item(i));
    if (!textArea) {
      continue;
    }
    DOMString autocomplete;
    textArea->GetAutocomplete(autocomplete);
    if (autocomplete.AsAString().EqualsLiteral("off")) {
      continue;
    }
    nsAutoString id;
    textArea->GetId(id);
    if (id.IsEmpty() && (aGeneratedCount > kMaxTraversedXPaths)) {
      continue;
    }
    nsString value;
    textArea->GetValue(value);
    // In order to reduce XPath generation (which is slow), we only save data
    // for form fields that have been changed. (cf. bug 537289)
    if (textArea->AttrValueIs(kNameSpaceID_None, nsGkAtoms::value, value,
                              eCaseMatters)) {
      continue;
    }
    AppendValueToCollectedData(textArea, id, value, aGeneratedCount,
                               std::forward<ArgsT>(args)...);
  }
}

/* static */
template <typename... ArgsT>
void SessionStoreUtils::CollectFromInputElement(Document& aDocument,
                                                uint16_t& aGeneratedCount,
                                                ArgsT&&... args) {
  RefPtr<nsContentList> inputlist = NS_GetContentList(
      &aDocument, kNameSpaceID_XHTML, NS_LITERAL_STRING("input"));
  uint32_t length = inputlist->Length(true);
  for (uint32_t i = 0; i < length; ++i) {
    MOZ_ASSERT(inputlist->Item(i), "null item in node list!");
    nsCOMPtr<nsIFormControl> formControl =
        do_QueryInterface(inputlist->Item(i));
    if (formControl) {
      uint8_t controlType = formControl->ControlType();
      if (controlType == NS_FORM_INPUT_PASSWORD ||
          controlType == NS_FORM_INPUT_HIDDEN ||
          controlType == NS_FORM_INPUT_BUTTON ||
          controlType == NS_FORM_INPUT_IMAGE ||
          controlType == NS_FORM_INPUT_SUBMIT ||
          controlType == NS_FORM_INPUT_RESET) {
        continue;
      }
    }
    RefPtr<HTMLInputElement> input =
        HTMLInputElement::FromNodeOrNull(inputlist->Item(i));
    if (!input || !nsContentUtils::IsAutocompleteEnabled(input)) {
      continue;
    }
    nsAutoString id;
    input->GetId(id);
    if (id.IsEmpty() && (aGeneratedCount > kMaxTraversedXPaths)) {
      continue;
    }
    Nullable<AutocompleteInfo> aInfo;
    input->GetAutocompleteInfo(aInfo);
    if (!aInfo.IsNull() && !aInfo.Value().mCanAutomaticallyPersist) {
      continue;
    }

    if (input->ControlType() == NS_FORM_INPUT_CHECKBOX ||
        input->ControlType() == NS_FORM_INPUT_RADIO) {
      bool checked = input->Checked();
      if (checked == input->DefaultChecked()) {
        continue;
      }
      AppendValueToCollectedData(input, id, checked, aGeneratedCount,
                                 std::forward<ArgsT>(args)...);
    } else if (input->ControlType() == NS_FORM_INPUT_FILE) {
      IgnoredErrorResult rv;
      nsTArray<nsString> result;
      input->MozGetFileNameArray(result, rv);
      if (rv.Failed() || result.Length() == 0) {
        continue;
      }
      AppendValueToCollectedData(input, id, NS_LITERAL_STRING("file"), result,
                                 aGeneratedCount, std::forward<ArgsT>(args)...);
    } else {
      nsString value;
      input->GetValue(value, CallerType::System);
      // In order to reduce XPath generation (which is slow), we only save data
      // for form fields that have been changed. (cf. bug 537289)
      // Also, don't want to collect credit card number.
      if (value.IsEmpty() || IsValidCCNumber(value) ||
          input->HasBeenTypePassword() ||
          input->AttrValueIs(kNameSpaceID_None, nsGkAtoms::value, value,
                             eCaseMatters)) {
        continue;
      }
      AppendValueToCollectedData(aDocument, input, id, value, aGeneratedCount,
                                 std::forward<ArgsT>(args)...);
    }
  }
}

/* static */
template <typename... ArgsT>
void SessionStoreUtils::CollectFromSelectElement(Document& aDocument,
                                                 uint16_t& aGeneratedCount,
                                                 ArgsT&&... args) {
  RefPtr<nsContentList> selectlist = NS_GetContentList(
      &aDocument, kNameSpaceID_XHTML, NS_LITERAL_STRING("select"));
  uint32_t length = selectlist->Length(true);
  for (uint32_t i = 0; i < length; ++i) {
    MOZ_ASSERT(selectlist->Item(i), "null item in node list!");
    RefPtr<HTMLSelectElement> select =
        HTMLSelectElement::FromNodeOrNull(selectlist->Item(i));
    if (!select) {
      continue;
    }
    nsAutoString id;
    select->GetId(id);
    if (id.IsEmpty() && (aGeneratedCount > kMaxTraversedXPaths)) {
      continue;
    }
    AutocompleteInfo aInfo;
    select->GetAutocompleteInfo(aInfo);
    if (!aInfo.mCanAutomaticallyPersist) {
      continue;
    }
    nsAutoCString value;
    if (!select->Multiple()) {
      // <select>s without the multiple attribute are hard to determine the
      // default value, so assume we don't have the default.
      DOMString selectVal;
      select->GetValue(selectVal);
      CollectedNonMultipleSelectValue val;
      val.mSelectedIndex = select->SelectedIndex();
      val.mValue = selectVal.AsAString();
      AppendValueToCollectedData(select, id, val, aGeneratedCount,
                                 std::forward<ArgsT>(args)...);
    } else {
      // <select>s with the multiple attribute are easier to determine the
      // default value since each <option> has a defaultSelected property
      HTMLOptionsCollection* options = select->GetOptions();
      if (!options) {
        continue;
      }
      bool hasDefaultValue = true;
      nsTArray<nsString> selectslist;
      uint32_t numOptions = options->Length();
      for (uint32_t idx = 0; idx < numOptions; idx++) {
        HTMLOptionElement* option = options->ItemAsOption(idx);
        bool selected = option->Selected();
        if (!selected) {
          continue;
        }
        option->GetValue(*selectslist.AppendElement());
        hasDefaultValue =
            hasDefaultValue && (selected == option->DefaultSelected());
      }
      // In order to reduce XPath generation (which is slow), we only save data
      // for form fields that have been changed. (cf. bug 537289)
      if (hasDefaultValue) {
        continue;
      }

      AppendValueToCollectedData(
          select, id, NS_LITERAL_STRING("multipleSelect"), selectslist,
          aGeneratedCount, std::forward<ArgsT>(args)...);
    }
  }
}

static void CollectCurrentFormData(JSContext* aCx, Document& aDocument,
                                   Nullable<CollectedData>& aRetVal) {
  uint16_t generatedCount = 0;
  /* textarea element */
  SessionStoreUtils::CollectFromTextAreaElement(aDocument, generatedCount,
                                                aRetVal);
  /* input element */
  SessionStoreUtils::CollectFromInputElement(aDocument, generatedCount, aCx,
                                             aRetVal);
  /* select element */
  SessionStoreUtils::CollectFromSelectElement(aDocument, generatedCount, aCx,
                                              aRetVal);

  Element* bodyElement = aDocument.GetBody();
  if (aDocument.HasFlag(NODE_IS_EDITABLE) && bodyElement) {
    bodyElement->GetInnerHTML(aRetVal.SetValue().mInnerHTML.Construct(),
                              IgnoreErrors());
  }

  if (aRetVal.IsNull()) {
    return;
  }

  // Store the frame's current URL with its form data so that we can compare
  // it when restoring data to not inject form data into the wrong document.
  nsIURI* uri = aDocument.GetDocumentURI();
  if (uri) {
    uri->GetSpecIgnoringRef(aRetVal.SetValue().mUrl.Construct());
  }
}

MOZ_CAN_RUN_SCRIPT
static void SetElementAsString(Element* aElement, const nsAString& aValue) {
  IgnoredErrorResult rv;
  HTMLTextAreaElement* textArea = HTMLTextAreaElement::FromNode(aElement);
  if (textArea) {
    textArea->SetValue(aValue, rv);
    if (!rv.Failed()) {
      nsContentUtils::DispatchInputEvent(aElement);
    }
    return;
  }
  HTMLInputElement* input = HTMLInputElement::FromNode(aElement);
  if (input) {
    input->SetValue(aValue, CallerType::NonSystem, rv);
    if (!rv.Failed()) {
      nsContentUtils::DispatchInputEvent(aElement);
      return;
    }
  }
  input = HTMLInputElement::FromNodeOrNull(
      nsFocusManager::GetRedirectedFocus(aElement));
  if (input) {
    input->SetValue(aValue, CallerType::NonSystem, rv);
    if (!rv.Failed()) {
      nsContentUtils::DispatchInputEvent(aElement);
    }
  }
}

MOZ_CAN_RUN_SCRIPT
static void SetElementAsBool(Element* aElement, bool aValue) {
  HTMLInputElement* input = HTMLInputElement::FromNode(aElement);
  if (input) {
    bool checked = input->Checked();
    if (aValue != checked) {
      input->SetChecked(aValue);
      nsContentUtils::DispatchInputEvent(aElement);
    }
  }
}

MOZ_CAN_RUN_SCRIPT
static void SetElementAsFiles(HTMLInputElement* aElement,
                              const CollectedFileListValue& aValue) {
  nsTArray<nsString> fileList;
  IgnoredErrorResult rv;
  aElement->MozSetFileNameArray(aValue.mFileList, rv);
  if (rv.Failed()) {
    return;
  }
  nsContentUtils::DispatchInputEvent(aElement);
}

MOZ_CAN_RUN_SCRIPT
static void SetElementAsSelect(HTMLSelectElement* aElement,
                               const CollectedNonMultipleSelectValue& aValue) {
  HTMLOptionsCollection* options = aElement->GetOptions();
  if (!options) {
    return;
  }
  int32_t selectIdx = options->SelectedIndex();
  if (selectIdx >= 0) {
    nsAutoString selectOptionVal;
    options->ItemAsOption(selectIdx)->GetValue(selectOptionVal);
    if (aValue.mValue.Equals(selectOptionVal)) {
      return;
    }
  }
  uint32_t numOptions = options->Length();
  for (uint32_t idx = 0; idx < numOptions; idx++) {
    HTMLOptionElement* option = options->ItemAsOption(idx);
    nsAutoString optionValue;
    option->GetValue(optionValue);
    if (aValue.mValue.Equals(optionValue)) {
      aElement->SetSelectedIndex(idx);
      nsContentUtils::DispatchInputEvent(aElement);
    }
  }
}

MOZ_CAN_RUN_SCRIPT
static void SetElementAsMultiSelect(HTMLSelectElement* aElement,
                                    const nsTArray<nsString>& aValueArray) {
  bool fireEvent = false;
  HTMLOptionsCollection* options = aElement->GetOptions();
  if (!options) {
    return;
  }
  uint32_t numOptions = options->Length();
  for (uint32_t idx = 0; idx < numOptions; idx++) {
    HTMLOptionElement* option = options->ItemAsOption(idx);
    nsAutoString optionValue;
    option->GetValue(optionValue);
    for (uint32_t i = 0, l = aValueArray.Length(); i < l; ++i) {
      if (optionValue.Equals(aValueArray[i])) {
        option->SetSelected(true);
        if (!option->DefaultSelected()) {
          fireEvent = true;
        }
      }
    }
  }
  if (fireEvent) {
    nsContentUtils::DispatchInputEvent(aElement);
  }
}

MOZ_CAN_RUN_SCRIPT
static void SetElementAsObject(JSContext* aCx, Element* aElement,
                               JS::Handle<JS::Value> aObject) {
  RefPtr<HTMLInputElement> input = HTMLInputElement::FromNode(aElement);
  if (input) {
    if (input->ControlType() == NS_FORM_INPUT_FILE) {
      CollectedFileListValue value;
      if (value.Init(aCx, aObject)) {
        SetElementAsFiles(input, value);
      } else {
        JS_ClearPendingException(aCx);
      }
    }
    return;
  }
  RefPtr<HTMLSelectElement> select = HTMLSelectElement::FromNode(aElement);
  if (select) {
    // For Single Select Element
    if (!select->Multiple()) {
      CollectedNonMultipleSelectValue value;
      if (value.Init(aCx, aObject)) {
        SetElementAsSelect(select, value);
      } else {
        JS_ClearPendingException(aCx);
      }
      return;
    }

    // For Multiple Selects Element
    bool isArray = false;
    JS::IsArrayObject(aCx, aObject, &isArray);
    if (!isArray) {
      return;
    }
    JS::Rooted<JSObject*> arrayObj(aCx, &aObject.toObject());
    uint32_t arrayLength = 0;
    if (!JS::GetArrayLength(aCx, arrayObj, &arrayLength)) {
      JS_ClearPendingException(aCx);
      return;
    }
    nsTArray<nsString> array(arrayLength);
    for (uint32_t arrayIdx = 0; arrayIdx < arrayLength; arrayIdx++) {
      JS::Rooted<JS::Value> element(aCx);
      if (!JS_GetElement(aCx, arrayObj, arrayIdx, &element)) {
        JS_ClearPendingException(aCx);
        return;
      }
      if (!element.isString()) {
        return;
      }
      nsAutoJSString value;
      if (!value.init(aCx, element)) {
        JS_ClearPendingException(aCx);
        return;
      }
      array.AppendElement(value);
    }
    SetElementAsMultiSelect(select, array);
  }
}

MOZ_CAN_RUN_SCRIPT
static void SetRestoreData(JSContext* aCx, Element* aElement,
                           JS::MutableHandle<JS::Value> aObject) {
  nsAutoString data;
  if (nsContentUtils::StringifyJSON(aCx, aObject, data)) {
    SetElementAsString(aElement, data);
  } else {
    JS_ClearPendingException(aCx);
  }
}

MOZ_CAN_RUN_SCRIPT
static void SetInnerHTML(Document& aDocument, const CollectedData& aData) {
  RefPtr<Element> bodyElement = aDocument.GetBody();
  if (aDocument.HasFlag(NODE_IS_EDITABLE) && bodyElement) {
    IgnoredErrorResult rv;
    bodyElement->SetInnerHTML(aData.mInnerHTML.Value(),
                              aDocument.NodePrincipal(), rv);
    if (!rv.Failed()) {
      nsContentUtils::DispatchInputEvent(bodyElement);
    }
  }
}

class FormDataParseContext : public txIParseContext {
 public:
  explicit FormDataParseContext(bool aCaseInsensitive)
      : mIsCaseInsensitive(aCaseInsensitive) {}

  nsresult resolveNamespacePrefix(nsAtom* aPrefix, int32_t& aID) override {
    if (aPrefix == nsGkAtoms::xul) {
      aID = kNameSpaceID_XUL;
    } else {
      MOZ_ASSERT(nsDependentAtomString(aPrefix).EqualsLiteral("xhtml"));
      aID = kNameSpaceID_XHTML;
    }
    return NS_OK;
  }

  nsresult resolveFunctionCall(nsAtom* aName, int32_t aID,
                               FunctionCall** aFunction) override {
    return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
  }

  bool caseInsensitiveNameTests() override { return mIsCaseInsensitive; }

  void SetErrorOffset(uint32_t aOffset) override {}

 private:
  bool mIsCaseInsensitive;
};

static Element* FindNodeByXPath(JSContext* aCx, Document& aDocument,
                                const nsAString& aExpression) {
  FormDataParseContext parsingContext(aDocument.IsHTMLDocument());
  IgnoredErrorResult rv;
  UniquePtr<XPathExpression> expression(
      aDocument.XPathEvaluator()->CreateExpression(aExpression, &parsingContext,
                                                   &aDocument, rv));
  if (rv.Failed()) {
    return nullptr;
  }
  RefPtr<XPathResult> result = expression->Evaluate(
      aCx, aDocument, XPathResult::FIRST_ORDERED_NODE_TYPE, nullptr, rv);
  if (rv.Failed()) {
    return nullptr;
  }
  return Element::FromNodeOrNull(result->GetSingleNodeValue(rv));
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY
/* static */
bool SessionStoreUtils::RestoreFormData(const GlobalObject& aGlobal,
                                        Document& aDocument,
                                        const CollectedData& aData) {
  if (!aData.mUrl.WasPassed()) {
    return true;
  }
  // Don't restore any data for the given frame if the URL
  // stored in the form data doesn't match its current URL.
  nsAutoCString url;
  Unused << aDocument.GetDocumentURI()->GetSpecIgnoringRef(url);
  if (!aData.mUrl.Value().Equals(url)) {
    return false;
  }
  if (aData.mInnerHTML.WasPassed()) {
    SetInnerHTML(aDocument, aData);
  }
  if (aData.mId.WasPassed()) {
    for (auto& entry : aData.mId.Value().Entries()) {
      RefPtr<Element> node = aDocument.GetElementById(entry.mKey);
      if (node == nullptr) {
        continue;
      }
      if (entry.mValue.IsString()) {
        SetElementAsString(node, entry.mValue.GetAsString());
      } else if (entry.mValue.IsBoolean()) {
        SetElementAsBool(node, entry.mValue.GetAsBoolean());
      } else {
        // For about:{sessionrestore,welcomeback} we saved the field as JSON to
        // avoid nested instances causing humongous sessionstore.js files.
        // cf. bug 467409
        JSContext* cx = aGlobal.Context();
        if (entry.mKey.EqualsLiteral("sessionData")) {
          nsAutoCString url;
          Unused << aDocument.GetDocumentURI()->GetSpecIgnoringRef(url);
          if (url.EqualsLiteral("about:sessionrestore") ||
              url.EqualsLiteral("about:welcomeback")) {
            JS::Rooted<JS::Value> object(
                cx, JS::ObjectValue(*entry.mValue.GetAsObject()));
            SetRestoreData(cx, node, &object);
            continue;
          }
        }
        JS::Rooted<JS::Value> object(
            cx, JS::ObjectValue(*entry.mValue.GetAsObject()));
        SetElementAsObject(cx, node, object);
      }
    }
  }
  if (aData.mXpath.WasPassed()) {
    for (auto& entry : aData.mXpath.Value().Entries()) {
      RefPtr<Element> node =
          FindNodeByXPath(aGlobal.Context(), aDocument, entry.mKey);
      if (node == nullptr) {
        continue;
      }
      if (entry.mValue.IsString()) {
        SetElementAsString(node, entry.mValue.GetAsString());
      } else if (entry.mValue.IsBoolean()) {
        SetElementAsBool(node, entry.mValue.GetAsBoolean());
      } else {
        JS::Rooted<JS::Value> object(
            aGlobal.Context(), JS::ObjectValue(*entry.mValue.GetAsObject()));
        SetElementAsObject(aGlobal.Context(), node, object);
      }
    }
  }
  return true;
}

/* Read entries in the session storage data contained in a tab's history. */
static void ReadAllEntriesFromStorage(nsPIDOMWindowOuter* aWindow,
                                      nsTArray<nsCString>& aOrigins,
                                      nsTArray<nsString>& aKeys,
                                      nsTArray<nsString>& aValues) {
  BrowsingContext* const browsingContext = aWindow->GetBrowsingContext();
  if (!browsingContext) {
    return;
  }

  Document* doc = aWindow->GetDoc();
  if (!doc) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  if (!principal) {
    return;
  }

  nsCOMPtr<nsIPrincipal> storagePrincipal = doc->EffectiveStoragePrincipal();
  if (!storagePrincipal) {
    return;
  }

  nsAutoCString origin;
  nsresult rv = principal->GetOrigin(origin);
  if (NS_FAILED(rv) || aOrigins.Contains(origin)) {
    // Don't read a host twice.
    return;
  }

  /* Completed checking for recursion and is about to read storage*/
  const RefPtr<SessionStorageManager> storageManager =
      browsingContext->GetSessionStorageManager();
  if (!storageManager) {
    return;
  }
  RefPtr<Storage> storage;
  storageManager->GetStorage(aWindow->GetCurrentInnerWindow(), principal,
                             storagePrincipal, false, getter_AddRefs(storage));
  if (!storage) {
    return;
  }
  mozilla::IgnoredErrorResult result;
  uint32_t len = storage->GetLength(*principal, result);
  if (result.Failed() || len == 0) {
    return;
  }
  int64_t storageUsage = storage->GetOriginQuotaUsage();
  if (storageUsage > StaticPrefs::browser_sessionstore_dom_storage_limit()) {
    return;
  }

  for (uint32_t i = 0; i < len; i++) {
    nsString key, value;
    mozilla::IgnoredErrorResult res;
    storage->Key(i, key, *principal, res);
    if (res.Failed()) {
      continue;
    }

    storage->GetItem(key, value, *principal, res);
    if (res.Failed()) {
      continue;
    }

    aKeys.AppendElement(key);
    aValues.AppendElement(value);
    aOrigins.AppendElement(origin);
  }
}

/* Collect Collect session storage from current frame and all child frame */
/* static */
void SessionStoreUtils::CollectedSessionStorage(
    BrowsingContext* aBrowsingContext, nsTArray<nsCString>& aOrigins,
    nsTArray<nsString>& aKeys, nsTArray<nsString>& aValues) {
  /* Collect session store from current frame */
  nsPIDOMWindowOuter* window = aBrowsingContext->GetDOMWindow();
  if (!window) {
    return;
  }
  ReadAllEntriesFromStorage(window, aOrigins, aKeys, aValues);

  /* Collect session storage from all child frame */
  nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
  if (!docShell) {
    return;
  }

  // This is not going to work for fission. Bug 1572084 for tracking it.
  for (BrowsingContext* child : aBrowsingContext->Children()) {
    window = child->GetDOMWindow();
    if (!window) {
      return;
    }
    docShell = window->GetDocShell();
    if (!docShell) {
      return;
    }
    bool isDynamic = false;
    nsresult rv = docShell->GetCreatedDynamically(&isDynamic);
    if (NS_SUCCEEDED(rv) && isDynamic) {
      continue;
    }
    SessionStoreUtils::CollectedSessionStorage(child, aOrigins, aKeys, aValues);
  }
}

/* static */
void SessionStoreUtils::RestoreSessionStorage(
    const GlobalObject& aGlobal, nsIDocShell* aDocShell,
    const Record<nsString, Record<nsString, nsString>>& aData) {
  for (auto& entry : aData.Entries()) {
    // NOTE: In capture() we record the full origin for the URI which the
    // sessionStorage is being captured for. As of bug 1235657 this code
    // stopped parsing any origins which have originattributes correctly, as
    // it decided to use the origin attributes from the docshell, and try to
    // interpret the origin as a URI. Since bug 1353844 this code now correctly
    // parses the full origin, and then discards the origin attributes, to
    // make the behavior line up with the original intentions in bug 1235657
    // while preserving the ability to read all session storage from
    // previous versions. In the future, if this behavior is desired, we may
    // want to use the spec instead of the origin as the key, and avoid
    // transmitting origin attribute information which we then discard when
    // restoring.
    //
    // If changing this logic, make sure to also change the principal
    // computation logic in SessionStore::_sendRestoreHistory.

    // OriginAttributes are always after a '^' character
    int32_t pos = entry.mKey.RFindChar('^');
    nsCOMPtr<nsIPrincipal> principal = BasePrincipal::CreateContentPrincipal(
        NS_ConvertUTF16toUTF8(Substring(entry.mKey, 0, pos)));
    BrowsingContext* const browsingContext =
        nsDocShell::Cast(aDocShell)->GetBrowsingContext();
    if (!browsingContext) {
      return;
    }
    const RefPtr<SessionStorageManager> storageManager =
        browsingContext->GetSessionStorageManager();
    if (!storageManager) {
      return;
    }
    RefPtr<Storage> storage;
    // There is no need to pass documentURI, it's only used to fill documentURI
    // property of domstorage event, which in this case has no consumer.
    // Prevention of events in case of missing documentURI will be solved in a
    // followup bug to bug 600307.
    // Null window because the current window doesn't match the principal yet
    // and loads about:blank.
    storageManager->CreateStorage(nullptr, principal, principal, EmptyString(),
                                  false, getter_AddRefs(storage));
    if (!storage) {
      continue;
    }
    for (auto& InnerEntry : entry.mValue.Entries()) {
      IgnoredErrorResult result;
      storage->SetItem(InnerEntry.mKey, InnerEntry.mValue, *principal, result);
      if (result.Failed()) {
        NS_WARNING("storage set item failed!");
      }
    }
  }
}

typedef void (*CollectorFunc)(JSContext* aCx, Document& aDocument,
                              Nullable<CollectedData>& aRetVal);

/**
 * A function that will recursively call |CollectorFunc| to collect data for all
 * non-dynamic frames in the current frame/docShell tree.
 */
static void CollectFrameTreeData(JSContext* aCx,
                                 BrowsingContext* aBrowsingContext,
                                 Nullable<CollectedData>& aRetVal,
                                 CollectorFunc aFunc) {
  nsPIDOMWindowOuter* window = aBrowsingContext->GetDOMWindow();
  if (!window) {
    return;
  }

  nsIDocShell* docShell = window->GetDocShell();
  if (!docShell || docShell->GetCreatedDynamically()) {
    return;
  }

  Document* document = window->GetDoc();
  if (!document) {
    return;
  }

  /* Collect data from current frame */
  aFunc(aCx, *document, aRetVal);

  /* Collect data from all child frame */
  nsTArray<JSObject*> childrenData;
  SequenceRooter<JSObject*> rooter(aCx, &childrenData);
  uint32_t trailingNullCounter = 0;

  // This is not going to work for fission. Bug 1572084 for tracking it.
  for (auto& child : aBrowsingContext->Children()) {
    NullableRootedDictionary<CollectedData> data(aCx);
    CollectFrameTreeData(aCx, child, data, aFunc);
    if (data.IsNull()) {
      childrenData.AppendElement(nullptr);
      trailingNullCounter++;
      continue;
    }
    JS::Rooted<JS::Value> jsval(aCx);
    if (!ToJSValue(aCx, data.SetValue(), &jsval)) {
      JS_ClearPendingException(aCx);
      continue;
    }
    childrenData.AppendElement(&jsval.toObject());
    trailingNullCounter = 0;
  }

  if (trailingNullCounter != childrenData.Length()) {
    childrenData.TruncateLength(childrenData.Length() - trailingNullCounter);
    aRetVal.SetValue().mChildren.Construct().SwapElements(childrenData);
  }
}

/* static */ void SessionStoreUtils::CollectScrollPosition(
    const GlobalObject& aGlobal, WindowProxyHolder& aWindow,
    Nullable<CollectedData>& aRetVal) {
  CollectFrameTreeData(aGlobal.Context(), aWindow.get(), aRetVal,
                       CollectCurrentScrollPosition);
}

/* static */ void SessionStoreUtils::CollectFormData(
    const GlobalObject& aGlobal, WindowProxyHolder& aWindow,
    Nullable<CollectedData>& aRetVal) {
  CollectFrameTreeData(aGlobal.Context(), aWindow.get(), aRetVal,
                       CollectCurrentFormData);
}

/* static */ void SessionStoreUtils::ComposeInputData(
    const nsTArray<CollectedInputDataValue>& aData, InputElementData& ret) {
  nsTArray<int> selectedIndex, valueIdx;
  nsTArray<nsString> id, selectVal, strVal, type;
  nsTArray<bool> boolVal;

  for (const CollectedInputDataValue& data : aData) {
    id.AppendElement(data.id);
    type.AppendElement(data.type);

    if (data.value.is<mozilla::dom::CollectedNonMultipleSelectValue>()) {
      valueIdx.AppendElement(selectVal.Length());
      selectedIndex.AppendElement(
          data.value.as<mozilla::dom::CollectedNonMultipleSelectValue>()
              .mSelectedIndex);
      selectVal.AppendElement(
          data.value.as<mozilla::dom::CollectedNonMultipleSelectValue>()
              .mValue);
    } else if (data.value.is<nsTArray<nsString>>()) {
      // The first valueIdx is "index of the first string value"
      valueIdx.AppendElement(strVal.Length());
      strVal.AppendElements(data.value.as<nsTArray<nsString>>());
      // The second valueIdx is "index of the last string value" + 1
      id.AppendElement(data.id);
      type.AppendElement(data.type);
      valueIdx.AppendElement(strVal.Length());
    } else if (data.value.is<nsString>()) {
      valueIdx.AppendElement(strVal.Length());
      strVal.AppendElement(data.value.as<nsString>());
    } else if (data.type.EqualsLiteral("bool")) {
      valueIdx.AppendElement(boolVal.Length());
      boolVal.AppendElement(data.value.as<bool>());
    }
  }

  if (selectedIndex.Length() != 0) {
    ret.mSelectedIndex.Construct(std::move(selectedIndex));
  }
  if (valueIdx.Length() != 0) {
    ret.mValueIdx.Construct(std::move(valueIdx));
  }
  if (id.Length() != 0) {
    ret.mId.Construct(std::move(id));
  }
  if (selectVal.Length() != 0) {
    ret.mSelectVal.Construct(std::move(selectVal));
  }
  if (strVal.Length() != 0) {
    ret.mStrVal.Construct(std::move(strVal));
  }
  if (type.Length() != 0) {
    ret.mType.Construct(std::move(type));
  }
  if (boolVal.Length() != 0) {
    ret.mBoolVal.Construct(std::move(boolVal));
  }
}
