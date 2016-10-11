/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Original author: bcampen@mozilla.com */

#include "rlogconnector.h"

extern "C" {
#include "registry.h"
#include "r_log.h"
}

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

#include <deque>
#include <string>
#include <vector>

using mozilla::RLogConnector;

int NR_LOG_TEST = 0;

class RLogConnectorTest : public ::testing::Test {
  public:
    RLogConnectorTest() {
      Init();
    }

    ~RLogConnectorTest() {
      Free();
    }

    static void SetUpTestCase() {
      NR_reg_init(NR_REG_MODE_LOCAL);
      r_log_init();
      /* Would be nice to be able to unregister in the fixture */
      const char* facility = "rlogconnector_test";
      r_log_register(const_cast<char*>(facility), &NR_LOG_TEST);
    }

    void Init() {
      RLogConnector::CreateInstance();
    }

    void Free() {
      RLogConnector::DestroyInstance();
    }

    void ReInit() {
      Free();
      Init();
    }
};

TEST_F(RLogConnectorTest, TestGetFree) {
  RLogConnector* instance = RLogConnector::GetInstance();
  ASSERT_NE(nullptr, instance);
}

TEST_F(RLogConnectorTest, TestFilterEmpty) {
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->GetAny(0, &logs);
  ASSERT_EQ(0U, logs.size());
}

TEST_F(RLogConnectorTest, TestBasicFilter) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->Filter("Test", 0, &logs);
  ASSERT_EQ(1U, logs.size());
}

TEST_F(RLogConnectorTest, TestBasicFilterContent) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->Filter("Test", 0, &logs);
  ASSERT_EQ("Test", logs.back());
}

TEST_F(RLogConnectorTest, TestFilterAnyFrontMatch) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test");
  std::vector<std::string> substrings;
  substrings.push_back("foo");
  substrings.push_back("Test");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->FilterAny(substrings, 0, &logs);
  ASSERT_EQ("Test", logs.back());
}

TEST_F(RLogConnectorTest, TestFilterAnyBackMatch) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test");
  std::vector<std::string> substrings;
  substrings.push_back("Test");
  substrings.push_back("foo");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->FilterAny(substrings, 0, &logs);
  ASSERT_EQ("Test", logs.back());
}

TEST_F(RLogConnectorTest, TestFilterAnyBothMatch) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test");
  std::vector<std::string> substrings;
  substrings.push_back("Tes");
  substrings.push_back("est");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->FilterAny(substrings, 0, &logs);
  ASSERT_EQ("Test", logs.back());
}

TEST_F(RLogConnectorTest, TestFilterAnyNeitherMatch) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test");
  std::vector<std::string> substrings;
  substrings.push_back("tes");
  substrings.push_back("esT");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->FilterAny(substrings, 0, &logs);
  ASSERT_EQ(0U, logs.size());
}

TEST_F(RLogConnectorTest, TestAllMatch) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test1");
  r_log(NR_LOG_TEST, LOG_INFO, "Test2");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->GetAny(0, &logs);
  ASSERT_EQ(2U, logs.size());
}

TEST_F(RLogConnectorTest, TestOrder) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test1");
  r_log(NR_LOG_TEST, LOG_INFO, "Test2");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->GetAny(0, &logs);
  ASSERT_EQ("Test2", logs.back());
  ASSERT_EQ("Test1", logs.front());
}

TEST_F(RLogConnectorTest, TestNoMatch) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test1");
  r_log(NR_LOG_TEST, LOG_INFO, "Test2");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->Filter("foo", 0, &logs);
  ASSERT_EQ(0U, logs.size());
}

TEST_F(RLogConnectorTest, TestSubstringFilter) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test1");
  r_log(NR_LOG_TEST, LOG_INFO, "Test2");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->Filter("t1", 0, &logs);
  ASSERT_EQ(1U, logs.size());
  ASSERT_EQ("Test1", logs.back());
}

TEST_F(RLogConnectorTest, TestFilterLimit) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test1");
  r_log(NR_LOG_TEST, LOG_INFO, "Test2");
  r_log(NR_LOG_TEST, LOG_INFO, "Test3");
  r_log(NR_LOG_TEST, LOG_INFO, "Test4");
  r_log(NR_LOG_TEST, LOG_INFO, "Test5");
  r_log(NR_LOG_TEST, LOG_INFO, "Test6");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->Filter("Test", 2, &logs);
  ASSERT_EQ(2U, logs.size());
  ASSERT_EQ("Test6", logs.back());
  ASSERT_EQ("Test5", logs.front());
}

TEST_F(RLogConnectorTest, TestFilterAnyLimit) {
  r_log(NR_LOG_TEST, LOG_INFO, "TestOne");
  r_log(NR_LOG_TEST, LOG_INFO, "TestTwo");
  r_log(NR_LOG_TEST, LOG_INFO, "TestThree");
  r_log(NR_LOG_TEST, LOG_INFO, "TestFour");
  r_log(NR_LOG_TEST, LOG_INFO, "TestFive");
  r_log(NR_LOG_TEST, LOG_INFO, "TestSix");
  std::vector<std::string> substrings;
  // Matches Two, Three, Four, and Six
  substrings.push_back("tT");
  substrings.push_back("o");
  substrings.push_back("r");
  substrings.push_back("S");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->FilterAny(substrings, 2, &logs);
  ASSERT_EQ(2U, logs.size());
  ASSERT_EQ("TestSix", logs.back());
  ASSERT_EQ("TestFour", logs.front());
}

TEST_F(RLogConnectorTest, TestLimit) {
  RLogConnector::GetInstance()->SetLogLimit(3);
  r_log(NR_LOG_TEST, LOG_INFO, "Test1");
  r_log(NR_LOG_TEST, LOG_INFO, "Test2");
  r_log(NR_LOG_TEST, LOG_INFO, "Test3");
  r_log(NR_LOG_TEST, LOG_INFO, "Test4");
  r_log(NR_LOG_TEST, LOG_INFO, "Test5");
  r_log(NR_LOG_TEST, LOG_INFO, "Test6");
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->GetAny(0, &logs);
  ASSERT_EQ(3U, logs.size());
  ASSERT_EQ("Test6", logs.back());
  ASSERT_EQ("Test4", logs.front());
}

TEST_F(RLogConnectorTest, TestLimitBulkDiscard) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test1");
  r_log(NR_LOG_TEST, LOG_INFO, "Test2");
  r_log(NR_LOG_TEST, LOG_INFO, "Test3");
  r_log(NR_LOG_TEST, LOG_INFO, "Test4");
  r_log(NR_LOG_TEST, LOG_INFO, "Test5");
  r_log(NR_LOG_TEST, LOG_INFO, "Test6");
  RLogConnector::GetInstance()->SetLogLimit(3);
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->GetAny(0, &logs);
  ASSERT_EQ(3U, logs.size());
  ASSERT_EQ("Test6", logs.back());
  ASSERT_EQ("Test4", logs.front());
}

TEST_F(RLogConnectorTest, TestIncreaseLimit) {
  RLogConnector::GetInstance()->SetLogLimit(3);
  r_log(NR_LOG_TEST, LOG_INFO, "Test1");
  r_log(NR_LOG_TEST, LOG_INFO, "Test2");
  r_log(NR_LOG_TEST, LOG_INFO, "Test3");
  r_log(NR_LOG_TEST, LOG_INFO, "Test4");
  r_log(NR_LOG_TEST, LOG_INFO, "Test5");
  r_log(NR_LOG_TEST, LOG_INFO, "Test6");
  RLogConnector::GetInstance()->SetLogLimit(300);
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->GetAny(0, &logs);
  ASSERT_EQ(3U, logs.size());
  ASSERT_EQ("Test6", logs.back());
  ASSERT_EQ("Test4", logs.front());
}

TEST_F(RLogConnectorTest, TestClear) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test1");
  r_log(NR_LOG_TEST, LOG_INFO, "Test2");
  r_log(NR_LOG_TEST, LOG_INFO, "Test3");
  r_log(NR_LOG_TEST, LOG_INFO, "Test4");
  r_log(NR_LOG_TEST, LOG_INFO, "Test5");
  r_log(NR_LOG_TEST, LOG_INFO, "Test6");
  RLogConnector::GetInstance()->SetLogLimit(0);
  RLogConnector::GetInstance()->SetLogLimit(4096);
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->GetAny(0, &logs);
  ASSERT_EQ(0U, logs.size());
}

TEST_F(RLogConnectorTest, TestReInit) {
  r_log(NR_LOG_TEST, LOG_INFO, "Test1");
  r_log(NR_LOG_TEST, LOG_INFO, "Test2");
  r_log(NR_LOG_TEST, LOG_INFO, "Test3");
  r_log(NR_LOG_TEST, LOG_INFO, "Test4");
  r_log(NR_LOG_TEST, LOG_INFO, "Test5");
  r_log(NR_LOG_TEST, LOG_INFO, "Test6");
  ReInit();
  std::deque<std::string> logs;
  RLogConnector::GetInstance()->GetAny(0, &logs);
  ASSERT_EQ(0U, logs.size());
}
