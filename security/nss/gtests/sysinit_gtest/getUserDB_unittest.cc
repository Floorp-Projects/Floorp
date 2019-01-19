/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "prenv.h"
#include "seccomon.h"

#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

namespace nss_test {

// Return the path to user's NSS database.
extern "C" char *getUserDB(void);

class Sysinit : public ::testing::Test {
 protected:
  void SetUp() {
    home_var_ = PR_GetEnvSecure("HOME");
    if (home_var_) {
      old_home_dir_ = home_var_;
    }
    xdg_data_home_var_ = PR_GetEnvSecure("XDG_DATA_HOME");
    if (xdg_data_home_var_) {
      old_xdg_data_home_ = xdg_data_home_var_;
      ASSERT_EQ(0, unsetenv("XDG_DATA_HOME"));
    }
    char tmp[] = "/tmp/nss-tmp.XXXXXX";
    tmp_home_ = mkdtemp(tmp);
    ASSERT_EQ(0, setenv("HOME", tmp_home_.c_str(), 1));
  }

  void TearDown() {
    // Set HOME back to original
    if (home_var_) {
      ASSERT_EQ(0, setenv("HOME", old_home_dir_.c_str(), 1));
    } else {
      ASSERT_EQ(0, unsetenv("HOME"));
    }
    // Set XDG_DATA_HOME back to original
    if (xdg_data_home_var_) {
      ASSERT_EQ(0, setenv("XDG_DATA_HOME", old_xdg_data_home_.c_str(), 1));
    }
    // Remove test dirs.
    if (!nssdir_.empty()) {
      ASSERT_EQ(0, RemoveEmptyDirsFromStart(nssdir_, tmp_home_));
    }
  }

  // Remove all dirs within @start from @path containing only empty dirs.
  // Assumes @start already exists.
  // Upon successful completion, return 0. Otherwise, -1.
  static int RemoveEmptyDirsFromStart(std::string path, std::string start) {
    if (path.find(start) == std::string::npos) {
      return -1;
    }
    std::string temp = path;
    if (rmdir(temp.c_str())) {
      return -1;
    }
    for (size_t i = temp.length() - 1; i > start.length(); --i) {
      if (temp[i] == '/') {
        temp[i] = '\0';
        if (rmdir(temp.c_str())) {
          return -1;
        }
      }
    }
    if (rmdir(start.c_str())) {
      return -1;
    }
    return 0;
  }

  // Create empty dirs appending @path to @start with mode @mode.
  // Assumes @start already exists.
  // Upon successful completion, return the string @start + @path.
  static std::string CreateEmptyDirsFromStart(std::string start,
                                              std::string path, mode_t mode) {
    std::string temp = start + "/";
    for (size_t i = 1; i < path.length(); ++i) {
      if (path[i] == '/') {
        EXPECT_EQ(0, mkdir(temp.c_str(), mode));
      }
      temp += path[i];
    }
    // We reach the end of string before the last dir is created
    EXPECT_EQ(0, mkdir(temp.c_str(), mode));
    return temp;
  }

  char *home_var_;
  char *xdg_data_home_var_;
  std::string old_home_dir_;
  std::string old_xdg_data_home_;
  std::string nssdir_;
  std::string tmp_home_;
};

class SysinitSetXdgUserDataHome : public Sysinit {
 protected:
  void SetUp() {
    Sysinit::SetUp();
    ASSERT_EQ(0, setenv("XDG_DATA_HOME", tmp_home_.c_str(), 1));
  }
};

class SysinitSetTrashXdgUserDataHome : public Sysinit {
 protected:
  void SetUp() {
    Sysinit::SetUp();
    std::string trashPath = tmp_home_ + "/this/path/does/not/exist";
    ASSERT_EQ(0, setenv("XDG_DATA_HOME", trashPath.c_str(), 1));
  }

  void TearDown() {
    ASSERT_EQ(0, rmdir(tmp_home_.c_str()));
    Sysinit::TearDown();
  }
};

// Check if $HOME/.pki/nssdb is used if it exists
TEST_F(Sysinit, LegacyPath) {
  nssdir_ = CreateEmptyDirsFromStart(tmp_home_, "/.pki/nssdb", 0760);
  char *nssdb = getUserDB();
  ASSERT_EQ(nssdir_, nssdb);
  PORT_Free(nssdb);
}

// Check if $HOME/.local/share/pki/nssdb is used if:
// - $HOME/.pki/nssdb does not exist;
// - XDG_DATA_HOME is not set.
TEST_F(Sysinit, XdgDefaultPath) {
  nssdir_ = CreateEmptyDirsFromStart(tmp_home_, "/.local/share", 0755);
  nssdir_ = CreateEmptyDirsFromStart(nssdir_, "/pki/nssdb", 0760);
  char *nssdb = getUserDB();
  ASSERT_EQ(nssdir_, nssdb);
  PORT_Free(nssdb);
}

// Check if ${XDG_DATA_HOME}/pki/nssdb is used if:
// - $HOME/.pki/nssdb does not exist;
// - XDG_DATA_HOME is set and the path exists.
TEST_F(SysinitSetXdgUserDataHome, XdgSetPath) {
  // XDG_DATA_HOME is set to HOME
  nssdir_ = CreateEmptyDirsFromStart(tmp_home_, "/pki/nssdb", 0760);
  char *nssdb = getUserDB();
  ASSERT_EQ(nssdir_, nssdb);
  PORT_Free(nssdb);
}

// Check if it fails when:
// - XDG_DATA_HOME is set to a path that does not exist;
// - $HOME/.pki/nssdb also does not exist. */
TEST_F(SysinitSetTrashXdgUserDataHome, XdgSetToTrashPath) {
  char *nssdb = getUserDB();
  ASSERT_EQ(nullptr, nssdb);
}

}  // namespace nss_test
