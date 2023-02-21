#!/usr/bin/env vpython3
# Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""
This script is the wrapper that runs the "shared_screencast_screen" test.
"""

import argparse
import json
import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# Get rid of "modules/desktop_capture/linux/wayland/test"
ROOT_DIR = os.path.normpath(
    os.path.join(SCRIPT_DIR, os.pardir, os.pardir, os.pardir, os.pardir,
                 os.pardir))


def _ParseArgs():
  parser = argparse.ArgumentParser(
      description='Run shared_screencast_screen test.')
  parser.add_argument('build_dir',
                      help='Path to the build directory (e.g. out/Release).')
  # Unused args
  # We just need to avoid passing these to the test
  parser.add_argument(
      '--isolated-script-test-perf-output',
      default=None,
      help='Path to store perf results in histogram proto format.')
  parser.add_argument(
      '--isolated-script-test-output',
      default=None,
      help='Path to output JSON file which Chromium infra requires.')

  return parser.parse_known_args()


def _GetPipeWireDir():
  pipewire_dir = os.path.join(ROOT_DIR, 'third_party', 'pipewire',
                              'linux-amd64')

  if not os.path.isdir(pipewire_dir):
    pipewire_dir = None

  return pipewire_dir


def _ConfigurePipeWirePaths(path):
  library_dir = os.path.join(path, 'lib64')
  pipewire_binary_dir = os.path.join(path, 'bin')
  pipewire_config_prefix = os.path.join(path, 'share', 'pipewire')
  pipewire_module_dir = os.path.join(library_dir, 'pipewire-0.3')
  spa_plugin_dir = os.path.join(library_dir, 'spa-0.2')
  media_session_config_dir = os.path.join(pipewire_config_prefix,
                                          'media-session.d')

  env_vars = os.environ
  env_vars['LD_LIBRARY_PATH'] = library_dir
  env_vars['PIPEWIRE_CONFIG_PREFIX'] = pipewire_config_prefix
  env_vars['PIPEWIRE_MODULE_DIR'] = pipewire_module_dir
  env_vars['SPA_PLUGIN_DIR'] = spa_plugin_dir
  env_vars['MEDIA_SESSION_CONFIG_DIR'] = media_session_config_dir
  env_vars['PIPEWIRE_RUNTIME_DIR'] = '/tmp'
  env_vars['PATH'] = env_vars['PATH'] + ':' + pipewire_binary_dir


def main():
  args, extra_args = _ParseArgs()

  pipewire_dir = _GetPipeWireDir()

  if pipewire_dir is None:
    return 1

  _ConfigurePipeWirePaths(pipewire_dir)

  pipewire_process = subprocess.Popen(["pipewire"], stdout=None)
  pipewire_media_session_process = subprocess.Popen(["pipewire-media-session"],
                                                    stdout=None)

  test_command = os.path.join(args.build_dir, 'shared_screencast_stream_test')
  pipewire_test_process = subprocess.run([test_command] + extra_args,
                                         stdout=True,
                                         check=False)

  return_value = pipewire_test_process.returncode

  pipewire_media_session_process.terminate()
  pipewire_process.terminate()

  if args.isolated_script_test_output:
    with open(args.isolated_script_test_output, 'w') as f:
      json.dump({"version": 3}, f)

  return return_value


if __name__ == '__main__':
  sys.exit(main())
