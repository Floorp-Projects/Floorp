/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MockDragServiceController_h__
#define MockDragServiceController_h__

#include "nsIMockDragServiceController.h"

namespace mozilla::test {

class MockDragService;

class MockDragServiceController : public nsIMockDragServiceController {
 public:
  MockDragServiceController();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOCKDRAGSERVICECONTROLLER

 private:
  virtual ~MockDragServiceController();
  RefPtr<MockDragService> mDragService;
};

}  // namespace mozilla::test

#endif  // MockDragServiceController_h__
