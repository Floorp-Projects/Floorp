/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/fileutils.h"

#include <stdio.h>

#include <list>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/test/testsupport/gtest_disable.h"

#ifdef WIN32
#define chdir _chdir
static const char* kPathDelimiter = "\\";
#else
static const char* kPathDelimiter = "/";
#endif

static const std::string kResourcesDir = "resources";
static const std::string kTestName = "fileutils_unittest";
static const std::string kExtension = "tmp";

typedef std::list<std::string> FileList;

namespace webrtc {

// Test fixture to restore the working directory between each test, since some
// of them change it with chdir during execution (not restored by the
// gtest framework).
class FileUtilsTest : public testing::Test {
 protected:
  FileUtilsTest() {
  }
  virtual ~FileUtilsTest() {}
  // Runs before the first test
  static void SetUpTestCase() {
    original_working_dir_ = webrtc::test::WorkingDir();
    std::string resources_path = original_working_dir_ + kPathDelimiter +
        kResourcesDir + kPathDelimiter;
    webrtc::test::CreateDirectory(resources_path);

    files_.push_back(resources_path + kTestName + "." + kExtension);
    files_.push_back(resources_path + kTestName + "_32." + kExtension);
    files_.push_back(resources_path + kTestName + "_64." + kExtension);
    files_.push_back(resources_path + kTestName + "_linux." + kExtension);
    files_.push_back(resources_path + kTestName + "_mac." + kExtension);
    files_.push_back(resources_path + kTestName + "_win." + kExtension);
    files_.push_back(resources_path + kTestName + "_linux_32." + kExtension);
    files_.push_back(resources_path + kTestName + "_mac_32." + kExtension);
    files_.push_back(resources_path + kTestName + "_win_32." + kExtension);
    files_.push_back(resources_path + kTestName + "_linux_64." + kExtension);
    files_.push_back(resources_path + kTestName + "_mac_64." + kExtension);
    files_.push_back(resources_path + kTestName + "_win_64." + kExtension);

    // Now that the resources dir exists, write some empty test files into it.
    for (FileList::iterator file_it = files_.begin();
        file_it != files_.end(); ++file_it) {
      FILE* file = fopen(file_it->c_str(), "wb");
      ASSERT_TRUE(file != NULL) << "Failed to write file: " << file_it->c_str();
      ASSERT_GT(fprintf(file, "%s",  "Dummy data"), 0);
      fclose(file);
    }
  }
  static void TearDownTestCase() {
    // Clean up all resource files written
    for (FileList::iterator file_it = files_.begin();
            file_it != files_.end(); ++file_it) {
      remove(file_it->c_str());
    }
  }
  void SetUp() {
    ASSERT_EQ(chdir(original_working_dir_.c_str()), 0);
  }
  void TearDown() {
    ASSERT_EQ(chdir(original_working_dir_.c_str()), 0);
  }
 protected:
  static FileList files_;
 private:
  static std::string original_working_dir_;
};

FileList FileUtilsTest::files_;
std::string FileUtilsTest::original_working_dir_ = "";

// Tests that the project root path is returned for the default working
// directory that is automatically set when the test executable is launched.
// The test is not fully testing the implementation, since we cannot be sure
// of where the executable was launched from.
TEST_F(FileUtilsTest, ProjectRootPath) {
  std::string project_root = webrtc::test::ProjectRootPath();
  // Not very smart, but at least tests something.
  ASSERT_GT(project_root.length(), 0u);
}

// Similar to the above test, but for the output dir
TEST_F(FileUtilsTest, OutputPathFromUnchangedWorkingDir) {
  std::string path = webrtc::test::OutputPath();
  std::string expected_end = "out";
  expected_end = kPathDelimiter + expected_end + kPathDelimiter;
  ASSERT_EQ(path.length() - expected_end.length(), path.find(expected_end));
}

// Tests with current working directory set to a directory higher up in the
// directory tree than the project root dir.
TEST_F(FileUtilsTest, DISABLED_ON_ANDROID(OutputPathFromRootWorkingDir)) {
  ASSERT_EQ(0, chdir(kPathDelimiter));
  ASSERT_EQ("./", webrtc::test::OutputPath());
}

// Only tests that the code executes
TEST_F(FileUtilsTest, CreateDirectory) {
  std::string directory = "fileutils-unittest-empty-dir";
  // Make sure it's removed if a previous test has failed:
  remove(directory.c_str());
  ASSERT_TRUE(webrtc::test::CreateDirectory(directory));
  remove(directory.c_str());
}

TEST_F(FileUtilsTest, WorkingDirReturnsValue) {
  // Hard to cover all platforms. Just test that it returns something without
  // crashing:
  std::string working_dir = webrtc::test::WorkingDir();
  ASSERT_GT(working_dir.length(), 0u);
}

// Due to multiple platforms, it is hard to make a complete test for
// ResourcePath. Manual testing has been performed by removing files and
// verified the result confirms with the specified documentation for the
// function.
TEST_F(FileUtilsTest, ResourcePathReturnsValue) {
  std::string resource = webrtc::test::ResourcePath(kTestName, kExtension);
  ASSERT_GT(resource.find(kTestName), 0u);
  ASSERT_GT(resource.find(kExtension), 0u);
}

TEST_F(FileUtilsTest, ResourcePathFromRootWorkingDir) {
  ASSERT_EQ(0, chdir(kPathDelimiter));
  std::string resource = webrtc::test::ResourcePath(kTestName, kExtension);
  ASSERT_NE(resource.find("resources"), std::string::npos);
  ASSERT_GT(resource.find(kTestName), 0u);
  ASSERT_GT(resource.find(kExtension), 0u);
}

TEST_F(FileUtilsTest, GetFileSizeExistingFile) {
  ASSERT_GT(webrtc::test::GetFileSize(files_.front()), 0u);
}

TEST_F(FileUtilsTest, GetFileSizeNonExistingFile) {
  ASSERT_EQ(0u, webrtc::test::GetFileSize("non-existing-file.tmp"));
}

}  // namespace webrtc
