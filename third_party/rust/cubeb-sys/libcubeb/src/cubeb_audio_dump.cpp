/*
 * Copyright © 2023 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#define NOMINMAX

#include "cubeb_audio_dump.h"
#include "cubeb/cubeb.h"
#include "cubeb_ringbuffer.h"
#include <chrono>
#include <limits>
#include <thread>
#include <vector>

using std::thread;
using std::vector;

uint32_t
bytes_per_sample(cubeb_stream_params params)
{
  switch (params.format) {
  case CUBEB_SAMPLE_S16LE:
  case CUBEB_SAMPLE_S16BE:
    return sizeof(int16_t);
  case CUBEB_SAMPLE_FLOAT32LE:
  case CUBEB_SAMPLE_FLOAT32BE:
    return sizeof(float);
  };
}

struct cubeb_audio_dump_stream {
public:
  explicit cubeb_audio_dump_stream(cubeb_stream_params params)
      : sample_size(bytes_per_sample(params)),
        ringbuffer(
            static_cast<int>(params.rate * params.channels * sample_size))
  {
  }

  int open(const char * name)
  {
    file = fopen(name, "wb");
    if (!file) {
      return CUBEB_ERROR;
    }
    return CUBEB_OK;
  }
  int close()
  {
    if (fclose(file)) {
      return CUBEB_ERROR;
    }
    return CUBEB_OK;
  }

  // Directly write to the file. Useful to write the header.
  size_t write(uint8_t * data, uint32_t count)
  {
    return fwrite(data, count, 1, file);
  }

  size_t write_all()
  {
    size_t written = 0;
    const int buf_sz = 16 * 1024;
    uint8_t buf[buf_sz];
    while (int rv = ringbuffer.dequeue(buf, buf_sz)) {
      written += fwrite(buf, rv, 1, file);
    }
    return written;
  }
  int dump(void * samples, uint32_t count)
  {
    int bytes = static_cast<int>(count * sample_size);
    int rv = ringbuffer.enqueue(static_cast<uint8_t *>(samples), bytes);
    return rv == bytes;
  }

private:
  uint32_t sample_size;
  FILE * file{};
  lock_free_queue<uint8_t> ringbuffer;
};

struct cubeb_audio_dump_session {
public:
  cubeb_audio_dump_session() = default;
  ~cubeb_audio_dump_session()
  {
    assert(streams.empty());
    session_thread.join();
  }
  cubeb_audio_dump_session(const cubeb_audio_dump_session &) = delete;
  cubeb_audio_dump_session &
  operator=(const cubeb_audio_dump_session &) = delete;
  cubeb_audio_dump_session & operator=(cubeb_audio_dump_session &&) = delete;

  cubeb_audio_dump_stream_t create_stream(cubeb_stream_params params,
                                          const char * name)
  {
    if (running) {
      return nullptr;
    }
    auto * stream = new cubeb_audio_dump_stream(params);
    streams.push_back(stream);
    int rv = stream->open(name);
    if (rv != CUBEB_OK) {
      delete stream;
      return nullptr;
    }

    struct riff_header {
      char chunk_id[4] = {'R', 'I', 'F', 'F'};
      int32_t chunk_size = 0;
      char format[4] = {'W', 'A', 'V', 'E'};

      char subchunk_id_1[4] = {'f', 'm', 't', 0x20};
      int32_t subchunk_1_size = 16;
      int16_t audio_format{};
      int16_t num_channels{};
      int32_t sample_rate{};
      int32_t byte_rate{};
      int16_t block_align{};
      int16_t bits_per_sample{};

      char subchunk_id_2[4] = {'d', 'a', 't', 'a'};
      int32_t subchunkd_2_size = std::numeric_limits<int32_t>::max();
    };

    riff_header header;
    // 1 is integer PCM, 3 is float PCM
    header.audio_format = bytes_per_sample(params) == 2 ? 1 : 3;
    header.num_channels = params.channels;
    header.sample_rate = params.rate;
    header.byte_rate = bytes_per_sample(params) * params.rate * params.channels;
    header.block_align = params.channels * bytes_per_sample(params);
    header.bits_per_sample = bytes_per_sample(params) * 8;

    stream->write(reinterpret_cast<uint8_t *>(&header), sizeof(riff_header));

    return stream;
  }
  int delete_stream(cubeb_audio_dump_stream * stream)
  {
    assert(!running);
    stream->close();
    streams.erase(std::remove(streams.begin(), streams.end(), stream),
                  streams.end());
    return CUBEB_OK;
  }
  int start()
  {
    assert(!running);
    running = true;
    session_thread = std::thread([this] {
      while (running) {
        for (auto * stream : streams) {
          stream->write_all();
        }
        const int DUMP_INTERVAL = 10;
        std::this_thread::sleep_for(std::chrono::milliseconds(DUMP_INTERVAL));
      }
    });
    return CUBEB_OK;
  }
  int stop()
  {
    assert(running);
    running = false;
    return CUBEB_OK;
  }

private:
  thread session_thread;
  vector<cubeb_audio_dump_stream_t> streams{};
  std::atomic<bool> running = false;
};

int
cubeb_audio_dump_init(cubeb_audio_dump_session_t * session)
{
  *session = new cubeb_audio_dump_session;
  return CUBEB_OK;
}

int
cubeb_audio_dump_shutdown(cubeb_audio_dump_session_t session)
{
  delete session;
  return CUBEB_OK;
}

int
cubeb_audio_dump_stream_init(cubeb_audio_dump_session_t session,
                             cubeb_audio_dump_stream_t * stream,
                             cubeb_stream_params stream_params,
                             const char * name)
{
  *stream = session->create_stream(stream_params, name);
  return CUBEB_OK;
}

int
cubeb_audio_dump_stream_shutdown(cubeb_audio_dump_session_t session,
                                 cubeb_audio_dump_stream_t stream)
{
  return session->delete_stream(stream);
}

int
cubeb_audio_dump_start(cubeb_audio_dump_session_t session)
{
  return session->start();
}

int
cubeb_audio_dump_stop(cubeb_audio_dump_session_t session)
{
  return session->stop();
}

int
cubeb_audio_dump_write(cubeb_audio_dump_stream_t stream, void * audio_samples,
                       uint32_t count)
{
  stream->dump(audio_samples, count);
  return CUBEB_OK;
}
