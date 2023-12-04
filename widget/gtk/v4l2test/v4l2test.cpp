/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#  include <sys/videoio.h>
#elif defined(__sun)
#  include <sys/videodev2.h>
#else
#  include <linux/videodev2.h>
#endif
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <stdarg.h>

#if defined(MOZ_ASAN) || defined(FUZZING)
#  include <signal.h>
#endif

#include "mozilla/ScopeExit.h"

#ifdef __SUNPRO_CC
#  include <stdio.h>
#endif

#include "mozilla/GfxInfoUtils.h"

// Print test results to stdout and logging to stderr
#define OUTPUT_PIPE 1

// Convert an integer pixfmt to a 4-character string.  str must have a length
// of at least 5 to include null-termination.
static void v4l2_pixfmt_to_str(uint32_t pixfmt, char* str) {
  for (int i = 0; i < 4; i++) {
    str[i] = (pixfmt >> (i * 8)) & 0xff;
  }
  str[4] = 0;
}

// Enumerate the buffer formats supported on a V4L2 buffer queue.  aTypeStr
// is the queue type, i.e. CAPTURE or OUTPUT.
static void v4l2_enumfmt(int aFd, int aType, const char* aTypeStr) {
  struct v4l2_fmtdesc fmt {};
  char pix_fmt_str[5];
  fmt.type = aType;
  record_value("V4L2_%s_FMTS\n", aTypeStr);
  for (fmt.index = 0;; fmt.index++) {
    int result = ioctl(aFd, VIDIOC_ENUM_FMT, &fmt);
    if (result < 0) {
      break;
    }
    v4l2_pixfmt_to_str(fmt.pixelformat, pix_fmt_str);
    record_value(" %s", pix_fmt_str);
  }
  record_value("\n");
}

// Probe a V4L2 device to work out what it supports
static void v4l2_check_device(const char* aVideoDevice) {
  int fd = -1;
  int result = -1;

  log("v4l2test probing device '%s'\n", aVideoDevice);

  auto autoRelease = mozilla::MakeScopeExit([&] {
    if (fd >= 0) {
      close(fd);
    }
  });

  fd = open(aVideoDevice, O_RDWR | O_NONBLOCK, 0);
  if (fd < 0) {
    record_value("ERROR\nV4L2 failed to open device %s: %s\n", aVideoDevice,
                 strerror(errno));
    return;
  }

  struct v4l2_capability cap {};
  result = ioctl(fd, VIDIOC_QUERYCAP, &cap);
  if (result < 0) {
    record_value("ERROR\nV4L2 device %s failed to query capabilities\n",
                 aVideoDevice);
    return;
  }
  log("v4l2test driver %s card %s bus_info %s version %d\n", cap.driver,
      cap.card, cap.bus_info, cap.version);

  if (!(cap.capabilities & V4L2_CAP_DEVICE_CAPS)) {
    record_value("ERROR\nV4L2 device %s does not support DEVICE_CAPS\n",
                 aVideoDevice);
    return;
  }

  if (!(cap.device_caps & V4L2_CAP_STREAMING)) {
    record_value("ERROR\nV4L2 device %s does not support V4L2_CAP_STREAMING\n",
                 aVideoDevice);
    return;
  }

  // Work out whether the device supports planar or multiplaner bitbuffers and
  // framebuffers
  bool splane = cap.device_caps & V4L2_CAP_VIDEO_M2M;
  bool mplane = cap.device_caps & V4L2_CAP_VIDEO_M2M_MPLANE;
  if (!splane && !mplane) {
    record_value("ERROR\nV4L2 device %s does not support M2M modes\n",
                 aVideoDevice);
    // (It's probably a webcam!)
    return;
  }
  record_value("V4L2_SPLANE\n%s\n", splane ? "TRUE" : "FALSE");
  record_value("V4L2_MPLANE\n%s\n", mplane ? "TRUE" : "FALSE");

  // Now check the formats supported for CAPTURE and OUTPUT buffers.
  // For a V4L2-M2M decoder, OUTPUT is actually the bitbuffers we put in and
  // CAPTURE is the framebuffers we get out.
  v4l2_enumfmt(
      fd,
      mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE,
      "CAPTURE");
  v4l2_enumfmt(
      fd,
      mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT,
      "OUTPUT");

  record_value("V4L2_SUPPORTED\nTRUE\n");
}

static void PrintUsage() {
  printf(
      "Firefox V4L2-M2M probe utility\n"
      "\n"
      "usage: v4l2test [options]\n"
      "\n"
      "Options:\n"
      "\n"
      "  -h --help                 show this message\n"
      "  -d --device device        Probe a v4l2 device (e.g. /dev/video10)\n"
      "\n");
}

int main(int argc, char** argv) {
  struct option longOptions[] = {{"help", no_argument, nullptr, 'h'},
                                 {"device", required_argument, nullptr, 'd'},
                                 {nullptr, 0, nullptr, 0}};
  const char* shortOptions = "hd:";
  int c;
  const char* device = nullptr;
  while ((c = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) !=
         -1) {
    switch (c) {
      case 'd':
        device = optarg;
        break;
      case 'h':
      default:
        break;
    }
  }

  if (device) {
#if defined(MOZ_ASAN) || defined(FUZZING)
    // If handle_segv=1 (default), then glxtest crash will print a sanitizer
    // report which can confuse the harness in fuzzing automation.
    signal(SIGSEGV, SIG_DFL);
#endif
    const char* env = getenv("MOZ_GFX_DEBUG");
    enable_logging = env && *env == '1';
    output_pipe = OUTPUT_PIPE;
    if (!enable_logging) {
      close_logging();
    }
    v4l2_check_device(device);
    record_flush();
    return EXIT_SUCCESS;
  }
  PrintUsage();
  return 0;
}
