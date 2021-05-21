#!/usr/bin/env python3
"""Helper for running tests for all platforms.

One-time setup:
sudo apt-get install npm
sudo npm install -g npm jsvu

Usage:
third_party/highway/test.py [--test=memory_test]

"""

import os
import sys
import tarfile
import tempfile
import argparse
import subprocess


def run_subprocess(args, work_dir):
  """Runs subprocess and checks for success."""
  process = subprocess.Popen(args, cwd=work_dir)
  process.communicate()
  assert process.returncode == 0


def print_status(name):
  print("=" * 60, name)


def run_blaze_tests(work_dir, target, desired_config, config_name, blazerc,
                    config):
  """Builds and runs via blaze or returns 0 if skipped."""
  if desired_config is not None and desired_config != config_name:
    return 0
  print_status(config_name)
  default_config = ["-c", "opt", "--copt=-DHWY_COMPILE_ALL_ATTAINABLE"]
  args = ["blaze"] + blazerc + ["test", ":" + target] + config + default_config
  run_subprocess(args, work_dir)
  return 1


def run_wasm_tests(work_dir, target, desired_config, config_name, options):
  """Runs wasm via blaze/v8, or returns 0 if skipped."""
  if desired_config is not None and desired_config != config_name:
    return 0
  args = [options.v8, "--no-liftoff", "--experimental-wasm-simd"]
  # Otherwise v8 returns 0 even after compile failures!
  args.append("--no-wasm-async-compilation")
  if options.profile:
    args.append("--perf-basic-prof-only-functions")

  num_tests_run = 0
  skipped = []

  # Build (no blazerc to avoid failures caused by local .blazerc)
  run_subprocess([
      "blaze", "--blazerc=/dev/null", "build", "--config=wasm",
      "--features=wasm_simd", ":" + target
  ], work_dir)

  path = "blaze-bin/third_party/highway/"
  TEST_SUFFIX = "_test"
  for test in os.listdir(path):
    # Only test binaries (avoids non-tar files)
    if not test.endswith(TEST_SUFFIX):
      continue

    # Skip directories
    tar_pathname = os.path.join(path, test)
    if os.path.isdir(tar_pathname):
      continue

    # Only the desired test (if given)
    if options.test is not None and options.test != test:
      skipped.append(test[0:-len(TEST_SUFFIX)])
      continue

    with tempfile.TemporaryDirectory() as extract_dir:
      with tarfile.open(tar_pathname, mode="r:") as tar:
        tar.extractall(extract_dir)

      test_args = args + [os.path.join(extract_dir, test) + ".js"]
      run_subprocess(test_args, extract_dir)
      num_tests_run += 1

  print("Finished", num_tests_run, "; skipped", ",".join(skipped))
  assert (num_tests_run != 0)
  return 1


def main(args):
  parser = argparse.ArgumentParser(description="Run test(s)")
  parser.add_argument("--v8", help="Pathname to v8 (default ~/jsvu/v8)")
  parser.add_argument("--test", help="Which test to run (defaults to all)")
  parser.add_argument("--config", help="Which config to run (defaults to all)")
  parser.add_argument("--profile", action="store_true", help="Enable profiling")
  options = parser.parse_args(args)
  if options.v8 is None:
    options.v8 = os.path.join(os.getenv("HOME"), ".jsvu", "v8")

  work_dir = os.path.dirname(os.path.realpath(__file__))
  target = "hwy_ops_tests" if options.test is None else options.test

  num_config = 0
  num_config += run_blaze_tests(work_dir, target, options.config, "x86", [], [])
  num_config += run_blaze_tests(
      work_dir, target, options.config, "rvv",
      ["--blazerc=../../third_party/unsupported_toolchains/mpu/blazerc"],
      ["--config=mpu64_gcc"])
  num_config += run_blaze_tests(work_dir, target, options.config, "arm8", [],
                                ["--config=android_arm64"])
  num_config += run_blaze_tests(work_dir, target, options.config, "arm7", [], [
      "--config=android_arm", "--copt=-mfpu=neon-vfpv4",
      "--copt=-mfloat-abi=softfp"
  ])
  num_config += run_blaze_tests(work_dir, target, options.config, "msvc", [],
                                ["--config=msvc"])
  num_config += run_wasm_tests(work_dir, target, options.config, "wasm",
                               options)

  if num_config == 0:
    print_status("ERROR: unknown --config=%s, omit to see valid names" %
                 (options.config,))
  else:
    print_status("done")


if __name__ == "__main__":
  main(sys.argv[1:])
