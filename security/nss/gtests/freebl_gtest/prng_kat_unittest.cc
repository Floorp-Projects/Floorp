// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include <cstdlib>
#include <fstream>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "util.h"

#include "blapi.h"

extern std::string g_source_dir;

namespace nss_test {

struct PRNGTestValues {
  std::vector<uint8_t> entropy;
  std::vector<uint8_t> nonce;
  std::vector<uint8_t> personal;
  std::vector<uint8_t> expected_result;
  std::vector<uint8_t> additional_entropy;
  std::vector<uint8_t> additional_input_reseed;
  std::vector<std::vector<uint8_t>> additional_input;
};

bool contains(std::string& s, const char* to_find) {
  return s.find(to_find) != std::string::npos;
}

std::string trim(std::string str) {
  std::string whitespace = " \t\r\n";
  const auto strBegin = str.find_first_not_of(whitespace);
  if (strBegin == std::string::npos) {
    return "";
  }
  const auto strEnd = str.find_last_not_of(whitespace);
  const auto strRange = strEnd - strBegin + 1;
  return str.substr(strBegin, strRange);
}

std::vector<uint8_t> read_option_s(std::string& s) {
  size_t start = s.find("=") + 1;
  assert(start > 0);
  return hex_string_to_bytes(trim(s.substr(start, s.find("]", start))));
}

void print_bytes(std::vector<uint8_t> bytes, std::string name) {
  std::cout << name << ": ";
  for (auto b : bytes) {
    std::cout << std::setfill('0') << std::setw(2) << std::hex
              << static_cast<int>(b);
  }
  std::cout << std::endl;
}

static std::vector<PRNGTestValues> ReadFile(const std::string file_name) {
  std::vector<PRNGTestValues> test_vector;
  std::ifstream infile(file_name);
  EXPECT_FALSE(infile.fail()) << "kat file: " << file_name;
  std::string line;

  // Variables holding the input for each test.
  bool valid_option = false;

  // Read the file.
  std::streampos pos;
  while (std::getline(infile, line)) {
    // We only implement SHA256. Skip all other tests.
    if (contains(line, "[SHA-")) {
      valid_option = contains(line, "[SHA-256]");
    }
    if (!valid_option) {
      continue;
    }

    // We ignore the options and infer them from the test case.

    PRNGTestValues test;
    if (line.find("COUNT =")) {
      continue;
    }

    // Read test input.
    do {
      pos = infile.tellg();
      std::getline(infile, line);
      if (contains(line, "EntropyInput ")) {
        test.entropy = read_option_s(line);
        continue;
      }
      if (contains(line, "Nonce")) {
        test.nonce = read_option_s(line);
        continue;
      }
      if (contains(line, "PersonalizationString")) {
        test.personal = read_option_s(line);
        continue;
      }
      if (contains(line, "AdditionalInput ")) {
        test.additional_input.push_back(read_option_s(line));
        continue;
      }
      if (contains(line, "EntropyInputReseed")) {
        test.additional_entropy = read_option_s(line);
        continue;
      }
      if (contains(line, "AdditionalInputReseed")) {
        test.additional_input_reseed = read_option_s(line);
        continue;
      }
      if (contains(line, "ReturnedBits")) {
        test.expected_result = read_option_s(line);
        continue;
      }
    } while (!infile.eof() && line.find("COUNT =") && line.find("["));

    // Save test case.
    test_vector.push_back(test);
    test = {};
    infile.seekg(pos);
  }
  return test_vector;
}

class PRNGTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_vector_ = ReadFile(::g_source_dir + "/kat/Hash_DRBG.rsp");
    ASSERT_FALSE(test_vector_.empty());
  }

  void RunTest(PRNGTestValues& test) {
    ASSERT_EQ(2U, test.additional_input.size());
    SECStatus rv = PRNGTEST_Instantiate_Kat(
        test.entropy.data(), test.entropy.size(), test.nonce.data(),
        test.nonce.size(), test.personal.data(), test.personal.size());
    ASSERT_EQ(SECSuccess, rv);
    rv = PRNGTEST_Reseed(test.additional_entropy.data(),
                         test.additional_entropy.size(),
                         test.additional_input_reseed.data(),
                         test.additional_input_reseed.size());
    ASSERT_EQ(SECSuccess, rv);

    // Generate bytes.
    uint8_t bytes[128];
    PRNGTEST_Generate(bytes, 128, test.additional_input[0].data(),
                      test.additional_input[0].size());
    PRNGTEST_Generate(bytes, 128, test.additional_input[1].data(),
                      test.additional_input[1].size());
    std::vector<uint8_t> result(bytes, bytes + 128);
    if (result != test.expected_result) {
      print_bytes(result, "result  ");
      print_bytes(test.expected_result, "expected");
    }
    ASSERT_EQ(test.expected_result, result);
    rv = PRNGTEST_Uninstantiate();
    ASSERT_EQ(SECSuccess, rv);
  }

 protected:
  std::vector<PRNGTestValues> test_vector_;
};

TEST_F(PRNGTest, HashDRBG) {
  for (auto& v : test_vector_) {
    RunTest(v);
  }
}

}  // namespace nss_test
