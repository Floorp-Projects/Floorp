#include "gtest/gtest.h"
#include "cubeb_utils.h"

TEST(cubeb, auto_array)
{
  auto_array<uint32_t> array;
  auto_array<uint32_t> array2(10);
  uint32_t a[10];

  ASSERT_EQ(array2.length(), 0u);
  ASSERT_EQ(array2.capacity(), 10u);


  for (uint32_t i = 0; i < 10; i++) {
    a[i] = i;
  }

  ASSERT_EQ(array.capacity(), 0u);
  ASSERT_EQ(array.length(), 0u);

  array.push(a, 10);

  ASSERT_TRUE(!array.reserve(9));

  for (uint32_t i = 0; i < 10; i++) {
    ASSERT_EQ(array.data()[i], i);
  }

  ASSERT_EQ(array.capacity(), 10u);
  ASSERT_EQ(array.length(), 10u);

  uint32_t b[10];

  array.pop(b, 5);

  ASSERT_EQ(array.capacity(), 10u);
  ASSERT_EQ(array.length(), 5u);
  for (uint32_t i = 0; i < 5; i++) {
    ASSERT_EQ(b[i], i);
    ASSERT_EQ(array.data()[i], 5 + i);
  }
  uint32_t* bb = b + 5;
  array.pop(bb, 5);

  ASSERT_EQ(array.capacity(), 10u);
  ASSERT_EQ(array.length(), 0u);
  for (uint32_t i = 0; i < 5; i++) {
    ASSERT_EQ(bb[i], 5 + i);
  }

  ASSERT_TRUE(!array.pop(nullptr, 1));

  array.push(a, 10);
  array.push(a, 10);

  for (uint32_t j = 0; j < 2; j++) {
    for (uint32_t i = 0; i < 10; i++) {
      ASSERT_EQ(array.data()[10 * j + i], i);
    }
  }
  ASSERT_EQ(array.length(), 20u);
  ASSERT_EQ(array.capacity(), 20u);
  array.pop(nullptr, 5);

  for (uint32_t i = 0; i < 5; i++) {
    ASSERT_EQ(array.data()[i], 5 + i);
  }

  ASSERT_EQ(array.length(), 15u);
  ASSERT_EQ(array.capacity(), 20u);
}

