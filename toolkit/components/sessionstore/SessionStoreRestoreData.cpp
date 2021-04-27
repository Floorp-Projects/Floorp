/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

namespace mozilla {
namespace dom {

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/sessionstore/SessionStoreTypes.h"
#include "nsISessionStoreRestoreData.h"

bool SessionStoreRestoreData::IsEmpty() {
  return (!mURI && mScroll.IsEmpty() && mInnerHTML.IsEmpty() &&
          mEntries.IsEmpty() && mChildren.IsEmpty());
}

SessionStoreRestoreData* SessionStoreRestoreData::FindDataForChild(
    BrowsingContext* aContext) {
  nsTArray<uint32_t> offsets;
  for (const BrowsingContext* current = aContext;
       current && current->GetParent(); current = current->GetParent()) {
    // Don't bother continuing if any frame in our chain was created
    // dynamically.
    if (current->ChildOffset() < 0) {
      return nullptr;
    }
    offsets.AppendElement(current->ChildOffset());
  }

  SessionStoreRestoreData* data = this;

  for (uint32_t offset : Reversed(offsets)) {
    if (!data || data->mChildren.Length() <= offset ||
        !data->mChildren[offset] || data->mChildren[offset]->IsEmpty()) {
      return nullptr;
    }
    data = data->mChildren[offset];
  }

  return data;
}

bool SessionStoreRestoreData::CanRestoreInto(nsIURI* aDocumentURI) {
  if (!mURI) {
    // This should mean that we don't have form data. It's fine to restore this
    // data into any document — the worst that will happen is that we restore an
    // incorrect scroll position.
    MOZ_ASSERT(mEntries.IsEmpty());
    MOZ_ASSERT(mInnerHTML.IsEmpty());
    return true;
  }
  bool equalsExceptRef = false;
  return (aDocumentURI &&
          NS_SUCCEEDED(mURI->EqualsExceptRef(aDocumentURI, &equalsExceptRef)) &&
          equalsExceptRef);
}

MOZ_CAN_RUN_SCRIPT
bool SessionStoreRestoreData::RestoreInto(RefPtr<BrowsingContext> aContext) {
  if (!aContext->IsInProcess()) {
    return false;
  }

  if (WindowContext* window = aContext->GetCurrentWindowContext()) {
    if (!mScroll.IsEmpty()) {
      if (nsGlobalWindowInner* inner = window->GetInnerWindow()) {
        SessionStoreUtils::RestoreScrollPosition(*inner, mScroll);
      }
    }
    if (mURI) {
      if (nsCOMPtr<Document> doc = window->GetExtantDoc()) {
        if (!CanRestoreInto(doc->GetDocumentURI())) {
          return false;
        }
        SessionStoreUtils::RestoreFormData(*doc, mInnerHTML, mEntries);
      }
    }
  }

  return true;
}

void SessionStoreRestoreData::AddFormEntry(
    bool aIsXPath, const nsAString& aIdOrXPath,
    sessionstore::FormEntryValue aValue) {
  mEntries.AppendElement(
      Entry{sessionstore::FormEntry{nsString(aIdOrXPath), aValue}, aIsXPath});
}

NS_IMPL_ISUPPORTS(SessionStoreRestoreData, nsISessionStoreRestoreData,
                  SessionStoreRestoreData)

NS_IMETHODIMP
SessionStoreRestoreData::GetUrl(nsACString& aURL) {
  if (mURI) {
    nsresult rv = mURI->GetSpec(aURL);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::SetUrl(const nsACString& aURL) {
  nsresult rv = NS_NewURI(getter_AddRefs(mURI), aURL);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::GetInnerHTML(nsAString& aInnerHTML) {
  aInnerHTML = mInnerHTML;
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::SetInnerHTML(const nsAString& aInnerHTML) {
  mInnerHTML = aInnerHTML;
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::GetScroll(nsACString& aScroll) {
  aScroll = mScroll;
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::SetScroll(const nsACString& aScroll) {
  mScroll = aScroll;
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::AddTextField(bool aIsXPath,
                                      const nsAString& aIdOrXPath,
                                      const nsAString& aValue) {
  AddFormEntry(aIsXPath, aIdOrXPath, sessionstore::TextField{nsString(aValue)});
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::AddCheckbox(bool aIsXPath, const nsAString& aIdOrXPath,
                                     const bool aValue) {
  AddFormEntry(aIsXPath, aIdOrXPath, sessionstore::Checkbox{aValue});
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::AddFileList(bool aIsXPath, const nsAString& aIdOrXPath,
                                     const nsAString& aType,
                                     const nsTArray<nsString>& aFileList) {
  AddFormEntry(aIsXPath, aIdOrXPath, sessionstore::FileList{aFileList});
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::AddSingleSelect(bool aIsXPath,
                                         const nsAString& aIdOrXPath,
                                         uint32_t aSelectedIndex,
                                         const nsAString& aValue) {
  AddFormEntry(aIsXPath, aIdOrXPath,
               sessionstore::SingleSelect{aSelectedIndex, nsString(aValue)});
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::AddMultipleSelect(bool aIsXPath,
                                           const nsAString& aIdOrXPath,
                                           const nsTArray<nsString>& aValues) {
  AddFormEntry(aIsXPath, aIdOrXPath, sessionstore::MultipleSelect{aValues});
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreRestoreData::AddChild(nsISessionStoreRestoreData* aChild,
                                  uint32_t aIndex) {
  if (nsCOMPtr<SessionStoreRestoreData> child = do_QueryInterface(aChild)) {
    if (aIndex > mChildren.Length()) {
      mChildren.SetLength(aIndex);
    }
    mChildren.InsertElementAt(aIndex, child);
  }
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
