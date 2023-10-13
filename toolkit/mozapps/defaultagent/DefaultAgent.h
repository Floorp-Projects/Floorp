/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_AGENT_DEFAULT_AGENT_H__
#define __DEFAULT_BROWSER_AGENT_DEFAULT_AGENT_H__

#include "nsIDefaultAgent.h"

namespace mozilla::default_agent {

class DefaultAgent final : public nsIDefaultAgent {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDEFAULTAGENT

  DefaultAgent() = default;

 private:
  // A private destructor must be declared.
  ~DefaultAgent() = default;
};

}  // namespace mozilla::default_agent

#endif  // __DEFAULT_BROWSER_AGENT_DEFAULT_AGENT_H__
