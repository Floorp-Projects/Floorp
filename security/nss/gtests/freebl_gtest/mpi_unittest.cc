// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "gtest/gtest.h"

#include <stdint.h>
#include <string.h>
#include <memory>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "mpi.h"
namespace nss_test {

void gettime(struct timespec* tp) {
#ifdef __MACH__
  clock_serv_t cclock;
  mach_timespec_t mts;

  host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
  clock_get_time(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);

  tp->tv_sec = mts.tv_sec;
  tp->tv_nsec = mts.tv_nsec;
#else
  ASSERT_NE(0, timespec_get(tp, TIME_UTC));
#endif
}

class MPITest : public ::testing::Test {
 protected:
  void TestCmp(const std::string a_string, const std::string b_string,
               int result) {
    mp_int a, b;
    MP_DIGITS(&a) = 0;
    MP_DIGITS(&b) = 0;
    ASSERT_EQ(MP_OKAY, mp_init(&a));
    ASSERT_EQ(MP_OKAY, mp_init(&b));

    mp_read_radix(&a, a_string.c_str(), 16);
    mp_read_radix(&b, b_string.c_str(), 16);
    EXPECT_EQ(result, mp_cmp(&a, &b));

    mp_clear(&a);
    mp_clear(&b);
  }

  void TestDiv(const std::string a_string, const std::string b_string,
               const std::string result) {
    mp_int a, b, c;
    MP_DIGITS(&a) = 0;
    MP_DIGITS(&b) = 0;
    MP_DIGITS(&c) = 0;
    ASSERT_EQ(MP_OKAY, mp_init(&a));
    ASSERT_EQ(MP_OKAY, mp_init(&b));
    ASSERT_EQ(MP_OKAY, mp_init(&c));

    mp_read_radix(&a, a_string.c_str(), 16);
    mp_read_radix(&b, b_string.c_str(), 16);
    mp_read_radix(&c, result.c_str(), 16);
    EXPECT_EQ(MP_OKAY, mp_div(&a, &b, &a, &b));
    EXPECT_EQ(0, mp_cmp(&a, &c));

    mp_clear(&a);
    mp_clear(&b);
    mp_clear(&c);
  }

  void dump(const std::string& prefix, const uint8_t* buf, size_t len) {
    auto flags = std::cerr.flags();
    std::cerr << prefix << ": [" << std::dec << len << "] ";
    for (size_t i = 0; i < len; ++i) {
      std::cerr << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(buf[i]);
    }
    std::cerr << std::endl << std::resetiosflags(flags);
  }

  void TestToFixedOctets(const std::vector<uint8_t>& ref, size_t len) {
    mp_int a;
    ASSERT_EQ(MP_OKAY, mp_init(&a));
    ASSERT_EQ(MP_OKAY, mp_read_unsigned_octets(&a, ref.data(), ref.size()));
    std::unique_ptr<uint8_t[]> buf(new uint8_t[len]);
    ASSERT_NE(buf, nullptr);
    ASSERT_EQ(MP_OKAY, mp_to_fixlen_octets(&a, buf.get(), len));
    size_t compare;
    if (len > ref.size()) {
      for (size_t i = 0; i < len - ref.size(); ++i) {
        ASSERT_EQ(0U, buf[i]) << "index " << i << " should be zero";
      }
      compare = ref.size();
    } else {
      compare = len;
    }
    dump("value", ref.data(), ref.size());
    dump("output", buf.get(), len);
    ASSERT_EQ(0, memcmp(buf.get() + len - compare,
                        ref.data() + ref.size() - compare, compare))
        << "comparing " << compare << " octets";
    mp_clear(&a);
  }
};

TEST_F(MPITest, MpiCmp01Test) { TestCmp("0", "1", -1); }
TEST_F(MPITest, MpiCmp10Test) { TestCmp("1", "0", 1); }
TEST_F(MPITest, MpiCmp00Test) { TestCmp("0", "0", 0); }
TEST_F(MPITest, MpiCmp11Test) { TestCmp("1", "1", 0); }
TEST_F(MPITest, MpiDiv32ErrorTest) {
  TestDiv("FFFF00FFFFFFFF000000000000", "FFFF00FFFFFFFFFF", "FFFFFFFFFF");
}

#ifdef NSS_X64
// This tests assumes 64-bit mp_digits.
TEST_F(MPITest, MpiCmpUnalignedTest) {
  mp_int a, b, c;
  MP_DIGITS(&a) = 0;
  MP_DIGITS(&b) = 0;
  MP_DIGITS(&c) = 0;
  ASSERT_EQ(MP_OKAY, mp_init(&a));
  ASSERT_EQ(MP_OKAY, mp_init(&b));
  ASSERT_EQ(MP_OKAY, mp_init(&c));

  mp_read_radix(&a, "ffffffffffffffff3b4e802b4e1478", 16);
  mp_read_radix(&b, "ffffffffffffffff3b4e802b4e1478", 16);
  EXPECT_EQ(0, mp_cmp(&a, &b));

  // Now change a and b such that they contain the same numbers but are not
  // aligned.
  // a = ffffffffffffff|ff3b4e802b4e1478
  // b = ffffffffffffffff|3b4e802b4e1478
  MP_DIGITS(&b)[0] &= 0x00ffffffffffffff;
  MP_DIGITS(&b)[1] = 0xffffffffffffffff;
  EXPECT_EQ(-1, mp_cmp(&a, &b));

  ASSERT_EQ(MP_OKAY, mp_sub(&a, &b, &c));
  char c_tmp[40];
  ASSERT_EQ(MP_OKAY, mp_toradix(&c, c_tmp, 16));
  ASSERT_TRUE(strncmp(c_tmp, "feffffffffffffff100000000000000", 31));

  mp_clear(&a);
  mp_clear(&b);
  mp_clear(&c);
}
#endif

TEST_F(MPITest, MpiFixlenOctetsZero) {
  std::vector<uint8_t> zero = {0};
  TestToFixedOctets(zero, 1);
  TestToFixedOctets(zero, 2);
  TestToFixedOctets(zero, sizeof(mp_digit));
  TestToFixedOctets(zero, sizeof(mp_digit) + 1);
}

TEST_F(MPITest, MpiFixlenOctetsVarlen) {
  std::vector<uint8_t> packed;
  for (size_t i = 0; i < sizeof(mp_digit) * 2; ++i) {
    packed.push_back(0xa4);  // Any non-zero value will do.
    TestToFixedOctets(packed, packed.size());
    TestToFixedOctets(packed, packed.size() + 1);
    TestToFixedOctets(packed, packed.size() + sizeof(mp_digit));
  }
}

TEST_F(MPITest, MpiFixlenOctetsTooSmall) {
  uint8_t buf[sizeof(mp_digit) * 3];
  std::vector<uint8_t> ref;
  for (size_t i = 0; i < sizeof(mp_digit) * 2; i++) {
    ref.push_back(3);  // Any non-zero value will do.
    dump("ref", ref.data(), ref.size());

    mp_int a;
    ASSERT_EQ(MP_OKAY, mp_init(&a));
    ASSERT_EQ(MP_OKAY, mp_read_unsigned_octets(&a, ref.data(), ref.size()));
#ifdef DEBUG
    // ARGCHK maps to assert() in a debug build.
    EXPECT_DEATH(mp_to_fixlen_octets(&a, buf, ref.size() - 1), "");
#else
    EXPECT_EQ(MP_BADARG, mp_to_fixlen_octets(&a, buf, ref.size() - 1));
#endif
    ASSERT_EQ(MP_OKAY, mp_to_fixlen_octets(&a, buf, ref.size()));
    ASSERT_EQ(0, memcmp(buf, ref.data(), ref.size()));

    mp_clear(&a);
  }
}

// This test is slow. Disable it by default so we can run these tests on CI.
class DISABLED_MPITest : public ::testing::Test {};

TEST_F(DISABLED_MPITest, MpiCmpConstTest) {
  mp_int a, b, c;
  MP_DIGITS(&a) = 0;
  MP_DIGITS(&b) = 0;
  MP_DIGITS(&c) = 0;
  ASSERT_EQ(MP_OKAY, mp_init(&a));
  ASSERT_EQ(MP_OKAY, mp_init(&b));
  ASSERT_EQ(MP_OKAY, mp_init(&c));

  mp_read_radix(
      &a,
      const_cast<char*>(
          "FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551"),
      16);
  mp_read_radix(
      &b,
      const_cast<char*>(
          "FF0FFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551"),
      16);
  mp_read_radix(
      &c,
      const_cast<char*>(
          "FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632550"),
      16);

#ifdef CT_VERIF
  mp_taint(&b);
  mp_taint(&c);
#endif

  uint32_t runs = 5000000;
  uint32_t time_b = 0, time_c = 0;
  for (uint32_t i = 0; i < runs; ++i) {
    struct timespec start, end;
    gettime(&start);
    int r = mp_cmp(&a, &b);
    gettime(&end);
    unsigned long long used = end.tv_sec * 1000000000L + end.tv_nsec;
    used -= static_cast<unsigned long long>(start.tv_sec * 1000000000L +
                                            start.tv_nsec);
    time_b += used;
    ASSERT_EQ(1, r);
  }
  printf("time b: %u\n", time_b / runs);

  for (uint32_t i = 0; i < runs; ++i) {
    struct timespec start, end;
    gettime(&start);
    int r = mp_cmp(&a, &c);
    gettime(&end);
    unsigned long long used = end.tv_sec * 1000000000L + end.tv_nsec;
    used -= static_cast<unsigned long long>(start.tv_sec * 1000000000L +
                                            start.tv_nsec);
    time_c += used;
    ASSERT_EQ(1, r);
  }
  printf("time c: %u\n", time_c / runs);

  mp_clear(&a);
  mp_clear(&b);
  mp_clear(&c);
}

}  // nss_test
