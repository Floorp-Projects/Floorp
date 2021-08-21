/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Array.h"  // JS::GetArrayLength, JS::IsArrayObject
#include "js/JSON.h"
#include "js/PropertyAndElement.h"  // JS_GetElement
#include "jsapi.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/AutocompleteInfoBinding.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/SessionStorageManager.h"
#include "mozilla/dom/PBackgroundSessionStorageCache.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/txIXPathContext.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/dom/XPathResult.h"
#include "mozilla/dom/XPathEvaluator.h"
#include "mozilla/dom/XPathExpression.h"
#include "mozilla/dom/PBackgroundSessionStorageCache.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ReverseIterator.h"
#include "mozilla/UniquePtr.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentList.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsGlobalWindowOuter.h"
#include "nsIDocShell.h"
#include "nsIFormControl.h"
#include "nsIScrollableFrame.h"
#include "nsISHistory.h"
#include "nsIXULRuntime.h"
#include "nsPresContext.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::sessionstore;

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
    if (!outer || !outer->GetDocShell()) {
      return false;
    }

    RefPtr<BrowsingContext> context = outer->GetBrowsingContext();
    return context && !context->CreatedDynamically();
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

    RefPtr<BrowsingContext> context = item->GetBrowsingContext();
    if (!context) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    if (!context->CreatedDynamically()) {
      int32_t childOffset = context->ChildOffset();
      aCallback.Call(WindowProxyHolder(context.forget()), childOffset);
    }
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
    nsIDocShell* aDocShell, const nsCString& aDisallowCapabilities) {
  aDocShell->SetAllowPlugins(true);
  aDocShell->SetAllowMetaRedirects(true);
  aDocShell->SetAllowSubframes(true);
  aDocShell->SetAllowImages(true);
  aDocShell->SetAllowMedia(true);
  aDocShell->SetAllowDNSPrefetch(true);
  aDocShell->SetAllowWindowControl(true);
  aDocShell->SetAllowContentRetargeting(true);
  aDocShell->SetAllowContentRetargetingOnChildren(true);

  bool allowJavascript = true;
  for (const nsACString& token :
       nsCCharSeparatedTokenizer(aDisallowCapabilities, ',').ToRange()) {
    if (token.EqualsLiteral("Plugins")) {
      aDocShell->SetAllowPlugins(false);
    } else if (token.EqualsLiteral("Javascript")) {
      allowJavascript = false;
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

  if (!mozilla::SessionHistoryInParent()) {
    // With SessionHistoryInParent, this is set from the parent process.
    BrowsingContext* bc = aDocShell->GetBrowsingContext();
    Unused << bc->SetAllowJavascript(allowJavascript);
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
  if (aData.mScroll.WasPassed()) {
    RestoreScrollPosition(aWindow, aData.mScroll.Value());
  }
}

/* static */
void SessionStoreUtils::RestoreScrollPosition(
    nsGlobalWindowInner& aWindow, const nsCString& aScrollPosition) {
  nsCCharSeparatedTokenizer tokenizer(aScrollPosition, ',');
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

/* for nsString value */
static void AppendValueToCollectedData(nsINode* aNode, const nsAString& aId,
                                       const nsString& aValue,
                                       uint16_t& aGeneratedCount,
                                       Nullable<CollectedData>& aRetVal) {
  Record<nsString, OwningStringOrBooleanOrObject>::EntryType* entry =
      AppendEntryToCollectedData(aNode, aId, aGeneratedCount, aRetVal);
  entry->mValue.SetAsString() = aValue;
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
    val.mFileList = std::move(aValue);
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

static void AppendEntry(nsINode* aNode, const nsString& aId,
                        const FormEntryValue& aValue,
                        sessionstore::FormData& aFormData) {
  if (aId.IsEmpty()) {
    FormEntry* entry = aFormData.xpath().AppendElement();
    entry->value() = aValue;
    aNode->GenerateXPath(entry->id());
  } else {
    aFormData.id().AppendElement(FormEntry{aId, aValue});
  }
}

static void CollectTextAreaElement(Document* aDocument,
                                   sessionstore::FormData& aFormData) {
  RefPtr<nsContentList> textlist =
      NS_GetContentList(aDocument, kNameSpaceID_XHTML, u"textarea"_ns);
  uint32_t length = textlist->Length();
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
    if (id.IsEmpty() && (aFormData.xpath().Length() > kMaxTraversedXPaths)) {
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

    AppendEntry(textArea, id, TextField{value}, aFormData);
  }
}

static void CollectInputElement(Document* aDocument,
                                sessionstore::FormData& aFormData) {
  RefPtr<nsContentList> inputlist =
      NS_GetContentList(aDocument, kNameSpaceID_XHTML, u"input"_ns);
  uint32_t length = inputlist->Length();
  for (uint32_t i = 0; i < length; ++i) {
    MOZ_ASSERT(inputlist->Item(i), "null item in node list!");
    nsCOMPtr<nsIFormControl> formControl =
        do_QueryInterface(inputlist->Item(i));
    if (formControl) {
      auto controlType = formControl->ControlType();
      if (controlType == FormControlType::InputPassword ||
          controlType == FormControlType::InputHidden ||
          controlType == FormControlType::InputButton ||
          controlType == FormControlType::InputImage ||
          controlType == FormControlType::InputSubmit ||
          controlType == FormControlType::InputReset) {
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
    if (id.IsEmpty() && (aFormData.xpath().Length() > kMaxTraversedXPaths)) {
      continue;
    }
    Nullable<AutocompleteInfo> aInfo;
    input->GetAutocompleteInfo(aInfo);
    if (!aInfo.IsNull() && !aInfo.Value().mCanAutomaticallyPersist) {
      continue;
    }

    FormEntryValue value;
    if (input->ControlType() == FormControlType::InputCheckbox ||
        input->ControlType() == FormControlType::InputRadio) {
      bool checked = input->Checked();
      if (checked == input->DefaultChecked()) {
        continue;
      }
      AppendEntry(input, id, Checkbox{checked}, aFormData);
    } else if (input->ControlType() == FormControlType::InputFile) {
      IgnoredErrorResult rv;
      sessionstore::FileList file;
      input->MozGetFileNameArray(file.valueList(), rv);
      if (rv.Failed() || file.valueList().IsEmpty()) {
        continue;
      }
      AppendEntry(input, id, file, aFormData);
    } else {
      TextField field;
      input->GetValue(field.value(), CallerType::System);
      auto& value = field.value();
      // In order to reduce XPath generation (which is slow), we only save data
      // for form fields that have been changed. (cf. bug 537289)
      // Also, don't want to collect credit card number.
      if (value.IsEmpty() || IsValidCCNumber(value) ||
          input->HasBeenTypePassword() ||
          input->AttrValueIs(kNameSpaceID_None, nsGkAtoms::value, value,
                             eCaseMatters)) {
        continue;
      }
      AppendEntry(input, id, field, aFormData);
    }
  }
}

static void CollectSelectElement(Document* aDocument,
                                 sessionstore::FormData& aFormData) {
  RefPtr<nsContentList> selectlist =
      NS_GetContentList(aDocument, kNameSpaceID_XHTML, u"select"_ns);
  uint32_t length = selectlist->Length();
  for (uint32_t i = 0; i < length; ++i) {
    MOZ_ASSERT(selectlist->Item(i), "null item in node list!");
    RefPtr<HTMLSelectElement> select =
        HTMLSelectElement::FromNodeOrNull(selectlist->Item(i));
    if (!select) {
      continue;
    }
    nsAutoString id;
    select->GetId(id);
    if (id.IsEmpty() && (aFormData.xpath().Length() > kMaxTraversedXPaths)) {
      continue;
    }
    AutocompleteInfo aInfo;
    select->GetAutocompleteInfo(aInfo);
    if (!aInfo.mCanAutomaticallyPersist) {
      continue;
    }

    if (!select->Multiple()) {
      HTMLOptionsCollection* options = select->GetOptions();
      if (!options) {
        continue;
      }

      uint32_t numOptions = options->Length();
      int32_t defaultIndex = 0;
      for (uint32_t idx = 0; idx < numOptions; idx++) {
        HTMLOptionElement* option = options->ItemAsOption(idx);
        if (option->DefaultSelected()) {
          defaultIndex = option->Index();
        }
      }

      int32_t selectedIndex = select->SelectedIndex();
      if (selectedIndex == defaultIndex || selectedIndex < 0) {
        continue;
      }

      DOMString selectVal;
      select->GetValue(selectVal);
      AppendEntry(select, id,
                  SingleSelect{static_cast<uint32_t>(selectedIndex),
                               selectVal.AsAString()},
                  aFormData);
    } else {
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

        hasDefaultValue =
            hasDefaultValue && (selected == option->DefaultSelected());

        if (!selected) {
          continue;
        }
        option->GetValue(*selectslist.AppendElement());
      }
      // In order to reduce XPath generation (which is slow), we only save data
      // for form fields that have been changed. (cf. bug 537289)
      if (hasDefaultValue) {
        continue;
      }

      AppendEntry(select, id, MultipleSelect{selectslist}, aFormData);
    }
  }
}

/* static */
void SessionStoreUtils::CollectFormData(Document* aDocument,
                                        sessionstore::FormData& aFormData) {
  MOZ_DIAGNOSTIC_ASSERT(aDocument);
  CollectTextAreaElement(aDocument, aFormData);
  CollectInputElement(aDocument, aFormData);
  CollectSelectElement(aDocument, aFormData);

  aFormData.hasData() =
      !aFormData.id().IsEmpty() || !aFormData.xpath().IsEmpty();
}

/* static */
template <typename... ArgsT>
void SessionStoreUtils::CollectFromTextAreaElement(Document& aDocument,
                                                   uint16_t& aGeneratedCount,
                                                   ArgsT&&... args) {
  RefPtr<nsContentList> textlist =
      NS_GetContentList(&aDocument, kNameSpaceID_XHTML, u"textarea"_ns);
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
  RefPtr<nsContentList> inputlist =
      NS_GetContentList(&aDocument, kNameSpaceID_XHTML, u"input"_ns);
  uint32_t length = inputlist->Length(true);
  for (uint32_t i = 0; i < length; ++i) {
    MOZ_ASSERT(inputlist->Item(i), "null item in node list!");
    nsCOMPtr<nsIFormControl> formControl =
        do_QueryInterface(inputlist->Item(i));
    if (formControl) {
      auto controlType = formControl->ControlType();
      if (controlType == FormControlType::InputPassword ||
          controlType == FormControlType::InputHidden ||
          controlType == FormControlType::InputButton ||
          controlType == FormControlType::InputImage ||
          controlType == FormControlType::InputSubmit ||
          controlType == FormControlType::InputReset) {
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

    if (input->ControlType() == FormControlType::InputCheckbox ||
        input->ControlType() == FormControlType::InputRadio) {
      bool checked = input->Checked();
      if (checked == input->DefaultChecked()) {
        continue;
      }
      AppendValueToCollectedData(input, id, checked, aGeneratedCount,
                                 std::forward<ArgsT>(args)...);
    } else if (input->ControlType() == FormControlType::InputFile) {
      IgnoredErrorResult rv;
      nsTArray<nsString> result;
      input->MozGetFileNameArray(result, rv);
      if (rv.Failed() || result.Length() == 0) {
        continue;
      }
      AppendValueToCollectedData(input, id, u"file"_ns, result, aGeneratedCount,
                                 std::forward<ArgsT>(args)...);
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
  RefPtr<nsContentList> selectlist =
      NS_GetContentList(&aDocument, kNameSpaceID_XHTML, u"select"_ns);
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

      AppendValueToCollectedData(select, id, u"multipleSelect"_ns, selectslist,
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
    if (input->ControlType() == FormControlType::InputFile) {
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
static void SetSessionData(JSContext* aCx, Element* aElement,
                           JS::MutableHandle<JS::Value> aObject) {
  nsAutoString data;
  if (nsContentUtils::StringifyJSON(aCx, aObject, data)) {
    SetElementAsString(aElement, data);
  } else {
    JS_ClearPendingException(aCx);
  }
}

MOZ_CAN_RUN_SCRIPT
static void SetInnerHTML(Document& aDocument, const nsString& aInnerHTML) {
  RefPtr<Element> bodyElement = aDocument.GetBody();
  if (aDocument.HasFlag(NODE_IS_EDITABLE) && bodyElement) {
    IgnoredErrorResult rv;
    bodyElement->SetInnerHTML(aInnerHTML, aDocument.NodePrincipal(), rv);
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

static Element* FindNodeByXPath(Document& aDocument,
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
      aDocument, XPathResult::FIRST_ORDERED_NODE_TYPE, nullptr, rv);
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
    SetInnerHTML(aDocument, aData.mInnerHTML.Value());
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
          if (url.EqualsLiteral("about:sessionrestore") ||
              url.EqualsLiteral("about:welcomeback")) {
            JS::Rooted<JS::Value> object(
                cx, JS::ObjectValue(*entry.mValue.GetAsObject()));
            SetSessionData(cx, node, &object);
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
      RefPtr<Element> node = FindNodeByXPath(aDocument, entry.mKey);
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

MOZ_CAN_RUN_SCRIPT
void RestoreFormEntry(Element* aNode, const FormEntryValue& aValue) {
  using Type = sessionstore::FormEntryValue::Type;
  switch (aValue.type()) {
    case Type::TCheckbox:
      SetElementAsBool(aNode, aValue.get_Checkbox().value());
      break;
    case Type::TTextField:
      SetElementAsString(aNode, aValue.get_TextField().value());
      break;
    case Type::TFileList: {
      if (RefPtr<HTMLInputElement> input = HTMLInputElement::FromNode(aNode);
          input && input->ControlType() == FormControlType::InputFile) {
        CollectedFileListValue value;
        value.mFileList = aValue.get_FileList().valueList().Clone();
        SetElementAsFiles(input, value);
      }
      break;
    }
    case Type::TSingleSelect: {
      if (RefPtr<HTMLSelectElement> select = HTMLSelectElement::FromNode(aNode);
          select && !select->Multiple()) {
        CollectedNonMultipleSelectValue value;
        value.mSelectedIndex = aValue.get_SingleSelect().index();
        value.mValue = aValue.get_SingleSelect().value();
        SetElementAsSelect(select, value);
      }
      break;
    }
    case Type::TMultipleSelect: {
      if (RefPtr<HTMLSelectElement> select = HTMLSelectElement::FromNode(aNode);
          select && select->Multiple()) {
        SetElementAsMultiSelect(select,
                                aValue.get_MultipleSelect().valueList());
      }
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE();
  }
}

MOZ_CAN_RUN_SCRIPT
/* static */
void SessionStoreUtils::RestoreFormData(
    Document& aDocument, const nsString& aInnerHTML,
    const nsTArray<SessionStoreRestoreData::Entry>& aEntries) {
  if (!aInnerHTML.IsEmpty()) {
    SetInnerHTML(aDocument, aInnerHTML);
  }

  for (const auto& entry : aEntries) {
    RefPtr<Element> node = entry.mIsXPath
                               ? FindNodeByXPath(aDocument, entry.mData.id())
                               : aDocument.GetElementById(entry.mData.id());
    if (node) {
      RestoreFormEntry(node, entry.mData.value());
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
  if (aBrowsingContext->CreatedDynamically()) {
    return;
  }

  nsPIDOMWindowOuter* window = aBrowsingContext->GetDOMWindow();
  if (!window || !window->GetDocShell()) {
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
    aRetVal.SetValue().mChildren.Construct() = std::move(childrenData);
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
    } else if (data.value.is<CopyableTArray<nsString>>()) {
      // The first valueIdx is "index of the first string value"
      valueIdx.AppendElement(strVal.Length());
      strVal.AppendElements(data.value.as<CopyableTArray<nsString>>());
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

MOZ_CAN_RUN_SCRIPT
already_AddRefed<nsISessionStoreRestoreData>
SessionStoreUtils::ConstructSessionStoreRestoreData(
    const GlobalObject& aGlobal) {
  nsCOMPtr<nsISessionStoreRestoreData> data = new SessionStoreRestoreData();
  return data.forget();
}

/* static */
MOZ_CAN_RUN_SCRIPT
already_AddRefed<Promise> SessionStoreUtils::InitializeRestore(
    const GlobalObject& aGlobal, CanonicalBrowsingContext& aContext,
    nsISessionStoreRestoreData* aData, ErrorResult& aError) {
  if (!mozilla::SessionHistoryInParent()) {
    MOZ_CRASH("why were we called?");
  }

  MOZ_DIAGNOSTIC_ASSERT(aContext.IsTop());

  MOZ_DIAGNOSTIC_ASSERT(aData);
  nsCOMPtr<SessionStoreRestoreData> data = do_QueryInterface(aData);
  aContext.SetRestoreData(data, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  MOZ_DIAGNOSTIC_ASSERT(aContext.GetSessionHistory());
  aContext.GetSessionHistory()->ReloadCurrentEntry();

  return aContext.GetRestorePromise();
}

/* static */
void SessionStoreUtils::RestoreDocShellState(
    nsIDocShell* aDocShell, const DocShellRestoreState& aState) {
  if (aDocShell) {
    if (aState.URI()) {
      aDocShell->SetCurrentURI(aState.URI());
    }
    RestoreDocShellCapabilities(aDocShell, aState.docShellCaps());
  }
}

/* static */
already_AddRefed<Promise> SessionStoreUtils::RestoreDocShellState(
    const GlobalObject& aGlobal, CanonicalBrowsingContext& aContext,
    const nsACString& aURL, const nsCString& aDocShellCaps,
    ErrorResult& aError) {
  MOZ_RELEASE_ASSERT(mozilla::SessionHistoryInParent());
  MOZ_RELEASE_ASSERT(aContext.IsTop());

  WindowGlobalParent* wgp = aContext.GetCurrentWindowGlobal();
  if (!wgp) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_DIAGNOSTIC_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aError);
  if (aError.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  if (!aURL.IsEmpty()) {
    if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), aURL))) {
      aError.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  }

  bool allowJavascript = true;
  for (const nsACString& token :
       nsCCharSeparatedTokenizer(aDocShellCaps, ',').ToRange()) {
    if (token.EqualsLiteral("Javascript")) {
      allowJavascript = false;
    }
  }

  Unused << aContext.SetAllowJavascript(allowJavascript);

  DocShellRestoreState state = {uri, aDocShellCaps};

  // TODO (anny): Investigate removing this roundtrip.
  wgp->SendRestoreDocShellState(state)->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise](void) { promise->MaybeResolveWithUndefined(); },
      [promise](void) { promise->MaybeRejectWithUndefined(); });

  return promise.forget();
}

/* static */
void SessionStoreUtils::RestoreSessionStorageFromParent(
    const GlobalObject& aGlobal, const CanonicalBrowsingContext& aContext,
    const Record<nsCString, Record<nsString, nsString>>& aSessionStorage) {
  nsTArray<SSCacheCopy> cacheInitList;
  for (const auto& originEntry : aSessionStorage.Entries()) {
    nsCOMPtr<nsIPrincipal> storagePrincipal =
        BasePrincipal::CreateContentPrincipal(originEntry.mKey);

    nsCString originKey;
    nsresult rv = storagePrincipal->GetStorageOriginKey(originKey);
    if (NS_FAILED(rv)) {
      continue;
    }

    SSCacheCopy& cacheInit = *cacheInitList.AppendElement();

    cacheInit.originKey() = originKey;
    storagePrincipal->OriginAttributesRef().CreateSuffix(
        cacheInit.originAttributes());

    for (const auto& entry : originEntry.mValue.Entries()) {
      SSSetItemInfo& setItemInfo = *cacheInit.data().AppendElement();
      setItemInfo.key() = entry.mKey;
      setItemInfo.value() = entry.mValue;
    }
  }

  BackgroundSessionStorageManager::LoadData(aContext.Id(), cacheInitList);
}

/* static */
nsresult SessionStoreUtils::ConstructFormDataValues(
    JSContext* aCx, const nsTArray<sessionstore::FormEntry>& aValues,
    nsTArray<Record<nsString, OwningStringOrBooleanOrObject>::EntryType>&
        aEntries,
    bool aParseSessionData) {
  using EntryType = Record<nsString, OwningStringOrBooleanOrObject>::EntryType;

  if (!aEntries.SetCapacity(aValues.Length(), fallible)) {
    return NS_ERROR_FAILURE;
  }

  for (const auto& value : aValues) {
    EntryType* entry = aEntries.AppendElement();

    using Type = sessionstore::FormEntryValue::Type;
    switch (value.value().type()) {
      case Type::TCheckbox:
        entry->mValue.SetAsBoolean() = value.value().get_Checkbox().value();
        break;
      case Type::TTextField: {
        if (aParseSessionData && value.id() == u"sessionData"_ns) {
          JS::Rooted<JS::Value> jsval(aCx);
          const auto& fieldValue = value.value().get_TextField().value();
          if (!JS_ParseJSON(aCx, fieldValue.get(), fieldValue.Length(),
                            &jsval) ||
              !jsval.isObject()) {
            return NS_ERROR_FAILURE;
          }
          entry->mValue.SetAsObject() = &jsval.toObject();
        } else {
          entry->mValue.SetAsString() = value.value().get_TextField().value();
        }
        break;
      }
      case Type::TFileList: {
        CollectedFileListValue file;
        file.mFileList = value.value().get_FileList().valueList().Clone();

        JS::Rooted<JS::Value> jsval(aCx);
        if (!ToJSValue(aCx, file, &jsval) || !jsval.isObject()) {
          return NS_ERROR_FAILURE;
        }
        entry->mValue.SetAsObject() = &jsval.toObject();
        break;
      }
      case Type::TSingleSelect: {
        CollectedNonMultipleSelectValue select;
        select.mSelectedIndex = value.value().get_SingleSelect().index();
        select.mValue = value.value().get_SingleSelect().value();

        JS::Rooted<JS::Value> jsval(aCx);
        if (!ToJSValue(aCx, select, &jsval) || !jsval.isObject()) {
          return NS_ERROR_FAILURE;
        }
        entry->mValue.SetAsObject() = &jsval.toObject();
        break;
      }
      case Type::TMultipleSelect: {
        JS::Rooted<JS::Value> jsval(aCx);
        if (!ToJSValue(aCx, value.value().get_MultipleSelect().valueList(),
                       &jsval) ||
            !jsval.isObject()) {
          return NS_ERROR_FAILURE;
        }
        entry->mValue.SetAsObject() = &jsval.toObject();
        break;
      }
      default:
        break;
    }

    entry->mKey = value.id();
  }

  return NS_OK;
}

static nsresult ConstructSessionStorageValue(
    const nsTArray<SSSetItemInfo>& aValues,
    Record<nsString, nsString>& aRecord) {
  auto& entries = aRecord.Entries();
  for (const auto& value : aValues) {
    auto entry = entries.AppendElement();
    entry->mKey = value.key();
    entry->mValue = value.value();
  }

  return NS_OK;
}

/* static */
nsresult SessionStoreUtils::ConstructSessionStorageValues(
    CanonicalBrowsingContext* aBrowsingContext,
    const nsTArray<SSCacheCopy>& aValues,
    Record<nsCString, Record<nsString, nsString>>& aRecord) {
  if (!aRecord.Entries().SetCapacity(aValues.Length(), fallible)) {
    return NS_ERROR_FAILURE;
  }

  // We wish to remove this step of mapping originAttributes+originKey
  // to a storage principal in Bug 1711886 by consolidating the
  // storage format in SessionStorageManagerBase and Session Store.
  nsTHashMap<nsCStringHashKey, nsIPrincipal*> storagePrincipalList;
  aBrowsingContext->PreOrderWalk([&storagePrincipalList](
                                     BrowsingContext* aContext) {
    WindowGlobalParent* windowParent =
        aContext->Canonical()->GetCurrentWindowGlobal();
    if (!windowParent) {
      return;
    }

    nsIPrincipal* storagePrincipal = windowParent->DocumentStoragePrincipal();
    if (!storagePrincipal) {
      return;
    }

    const OriginAttributes& originAttributes =
        storagePrincipal->OriginAttributesRef();
    nsAutoCString originAttributesSuffix;
    originAttributes.CreateSuffix(originAttributesSuffix);

    nsAutoCString originKey;
    storagePrincipal->GetStorageOriginKey(originKey);

    storagePrincipalList.InsertOrUpdate(originAttributesSuffix + originKey,
                                        storagePrincipal);
  });

  for (const auto& value : aValues) {
    nsIPrincipal* storagePrincipal =
        storagePrincipalList.Get(value.originAttributes() + value.originKey());
    if (!storagePrincipal) {
      continue;
    }

    auto entry = aRecord.Entries().AppendElement();

    if (!entry->mValue.Entries().SetCapacity(value.data().Length(), fallible)) {
      return NS_ERROR_FAILURE;
    }

    if (NS_FAILED(storagePrincipal->GetOrigin(entry->mKey))) {
      return NS_ERROR_FAILURE;
    }

    ConstructSessionStorageValue(value.data(), entry->mValue);
  }

  return NS_OK;
}

/* static */ void SessionStoreUtils::ResetSessionStore(
    BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(NATIVE_LISTENER);
  WindowContext* windowContext = aContext->GetCurrentWindowContext();
  if (!windowContext) {
    return;
  }

  WindowGlobalChild* windowChild = windowContext->GetWindowGlobalChild();
  if (!windowChild || !windowChild->CanSend()) {
    return;
  }

  uint32_t epoch = aContext->GetSessionStoreEpoch();

  Unused << windowChild->SendResetSessionStore(epoch);
}
