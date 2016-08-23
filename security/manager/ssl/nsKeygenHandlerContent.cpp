/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 sts=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsKeygenHandlerContent.h"

#include "nsIFormProcessor.h"
#include "nsString.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Unused.h"

#include "keythi.h"
#include "nss.h"
#include "secmodt.h"
#include "nsKeygenHandler.h"

using mozilla::dom::ContentChild;
using mozilla::Unused;

NS_IMPL_ISUPPORTS(nsKeygenFormProcessorContent, nsIFormProcessor)

nsKeygenFormProcessorContent::nsKeygenFormProcessorContent()
{
}

nsKeygenFormProcessorContent::~nsKeygenFormProcessorContent()
{
}

nsresult
nsKeygenFormProcessorContent::ProcessValue(nsIDOMHTMLElement* aElement,
                                           const nsAString& aName,
                                           nsAString& aValue)
{
  nsAutoString challengeValue;
  nsAutoString keyTypeValue;
  nsAutoString keyParamsValue;
  nsKeygenFormProcessor::ExtractParams(aElement, challengeValue, keyTypeValue, keyParamsValue);

  ContentChild* child = ContentChild::GetSingleton();

  nsString oldValue(aValue);
  nsString newValue;
  Unused << child->SendKeygenProcessValue(oldValue, challengeValue,
                                          keyTypeValue, keyParamsValue,
                                          &newValue);

  aValue.Assign(newValue);
  return NS_OK;
}

nsresult
nsKeygenFormProcessorContent::ProcessValueIPC(const nsAString& aOldValue,
                                              const nsAString& aChallenge,
                                              const nsAString& aKeyType,
                                              const nsAString& aKeyParams,
                                              nsAString& aNewValue)
{
  MOZ_ASSERT(false, "should never be called in the child process");
  return NS_ERROR_UNEXPECTED;
}

nsresult
nsKeygenFormProcessorContent::ProvideContent(const nsAString& aFormType,
                                             nsTArray<nsString>& aContent,
                                             nsAString& aAttribute)
{
  nsString attribute;
  Unused <<
    ContentChild::GetSingleton()->SendKeygenProvideContent(&attribute,
                                                           &aContent);
  aAttribute.Assign(attribute);
  return NS_OK;
}
