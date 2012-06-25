/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testsupport/fileutils.h"

#include <cstdio>
#include <list>
#include <string>

#include "gtest/gtest.h"

#ifdef WIN32
#define chdir _chdir
static const char* kPathDelimiter = "\\";
#else
static const char* kPathDelimiter = "/";
#endif

static const std::string kDummyDir = "file_utils_unittest_dummy_dir";
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
    // Create a dummy subdir that can be chdir'ed into for testing purposes.
    empty_dummy_dir_ = original_working_dir_ + kPathDelimiter + kDummyDir;
    webrtc::test::CreateDirectory(empty_dummy_dir_);
  }
  static void TearDownTestCase() {
    // Clean up all resource files written
    for (FileList::iterator file_it = files_.begin();
            file_it != files_.end(); ++file_it) {
      remove(file_it->c_str());
    }
    std::remove(empty_dummy_dir_.c_str());
  }
  void SetUp() {
    ASSERT_EQ(chdir(original_working_dir_.c_str()), 0);
  }
  void TearDown() {
    ASSERT_EQ(chdir(original_working_dir_.c_str()), 0);
  }
 protected:
  static FileList files_;
  static std::string empty_dummy_dir_;
 private:
  static std::string original_working_dir_;
};

FileList FileUtilsTest::files_;
std::string FileUtilsTest::original_working_dir_ = "";
std::string FileUtilsTest::empty_dummy_dir_ = "";

// Tests that the project root path is returned for the default working
// directory that is automatically set when the test executable is launched.
// The test is not fully testing the implementation, since we cannot be sure
// of where the executable was launched from.
// The test will fail if the top level directory is not named "trunk".
TEST_F(FileUtilsTest, ProjectRootPathFromUnchangedWorkingDir) {
  std::string path = webrtc::test::ProjectRootPath();
  std::string expected_end = "trunk";
  expected_end = kPathDelimiter + expected_end + kPathDelimiter;
  ASSERT_EQ(path.length() - expected_end.length(), path.find(expected_end));
}

// Similar to the above test, but for the output dir
TEST_F(FileUtilsTest, OutputPathFromUnchangedWorkingDir) {
  std::string path = webrtc::test::OutputPath();
  std::string expected_end = "out";
  expected_end = kPathDelimiter + expected_end + kPathDelimiter;
  ASSERT_EQ(path.length() - expected_end.length(), path.find(expected_end));
}

// Tests setting the current working directory to a directory three levels
// deeper from the current one. Then testing that the project path returned
// is still the same, when the function under test is called again.
TEST_F(FileUtilsTest, ProjectRootPathFromDeeperWorkingDir) {
  std::string path = webrtc::test::ProjectRootPath();
  std::string original_working_dir = path;  // This is the correct project root
  // Change to a subdirectory path.
  ASSERT_EQ(0, chdir(empty_dummy_dir_.c_str()));
  ASSERT_EQ(original_working_dir, webrtc::test::ProjectRootPath());
}

// Similar to the above test, but for the output dir
TEST_F(FileUtilsTest, OutputPathFromDeeperWorkingDir) {
  std::string path = webrtc::test::OutputPath();
  std::string original_working_dir = path;
  ASSERT_EQ(0, chdir(empty_dummy_dir_.c_str()));
  ASSERT_EQ(original_working_dir, webrtc::test::OutputPath());
}

// Tests with current working directory set to a directory higher up in the
// directory tree than the project root dir. This case shall return a specified
// error string as a directory (which will be an invalid path).
TEST_F(FileUtilsTest, ProjectRootPathFromRootWorkingDir) {
  // Change current working dir to the root of the current file system
  // (this will always be "above" our project root dir).
  ASSERT_EQ(0, chdir(kPathDelimiter));
  ASSERT_EQ(webrtc::test::kCannotFindProjectRootDir,
            webrtc::test::ProjectRootPath());
}

// Similar to the above test, but for the output dir
TEST_F(FileUtilsTest, OutputPathFromRootWorkingDir) {
  ASSERT_EQ(0, chdir(kPathDelimiter));
  ASSERT_EQ("./", webrtc::test::OutputPath());
}

// Only tests that the code executes
TEST_F(FileUtilsTest, CreateDirectory) {
  std::string directory = "fileutils-unittest-empty-dir";
  // Make sure it's removed if a previous test has failed:
  std::remove(directory.c_str());
  ASSERT_TRUE(webrtc::test::CreateDirectory(directory));
  std::remove(directory.c_str());
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
  ASSERT_EQ(0, chdir(kPathDelimiter));
  ASSERT_EQ("./", webrtc::test::OutputPath());
}

TEST_F(FileUtilsTest, GetFileSizeExistingFile) {
  ASSERT_GT(webrtc::test::GetFileSize(files_.front()), 0u);
}

TEST_F(FileUtilsTest, GetFileSizeNonExistingFile) {
  ASSERT_EQ(0u, webrtc::test::GetFileSize("non-existing-file.tmp"));
}

}  // namespace webrtc
