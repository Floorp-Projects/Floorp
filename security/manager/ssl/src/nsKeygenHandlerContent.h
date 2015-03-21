/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 sts=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsKeygenHandlerContent_h
#define nsKeygenHandlerContent_h

#include "mozilla/Attributes.h"
#include "nsIFormProcessor.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

class nsIDOMHTMLElement;

class nsKeygenFormProcessorContent final : public nsIFormProcessor {
public:
  nsKeygenFormProcessorContent();

  virtual nsresult ProcessValue(nsIDOMHTMLElement* aElement,
                                const nsAString& aName,
                                nsAString& aValue) override;

  virtual nsresult ProcessValueIPC(const nsAString& aOldValue,
                                   const nsAString& aChallenge,
                                   const nsAString& aKeyType,
                                   const nsAString& aKeyParams,
                                   nsAString& aNewValue) override;

  virtual nsresult ProvideContent(const nsAString& aFormType,
                                  nsTArray<nsString>& aContent,
                                  nsAString& aAttribute) override;

  NS_DECL_ISUPPORTS

protected:
  ~nsKeygenFormProcessorContent();
};

#endif // nsKeygenHandlerContent_h
