/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsIObserver.h"
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

extern "C" void Rust_GetMultipleInterfaces(nsIRunnable** aRunnable,
                                           nsIObserver** aObserver);

TEST(RustXpcom, DynamicCastVoid)
{
  nsCOMPtr<nsIRunnable> runnable;
  nsCOMPtr<nsIObserver> observer;
  Rust_GetMultipleInterfaces(getter_AddRefs(runnable),
                             getter_AddRefs(observer));

  // They should have different addresses when `static_cast` to void*
  EXPECT_NE(static_cast<void*>(runnable.get()),
            static_cast<void*>(observer.get()));

  // These should be the same object
  nsCOMPtr<nsISupports> runnableSupports = do_QueryInterface(runnable);
  nsCOMPtr<nsISupports> observerSupports = do_QueryInterface(observer);
  EXPECT_EQ(runnableSupports.get(), observerSupports.get());

#ifndef XP_WIN
  // They should have the same address when dynamic_cast to void*
  // dynamic_cast<void*> is not supported without rtti on windows.
  EXPECT_EQ(dynamic_cast<void*>(runnable.get()),
            dynamic_cast<void*>(observer.get()));

  // The nsISupports pointer from `do_QueryInterface` should match
  // `dynamic_cast<void*>`
  EXPECT_EQ(dynamic_cast<void*>(observer.get()),
            static_cast<void*>(observerSupports.get()));
#endif
}
