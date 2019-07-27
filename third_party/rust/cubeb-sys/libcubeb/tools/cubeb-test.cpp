#define __STDC_FORMAT_MACROS
#include "cubeb/cubeb.h"
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <inttypes.h>
#include <iostream>
#ifdef _WIN32
#include <objbase.h> // Used by CoInitialize()
#endif

#ifndef M_PI
#define M_PI 3.14159263
#endif

// Default values if none specified
#define DEFAULT_RATE 44100
#define DEFAULT_CHANNELS 2

static const char* state_to_string(cubeb_state state) {
  switch (state) {
    case CUBEB_STATE_STARTED:
      return "CUBEB_STATE_STARTED";
    case CUBEB_STATE_STOPPED:
      return "CUBEB_STATE_STOPPED";
    case CUBEB_STATE_DRAINED:
      return "CUBEB_STATE_DRAINED";
    case CUBEB_STATE_ERROR:
      return "CUBEB_STATE_ERROR";
    default:
      return "Undefined state";
  }
}

void print_log(const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  vprintf(msg, args);
  va_end(args);
}

class cubeb_client final {
public:
  cubeb_client() {}
  ~cubeb_client() {}

  bool init(char const * backend_name = nullptr);
  bool init_stream();
  bool start_stream() const;
  bool stop_stream() const;
  bool destroy_stream() const;
  bool destroy();
  bool activate_log(cubeb_log_level log_level) const;
  uint64_t get_stream_position() const;

  long user_data_cb(cubeb_stream* stm, void* user, const void* input_buffer,
                    void* output_buffer, long nframes);

  void user_state_cb(cubeb_stream* stm, void* user, cubeb_state state);

  bool register_device_collection_changed(cubeb_device_type devtype) const;
  bool unregister_device_collection_changed(cubeb_device_type devtype) const;

  cubeb_stream_params output_params = {};
  cubeb_stream_params input_params = {};

private:
  bool has_input() { return input_params.rate != 0; }
  bool has_output() { return output_params.rate != 0; }

  cubeb* context = nullptr;

  cubeb_stream* stream = nullptr;
  cubeb_devid output_device = nullptr;
  cubeb_devid input_device = nullptr;

  /* Accessed only from client and audio thread. */
  std::atomic<uint32_t> _rate = {0};
  std::atomic<uint32_t> _channels = {0};

  /* Accessed only from audio thread. */
  uint32_t _total_frames = 0;
};

bool cubeb_client::init(char const * backend_name) {
  int rv = cubeb_init(&context, "Cubeb Test Application", nullptr);
  if (rv != CUBEB_OK) {
    fprintf(stderr, "Could not init cubeb\n");
    return false;
  }
  return true;
}

static long user_data_cb_s(cubeb_stream* stm, void* user,
                           const void* input_buffer, void* output_buffer,
                           long nframes) {
  assert(user);
  return static_cast<cubeb_client*>(user)->user_data_cb(stm, user, input_buffer,
                                                        output_buffer, nframes);
}

static void user_state_cb_s(cubeb_stream* stm, void* user, cubeb_state state) {
  assert(user);
  return static_cast<cubeb_client*>(user)->user_state_cb(stm, user, state);
}

void input_device_changed_callback_s(cubeb* context, void* user) {
  fprintf(stderr, "input_device_changed_callback_s\n");
}

void output_device_changed_callback_s(cubeb* context, void* user) {
  fprintf(stderr, "output_device_changed_callback_s\n");
}

void io_device_changed_callback_s(cubeb* context, void* user) {
  fprintf(stderr, "io_device_changed_callback\n");
}

bool cubeb_client::init_stream() {
  assert(has_input() || has_output());

  _rate = has_output() ? output_params.rate : input_params.rate;
  _channels = has_output() ? output_params.channels : input_params.channels;

  int rv =
      cubeb_stream_init(context, &stream, "Stream", input_device,
                        has_input() ? &input_params : nullptr, output_device,
                        has_output() ? &output_params : nullptr, 512,
                        user_data_cb_s, user_state_cb_s, this);
  if (rv != CUBEB_OK) {
    fprintf(stderr, "Could not open the stream\n");
    return false;
  }
  return true;
}

bool cubeb_client::start_stream() const {
  int rv = cubeb_stream_start(stream);
  if (rv != CUBEB_OK) {
    fprintf(stderr, "Could not start the stream\n");
    return false;
  }
  return true;
}

bool cubeb_client::stop_stream() const {
  int rv = cubeb_stream_stop(stream);
  if (rv != CUBEB_OK) {
    fprintf(stderr, "Could not stop the stream\n");
    return false;
  }
  return true;
}

uint64_t cubeb_client::get_stream_position() const {
  uint64_t pos = 0;
  int rv = cubeb_stream_get_position(stream, &pos);
  if (rv != CUBEB_OK) {
    fprintf(stderr, "Could not get the position the stream\n");
    return 0;
  }
  return pos;
}

bool cubeb_client::destroy_stream() const {
  cubeb_stream_destroy(stream);
  return true;
}

bool cubeb_client::destroy() {
  cubeb_destroy(context);
  return true;
}

bool cubeb_client::activate_log(cubeb_log_level log_level) const {
  cubeb_log_callback log_callback = nullptr;
  if (log_level != CUBEB_LOG_DISABLED) {
    log_callback = print_log;
  }

  if (cubeb_set_log_callback(log_level, log_callback) != CUBEB_OK) {
    fprintf(stderr, "Set log callback failed\n");
    return false;
  }
  return true;
}

static void fill_with_sine_tone(float* buf, uint32_t num_of_frames,
                               uint32_t num_of_channels, uint32_t frame_rate,
                               uint32_t position) {
  for (uint32_t i = 0; i < num_of_frames; ++i) {
    for (uint32_t c = 0; c < num_of_channels; ++c) {
      buf[i * num_of_channels + c] =
          0.2 * sin(2 * M_PI * (i + position) * 350 / frame_rate);
      buf[i * num_of_channels + c] +=
          0.2 * sin(2 * M_PI * (i + position) * 440 / frame_rate);
    }
  }
}

long cubeb_client::user_data_cb(cubeb_stream* stm, void* user,
                                const void* input_buffer, void* output_buffer,
                                long nframes) {
  if (input_buffer && output_buffer) {
    const float* in = static_cast<const float*>(input_buffer);
    float* out = static_cast<float*>(output_buffer);
    memcpy(out, in, sizeof(float) * nframes * _channels);
  } else if (output_buffer && !input_buffer) {
    fill_with_sine_tone(static_cast<float*>(output_buffer), nframes, _channels,
                       _rate, _total_frames);
  }

  _total_frames += nframes;
  return nframes;
}

void cubeb_client::user_state_cb(cubeb_stream* stm, void* user,
                                 cubeb_state state) {
  fprintf(stderr, "state is %s\n", state_to_string(state));
}

bool cubeb_client::register_device_collection_changed(
    cubeb_device_type devtype) const {
  cubeb_device_collection_changed_callback callback = nullptr;
  if (devtype == static_cast<cubeb_device_type>(CUBEB_DEVICE_TYPE_INPUT |
                                                CUBEB_DEVICE_TYPE_OUTPUT)) {
    callback = io_device_changed_callback_s;
  } else if (devtype & CUBEB_DEVICE_TYPE_OUTPUT) {
    callback = output_device_changed_callback_s;
  } else if (devtype & CUBEB_DEVICE_TYPE_INPUT) {
    callback = input_device_changed_callback_s;
  }
  int r = cubeb_register_device_collection_changed(
            context, devtype, callback, nullptr);
  if (r != CUBEB_OK) {
    return false;
  }
  return true;
}

bool cubeb_client::unregister_device_collection_changed(
    cubeb_device_type devtype) const {
  int r = cubeb_register_device_collection_changed(
            context, devtype, nullptr, nullptr);
  if (r != CUBEB_OK) {
    return false;
  }
  return true;
}

enum play_mode {
  RECORD,
  PLAYBACK,
  DUPLEX,
  COLLECTION_CHANGE,
};

struct operation_data {
  play_mode pm;
  uint32_t rate;
  cubeb_device_type collection_device_type;
};

void print_help() {
  const char * msg =
    "0: change log level to disabled\n"
    "1: change log level to normal\n"
    "2: change log level to verbose\n"
    "p: start a initialized stream\n"
    "s: stop a started stream\n"
    "c: get stream position (client thread)\n"
    "i: change device type to input\n"
    "o: change device type to output\n"
    "a: change device type to input and output\n"
    "k: change device type to unknown\n"
    "r: register device collection changed callback for the current device type\n"
    "u: unregister device collection changed callback for the current device type\n"
    "q: quit\n"
    "h: print this message\n";
  fprintf(stderr, "%s\n", msg);
}

bool choose_action(const cubeb_client& cl, operation_data * op, char c) {
  while (c == 10 || c == 32) {
    // Consume "enter and "space"
    c = getchar();
  }

  if (c == 'q') {
    if (op->pm == PLAYBACK || op->pm == RECORD || op->pm == DUPLEX) {
      bool res = cl.stop_stream();
      if (!res) {
        fprintf(stderr, "stop_stream failed\n");
      }
      res = cl.destroy_stream();
      if (!res) {
        fprintf(stderr, "destroy_stream failed\n");
      }
    } else if (op->pm == COLLECTION_CHANGE) {
      bool res = cl.unregister_device_collection_changed(op->collection_device_type);
      if (!res) {
        fprintf(stderr, "unregister_device_collection_changed failed\n");
      }
    }
    return false; // exit the loop
  } else if (c == 'h') {
    print_help();
  } else if (c == '0') {
    cl.activate_log(CUBEB_LOG_DISABLED);
    fprintf(stderr, "Log level changed to DISABLED\n");
  } else if (c == '1') {
    cl.activate_log(CUBEB_LOG_DISABLED);
    cl.activate_log(CUBEB_LOG_NORMAL);
    fprintf(stderr, "Log level changed to NORMAL\n");
  } else if (c == '2') {
    cl.activate_log(CUBEB_LOG_DISABLED);
    cl.activate_log(CUBEB_LOG_VERBOSE);
    fprintf(stderr, "Log level changed to VERBOSE\n");
  } else if (c == 'p') {
    bool res = cl.start_stream();
    if (res) {
      fprintf(stderr, "start_stream succeed\n");
    } else {
      fprintf(stderr, "start_stream failed\n");
    }
  } else if (c == 's') {
    bool res = cl.stop_stream();
    if (res) {
      fprintf(stderr, "stop_stream succeed\n");
    } else {
      fprintf(stderr, "stop_stream failed\n");
    }
  } else if (c == 'c') {
    uint64_t pos = cl.get_stream_position();
    fprintf(stderr, "stream position %" PRIu64 "\n", pos);
  } else if (c == 'i') {
    op->collection_device_type = CUBEB_DEVICE_TYPE_INPUT;
    fprintf(stderr, "collection device type changed to INPUT\n");
  } else if (c == 'o') {
    op->collection_device_type = CUBEB_DEVICE_TYPE_OUTPUT;
    fprintf(stderr, "collection device type changed to OUTPUT\n");
  } else if (c == 'a') {
    op->collection_device_type = static_cast<cubeb_device_type>(CUBEB_DEVICE_TYPE_INPUT | CUBEB_DEVICE_TYPE_OUTPUT);
    fprintf(stderr, "collection device type changed to INPUT | OUTPUT\n");
  } else if (c == 'k') {
    op->collection_device_type = CUBEB_DEVICE_TYPE_UNKNOWN;
    fprintf(stderr, "collection device type changed to UNKNOWN\n");
  } else if (c == 'r') {
    bool res = cl.register_device_collection_changed(op->collection_device_type);
    if (res) {
      fprintf(stderr, "register_device_collection_changed succeed\n");
    } else {
      fprintf(stderr, "register_device_collection_changed failed\n");
    }
  } else if (c == 'u') {
    bool res = cl.unregister_device_collection_changed(op->collection_device_type);
    if (res) {
      fprintf(stderr, "unregister_device_collection_changed succeed\n");
    } else {
      fprintf(stderr, "unregister_device_collection_changed failed\n");
    }
  } else {
    fprintf(stderr, "Error: %c is not a valid entry\n", c);
  }

  return true; // Loop up
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
  CoInitialize(nullptr);
#endif

  operation_data op;
  op.pm = PLAYBACK;
  if (argc > 1) {
    if ('r' == argv[1][0]) {
      op.pm = RECORD;
    } else if ('p' == argv[1][0]) {
      op.pm = PLAYBACK;
    } else if ('d' == argv[1][0]) {
      op.pm = DUPLEX;
    } else if ('c' == argv[1][0]) {
      op.pm = COLLECTION_CHANGE;
    }
  }
  op.rate = DEFAULT_RATE;
  if (argc > 2) {
    op.rate = strtoul(argv[2], NULL, 0);
  }

  bool res = false;
  cubeb_client cl;
  cl.activate_log(CUBEB_LOG_DISABLED);
  fprintf(stderr, "Log level is DISABLED\n");
  cl.init();

  op.collection_device_type = CUBEB_DEVICE_TYPE_UNKNOWN;
  fprintf(stderr, "collection device type is UNKNOWN\n");
  if (op.pm == COLLECTION_CHANGE) {
    op.collection_device_type = CUBEB_DEVICE_TYPE_OUTPUT;
    fprintf(stderr, "collection device type changed to OUTPUT\n");
    res = cl.register_device_collection_changed(op.collection_device_type);
    if (res) {
      fprintf(stderr, "register_device_collection_changed succeed\n");
    } else {
      fprintf(stderr, "register_device_collection_changed failed\n");
    }
  } else {
    if (op.pm == PLAYBACK || op.pm == DUPLEX) {
      cl.output_params = {CUBEB_SAMPLE_FLOAT32NE, op.rate, DEFAULT_CHANNELS,
                          CUBEB_LAYOUT_UNDEFINED, CUBEB_STREAM_PREF_NONE};
    }
    if (op.pm == RECORD || op.pm == DUPLEX) {
      cl.input_params = {CUBEB_SAMPLE_FLOAT32NE, op.rate, DEFAULT_CHANNELS,
                         CUBEB_LAYOUT_UNDEFINED, CUBEB_STREAM_PREF_NONE};
    }
    res = cl.init_stream();
    if (!res) {
      fprintf(stderr, "stream_init failed\n");
      return -1;
    }
    fprintf(stderr, "stream_init succeed\n");

    res = cl.start_stream();
    if (res) {
      fprintf(stderr, "stream_start succeed\n");
    } else {
      fprintf(stderr, "stream_init failed\n");
    }
  }

  // User input
  do {
    fprintf(stderr, "press `q` to abort or `h` for help\n");
  } while (choose_action(cl, &op, getchar()));

  cl.destroy();

  return 0;
}
