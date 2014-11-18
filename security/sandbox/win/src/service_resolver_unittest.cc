// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for ServiceResolverThunk.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/resolver.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/service_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This is the concrete resolver used to perform service-call type functions
// inside ntdll.dll.
template<typename T>
class ResolverThunkTest : public T {
 public:
  // The service resolver needs a child process to write to.
  explicit ResolverThunkTest(bool relaxed)
      : T(::GetCurrentProcess(), relaxed) {}

  // Sets the interception target to the desired address.
  void set_target(void* target) {
    fake_target_ = target;
  }

 protected:
  // Overrides Resolver::Init
  virtual NTSTATUS Init(const void* target_module,
                        const void* interceptor_module,
                        const char* target_name,
                        const char* interceptor_name,
                        const void* interceptor_entry_point,
                        void* thunk_storage,
                        size_t storage_bytes) {
    NTSTATUS ret = STATUS_SUCCESS;
    ret = ResolverThunk::Init(target_module, interceptor_module, target_name,
                              interceptor_name, interceptor_entry_point,
                              thunk_storage, storage_bytes);
    EXPECT_EQ(STATUS_SUCCESS, ret);

    target_ = fake_target_;

    return ret;
  };

 private:
  // Holds the address of the fake target.
  void* fake_target_;

  DISALLOW_COPY_AND_ASSIGN(ResolverThunkTest);
};

typedef ResolverThunkTest<sandbox::ServiceResolverThunk> WinXpResolverTest;

#if !defined(_WIN64)
typedef ResolverThunkTest<sandbox::Win8ResolverThunk> Win8ResolverTest;
typedef ResolverThunkTest<sandbox::Wow64ResolverThunk> Wow64ResolverTest;
typedef ResolverThunkTest<sandbox::Wow64W8ResolverThunk> Wow64W8ResolverTest;
#endif

const BYTE kJump32 = 0xE9;

void CheckJump(void* source, void* target) {
#pragma pack(push)
#pragma pack(1)
  struct Code {
    BYTE jump;
    ULONG delta;
  };
#pragma pack(pop)

#if defined(_WIN64)
  FAIL() << "Running 32-bit codepath";
#else
  Code* patched = reinterpret_cast<Code*>(source);
  EXPECT_EQ(kJump32, patched->jump);

  ULONG source_addr = bit_cast<ULONG>(source);
  ULONG target_addr = bit_cast<ULONG>(target);
  EXPECT_EQ(target_addr + 19 - source_addr, patched->delta);
#endif
}

NTSTATUS PatchNtdllWithResolver(const char* function, bool relaxed,
                                sandbox::ServiceResolverThunk* resolver) {
  HMODULE ntdll_base = ::GetModuleHandle(L"ntdll.dll");
  EXPECT_TRUE(NULL != ntdll_base);

  void* target = ::GetProcAddress(ntdll_base, function);
  EXPECT_TRUE(NULL != target);
  if (NULL == target)
    return STATUS_UNSUCCESSFUL;

  BYTE service[50];
  memcpy(service, target, sizeof(service));

  static_cast<WinXpResolverTest*>(resolver)->set_target(service);

  // Any pointer will do as an interception_entry_point
  void* function_entry = resolver;
  size_t thunk_size = resolver->GetThunkSize();
  scoped_ptr<char[]> thunk(new char[thunk_size]);
  size_t used;

  resolver->AllowLocalPatches();

  NTSTATUS ret = resolver->Setup(ntdll_base, NULL, function, NULL,
                                 function_entry, thunk.get(), thunk_size,
                                 &used);
  if (NT_SUCCESS(ret)) {
    EXPECT_EQ(thunk_size, used);
    EXPECT_NE(0, memcmp(service, target, sizeof(service)));
    EXPECT_NE(kJump32, service[0]);

    if (relaxed) {
      // It's already patched, let's patch again, and simulate a direct patch.
      service[0] = kJump32;
      ret = resolver->Setup(ntdll_base, NULL, function, NULL, function_entry,
                            thunk.get(), thunk_size, &used);
      CheckJump(service, thunk.get());
    }
  }

  return ret;
}

sandbox::ServiceResolverThunk* GetTestResolver(bool relaxed) {
#if defined(_WIN64)
  return new WinXpResolverTest(relaxed);
#else
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->wow64_status() == base::win::OSInfo::WOW64_ENABLED) {
    if (os_info->version() >= base::win::VERSION_WIN8)
      return new Wow64W8ResolverTest(relaxed);
    return new Wow64ResolverTest(relaxed);
  }

  if (os_info->version() >= base::win::VERSION_WIN8)
    return new Win8ResolverTest(relaxed);

  return new WinXpResolverTest(relaxed);
#endif
}

NTSTATUS PatchNtdll(const char* function, bool relaxed) {
  sandbox::ServiceResolverThunk* resolver = GetTestResolver(relaxed);

  NTSTATUS ret = PatchNtdllWithResolver(function, relaxed, resolver);
  delete resolver;
  return ret;
}

TEST(ServiceResolverTest, PatchesServices) {
  NTSTATUS ret = PatchNtdll("NtClose", false);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtClose, last error: " << ::GetLastError();

  ret = PatchNtdll("NtCreateFile", false);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtCreateFile, last error: " <<
    ::GetLastError();

  ret = PatchNtdll("NtCreateMutant", false);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtCreateMutant, last error: " <<
    ::GetLastError();

  ret = PatchNtdll("NtMapViewOfSection", false);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtMapViewOfSection, last error: " <<
    ::GetLastError();
}

TEST(ServiceResolverTest, FailsIfNotService) {
#if !defined(_WIN64)
  EXPECT_NE(STATUS_SUCCESS, PatchNtdll("RtlUlongByteSwap", false));
#endif

  EXPECT_NE(STATUS_SUCCESS, PatchNtdll("LdrLoadDll", false));
}

TEST(ServiceResolverTest, PatchesPatchedServices) {
// We don't support "relaxed mode" for Win64 apps.
#if !defined(_WIN64)
  NTSTATUS ret = PatchNtdll("NtClose", true);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtClose, last error: " << ::GetLastError();

  ret = PatchNtdll("NtCreateFile", true);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtCreateFile, last error: " <<
    ::GetLastError();

  ret = PatchNtdll("NtCreateMutant", true);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtCreateMutant, last error: " <<
    ::GetLastError();

  ret = PatchNtdll("NtMapViewOfSection", true);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtMapViewOfSection, last error: " <<
    ::GetLastError();
#endif
}

TEST(ServiceResolverTest, MultiplePatchedServices) {
// We don't support "relaxed mode" for Win64 apps.
#if !defined(_WIN64)
  sandbox::ServiceResolverThunk* resolver = GetTestResolver(true);
  NTSTATUS ret = PatchNtdllWithResolver("NtClose", true, resolver);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtClose, last error: " << ::GetLastError();

  ret = PatchNtdllWithResolver("NtCreateFile", true, resolver);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtCreateFile, last error: " <<
    ::GetLastError();

  ret = PatchNtdllWithResolver("NtCreateMutant", true, resolver);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtCreateMutant, last error: " <<
    ::GetLastError();

  ret = PatchNtdllWithResolver("NtMapViewOfSection", true, resolver);
  EXPECT_EQ(STATUS_SUCCESS, ret) << "NtMapViewOfSection, last error: " <<
    ::GetLastError();
  delete resolver;
#endif
}

TEST(ServiceResolverTest, LocalPatchesAllowed) {
  sandbox::ServiceResolverThunk* resolver = GetTestResolver(true);

  HMODULE ntdll_base = ::GetModuleHandle(L"ntdll.dll");
  ASSERT_TRUE(NULL != ntdll_base);

  const char kFunctionName[] = "NtClose";

  void* target = ::GetProcAddress(ntdll_base, kFunctionName);
  ASSERT_TRUE(NULL != target);

  BYTE service[50];
  memcpy(service, target, sizeof(service));
  static_cast<WinXpResolverTest*>(resolver)->set_target(service);

  // Any pointer will do as an interception_entry_point
  void* function_entry = resolver;
  size_t thunk_size = resolver->GetThunkSize();
  scoped_ptr<char[]> thunk(new char[thunk_size]);
  size_t used;

  NTSTATUS ret = STATUS_UNSUCCESSFUL;

  // First try patching without having allowed local patches.
  ret = resolver->Setup(ntdll_base, NULL, kFunctionName, NULL,
                        function_entry, thunk.get(), thunk_size,
                        &used);
  EXPECT_FALSE(NT_SUCCESS(ret));

  // Now allow local patches and check that things work.
  resolver->AllowLocalPatches();
  ret = resolver->Setup(ntdll_base, NULL, kFunctionName, NULL,
                        function_entry, thunk.get(), thunk_size,
                        &used);
  EXPECT_EQ(STATUS_SUCCESS, ret);
}

}  // namespace
