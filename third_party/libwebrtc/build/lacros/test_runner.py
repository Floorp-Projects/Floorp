#!/usr/bin/env vpython
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script facilitates running tests for lacros on Linux.

  In order to run lacros tests on Linux, please first follow bit.ly/3juQVNJ
  to setup build directory with the lacros-chrome-on-linux build configuration,
  and corresponding test targets are built successfully.

  * Example usages:

  ./build/lacros/test_runner.py test out/lacros/url_unittests
  ./build/lacros/test_runner.py test out/lacros/browser_tests

  The commands above run url_unittests and browser_tests respecitively, and more
  specifically, url_unitests is executed directly while browser_tests is
  executed with the latest version of prebuilt ash-chrome, and the behavior is
  controlled by |_TARGETS_REQUIRE_ASH_CHROME|, and it's worth noting that the
  list is maintained manually, so if you see something is wrong, please upload a
  CL to fix it.

  ./build/lacros/test_runner.py test out/lacros/browser_tests \\
      --gtest_filter=BrowserTest.Title

  The above command only runs 'BrowserTest.Title', and any argument accepted by
  the underlying test binary can be specified in the command.

  ./build/lacros/test_runner.py test out/lacros/browser_tests \\
    --ash-chrome-version=793554

  The above command runs tests with a given version of ash-chrome, which is
  useful to reproduce test failures, the version corresponds to the commit
  position of commits on the master branch, and a list of prebuilt versions can
  be found at: gs://ash-chromium-on-linux-prebuilts/x86_64.

  ./testing/xvfb.py ./build/lacros/test_runner.py test out/lacros/browser_tests

  The above command starts ash-chrome with xvfb instead of an X11 window, and
  it's useful when running tests without a display attached, such as sshing.
"""

import argparse
import os
import logging
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import zipfile

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir))
sys.path.append(os.path.join(_SRC_ROOT, 'third_party', 'depot_tools'))

# Base GS URL to store prebuilt ash-chrome.
_GS_URL_BASE = 'gs://ash-chromium-on-linux-prebuilts/x86_64'

# Latest file version.
_GS_URL_LATEST_FILE = _GS_URL_BASE + '/latest/ash-chromium.txt'

# GS path to the zipped ash-chrome build with any given version.
_GS_ASH_CHROME_PATH = 'ash-chromium.zip'

# Directory to cache downloaded ash-chrome versions to avoid re-downloading.
_PREBUILT_ASH_CHROME_DIR = os.path.join(os.path.dirname(__file__),
                                        'prebuilt_ash_chrome')

# Number of seconds to wait for ash-chrome to start.
ASH_CHROME_TIMEOUT_SECONDS = 10

# List of targets that require ash-chrome as a Wayland server in order to run.
_TARGETS_REQUIRE_ASH_CHROME = [
    'app_shell_unittests',
    'aura_unittests',
    'browser_tests',
    'components_unittests',
    'compositor_unittests',
    'content_unittests',
    'dbus_unittests',
    'extensions_unittests',
    'message_center_unittests',
    'snapshot_unittests',
    'sync_integration_tests',
    'unit_tests',
    'views_unittests',
    'wm_unittests',

    # regex patters.
    '.*_browsertests',
    '.*interactive_ui_tests'
]


def _GetAshChromeDirPath(version):
  """Returns a path to the dir storing the downloaded version of ash-chrome."""
  return os.path.join(_PREBUILT_ASH_CHROME_DIR, version)


def _remove_unused_ash_chrome_versions(version_to_skip):
  """Removes unused ash-chrome versions to save disk space.

  Currently, when an ash-chrome zip is downloaded and unpacked, the atime/mtime
  of the dir and the files are NOW instead of the time when they were built, but
  there is no garanteen it will always be the behavior in the future, so avoid
  removing the current version just in case.

  Args:
    version_to_skip (str): the version to skip removing regardless of its age.
  """
  days = 7
  expiration_duration = 60 * 60 * 24 * days

  for f in os.listdir(_PREBUILT_ASH_CHROME_DIR):
    if f == version_to_skip:
      continue

    p = os.path.join(_PREBUILT_ASH_CHROME_DIR, f)
    if os.path.isfile(p):
      # The prebuilt ash-chrome dir is NOT supposed to contain any files, remove
      # them to keep the directory clean.
      os.remove(p)
      continue

    age = time.time() - os.path.getatime(os.path.join(p, 'chrome'))
    if age > expiration_duration:
      logging.info(
          'Removing ash-chrome: "%s" as it hasn\'t been used in the '
          'past %d days', p, days)
      shutil.rmtree(p)

def _GsutilCopyWithRetry(gs_path, local_name, retry_times=3):
  """Gsutil copy with retry.

  Args:
    gs_path: The gs path for remote location.
    local_name: The local file name.
    retry_times: The total try times if the gsutil call fails.

  Raises:
    RuntimeError: If failed to download the specified version, for example,
        if the version is not present on gcs.
  """
  import download_from_google_storage
  gsutil = download_from_google_storage.Gsutil(
      download_from_google_storage.GSUTIL_DEFAULT_PATH)
  exit_code = 1
  retry = 0
  while exit_code and retry < retry_times:
    retry += 1
    exit_code = gsutil.call('cp', gs_path, local_name)
  if exit_code:
    raise RuntimeError('Failed to download: "%s"' % gs_path)

def _DownloadAshChromeIfNecessary(version, is_download_for_bots=False):
  """Download a given version of ash-chrome if not already exists.

  Currently, a special constant version value is support: "for_bots", the reason
  is that version number is still not pinned to chromium/src, so a constant
  value is needed to make sure that after the builder who downloads and isolates
  ash-chrome, the tester knows where to look for the binary to use.
  Additionally, a is_download_for_bots boolean argument is introduced to
  indicate whether this function is downloading for bots specifically or
  downloading while running tests, and it is needed because when version is
  "for_bots", the expected behavior is different in the 2 scenarios: when
  downloading for bots to isolate, we always want to update for_bots/ to reflect
  the latest version; however, when downloading while running tests, we want to
  skip updating to latest because swarming testers aren't supposed to have
  external network access.
  TODO(crbug.com/1107010): remove the support once ash-chrome version is pinned
  to chromium/src.

  Args:
    version: A string representing the version, such as "793554".

  Raises:
      RuntimeError: If failed to download the specified version, for example,
          if the version is not present on gcs.
  """

  def IsAshChromeDirValid(ash_chrome_dir):
    # This function assumes that once 'chrome' is present, other dependencies
    # will be present as well, it's not always true, for example, if the test
    # runner process gets killed in the middle of unzipping (~2 seconds), but
    # it's unlikely for the assumption to break in practice.
    return os.path.isdir(ash_chrome_dir) and os.path.isfile(
        os.path.join(ash_chrome_dir, 'chrome'))

  ash_chrome_dir = _GetAshChromeDirPath(version)
  if not is_download_for_bots and IsAshChromeDirValid(ash_chrome_dir):
    return

  shutil.rmtree(ash_chrome_dir, ignore_errors=True)
  os.makedirs(ash_chrome_dir)
  with tempfile.NamedTemporaryFile() as tmp:
    gs_version = (_GetLatestVersionOfAshChrome()
                  if is_download_for_bots else version)
    logging.info('Ash-chrome version: %s', gs_version)
    gs_path = _GS_URL_BASE + '/' + gs_version + '/' + _GS_ASH_CHROME_PATH
    _GsutilCopyWithRetry(gs_path, tmp.name)

    # https://bugs.python.org/issue15795. ZipFile doesn't preserve permissions.
    # And in order to workaround the issue, this function is created and used
    # instead of ZipFile.extractall().
    # The solution is copied from:
    # https://stackoverflow.com/questions/42326428/zipfile-in-python-file-permission
    def ExtractFile(zf, info, extract_dir):
      zf.extract(info.filename, path=extract_dir)
      perm = info.external_attr >> 16
      os.chmod(os.path.join(extract_dir, info.filename), perm)

    with zipfile.ZipFile(tmp.name, 'r') as zf:
      # Extra all files instead of just 'chrome' binary because 'chrome' needs
      # other resources and libraries to run.
      for info in zf.infolist():
        ExtractFile(zf, info, ash_chrome_dir)

  _remove_unused_ash_chrome_versions(version)


def _GetLatestVersionOfAshChrome():
  """Returns the latest version of uploaded ash-chrome."""
  with tempfile.NamedTemporaryFile() as tmp:
    _GsutilCopyWithRetry(_GS_URL_LATEST_FILE, tmp.name)
    with open(tmp.name, 'r') as f:
      return f.read().strip()


def _WaitForAshChromeToStart(tmp_xdg_dir, lacros_mojo_socket_file,
                             is_lacros_chrome_browsertests):
  """Waits for Ash-Chrome to be up and running and returns a boolean indicator.

  Determine whether ash-chrome is up and running by checking whether two files
  (lock file + socket) have been created in the |XDG_RUNTIME_DIR| and the lacros
  mojo socket file has been created if running lacros_chrome_browsertests.
  TODO(crbug.com/1107966): Figure out a more reliable hook to determine the
  status of ash-chrome, likely through mojo connection.

  Args:
    tmp_xdg_dir (str): Path to the XDG_RUNTIME_DIR.
    lacros_mojo_socket_file (str): Path to the lacros mojo socket file.
    is_lacros_chrome_browsertests (bool): is running lacros_chrome_browsertests.

  Returns:
    A boolean indicating whether Ash-chrome is up and running.
  """

  def IsAshChromeReady(tmp_xdg_dir, lacros_mojo_socket_file,
                       is_lacros_chrome_browsertests):
    return (len(os.listdir(tmp_xdg_dir)) >= 2
            and (not is_lacros_chrome_browsertests
                 or os.path.exists(lacros_mojo_socket_file)))

  time_counter = 0
  while not IsAshChromeReady(tmp_xdg_dir, lacros_mojo_socket_file,
                             is_lacros_chrome_browsertests):
    time.sleep(0.5)
    time_counter += 0.5
    if time_counter > ASH_CHROME_TIMEOUT_SECONDS:
      break

  return IsAshChromeReady(tmp_xdg_dir, lacros_mojo_socket_file,
                          is_lacros_chrome_browsertests)


def _RunTestWithAshChrome(args, forward_args):
  """Runs tests with ash-chrome.

  Args:
    args (dict): Args for this script.
    forward_args (dict): Args to be forwarded to the test command.
  """
  ash_chrome_version = args.ash_chrome_version or _GetLatestVersionOfAshChrome()
  _DownloadAshChromeIfNecessary(ash_chrome_version)
  logging.info('Ash-chrome version: %s', ash_chrome_version)

  ash_chrome_file = os.path.join(_GetAshChromeDirPath(ash_chrome_version),
                                 'chrome')
  try:
    # Starts Ash-Chrome.
    tmp_xdg_dir_name = tempfile.mkdtemp()
    tmp_ash_data_dir_name = tempfile.mkdtemp()

    # Please refer to below file for how mojo connection is set up in testing.
    # //chrome/browser/chromeos/crosapi/test_mojo_connection_manager.h
    lacros_mojo_socket_file = '%s/lacros.sock' % tmp_ash_data_dir_name
    lacros_mojo_socket_arg = ('--lacros-mojo-socket-for-testing=%s' %
                              lacros_mojo_socket_file)
    is_lacros_chrome_browsertests = (os.path.basename(
        args.command) == 'lacros_chrome_browsertests')

    ash_process = None
    ash_env = os.environ.copy()
    ash_env['XDG_RUNTIME_DIR'] = tmp_xdg_dir_name
    ash_cmd = [
        ash_chrome_file,
        '--user-data-dir=%s' % tmp_ash_data_dir_name,
        '--enable-wayland-server',
        '--no-startup-window',
    ]
    if is_lacros_chrome_browsertests:
      ash_cmd.append(lacros_mojo_socket_arg)

    ash_process_has_started = False
    total_tries = 3
    num_tries = 0
    while not ash_process_has_started and num_tries < total_tries:
      num_tries += 1
      ash_process = subprocess.Popen(ash_cmd, env=ash_env)
      ash_process_has_started = _WaitForAshChromeToStart(
          tmp_xdg_dir_name, lacros_mojo_socket_file,
          is_lacros_chrome_browsertests)
      if ash_process_has_started:
        break

      logging.warning('Starting ash-chrome timed out after %ds',
                      ASH_CHROME_TIMEOUT_SECONDS)
      logging.warning('Printing the output of "ps aux" for debugging:')
      subprocess.call(['ps', 'aux'])
      if ash_process and ash_process.poll() is None:
        ash_process.kill()

    if not ash_process_has_started:
      raise RuntimeError('Timed out waiting for ash-chrome to start')

    # Starts tests.
    if is_lacros_chrome_browsertests:
      forward_args.append(lacros_mojo_socket_arg)

      reason_of_jobs_1 = (
          'multiple clients crosapi is not supported yet (crbug.com/1124490), '
          'lacros_chrome_browsertests has to run tests serially')

      if any('--test-launcher-jobs' in arg for arg in forward_args):
        raise RuntimeError(
            'Specifying "--test-launcher-jobs" is not allowed because %s. '
            'Please remove it and this script will automatically append '
            '"--test-launcher-jobs=1"' % reason_of_jobs_1)

      # TODO(crbug.com/1124490): Run lacros_chrome_browsertests in parallel once
      # the bug is fixed.
      logging.warning('Appending "--test-launcher-jobs=1" because %s',
                      reason_of_jobs_1)
      forward_args.append('--test-launcher-jobs=1')

    test_env = os.environ.copy()
    test_env['EGL_PLATFORM'] = 'surfaceless'
    test_env['XDG_RUNTIME_DIR'] = tmp_xdg_dir_name
    test_process = subprocess.Popen([args.command] + forward_args, env=test_env)
    return test_process.wait()

  finally:
    if ash_process and ash_process.poll() is None:
      ash_process.terminate()
      # Allow process to do cleanup and exit gracefully before killing.
      time.sleep(0.5)
      ash_process.kill()

    shutil.rmtree(tmp_xdg_dir_name, ignore_errors=True)
    shutil.rmtree(tmp_ash_data_dir_name, ignore_errors=True)


def _RunTestDirectly(args, forward_args):
  """Runs tests by invoking the test command directly.

  args (dict): Args for this script.
  forward_args (dict): Args to be forwarded to the test command.
  """
  try:
    p = None
    p = subprocess.Popen([args.command] + forward_args)
    return p.wait()
  finally:
    if p and p.poll() is None:
      p.terminate()
      time.sleep(0.5)
      p.kill()


def _HandleSignal(sig, _):
  """Handles received signals to make sure spawned test process are killed.

  sig (int): An integer representing the received signal, for example SIGTERM.
  """
  logging.warning('Received signal: %d, killing spawned processes', sig)

  # Don't do any cleanup here, instead, leave it to the finally blocks.
  # Assumption is based on https://docs.python.org/3/library/sys.html#sys.exit:
  # cleanup actions specified by finally clauses of try statements are honored.

  # https://tldp.org/LDP/abs/html/exitcodes.html:
  # Exit code 128+n -> Fatal error signal "n".
  sys.exit(128 + sig)


def _RunTest(args, forward_args):
  """Runs tests with given args.

  args (dict): Args for this script.
  forward_args (dict): Args to be forwarded to the test command.

  Raises:
      RuntimeError: If the given test binary doesn't exist or the test runner
          doesn't know how to run it.
  """

  if not os.path.isfile(args.command):
    raise RuntimeError('Specified test command: "%s" doesn\'t exist' %
                       args.command)

  # |_TARGETS_REQUIRE_ASH_CHROME| may not always be accurate as it is updated
  # with a best effort only, therefore, allow the invoker to override the
  # behavior with a specified ash-chrome version, which makes sure that
  # automated CI/CQ builders would always work correctly.
  requires_ash_chrome = any(
      re.match(t, os.path.basename(args.command))
      for t in _TARGETS_REQUIRE_ASH_CHROME)
  if not requires_ash_chrome and not args.ash_chrome_version:
    return _RunTestDirectly(args, forward_args)

  return _RunTestWithAshChrome(args, forward_args)


def Main():
  for sig in (signal.SIGTERM, signal.SIGINT):
    signal.signal(sig, _HandleSignal)

  logging.basicConfig(level=logging.INFO)
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  subparsers = arg_parser.add_subparsers()

  download_parser = subparsers.add_parser(
      'download_for_bots',
      help='Download prebuilt ash-chrome for bots so that tests are hermetic '
      'during execution')
  download_parser.set_defaults(
      func=lambda *_: _DownloadAshChromeIfNecessary('for_bots', True))

  test_parser = subparsers.add_parser('test', help='Run tests')
  test_parser.set_defaults(func=_RunTest)

  test_parser.add_argument(
      'command',
      help='A single command to invoke the tests, for example: '
      '"./url_unittests". Any argument unknown to this test runner script will '
      'be forwarded to the command, for example: "--gtest_filter=Suite.Test"')

  test_parser.add_argument(
      '-a',
      '--ash-chrome-version',
      type=str,
      help='Version of ash_chrome to use for testing, for example: "793554", '
      'and the version corresponds to the commit position of commits on the '
      'master branch. If not specified, will use the latest version available')

  args = arg_parser.parse_known_args()
  return args[0].func(args[0], args[1])


if __name__ == '__main__':
  sys.exit(Main())
