/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/JSON.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/WindowProxyHolder.h"
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

/* static */ void SessionStoreUtils::ForEachNonDynamicChildFrame(
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
  aRv = docShell->GetChildCount(&length);
  if (aRv.Failed()) {
    return;
  }

  for (int32_t i = 0; i < length; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    docShell->GetChildAt(i, getter_AddRefs(item));
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

/* static */ already_AddRefed<nsISupports>
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

/* static */ void SessionStoreUtils::RemoveDynamicFrameFilteredListener(
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

/* static */ void SessionStoreUtils::CollectDocShellCapabilities(
    const GlobalObject& aGlobal, nsIDocShell* aDocShell, nsCString& aRetVal) {
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

/* static */ void SessionStoreUtils::RestoreDocShellCapabilities(
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

/* static */ void SessionStoreUtils::CollectScrollPosition(
    const GlobalObject& aGlobal, Document& aDocument,
    SSScrollPositionDict& aRetVal) {
  nsIPresShell* presShell = aDocument.GetShell();
  if (!presShell) {
    return;
  }

  nsPoint scrollPos = presShell->GetVisualViewportOffset();
  int scrollX = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x);
  int scrollY = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y);

  if ((scrollX != 0) || (scrollY != 0)) {
    aRetVal.mScroll.Construct() = nsPrintfCString("%d,%d", scrollX, scrollY);
  }
}

/* static */ void SessionStoreUtils::RestoreScrollPosition(
    const GlobalObject& aGlobal, nsGlobalWindowInner& aWindow,
    const SSScrollPositionDict& aData) {
  if (!aData.mScroll.WasPassed()) {
    return;
  }

  nsCCharSeparatedTokenizer tokenizer(aData.mScroll.Value(), ',');
  nsAutoCString token(tokenizer.nextToken());
  int pos_X = atoi(token.get());
  token = tokenizer.nextToken();
  int pos_Y = atoi(token.get());
  aWindow.ScrollTo(pos_X, pos_Y);
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

// A helper function to append a element into mId or mXpath of CollectedFormData
static Record<nsString, OwningStringOrBooleanOrLongOrObject>::EntryType*
AppendEntryToCollectedData(nsINode* aNode, const nsAString& aId,
                           uint16_t& aGeneratedCount,
                           CollectedFormData& aRetVal) {
  Record<nsString, OwningStringOrBooleanOrLongOrObject>::EntryType* entry;
  if (!aId.IsEmpty()) {
    if (!aRetVal.mId.WasPassed()) {
      aRetVal.mId.Construct();
    }
    auto& recordEntries = aRetVal.mId.Value().Entries();
    entry = recordEntries.AppendElement();
    entry->mKey = aId;
  } else {
    if (!aRetVal.mXpath.WasPassed()) {
      aRetVal.mXpath.Construct();
    }
    auto& recordEntries = aRetVal.mXpath.Value().Entries();
    entry = recordEntries.AppendElement();
    nsAutoString xpath;
    aNode->GenerateXPath(xpath);
    aGeneratedCount++;
    entry->mKey = xpath;
  }
  return entry;
}

/*
  @param aDocument: DOMDocument instance to obtain form data for.
  @param aGeneratedCount: the current number of XPath expressions in the
                          returned object.
  @return aRetVal: Form data encoded in an object.
 */
static void CollectFromTextAreaElement(Document& aDocument,
                                       uint16_t& aGeneratedCount,
                                       CollectedFormData& aRetVal) {
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
    nsAutoString value;
    textArea->GetValue(value);
    // In order to reduce XPath generation (which is slow), we only save data
    // for form fields that have been changed. (cf. bug 537289)
    if (textArea->AttrValueIs(kNameSpaceID_None, nsGkAtoms::value, value,
                              eCaseMatters)) {
      continue;
    }
    Record<nsString, OwningStringOrBooleanOrLongOrObject>::EntryType* entry =
        AppendEntryToCollectedData(textArea, id, aGeneratedCount, aRetVal);
    entry->mValue.SetAsString() = value;
  }
}

/*
  @param aDocument: DOMDocument instance to obtain form data for.
  @param aGeneratedCount: the current number of XPath expressions in the
                          returned object.
  @return aRetVal: Form data encoded in an object.
 */
static void CollectFromInputElement(JSContext* aCx, Document& aDocument,
                                    uint16_t& aGeneratedCount,
                                    CollectedFormData& aRetVal) {
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
    nsAutoString value;
    if (input->ControlType() == NS_FORM_INPUT_CHECKBOX ||
        input->ControlType() == NS_FORM_INPUT_RADIO) {
      bool checked = input->Checked();
      if (checked == input->DefaultChecked()) {
        continue;
      }
      Record<nsString, OwningStringOrBooleanOrLongOrObject>::EntryType* entry =
          AppendEntryToCollectedData(input, id, aGeneratedCount, aRetVal);
      entry->mValue.SetAsBoolean() = checked;
    } else if (input->ControlType() == NS_FORM_INPUT_FILE) {
      IgnoredErrorResult rv;
      nsTArray<nsString> result;
      input->MozGetFileNameArray(result, rv);
      if (rv.Failed() || result.Length() == 0) {
        continue;
      }
      CollectedFileListValue val;
      val.mType = NS_LITERAL_STRING("file");
      val.mFileList.SwapElements(result);

      JS::Rooted<JS::Value> jsval(aCx);
      if (!ToJSValue(aCx, val, &jsval)) {
        JS_ClearPendingException(aCx);
        continue;
      }
      Record<nsString, OwningStringOrBooleanOrLongOrObject>::EntryType* entry =
          AppendEntryToCollectedData(input, id, aGeneratedCount, aRetVal);
      entry->mValue.SetAsObject() = &jsval.toObject();
    } else {
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
      if (!id.IsEmpty()) {
        // We want to avoid saving data for about:sessionrestore as a string.
        // Since it's stored in the form as stringified JSON, stringifying
        // further causes an explosion of escape characters. cf. bug 467409
        if (id.EqualsLiteral("sessionData")) {
          nsAutoCString url;
          Unused << aDocument.GetDocumentURI()->GetSpecIgnoringRef(url);
          if (url.EqualsLiteral("about:sessionrestore") ||
              url.EqualsLiteral("about:welcomeback")) {
            JS::Rooted<JS::Value> jsval(aCx);
            if (JS_ParseJSON(aCx, value.get(), value.Length(), &jsval) &&
                jsval.isObject()) {
              Record<nsString, OwningStringOrBooleanOrLongOrObject>::EntryType*
                  entry = AppendEntryToCollectedData(input, id, aGeneratedCount,
                                                     aRetVal);
              entry->mValue.SetAsObject() = &jsval.toObject();
            } else {
              JS_ClearPendingException(aCx);
            }
            continue;
          }
        }
      }
      Record<nsString, OwningStringOrBooleanOrLongOrObject>::EntryType* entry =
          AppendEntryToCollectedData(input, id, aGeneratedCount, aRetVal);
      entry->mValue.SetAsString() = value;
    }
  }
}

/*
  @param aDocument: DOMDocument instance to obtain form data for.
  @param aGeneratedCount: the current number of XPath expressions in the
                          returned object.
  @return aRetVal: Form data encoded in an object.
 */
static void CollectFromSelectElement(JSContext* aCx, Document& aDocument,
                                     uint16_t& aGeneratedCount,
                                     CollectedFormData& aRetVal) {
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

      JS::Rooted<JS::Value> jsval(aCx);
      if (!ToJSValue(aCx, val, &jsval)) {
        JS_ClearPendingException(aCx);
        continue;
      }
      Record<nsString, OwningStringOrBooleanOrLongOrObject>::EntryType* entry =
          AppendEntryToCollectedData(select, id, aGeneratedCount, aRetVal);
      entry->mValue.SetAsObject() = &jsval.toObject();
    } else {
      // <select>s with the multiple attribute are easier to determine the
      // default value since each <option> has a defaultSelected property
      HTMLOptionsCollection* options = select->GetOptions();
      if (!options) {
        continue;
      }
      bool hasDefaultValue = true;
      nsTArray<nsString> selectslist;
      int numOptions = options->Length();
      for (int idx = 0; idx < numOptions; idx++) {
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
      JS::Rooted<JS::Value> jsval(aCx);
      if (!ToJSValue(aCx, selectslist, &jsval)) {
        JS_ClearPendingException(aCx);
        continue;
      }
      Record<nsString, OwningStringOrBooleanOrLongOrObject>::EntryType* entry =
          AppendEntryToCollectedData(select, id, aGeneratedCount, aRetVal);
      entry->mValue.SetAsObject() = &jsval.toObject();
    }
  }
}

/*
  @param aDocument: DOMDocument instance to obtain form data for.
  @return aRetVal: Form data encoded in an object.
 */
static void CollectFromXULTextbox(Document& aDocument,
                                  CollectedFormData& aRetVal) {
  nsAutoCString url;
  Unused << aDocument.GetDocumentURI()->GetSpecIgnoringRef(url);
  if (!url.EqualsLiteral("about:config")) {
    return;
  }
  RefPtr<nsContentList> aboutConfigElements = NS_GetContentList(
      &aDocument, kNameSpaceID_XUL, NS_LITERAL_STRING("window"));
  uint32_t length = aboutConfigElements->Length(true);
  for (uint32_t i = 0; i < length; ++i) {
    MOZ_ASSERT(aboutConfigElements->Item(i), "null item in node list!");
    nsAutoString id, value;
    aboutConfigElements->Item(i)->AsElement()->GetId(id);
    if (!id.EqualsLiteral("config")) {
      continue;
    }
    RefPtr<nsContentList> textboxs =
        NS_GetContentList(aboutConfigElements->Item(i)->AsElement(),
                          kNameSpaceID_XUL, NS_LITERAL_STRING("textbox"));
    uint32_t boxsLength = textboxs->Length(true);
    for (uint32_t idx = 0; idx < boxsLength; idx++) {
      textboxs->Item(idx)->AsElement()->GetId(id);
      if (!id.EqualsLiteral("textbox")) {
        continue;
      }
      RefPtr<HTMLInputElement> input = HTMLInputElement::FromNode(
          nsFocusManager::GetRedirectedFocus(textboxs->Item(idx)));
      if (!input) {
        continue;
      }
      input->GetValue(value, CallerType::System);
      if (value.IsEmpty() ||
          input->AttrValueIs(kNameSpaceID_None, nsGkAtoms::value, value,
                             eCaseMatters)) {
        continue;
      }
      uint16_t generatedCount = 0;
      Record<nsString, OwningStringOrBooleanOrLongOrObject>::EntryType* entry =
          AppendEntryToCollectedData(input, id, generatedCount, aRetVal);
      entry->mValue.SetAsString() = value;
      return;
    }
  }
}

/* static */ void SessionStoreUtils::CollectFormData(
    const GlobalObject& aGlobal, Document& aDocument,
    CollectedFormData& aRetVal) {
  uint16_t generatedCount = 0;
  /* textarea element */
  CollectFromTextAreaElement(aDocument, generatedCount, aRetVal);
  /* input element */
  CollectFromInputElement(aGlobal.Context(), aDocument, generatedCount,
                          aRetVal);
  /* select element */
  CollectFromSelectElement(aGlobal.Context(), aDocument, generatedCount,
                           aRetVal);
  /* special case for about:config's search field */
  CollectFromXULTextbox(aDocument, aRetVal);

  Element* bodyElement = aDocument.GetBody();
  if (aDocument.HasFlag(NODE_IS_EDITABLE) && bodyElement) {
    bodyElement->GetInnerHTML(aRetVal.mInnerHTML.Construct(), IgnoreErrors());
  }
  if (!aRetVal.mId.WasPassed() && !aRetVal.mXpath.WasPassed() &&
      !aRetVal.mInnerHTML.WasPassed()) {
    return;
  }
  // Store the frame's current URL with its form data so that we can compare
  // it when restoring data to not inject form data into the wrong document.
  nsIURI* uri = aDocument.GetDocumentURI();
  if (uri) {
    uri->GetSpecIgnoringRef(aRetVal.mUrl.Construct());
  }
}
