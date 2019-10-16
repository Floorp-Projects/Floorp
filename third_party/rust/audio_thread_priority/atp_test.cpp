#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "audio_thread_priority.h"

int main() {
#ifdef __linux__
  atp_thread_info* info = atp_get_current_thread_info();
  atp_thread_info* info2 = nullptr;

  uint8_t buffer[ATP_THREAD_INFO_SIZE];
  atp_serialize_thread_info(info, buffer);

  info2 = atp_deserialize_thread_info(buffer);

  int rv = memcmp(info, info2, ATP_THREAD_INFO_SIZE);

  assert(!rv);

  atp_free_thread_info(info);
  atp_free_thread_info(info2);

  rv = atp_set_real_time_limit(0, 44100);
  assert(!rv);
#endif

  return 0;
}
