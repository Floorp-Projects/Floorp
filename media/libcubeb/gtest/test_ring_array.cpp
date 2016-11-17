#include "gtest/gtest.h"
#ifdef __APPLE__
#include <string.h>
#include <iostream>
#include <CoreAudio/CoreAudioTypes.h>
#include "cubeb/cubeb.h"
#include "cubeb_ring_array.h"

TEST(cubeb, ring_array)
{
  ring_array ra;

  ASSERT_EQ(ring_array_init(&ra, 0, 0, 1, 1), CUBEB_ERROR_INVALID_PARAMETER);
  ASSERT_EQ(ring_array_init(&ra, 1, 0, 0, 1), CUBEB_ERROR_INVALID_PARAMETER);

  unsigned int capacity = 8;
  ring_array_init(&ra, capacity, sizeof(int), 1, 1);
  int verify_data[capacity] ;// {1,2,3,4,5,6,7,8};
  AudioBuffer * p_data = NULL;

  for (unsigned int i = 0; i < capacity; ++i) {
    verify_data[i] = i; // in case capacity change value
    *(int*)ra.buffer_array[i].mData = i;
    ASSERT_EQ(ra.buffer_array[i].mDataByteSize, sizeof(int));
    ASSERT_EQ(ra.buffer_array[i].mNumberChannels, 1u);
  }

  /* Get store buffers*/
  for (unsigned int i = 0; i < capacity; ++i) {
    p_data = ring_array_get_free_buffer(&ra);
    ASSERT_NE(p_data, nullptr);
    ASSERT_EQ(*(int*)p_data->mData, verify_data[i]);
  }
  /*Now array is full extra store should give NULL*/
  ASSERT_EQ(ring_array_get_free_buffer(&ra), nullptr);
  /* Get fetch buffers*/
  for (unsigned int i = 0; i < capacity; ++i) {
    p_data = ring_array_get_data_buffer(&ra);
    ASSERT_NE(p_data, nullptr);
    ASSERT_EQ(*(int*)p_data->mData, verify_data[i]);
  }
  /*Now array is empty extra fetch should give NULL*/
  ASSERT_EQ(ring_array_get_data_buffer(&ra), nullptr);

  p_data = NULL;
  /* Repeated store fetch should can go for ever*/
  for (unsigned int i = 0; i < 2*capacity; ++i) {
    p_data = ring_array_get_free_buffer(&ra);
    ASSERT_NE(p_data, nullptr);
    ASSERT_EQ(ring_array_get_data_buffer(&ra), p_data);
  }

  p_data = NULL;
  /* Verify/modify buffer data*/
  for (unsigned int i = 0; i < capacity; ++i) {
    p_data = ring_array_get_free_buffer(&ra);
    ASSERT_NE(p_data, nullptr);
    ASSERT_EQ(*((int*)p_data->mData), verify_data[i]);
    (*((int*)p_data->mData))++; // Modify data
  }
  for (unsigned int i = 0; i < capacity; ++i) {
    p_data = ring_array_get_data_buffer(&ra);
    ASSERT_NE(p_data, nullptr);
    ASSERT_EQ(*((int*)p_data->mData), verify_data[i]+1); // Verify modified data
  }

  ring_array_destroy(&ra);
}
#else
TEST(cubeb, DISABLED_ring_array)
{
}
#endif
