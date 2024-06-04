/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/SpinEventLoopUntil.h"

#include "nsIFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"

#include "buildid.h"
#include "buildid_section.h"
#include "mozilla/toolkit/library/buildid_reader_ffi.h"

#include "nsXPCOMPrivate.h"
#include "nsAppRunner.h"

#include <string>

using namespace mozilla;

#define WAIT_FOR_EVENTS \
  SpinEventLoopUntil("BuildIDReader::emptyUtil"_ns, [&]() { return done; });

#if defined(XP_WIN)
#  define BROKEN_XUL_DLL u"xul_broken_buildid.dll"_ns
#  define CORRECT_XUL_DLL u"xul_correct_buildid.dll"_ns
#  define MISSING_XUL_DLL u"xul_missing_buildid.dll"_ns
#elif defined(XP_MACOSX)
#  define BROKEN_XUL_DLL u"libxul_broken_buildid.dylib"_ns
#  define CORRECT_XUL_DLL u"libxul_correct_buildid.dylib"_ns
#  define MISSING_XUL_DLL u"libxul_missing_buildid.dylib"_ns
#else
#  define BROKEN_XUL_DLL u"libxul_broken_buildid.so"_ns
#  define CORRECT_XUL_DLL u"libxul_correct_buildid.so"_ns
#  define MISSING_XUL_DLL u"libxul_missing_buildid.so"_ns
#endif

class BuildIDReader : public ::testing::Test {
 public:
  nsresult getLib(const nsString& lib, nsAutoString& val) {
    nsresult rv;
    nsCOMPtr<nsIFile> file
#if defined(ANDROID)
        = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    rv = file->InitWithPath(u"/data/local/tmp/test_root/"_ns);
#else
        ;
    rv = NS_GetSpecialDirectory(NS_GRE_BIN_DIR, getter_AddRefs(file));
#endif
    if (!NS_SUCCEEDED(rv)) {
      return rv;
    }

    rv = file->Append(u"gtest"_ns);
    if (!NS_SUCCEEDED(rv)) {
      return rv;
    }
    rv = file->Append(lib);
    if (!NS_SUCCEEDED(rv)) {
      return rv;
    }

    rv = file->GetPath(val);
    return rv;
  }

  void testFromLib() {}
};

TEST_F(BuildIDReader, ReadCorrectBuildIdFromLib) {
  nsAutoString xul;
  nsresult rv = getLib(CORRECT_XUL_DLL, xul);
  ASSERT_TRUE(NS_SUCCEEDED(rv))
  << "Unable to find correct lib "
  << NS_ConvertUTF16toUTF8(CORRECT_XUL_DLL).get();

  nsCString installedBuildID;
  nsCString sectionName(MOZ_BUILDID_SECTION_NAME);
  rv = read_toolkit_buildid_from_file(&xul, &sectionName, &installedBuildID);
  ASSERT_TRUE(NS_SUCCEEDED(rv))
  << "Error reading from " << NS_ConvertUTF16toUTF8(CORRECT_XUL_DLL).get()
  << ": " << std::hex << static_cast<uint32_t>(rv);

  EXPECT_EQ(installedBuildID, "12341201987654"_ns);
}

TEST_F(BuildIDReader, ReadIncorrectBuildIdFromLib) {
  nsAutoString xul;
  nsresult rv = getLib(BROKEN_XUL_DLL, xul);
  ASSERT_TRUE(NS_SUCCEEDED(rv))
  << "Unable to find correct lib "
  << NS_ConvertUTF16toUTF8(CORRECT_XUL_DLL).get();

  nsCString installedBuildID;
  nsCString sectionName(MOZ_BUILDID_SECTION_NAME);
  rv = read_toolkit_buildid_from_file(&xul, &sectionName, &installedBuildID);
  ASSERT_TRUE(NS_SUCCEEDED(rv))
  << "Error reading from " << NS_ConvertUTF16toUTF8(CORRECT_XUL_DLL).get()
  << ": " << std::hex << static_cast<uint32_t>(rv);

  EXPECT_EQ(installedBuildID, "12345678765428Y38AA76"_ns);
}

TEST_F(BuildIDReader, ReadMissingBuildIdFromLib) {
  nsAutoString xul;
  nsresult rv = getLib(MISSING_XUL_DLL, xul);
  ASSERT_TRUE(NS_SUCCEEDED(rv))
  << "Unable to find correct lib "
  << NS_ConvertUTF16toUTF8(CORRECT_XUL_DLL).get();

  nsCString installedBuildID;
  nsCString sectionName(MOZ_BUILDID_SECTION_NAME);
  rv = read_toolkit_buildid_from_file(&xul, &sectionName, &installedBuildID);
  ASSERT_FALSE(NS_SUCCEEDED(rv))
  << "No error reading from " << NS_ConvertUTF16toUTF8(MISSING_XUL_DLL).get()
  << ": " << std::hex << static_cast<uint32_t>(rv);
  EXPECT_EQ(installedBuildID, nsCString(""));
  EXPECT_EQ(rv, NS_ERROR_NOT_AVAILABLE);
}

TEST_F(BuildIDReader, ReadFromMissingLib) {
  nsAutoString xul;
  nsresult rv = getLib(u"inexistent-lib.so"_ns, xul);
  ASSERT_TRUE(NS_SUCCEEDED(rv))
  << "Unable to find correct lib "
  << NS_ConvertUTF16toUTF8(CORRECT_XUL_DLL).get();

  nsCString installedBuildID;
  nsCString sectionName(MOZ_BUILDID_SECTION_NAME);
  rv = read_toolkit_buildid_from_file(&xul, &sectionName, &installedBuildID);
  ASSERT_FALSE(NS_SUCCEEDED(rv))
  << "No error reading from " << NS_ConvertUTF16toUTF8(MISSING_XUL_DLL).get()
  << ": " << std::hex << static_cast<uint32_t>(rv);
  EXPECT_EQ(rv, NS_ERROR_INVALID_ARG);
}

TEST_F(BuildIDReader, ReadFromRealLib) {
  nsAutoString xul;
  nsresult rv = getLib(XUL_DLL u""_ns, xul);
  ASSERT_TRUE(NS_SUCCEEDED(rv))
  << "Unable to find correct lib "
  << NS_ConvertUTF16toUTF8(XUL_DLL u""_ns).get();

  nsCString installedBuildID;
  nsCString realMozBuildID(std::to_string(MOZ_BUILDID).c_str());
  ASSERT_TRUE(realMozBuildID.Length() == 14);

  nsCString sectionName(MOZ_BUILDID_SECTION_NAME);
  rv = read_toolkit_buildid_from_file(&xul, &sectionName, &installedBuildID);
  ASSERT_TRUE(NS_SUCCEEDED(rv))
  << "Error reading from " << NS_ConvertUTF16toUTF8(XUL_DLL u""_ns).get()
  << ": " << std::hex << static_cast<uint32_t>(rv);

  EXPECT_EQ(installedBuildID, realMozBuildID);
}

TEST_F(BuildIDReader, ReadFromNSAppRunner) {
  nsAutoString xul;
  nsresult rv = getLib(XUL_DLL u""_ns, xul);
  ASSERT_TRUE(NS_SUCCEEDED(rv))
  << "Unable to find correct lib "
  << NS_ConvertUTF16toUTF8(XUL_DLL u""_ns).get();

  nsCString installedBuildID;
  nsCString sectionName(MOZ_BUILDID_SECTION_NAME);
  rv = read_toolkit_buildid_from_file(&xul, &sectionName, &installedBuildID);
  ASSERT_TRUE(NS_SUCCEEDED(rv))
  << "Error reading from " << NS_ConvertUTF16toUTF8(XUL_DLL u""_ns).get()
  << ": " << std::hex << static_cast<uint32_t>(rv);

  nsCString realMozBuildID(PlatformBuildID());
  ASSERT_TRUE(realMozBuildID.Length() == 14);

  EXPECT_EQ(installedBuildID, realMozBuildID);
}
