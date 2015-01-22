// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for the sid class.

#define _ATL_NO_EXCEPTIONS
#include <atlbase.h>
#include <atlsecurity.h>

#include "sandbox/win/src/sid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Calls ::EqualSid. This function exists only to simplify the calls to
// ::EqualSid by removing the need to cast the input params.
BOOL EqualSid(const SID *sid1, const SID *sid2) {
  return ::EqualSid(const_cast<SID*>(sid1), const_cast<SID*>(sid2));
}

// Tests the creation if a Sid
TEST(SidTest, Constructors) {
  ATL::CSid sid_world = ATL::Sids::World();
  SID *sid_world_pointer = const_cast<SID*>(sid_world.GetPSID());

  // Check the SID* constructor
  Sid sid_sid_star(sid_world_pointer);
  ASSERT_TRUE(EqualSid(sid_world_pointer, sid_sid_star.GetPSID()));

  // Check the copy constructor
  Sid sid_copy(sid_sid_star);
  ASSERT_TRUE(EqualSid(sid_world_pointer, sid_copy.GetPSID()));

  // Note that the WELL_KNOWN_SID_TYPE constructor is tested in the GetPSID
  // test.
}

// Tests the method GetPSID
TEST(SidTest, GetPSID) {
  // Check for non-null result;
  ASSERT_NE(static_cast<SID*>(NULL), Sid(::WinLocalSid).GetPSID());
  ASSERT_NE(static_cast<SID*>(NULL), Sid(::WinCreatorOwnerSid).GetPSID());
  ASSERT_NE(static_cast<SID*>(NULL), Sid(::WinBatchSid).GetPSID());

  ASSERT_TRUE(EqualSid(Sid(::WinNullSid).GetPSID(),
                       ATL::Sids::Null().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinWorldSid).GetPSID(),
                       ATL::Sids::World().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinDialupSid).GetPSID(),
                       ATL::Sids::Dialup().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinNetworkSid).GetPSID(),
                       ATL::Sids::Network().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinBuiltinAdministratorsSid).GetPSID(),
                       ATL::Sids::Admins().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinBuiltinUsersSid).GetPSID(),
                       ATL::Sids::Users().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinBuiltinGuestsSid).GetPSID(),
                       ATL::Sids::Guests().GetPSID()));

  ASSERT_TRUE(EqualSid(Sid(::WinProxySid).GetPSID(),
                       ATL::Sids::Proxy().GetPSID()));
}

}  // namespace sandbox
