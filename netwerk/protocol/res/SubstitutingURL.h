/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SubstitutingURL_h
#define SubstitutingURL_h

#include "nsStandardURL.h"

class nsIIOService;

namespace mozilla {
namespace net {

// SubstitutingURL : overrides nsStandardURL::GetFile to provide nsIFile
// resolution
class SubstitutingURL : public nsStandardURL {
 public:
  virtual nsStandardURL* StartClone() override;
  [[nodiscard]] virtual nsresult EnsureFile() override;
  NS_IMETHOD GetClassIDNoAlloc(nsCID* aCID) override;

 private:
  explicit SubstitutingURL() : nsStandardURL(true) {}
  explicit SubstitutingURL(bool aSupportsFileURL) : nsStandardURL(true) {
    MOZ_ASSERT(aSupportsFileURL);
  }
  virtual nsresult Clone(nsIURI** aURI) override {
    return nsStandardURL::Clone(aURI);
  }

 public:
  class Mutator : public TemplatedMutator<SubstitutingURL> {
    NS_DECL_ISUPPORTS
   public:
    explicit Mutator() = default;

   private:
    virtual ~Mutator() = default;

    SubstitutingURL* Create() override { return new SubstitutingURL(); }
  };

  NS_IMETHOD Mutate(nsIURIMutator** aMutator) override {
    RefPtr<SubstitutingURL::Mutator> mutator = new SubstitutingURL::Mutator();
    nsresult rv = mutator->InitFromURI(this);
    if (NS_FAILED(rv)) {
      return rv;
    }
    mutator.forget(aMutator);
    return NS_OK;
  }

  NS_IMETHOD_(void) Serialize(ipc::URIParams& aParams) override;

  friend BaseURIMutator<SubstitutingURL>;
  friend TemplatedMutator<SubstitutingURL>;
};

}  // namespace net
}  // namespace mozilla

#endif /* SubstitutingURL_h */
