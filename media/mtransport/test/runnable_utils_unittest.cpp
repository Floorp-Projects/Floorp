/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com
#include <iostream>

#include "prio.h"

#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsXPCOM.h"

#include "mozilla/RefPtr.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsISocketTransportService.h"

#include "nsASocketHandler.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "runnable_utils.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;

namespace {

class Destructor {
 private:
  ~Destructor() {
    std::cerr << "Destructor called" << std::endl;
    *destroyed_ = true;
  }
 public:
  explicit Destructor(bool* destroyed) : destroyed_(destroyed) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Destructor)

 private:
  bool *destroyed_;
};

class TargetClass {
 public:
  explicit TargetClass(int *ran) : ran_(ran) {}

  void m1(int x) {
    std::cerr << __FUNCTION__ << " " << x << std::endl;
    *ran_ = 1;
  }

  void m2(int x, int y) {
    std::cerr << __FUNCTION__ << " " << x << " " << y << std::endl;
    *ran_ = 2;
  }

  void m1set(bool *z) {
    std::cerr << __FUNCTION__ << std::endl;
    *z = true;
  }
  int return_int(int x) {
    std::cerr << __FUNCTION__ << std::endl;
    return x;
  }
  void destructor_target(Destructor*) {
  }

  void destructor_target_ref(RefPtr<Destructor> destructor) {
  }

  int *ran_;
};


class RunnableArgsTest : public MtransportTest {
 public:
  RunnableArgsTest() : MtransportTest(), ran_(0), cl_(&ran_){}

  void Test1Arg() {
    Runnable * r = WrapRunnable(&cl_, &TargetClass::m1, 1);
    r->Run();
    ASSERT_EQ(1, ran_);
  }

  void Test2Args() {
    Runnable* r = WrapRunnable(&cl_, &TargetClass::m2, 1, 2);
    r->Run();
    ASSERT_EQ(2, ran_);
  }

 private:
  int ran_;
  TargetClass cl_;
};

class DispatchTest : public MtransportTest {
 public:
  DispatchTest() : MtransportTest(), ran_(0), cl_(&ran_) {}

  void SetUp() {
    MtransportTest::SetUp();

    nsresult rv;
    target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
  }

  void Test1Arg() {
    Runnable* r = WrapRunnable(&cl_, &TargetClass::m1, 1);
    target_->Dispatch(r, NS_DISPATCH_SYNC);
    ASSERT_EQ(1, ran_);
  }

  void Test2Args() {
    Runnable* r = WrapRunnable(&cl_, &TargetClass::m2, 1, 2);
    target_->Dispatch(r, NS_DISPATCH_SYNC);
    ASSERT_EQ(2, ran_);
  }

  void Test1Set() {
    bool x = false;
    target_->Dispatch(WrapRunnable(&cl_, &TargetClass::m1set, &x),
                      NS_DISPATCH_SYNC);
    ASSERT_TRUE(x);
  }

  void TestRet() {
    int z;
    int x = 10;

    target_->Dispatch(WrapRunnableRet(&z, &cl_, &TargetClass::return_int, x),
                      NS_DISPATCH_SYNC);
    ASSERT_EQ(10, z);
  }

 protected:
  int ran_;
  TargetClass cl_;
  nsCOMPtr<nsIEventTarget> target_;
};


TEST_F(RunnableArgsTest, OneArgument) {
  Test1Arg();
}

TEST_F(RunnableArgsTest, TwoArguments) {
  Test2Args();
}

TEST_F(DispatchTest, OneArgument) {
  Test1Arg();
}

TEST_F(DispatchTest, TwoArguments) {
  Test2Args();
}

TEST_F(DispatchTest, Test1Set) {
  Test1Set();
}

TEST_F(DispatchTest, TestRet) {
  TestRet();
}

void SetNonMethod(TargetClass *cl, int x) {
  cl->m1(x);
}

int SetNonMethodRet(TargetClass *cl, int x) {
  cl->m1(x);

  return x;
}

TEST_F(DispatchTest, TestNonMethod) {
  test_utils_->sts_target()->Dispatch(
      WrapRunnableNM(SetNonMethod, &cl_, 10), NS_DISPATCH_SYNC);

  ASSERT_EQ(1, ran_);
}

TEST_F(DispatchTest, TestNonMethodRet) {
  int z;

  test_utils_->sts_target()->Dispatch(
      WrapRunnableNMRet(&z, SetNonMethodRet, &cl_, 10), NS_DISPATCH_SYNC);

  ASSERT_EQ(1, ran_);
  ASSERT_EQ(10, z);
}

TEST_F(DispatchTest, TestDestructor) {
  bool destroyed = false;
  RefPtr<Destructor> destructor = new Destructor(&destroyed);
  target_->Dispatch(WrapRunnable(&cl_, &TargetClass::destructor_target,
                                 destructor),
                    NS_DISPATCH_SYNC);
  ASSERT_FALSE(destroyed);
  destructor = nullptr;
  ASSERT_TRUE(destroyed);
}

TEST_F(DispatchTest, TestDestructorRef) {
  bool destroyed = false;
  RefPtr<Destructor> destructor = new Destructor(&destroyed);
  target_->Dispatch(WrapRunnable(&cl_, &TargetClass::destructor_target_ref,
                                 destructor),
                    NS_DISPATCH_SYNC);
  ASSERT_FALSE(destroyed);
  destructor = nullptr;
  ASSERT_TRUE(destroyed);
}


} // end of namespace
