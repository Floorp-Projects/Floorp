/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsPrintingProxy_h
#define __nsPrintingProxy_h

#include "nsIPrintingPromptService.h"
#include "mozilla/embedding/PPrintingChild.h"

namespace mozilla {
namespace layout {
class PRemotePrintJobChild;
}
}  // namespace mozilla

class nsPrintingProxy final : public nsISupports,
                              public mozilla::embedding::PPrintingChild {
  friend class mozilla::embedding::PPrintingChild;

 public:
  static already_AddRefed<nsPrintingProxy> GetInstance();

  NS_DECL_ISUPPORTS

 protected:
  already_AddRefed<PRemotePrintJobChild> AllocPRemotePrintJobChild() final;

 private:
  nsPrintingProxy();

  ~nsPrintingProxy() final;

  nsresult Init();
};

#endif
