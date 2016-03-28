#include <cassert>
#include "cubeb_utils.h"

int test_auto_array()
{
  auto_array<uint32_t> array;
  auto_array<uint32_t> array2(10);
  uint32_t a[10];

  assert(array2.length() == 0);
  assert(array2.capacity() == 10);


  for (uint32_t i = 0; i < 10; i++) {
    a[i] = i;
  }

  assert(array.capacity() == 0);
  assert(array.length() == 0);

  array.push(a, 10);

  assert(!array.reserve(9));

  for (uint32_t i = 0; i < 10; i++) {
    assert(array.data()[i] == i);
  }

  assert(array.capacity() == 10);
  assert(array.length() == 10);

  uint32_t b[10];

  array.pop(b, 5);

  assert(array.capacity() == 10);
  assert(array.length() == 5);
  for (uint32_t i = 0; i < 5; i++) {
    assert(b[i] == i);
    assert(array.data()[i] == 5 + i);
  }
  uint32_t* bb = b + 5;
  array.pop(bb, 5);

  assert(array.capacity() == 10);
  assert(array.length() == 0);
  for (uint32_t i = 0; i < 5; i++) {
    assert(bb[i] == 5 + i);
  }

  assert(!array.pop(nullptr, 1));

  array.push(a, 10);
  array.push(a, 10);

  for (uint32_t j = 0; j < 2; j++) {
    for (uint32_t i = 0; i < 10; i++) {
      assert(array.data()[10 * j + i] == i);
    }
  }
  assert(array.length() == 20);
  assert(array.capacity() == 20);
  array.pop(nullptr, 5);

  for (uint32_t i = 0; i < 5; i++) {
    assert(array.data()[i] == 5 + i);
  }

  assert(array.length() == 15);
  assert(array.capacity() == 20);

  return 0;
}


int main()
{
  test_auto_array();
  return 0;
}
