// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "secdert.h"
#include "secitem.h"
#include "secport.h"

#include "gtest/gtest.h"

#include <stdint.h>
#include <string.h>
#include <string>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "mpi.h"
namespace nss_test {

void gettime(struct timespec *tp) {
#ifdef __MACH__
  clock_serv_t cclock;
  mach_timespec_t mts;

  host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
  clock_get_time(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);

  tp->tv_sec = mts.tv_sec;
  tp->tv_nsec = mts.tv_nsec;
#else
  clock_gettime(CLOCK_MONOTONIC, tp);
#endif
}

class MPITest : public ::testing::Test {
 protected:
  void TestCmp(const std::string a_string, const std::string b_string,
               int result) {
    mp_int a, b, c;
    MP_DIGITS(&a) = 0;
    MP_DIGITS(&b) = 0;
    MP_DIGITS(&c) = 0;
    ASSERT_EQ(MP_OKAY, mp_init(&a));
    ASSERT_EQ(MP_OKAY, mp_init(&b));

    mp_read_radix(&a, a_string.c_str(), 16);
    mp_read_radix(&b, b_string.c_str(), 16);
    EXPECT_EQ(result, mp_cmp(&a, &b));
  }
};

TEST_F(MPITest, MpiCmp01Test) { TestCmp("0", "1", -1); }
TEST_F(MPITest, MpiCmp10Test) { TestCmp("1", "0", 1); }
TEST_F(MPITest, MpiCmp00Test) { TestCmp("0", "0", 0); }
TEST_F(MPITest, MpiCmp11Test) { TestCmp("1", "1", 0); }

TEST_F(MPITest, MpiCmpConstTest) {
  mp_int a, b, c;
  MP_DIGITS(&a) = 0;
  MP_DIGITS(&b) = 0;
  MP_DIGITS(&c) = 0;
  ASSERT_EQ(MP_OKAY, mp_init(&a));
  ASSERT_EQ(MP_OKAY, mp_init(&b));
  ASSERT_EQ(MP_OKAY, mp_init(&c));

  mp_read_radix(
      &a,
      const_cast<char *>(
          "FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551"),
      16);
  mp_read_radix(
      &b,
      const_cast<char *>(
          "FF0FFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551"),
      16);
  mp_read_radix(
      &c,
      const_cast<char *>(
          "FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632550"),
      16);

  mp_taint(&b);
  mp_taint(&c);

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
}

}  // nss_test
