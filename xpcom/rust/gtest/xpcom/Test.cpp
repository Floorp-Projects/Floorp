/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"

extern "C" nsIObserverService* Rust_ObserveFromRust();

TEST(RustXpcom, ObserverFromRust)
{
  nsCOMPtr<nsIObserverService> rust = Rust_ObserveFromRust();
  nsCOMPtr<nsIObserverService> cpp = mozilla::services::GetObserverService();
  EXPECT_EQ(rust, cpp);
}

extern "C" void Rust_ImplementRunnableInRust(bool* aItWorked,
                                             nsIRunnable** aRunnable);

TEST(RustXpcom, ImplementRunnableInRust)
{
  bool itWorked = false;
  nsCOMPtr<nsIRunnable> runnable;
  Rust_ImplementRunnableInRust(&itWorked, getter_AddRefs(runnable));

  EXPECT_TRUE(runnable);
  EXPECT_FALSE(itWorked);
  runnable->Run();
  EXPECT_TRUE(itWorked);
}
