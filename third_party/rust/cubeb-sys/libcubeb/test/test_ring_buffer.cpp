/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#define NOMINMAX

#include "gtest/gtest.h"
#include "cubeb_ringbuffer.h"
#include <iostream>
#include <thread>
#include <chrono>

/* Generate a monotonically increasing sequence of numbers. */
template<typename T>
class sequence_generator
{
public:
  sequence_generator(size_t channels)
    : channels(channels)
  { }
  void get(T * elements, size_t frames)
  {
    for (size_t i = 0; i < frames; i++) {
      for (size_t c = 0; c < channels; c++) {
        elements[i * channels + c] = static_cast<T>(index_);
      }
      index_++;
    }
  }
  void rewind(size_t frames)
  {
    index_ -= frames;
  }
private:
  size_t index_ = 0;
  size_t channels = 0;
};

/* Checks that a sequence is monotonically increasing. */
template<typename T>
class sequence_verifier
{
  public:
  sequence_verifier(size_t channels)
    : channels(channels)
  { }
    void check(T * elements, size_t frames)
    {
      for (size_t i = 0; i < frames; i++) {
        for (size_t c = 0; c < channels; c++) {
          if (elements[i * channels + c] != static_cast<T>(index_)) {
            std::cerr << "Element " << i << " is different. Expected "
              << static_cast<T>(index_) << ", got " << elements[i]
              << ". (channel count: " << channels << ")." << std::endl;
            ASSERT_TRUE(false);
          }
        }
        index_++;
      }
    }
  private:
    size_t index_ = 0;
    size_t channels = 0;
};

template<typename T>
void test_ring(lock_free_audio_ring_buffer<T>& buf, int channels, int capacity_frames)
{
  std::unique_ptr<T[]> seq(new T[capacity_frames * channels]);
  sequence_generator<T> gen(channels);
  sequence_verifier<T> checker(channels);

  int iterations = 1002;

  const int block_size = 128;

  while(iterations--) {
    gen.get(seq.get(), block_size);
    int rv = buf.enqueue(seq.get(), block_size);
    ASSERT_EQ(rv, block_size);
    PodZero(seq.get(), block_size);
    rv = buf.dequeue(seq.get(), block_size);
    ASSERT_EQ(rv, block_size);
    checker.check(seq.get(), block_size);
  }
}

template<typename T>
void test_ring_multi(lock_free_audio_ring_buffer<T>& buf, int channels, int capacity_frames)
{
  sequence_verifier<T> checker(channels);
  std::unique_ptr<T[]> out_buffer(new T[capacity_frames * channels]);

  const int block_size = 128;

  std::thread t([=, &buf] {
    int iterations = 1002;
    std::unique_ptr<T[]> in_buffer(new T[capacity_frames * channels]);
    sequence_generator<T> gen(channels);

    while(iterations--) {
      std::this_thread::yield();
      gen.get(in_buffer.get(), block_size);
      int rv = buf.enqueue(in_buffer.get(), block_size);
      ASSERT_TRUE(rv <= block_size);
      if (rv != block_size) {
        gen.rewind(block_size - rv);
      }
    }
  });

  int remaining = 1002;

  while(remaining--) {
    std::this_thread::yield();
    int rv = buf.dequeue(out_buffer.get(), block_size);
    ASSERT_TRUE(rv <= block_size);
    checker.check(out_buffer.get(), rv);
  }

  t.join();
}

template<typename T>
void basic_api_test(T& ring)
{
  ASSERT_EQ(ring.capacity(), 128);

  ASSERT_EQ(ring.available_read(), 0);
  ASSERT_EQ(ring.available_write(), 128);

  int rv = ring.enqueue_default(63);

  ASSERT_TRUE(rv == 63);
  ASSERT_EQ(ring.available_read(), 63);
  ASSERT_EQ(ring.available_write(), 65);

  rv = ring.enqueue_default(65);

  ASSERT_EQ(rv, 65);
  ASSERT_EQ(ring.available_read(), 128);
  ASSERT_EQ(ring.available_write(), 0);

  rv = ring.dequeue(nullptr, 63);

  ASSERT_EQ(ring.available_read(), 65);
  ASSERT_EQ(ring.available_write(), 63);

  rv = ring.dequeue(nullptr, 65);

  ASSERT_EQ(ring.available_read(), 0);
  ASSERT_EQ(ring.available_write(), 128);
}

void test_reset_api() {
	const size_t ring_buffer_size = 128;
	const size_t enqueue_size = ring_buffer_size / 2;

	lock_free_queue<float> ring(ring_buffer_size);
	std::thread t([=, &ring] {
		std::unique_ptr<float[]> in_buffer(new float[enqueue_size]);
		ring.enqueue(in_buffer.get(), enqueue_size);
	});

	t.join();

	ring.reset_thread_ids();

	// Enqueue with a different thread. We have reset the thread ID
	// in the ring buffer, this should work.
	std::thread t2([=, &ring] {
		std::unique_ptr<float[]> in_buffer(new float[enqueue_size]);
		ring.enqueue(in_buffer.get(), enqueue_size);
	});

	t2.join();

	ASSERT_TRUE(true);
}

TEST(cubeb, ring_buffer)
{
  /* Basic API test. */
  const int min_channels = 1;
  const int max_channels = 10;
  const int min_capacity = 199;
  const int max_capacity = 1277;
  const int capacity_increment = 27;

  lock_free_queue<float> q1(128);
  basic_api_test(q1);
  lock_free_queue<short> q2(128);
  basic_api_test(q2);

  for (size_t channels = min_channels; channels < max_channels; channels++) {
    lock_free_audio_ring_buffer<float> q3(channels, 128);
    basic_api_test(q3);
    lock_free_audio_ring_buffer<short> q4(channels, 128);
    basic_api_test(q4);
  }

  /* Single thread testing. */
  /* Test mono to 9.1 */
  for (size_t channels = min_channels; channels < max_channels; channels++) {
    /* Use non power-of-two numbers to catch edge-cases. */
    for (size_t capacity_frames = min_capacity;
         capacity_frames < max_capacity; capacity_frames+=capacity_increment) {
      lock_free_audio_ring_buffer<float> ring(channels, capacity_frames);
      test_ring(ring, channels, capacity_frames);
    }
  }

  /* Multi thread testing */
  for (size_t channels = min_channels; channels < max_channels; channels++) {
    /* Use non power-of-two numbers to catch edge-cases. */
    for (size_t capacity_frames = min_capacity;
         capacity_frames < max_capacity; capacity_frames+=capacity_increment) {
      lock_free_audio_ring_buffer<short> ring(channels, capacity_frames);
      test_ring_multi(ring, channels, capacity_frames);
    }
  }

  test_reset_api();
}
