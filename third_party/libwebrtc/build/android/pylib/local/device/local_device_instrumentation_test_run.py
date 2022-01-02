# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import contextlib
import copy
import hashlib
import json
import logging
import os
import posixpath
import re
import shutil
import sys
import tempfile
import time

from devil import base_error
from devil.android import crash_handler
from devil.android import device_errors
from devil.android import device_temp_file
from devil.android import flag_changer
from devil.android.sdk import shared_prefs
from devil.android import logcat_monitor
from devil.android.tools import system_app
from devil.android.tools import webview_app
from devil.utils import reraiser_thread
from incremental_install import installer
from pylib import constants
from pylib import valgrind_tools
from pylib.base import base_test_result
from pylib.base import output_manager
from pylib.constants import host_paths
from pylib.instrumentation import instrumentation_test_instance
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_test_run
from pylib.output import remote_output_manager
from pylib.utils import chrome_proxy_utils
from pylib.utils import gold_utils
from pylib.utils import instrumentation_tracing
from pylib.utils import shared_preference_utils
from py_trace_event import trace_event
from py_trace_event import trace_time
from py_utils import contextlib_ext
from py_utils import tempfile_ext
import tombstones

with host_paths.SysPath(
    os.path.join(host_paths.DIR_SOURCE_ROOT, 'third_party'), 0):
  import jinja2  # pylint: disable=import-error
  import markupsafe  # pylint: disable=import-error,unused-import


_JINJA_TEMPLATE_DIR = os.path.join(
    host_paths.DIR_SOURCE_ROOT, 'build', 'android', 'pylib', 'instrumentation')
_JINJA_TEMPLATE_FILENAME = 'render_test.html.jinja'

_WPR_GO_LINUX_X86_64_PATH = os.path.join(host_paths.DIR_SOURCE_ROOT,
                                         'third_party', 'webpagereplay', 'bin',
                                         'linux', 'x86_64', 'wpr')

_TAG = 'test_runner_py'

TIMEOUT_ANNOTATIONS = [
  ('Manual', 10 * 60 * 60),
  ('IntegrationTest', 30 * 60),
  ('External', 10 * 60),
  ('EnormousTest', 10 * 60),
  ('LargeTest', 5 * 60),
  ('MediumTest', 3 * 60),
  ('SmallTest', 1 * 60),
]

LOGCAT_FILTERS = ['*:e', 'chromium:v', 'cr_*:v', 'DEBUG:I',
                  'StrictMode:D', '%s:I' % _TAG]

EXTRA_SCREENSHOT_FILE = (
    'org.chromium.base.test.ScreenshotOnFailureStatement.ScreenshotFile')

EXTRA_UI_CAPTURE_DIR = (
    'org.chromium.base.test.util.Screenshooter.ScreenshotDir')

EXTRA_TRACE_FILE = ('org.chromium.base.test.BaseJUnit4ClassRunner.TraceFile')

_EXTRA_TEST_LIST = (
    'org.chromium.base.test.BaseChromiumAndroidJUnitRunner.TestList')

_EXTRA_PACKAGE_UNDER_TEST = ('org.chromium.chrome.test.pagecontroller.rules.'
                             'ChromeUiApplicationTestRule.PackageUnderTest')

FEATURE_ANNOTATION = 'Feature'
RENDER_TEST_FEATURE_ANNOTATION = 'RenderTest'
WPR_ARCHIVE_FILE_PATH_ANNOTATION = 'WPRArchiveDirectory'
WPR_RECORD_REPLAY_TEST_FEATURE_ANNOTATION = 'WPRRecordReplayTest'

_DEVICE_GOLD_DIR = 'skia_gold'
# A map of Android product models to SDK ints.
RENDER_TEST_MODEL_SDK_CONFIGS = {
    'Nexus 5X': [23],
}

_TEST_BATCH_MAX_GROUP_SIZE = 256


@contextlib.contextmanager
def _LogTestEndpoints(device, test_name):
  device.RunShellCommand(
      ['log', '-p', 'i', '-t', _TAG, 'START %s' % test_name],
      check_return=True)
  try:
    yield
  finally:
    device.RunShellCommand(
        ['log', '-p', 'i', '-t', _TAG, 'END %s' % test_name],
        check_return=True)

# TODO(jbudorick): Make this private once the instrumentation test_runner
# is deprecated.
def DidPackageCrashOnDevice(package_name, device):
  # Dismiss any error dialogs. Limit the number in case we have an error
  # loop or we are failing to dismiss.
  try:
    for _ in xrange(10):
      package = device.DismissCrashDialogIfNeeded(timeout=10, retries=1)
      if not package:
        return False
      # Assume test package convention of ".test" suffix
      if package in package_name:
        return True
  except device_errors.CommandFailedError:
    logging.exception('Error while attempting to dismiss crash dialog.')
  return False


_CURRENT_FOCUS_CRASH_RE = re.compile(
    r'\s*mCurrentFocus.*Application (Error|Not Responding): (\S+)}')


def _GetTargetPackageName(test_apk):
  # apk_under_test does not work for smoke tests, where it is set to an
  # apk that is not listed as the targetPackage in the test apk's manifest.
  return test_apk.GetAllInstrumentations()[0]['android:targetPackage']


class LocalDeviceInstrumentationTestRun(
    local_device_test_run.LocalDeviceTestRun):
  def __init__(self, env, test_instance):
    super(LocalDeviceInstrumentationTestRun, self).__init__(
        env, test_instance)
    self._chrome_proxy = None
    self._context_managers = collections.defaultdict(list)
    self._flag_changers = {}
    self._render_tests_device_output_dir = None
    self._shared_prefs_to_restore = []
    self._skia_gold_session_manager = None
    self._skia_gold_work_dir = None

  #override
  def TestPackage(self):
    return self._test_instance.suite

  #override
  def SetUp(self):
    target_package = _GetTargetPackageName(self._test_instance.test_apk)

    @local_device_environment.handle_shard_failures_with(
        self._env.DenylistDevice)
    @trace_event.traced
    def individual_device_set_up(device, host_device_tuples):
      steps = []

      if self._test_instance.replace_system_package:
        @trace_event.traced
        def replace_package(dev):
          # We need the context manager to be applied before modifying any
          # shared preference files in case the replacement APK needs to be
          # set up, and it needs to be applied while the test is running.
          # Thus, it needs to be applied early during setup, but must still be
          # applied during _RunTest, which isn't possible using 'with' without
          # applying the context manager up in test_runner. Instead, we
          # manually invoke its __enter__ and __exit__ methods in setup and
          # teardown.
          system_app_context = system_app.ReplaceSystemApp(
              dev, self._test_instance.replace_system_package.package,
              self._test_instance.replace_system_package.replacement_apk)
          # Pylint is not smart enough to realize that this field has
          # an __enter__ method, and will complain loudly.
          # pylint: disable=no-member
          system_app_context.__enter__()
          # pylint: enable=no-member
          self._context_managers[str(dev)].append(system_app_context)

        steps.append(replace_package)

      if self._test_instance.system_packages_to_remove:

        @trace_event.traced
        def remove_packages(dev):
          logging.info('Attempting to remove system packages %s',
                       self._test_instance.system_packages_to_remove)
          system_app.RemoveSystemApps(
              dev, self._test_instance.system_packages_to_remove)
          logging.info('Done removing system packages')

        # This should be at the front in case we're removing the package to make
        # room for another APK installation later on. Since we disallow
        # concurrent adb with this option specified, this should be safe.
        steps.insert(0, remove_packages)

      if self._test_instance.use_webview_provider:
        @trace_event.traced
        def use_webview_provider(dev):
          # We need the context manager to be applied before modifying any
          # shared preference files in case the replacement APK needs to be
          # set up, and it needs to be applied while the test is running.
          # Thus, it needs to be applied early during setup, but must still be
          # applied during _RunTest, which isn't possible using 'with' without
          # applying the context manager up in test_runner. Instead, we
          # manually invoke its __enter__ and __exit__ methods in setup and
          # teardown.
          webview_context = webview_app.UseWebViewProvider(
              dev, self._test_instance.use_webview_provider)
          # Pylint is not smart enough to realize that this field has
          # an __enter__ method, and will complain loudly.
          # pylint: disable=no-member
          webview_context.__enter__()
          # pylint: enable=no-member
          self._context_managers[str(dev)].append(webview_context)

        steps.append(use_webview_provider)

      def install_helper(apk,
                         modules=None,
                         fake_modules=None,
                         permissions=None,
                         additional_locales=None):

        @instrumentation_tracing.no_tracing
        @trace_event.traced
        def install_helper_internal(d, apk_path=None):
          # pylint: disable=unused-argument
          d.Install(apk,
                    modules=modules,
                    fake_modules=fake_modules,
                    permissions=permissions,
                    additional_locales=additional_locales)

        return install_helper_internal

      def incremental_install_helper(apk, json_path, permissions):

        @trace_event.traced
        def incremental_install_helper_internal(d, apk_path=None):
          # pylint: disable=unused-argument
          installer.Install(d, json_path, apk=apk, permissions=permissions)
        return incremental_install_helper_internal

      permissions = self._test_instance.test_apk.GetPermissions()
      if self._test_instance.test_apk_incremental_install_json:
        steps.append(incremental_install_helper(
                         self._test_instance.test_apk,
                         self._test_instance.
                             test_apk_incremental_install_json,
                         permissions))
      else:
        steps.append(
            install_helper(
                self._test_instance.test_apk, permissions=permissions))

      steps.extend(
          install_helper(apk) for apk in self._test_instance.additional_apks)

      # The apk under test needs to be installed last since installing other
      # apks after will unintentionally clear the fake module directory.
      # TODO(wnwen): Make this more robust, fix crbug.com/1010954.
      if self._test_instance.apk_under_test:
        permissions = self._test_instance.apk_under_test.GetPermissions()
        if self._test_instance.apk_under_test_incremental_install_json:
          steps.append(
              incremental_install_helper(
                  self._test_instance.apk_under_test,
                  self._test_instance.apk_under_test_incremental_install_json,
                  permissions))
        else:
          steps.append(
              install_helper(self._test_instance.apk_under_test,
                             self._test_instance.modules,
                             self._test_instance.fake_modules, permissions,
                             self._test_instance.additional_locales))

      @trace_event.traced
      def set_debug_app(dev):
        # Set debug app in order to enable reading command line flags on user
        # builds
        cmd = ['am', 'set-debug-app', '--persistent']
        if self._test_instance.wait_for_java_debugger:
          cmd.append('-w')
        cmd.append(target_package)
        dev.RunShellCommand(cmd, check_return=True)

      @trace_event.traced
      def edit_shared_prefs(dev):
        for setting in self._test_instance.edit_shared_prefs:
          shared_pref = shared_prefs.SharedPrefs(
              dev, setting['package'], setting['filename'],
              use_encrypted_path=setting.get('supports_encrypted_path', False))
          pref_to_restore = copy.copy(shared_pref)
          pref_to_restore.Load()
          self._shared_prefs_to_restore.append(pref_to_restore)

          shared_preference_utils.ApplySharedPreferenceSetting(
              shared_pref, setting)

      @trace_event.traced
      def set_vega_permissions(dev):
        # Normally, installation of VrCore automatically grants storage
        # permissions. However, since VrCore is part of the system image on
        # the Vega standalone headset, we don't install the APK as part of test
        # setup. Instead, grant the permissions here so that it can take
        # screenshots.
        if dev.product_name == 'vega':
          dev.GrantPermissions('com.google.vr.vrcore', [
              'android.permission.WRITE_EXTERNAL_STORAGE',
              'android.permission.READ_EXTERNAL_STORAGE'
          ])

      @instrumentation_tracing.no_tracing
      def push_test_data(dev):
        device_root = posixpath.join(dev.GetExternalStoragePath(),
                                     'chromium_tests_root')
        host_device_tuples_substituted = [
            (h, local_device_test_run.SubstituteDeviceRoot(d, device_root))
            for h, d in host_device_tuples]
        logging.info('Pushing data dependencies.')
        for h, d in host_device_tuples_substituted:
          logging.debug('  %r -> %r', h, d)
        local_device_environment.place_nomedia_on_device(dev, device_root)
        dev.PushChangedFiles(host_device_tuples_substituted,
                             delete_device_stale=True)
        if not host_device_tuples_substituted:
          dev.RunShellCommand(['rm', '-rf', device_root], check_return=True)
          dev.RunShellCommand(['mkdir', '-p', device_root], check_return=True)

      @trace_event.traced
      def create_flag_changer(dev):
        if self._test_instance.flags:
          self._CreateFlagChangerIfNeeded(dev)
          logging.debug('Attempting to set flags: %r',
                        self._test_instance.flags)
          self._flag_changers[str(dev)].AddFlags(self._test_instance.flags)

        valgrind_tools.SetChromeTimeoutScale(
            dev, self._test_instance.timeout_scale)

      steps += [
          set_debug_app, edit_shared_prefs, push_test_data, create_flag_changer,
          set_vega_permissions
      ]

      def bind_crash_handler(step, dev):
        return lambda: crash_handler.RetryOnSystemCrash(step, dev)

      steps = [bind_crash_handler(s, device) for s in steps]

      try:
        if self._env.concurrent_adb:
          reraiser_thread.RunAsync(steps)
        else:
          for step in steps:
            step()
        if self._test_instance.store_tombstones:
          tombstones.ClearAllTombstones(device)
      except device_errors.CommandFailedError:
        if not device.IsOnline():
          raise

        # A bugreport can be large and take a while to generate, so only capture
        # one if we're using a remote manager.
        if isinstance(
            self._env.output_manager,
            remote_output_manager.RemoteOutputManager):
          logging.error(
              'Error when setting up device for tests. Taking a bugreport for '
              'investigation. This may take a while...')
          report_name = '%s.bugreport' % device.serial
          with self._env.output_manager.ArchivedTempfile(
              report_name, 'bug_reports') as report_file:
            device.TakeBugReport(report_file.name)
          logging.error('Bug report saved to %s', report_file.Link())
        raise

    self._env.parallel_devices.pMap(
        individual_device_set_up,
        self._test_instance.GetDataDependencies())
    # Created here instead of on a per-test basis so that the downloaded
    # expectations can be re-used between tests, saving a significant amount
    # of time.
    self._skia_gold_work_dir = tempfile.mkdtemp()
    self._skia_gold_session_manager = gold_utils.AndroidSkiaGoldSessionManager(
        self._skia_gold_work_dir, self._test_instance.skia_gold_properties)
    if self._test_instance.wait_for_java_debugger:
      logging.warning('*' * 80)
      logging.warning('Waiting for debugger to attach to process: %s',
                      target_package)
      logging.warning('*' * 80)

  #override
  def TearDown(self):
    shutil.rmtree(self._skia_gold_work_dir)
    self._skia_gold_work_dir = None
    self._skia_gold_session_manager = None
    # By default, teardown will invoke ADB. When receiving SIGTERM due to a
    # timeout, there's a high probability that ADB is non-responsive. In these
    # cases, sending an ADB command will potentially take a long time to time
    # out. Before this happens, the process will be hard-killed for not
    # responding to SIGTERM fast enough.
    if self._received_sigterm:
      return

    @local_device_environment.handle_shard_failures_with(
        self._env.DenylistDevice)
    @trace_event.traced
    def individual_device_tear_down(dev):
      if str(dev) in self._flag_changers:
        self._flag_changers[str(dev)].Restore()

      # Remove package-specific configuration
      dev.RunShellCommand(['am', 'clear-debug-app'], check_return=True)

      valgrind_tools.SetChromeTimeoutScale(dev, None)

      # Restore any shared preference files that we stored during setup.
      # This should be run sometime before the replace package contextmanager
      # gets exited so we don't have to special case restoring files of
      # replaced system apps.
      for pref_to_restore in self._shared_prefs_to_restore:
        pref_to_restore.Commit(force_commit=True)

      # Context manager exit handlers are applied in reverse order
      # of the enter handlers.
      for context in reversed(self._context_managers[str(dev)]):
        # See pylint-related comment above with __enter__()
        # pylint: disable=no-member
        context.__exit__(*sys.exc_info())
        # pylint: enable=no-member

    self._env.parallel_devices.pMap(individual_device_tear_down)

  def _CreateFlagChangerIfNeeded(self, device):
    if str(device) not in self._flag_changers:
      cmdline_file = 'test-cmdline-file'
      if self._test_instance.use_apk_under_test_flags_file:
        if self._test_instance.package_info:
          cmdline_file = self._test_instance.package_info.cmdline_file
        else:
          raise Exception('No PackageInfo found but'
                          '--use-apk-under-test-flags-file is specified.')
      self._flag_changers[str(device)] = flag_changer.FlagChanger(
          device, cmdline_file)

  #override
  def _CreateShards(self, tests):
    return tests

  #override
  def _GetTests(self):
    if self._test_instance.junit4_runner_supports_listing:
      raw_tests = self._GetTestsFromRunner()
      tests = self._test_instance.ProcessRawTests(raw_tests)
    else:
      tests = self._test_instance.GetTests()
    tests = self._ApplyExternalSharding(
        tests, self._test_instance.external_shard_index,
        self._test_instance.total_external_shards)
    return tests

  #override
  def _GroupTests(self, tests):
    batched_tests = dict()
    other_tests = []
    for test in tests:
      if 'Batch' in test['annotations'] and 'RequiresRestart' not in test[
          'annotations']:
        batch_name = test['annotations']['Batch']['value']
        if not batch_name:
          batch_name = test['class']
        if not batch_name in batched_tests:
          batched_tests[batch_name] = []
        batched_tests[batch_name].append(test)
      else:
        other_tests.append(test)

    all_tests = []
    for _, tests in batched_tests.items():
      tests.sort()  # Ensure a consistent ordering across external shards.
      all_tests.extend([
          tests[i:i + _TEST_BATCH_MAX_GROUP_SIZE]
          for i in range(0, len(tests), _TEST_BATCH_MAX_GROUP_SIZE)
      ])
    all_tests.extend(other_tests)
    return all_tests

  #override
  def _GetUniqueTestName(self, test):
    return instrumentation_test_instance.GetUniqueTestName(test)

  #override
  def _RunTest(self, device, test):
    extras = {}

    # Provide package name under test for apk_under_test.
    if self._test_instance.apk_under_test:
      package_name = self._test_instance.apk_under_test.GetPackageName()
      extras[_EXTRA_PACKAGE_UNDER_TEST] = package_name

    flags_to_add = []
    test_timeout_scale = None
    if self._test_instance.coverage_directory:
      coverage_basename = '%s.exec' % (
          '%s_%s_group' % (test[0]['class'], test[0]['method']) if isinstance(
              test, list) else '%s_%s' % (test['class'], test['method']))
      extras['coverage'] = 'true'
      coverage_directory = os.path.join(
          device.GetExternalStoragePath(), 'chrome', 'test', 'coverage')
      if not device.PathExists(coverage_directory):
        device.RunShellCommand(['mkdir', '-p', coverage_directory],
                               check_return=True)
      coverage_device_file = os.path.join(
          coverage_directory, coverage_basename)
      extras['coverageFile'] = coverage_device_file
    # Save screenshot if screenshot dir is specified (save locally) or if
    # a GS bucket is passed (save in cloud).
    screenshot_device_file = device_temp_file.DeviceTempFile(
        device.adb, suffix='.png', dir=device.GetExternalStoragePath())
    extras[EXTRA_SCREENSHOT_FILE] = screenshot_device_file.name

    # Set up the screenshot directory. This needs to be done for each test so
    # that we only get screenshots created by that test. It has to be on
    # external storage since the default location doesn't allow file creation
    # from the instrumentation test app on Android L and M.
    ui_capture_dir = device_temp_file.NamedDeviceTemporaryDirectory(
        device.adb,
        dir=device.GetExternalStoragePath())
    extras[EXTRA_UI_CAPTURE_DIR] = ui_capture_dir.name

    if self._env.trace_output:
      trace_device_file = device_temp_file.DeviceTempFile(
          device.adb, suffix='.json', dir=device.GetExternalStoragePath())
      extras[EXTRA_TRACE_FILE] = trace_device_file.name

    target = '%s/%s' % (self._test_instance.test_package,
                        self._test_instance.junit4_runner_class)
    if isinstance(test, list):

      def name_and_timeout(t):
        n = instrumentation_test_instance.GetTestName(t)
        i = self._GetTimeoutFromAnnotations(t['annotations'], n)
        return (n, i)

      test_names, timeouts = zip(*(name_and_timeout(t) for t in test))

      test_name = instrumentation_test_instance.GetTestName(test[0]) + '_batch'
      extras['class'] = ','.join(test_names)
      test_display_name = test_name
      timeout = sum(timeouts)
    else:
      assert test['is_junit4']
      test_name = instrumentation_test_instance.GetTestName(test)
      test_display_name = self._GetUniqueTestName(test)

      extras['class'] = test_name
      if 'flags' in test and test['flags']:
        flags_to_add.extend(test['flags'])
      timeout = self._GetTimeoutFromAnnotations(
        test['annotations'], test_display_name)

      test_timeout_scale = self._GetTimeoutScaleFromAnnotations(
          test['annotations'])
      if test_timeout_scale and test_timeout_scale != 1:
        valgrind_tools.SetChromeTimeoutScale(
            device, test_timeout_scale * self._test_instance.timeout_scale)

    if self._test_instance.wait_for_java_debugger:
      timeout = None
    logging.info('preparing to run %s: %s', test_display_name, test)

    if _IsRenderTest(test):
      # TODO(mikecase): Add DeviceTempDirectory class and use that instead.
      self._render_tests_device_output_dir = posixpath.join(
          device.GetExternalStoragePath(), 'render_test_output_dir')
      flags_to_add.append('--render-test-output-dir=%s' %
                          self._render_tests_device_output_dir)

    if _IsWPRRecordReplayTest(test):
      wpr_archive_relative_path = _GetWPRArchivePath(test)
      if not wpr_archive_relative_path:
        raise RuntimeError('Could not find the WPR archive file path '
                           'from annotation.')
      wpr_archive_path = os.path.join(host_paths.DIR_SOURCE_ROOT,
                                      wpr_archive_relative_path)
      if not os.path.isdir(wpr_archive_path):
        raise RuntimeError('WPRArchiveDirectory annotation should point '
                           'to a directory only. '
                           '{0} exist: {1}'.format(
                               wpr_archive_path,
                               os.path.exists(wpr_archive_path)))

      # Some linux version does not like # in the name. Replaces it with __.
      archive_path = os.path.join(
          wpr_archive_path,
          _ReplaceUncommonChars(self._GetUniqueTestName(test)) + '.wprgo')

      if not os.path.exists(_WPR_GO_LINUX_X86_64_PATH):
        # If we got to this stage, then we should have
        # checkout_android set.
        raise RuntimeError(
            'WPR Go binary not found at {}'.format(_WPR_GO_LINUX_X86_64_PATH))
      # Tells the server to use the binaries retrieved from CIPD.
      chrome_proxy_utils.ChromeProxySession.SetWPRServerBinary(
          _WPR_GO_LINUX_X86_64_PATH)
      self._chrome_proxy = chrome_proxy_utils.ChromeProxySession()
      self._chrome_proxy.wpr_record_mode = self._test_instance.wpr_record_mode
      self._chrome_proxy.Start(device, archive_path)
      flags_to_add.extend(self._chrome_proxy.GetFlags())

    if flags_to_add:
      self._CreateFlagChangerIfNeeded(device)
      self._flag_changers[str(device)].PushFlags(add=flags_to_add)

    time_ms = lambda: int(time.time() * 1e3)
    start_ms = time_ms()

    with ui_capture_dir:
      with self._ArchiveLogcat(device, test_name) as logcat_file:
        output = device.StartInstrumentation(
            target, raw=True, extras=extras, timeout=timeout, retries=0)

      duration_ms = time_ms() - start_ms

      with contextlib_ext.Optional(
          trace_event.trace('ProcessResults'),
          self._env.trace_output):
        output = self._test_instance.MaybeDeobfuscateLines(output)
        # TODO(jbudorick): Make instrumentation tests output a JSON so this
        # doesn't have to parse the output.
        result_code, result_bundle, statuses = (
            self._test_instance.ParseAmInstrumentRawOutput(output))
        results = self._test_instance.GenerateTestResults(
            result_code, result_bundle, statuses, duration_ms,
            device.product_cpu_abi, self._test_instance.symbolizer)

      if self._env.trace_output:
        self._SaveTraceData(trace_device_file, device, test['class'])


      def restore_flags():
        if flags_to_add:
          self._flag_changers[str(device)].Restore()

      def restore_timeout_scale():
        if test_timeout_scale:
          valgrind_tools.SetChromeTimeoutScale(
              device, self._test_instance.timeout_scale)

      def handle_coverage_data():
        if self._test_instance.coverage_directory:
          try:
            if not os.path.exists(self._test_instance.coverage_directory):
              os.makedirs(self._test_instance.coverage_directory)
            device.PullFile(coverage_device_file,
                            self._test_instance.coverage_directory)
            device.RemovePath(coverage_device_file, True)
          except (OSError, base_error.BaseError) as e:
            logging.warning('Failed to handle coverage data after tests: %s', e)

      def handle_render_test_data():
        if _IsRenderTest(test):
          # Render tests do not cause test failure by default. So we have to
          # check to see if any failure images were generated even if the test
          # does not fail.
          try:
            self._ProcessRenderTestResults(device, results)
          finally:
            device.RemovePath(self._render_tests_device_output_dir,
                              recursive=True,
                              force=True)
            self._render_tests_device_output_dir = None

      def pull_ui_screen_captures():
        screenshots = []
        for filename in device.ListDirectory(ui_capture_dir.name):
          if filename.endswith('.json'):
            screenshots.append(pull_ui_screenshot(filename))
        if screenshots:
          json_archive_name = 'ui_capture_%s_%s.json' % (
              test_name.replace('#', '.'),
              time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime()))
          with self._env.output_manager.ArchivedTempfile(
              json_archive_name, 'ui_capture', output_manager.Datatype.JSON
              ) as json_archive:
            json.dump(screenshots, json_archive)
          _SetLinkOnResults(results, 'ui screenshot', json_archive.Link())

      def pull_ui_screenshot(filename):
        source_dir = ui_capture_dir.name
        json_path = posixpath.join(source_dir, filename)
        json_data = json.loads(device.ReadFile(json_path))
        image_file_path = posixpath.join(source_dir, json_data['location'])
        with self._env.output_manager.ArchivedTempfile(
            json_data['location'], 'ui_capture', output_manager.Datatype.PNG
            ) as image_archive:
          device.PullFile(image_file_path, image_archive.name)
        json_data['image_link'] = image_archive.Link()
        return json_data

      def stop_chrome_proxy():
        # Removes the port forwarding
        if self._chrome_proxy:
          self._chrome_proxy.Stop(device)
          if not self._chrome_proxy.wpr_replay_mode:
            logging.info('WPR Record test generated archive file %s',
                         self._chrome_proxy.wpr_archive_path)
          self._chrome_proxy = None


      # While constructing the TestResult objects, we can parallelize several
      # steps that involve ADB. These steps should NOT depend on any info in
      # the results! Things such as whether the test CRASHED have not yet been
      # determined.
      post_test_steps = [
          restore_flags, restore_timeout_scale, stop_chrome_proxy,
          handle_coverage_data, handle_render_test_data, pull_ui_screen_captures
      ]
      if self._env.concurrent_adb:
        reraiser_thread.RunAsync(post_test_steps)
      else:
        for step in post_test_steps:
          step()

    if logcat_file:
      _SetLinkOnResults(results, 'logcat', logcat_file.Link())

    # Update the result name if the test used flags.
    if flags_to_add:
      for r in results:
        if r.GetName() == test_name:
          r.SetName(test_display_name)

    # Add UNKNOWN results for any missing tests.
    iterable_test = test if isinstance(test, list) else [test]
    test_names = set(self._GetUniqueTestName(t) for t in iterable_test)
    results_names = set(r.GetName() for r in results)
    results.extend(
        base_test_result.BaseTestResult(u, base_test_result.ResultType.UNKNOWN)
        for u in test_names.difference(results_names))

    # Update the result type if we detect a crash.
    try:
      if DidPackageCrashOnDevice(self._test_instance.test_package, device):
        for r in results:
          if r.GetType() == base_test_result.ResultType.UNKNOWN:
            r.SetType(base_test_result.ResultType.CRASH)
    except device_errors.CommandTimeoutError:
      logging.warning('timed out when detecting/dismissing error dialogs')
      # Attach screenshot to the test to help with debugging the dialog boxes.
      self._SaveScreenshot(device, screenshot_device_file, test_display_name,
                           results, 'dialog_box_screenshot')

    # The crash result can be set above or in
    # InstrumentationTestRun.GenerateTestResults. If a test crashes,
    # subprocesses such as the one used by EmbeddedTestServerRule can be left
    # alive in a bad state, so kill them now.
    for r in results:
      if r.GetType() == base_test_result.ResultType.CRASH:
        for apk in self._test_instance.additional_apks:
          device.ForceStop(apk.GetPackageName())

    # Handle failures by:
    #   - optionally taking a screenshot
    #   - logging the raw output at INFO level
    #   - clearing the application state while persisting permissions
    if any(r.GetType() not in (base_test_result.ResultType.PASS,
                               base_test_result.ResultType.SKIP)
           for r in results):
      self._SaveScreenshot(device, screenshot_device_file, test_display_name,
                           results, 'post_test_screenshot')

      logging.info('detected failure in %s. raw output:', test_display_name)
      for l in output:
        logging.info('  %s', l)
      if (not self._env.skip_clear_data
          and self._test_instance.package_info):
        permissions = (
            self._test_instance.apk_under_test.GetPermissions()
            if self._test_instance.apk_under_test
            else None)
        device.ClearApplicationState(self._test_instance.package_info.package,
                                     permissions=permissions)
    else:
      logging.debug('raw output from %s:', test_display_name)
      for l in output:
        logging.debug('  %s', l)

    if self._test_instance.store_tombstones:
      resolved_tombstones = tombstones.ResolveTombstones(
          device,
          resolve_all_tombstones=True,
          include_stack_symbols=False,
          wipe_tombstones=True,
          tombstone_symbolizer=self._test_instance.symbolizer)
      if resolved_tombstones:
        tombstone_filename = 'tombstones_%s_%s' % (time.strftime(
            '%Y%m%dT%H%M%S-UTC', time.gmtime()), device.serial)
        with self._env.output_manager.ArchivedTempfile(
            tombstone_filename, 'tombstones') as tombstone_file:
          tombstone_file.write('\n'.join(resolved_tombstones))

        # Associate tombstones with first crashing test.
        for result in results:
          if result.GetType() == base_test_result.ResultType.CRASH:
            result.SetLink('tombstones', tombstone_file.Link())
            break
        else:
          # We don't always detect crashes correctly. In this case,
          # associate with the first test.
          results[0].SetLink('tombstones', tombstone_file.Link())
    return results, None

  def _GetTestsFromRunner(self):
    test_apk_path = self._test_instance.test_apk.path
    pickle_path = '%s-runner.pickle' % test_apk_path
    # For incremental APKs, the code doesn't live in the apk, so instead check
    # the timestamp of the target's .stamp file.
    if self._test_instance.test_apk_incremental_install_json:
      with open(self._test_instance.test_apk_incremental_install_json) as f:
        data = json.load(f)
      out_dir = constants.GetOutDirectory()
      test_mtime = max(
          os.path.getmtime(os.path.join(out_dir, p)) for p in data['dex_files'])
    else:
      test_mtime = os.path.getmtime(test_apk_path)

    try:
      return instrumentation_test_instance.GetTestsFromPickle(
          pickle_path, test_mtime)
    except instrumentation_test_instance.TestListPickleException as e:
      logging.info('Could not get tests from pickle: %s', e)
    logging.info('Getting tests by having %s list them.',
                 self._test_instance.junit4_runner_class)
    def list_tests(d):
      def _run(dev):
        with device_temp_file.DeviceTempFile(
            dev.adb, suffix='.json',
            dir=dev.GetExternalStoragePath()) as dev_test_list_json:
          junit4_runner_class = self._test_instance.junit4_runner_class
          test_package = self._test_instance.test_package
          extras = {
            'log': 'true',
            # Workaround for https://github.com/mockito/mockito/issues/922
            'notPackage': 'net.bytebuddy',
          }
          extras[_EXTRA_TEST_LIST] = dev_test_list_json.name
          target = '%s/%s' % (test_package, junit4_runner_class)
          timeout = 240
          if self._test_instance.wait_for_java_debugger:
            timeout = None
          with self._ArchiveLogcat(dev, 'list_tests'):
            test_list_run_output = dev.StartInstrumentation(
                target, extras=extras, retries=0, timeout=timeout)
          if any(test_list_run_output):
            logging.error('Unexpected output while listing tests:')
            for line in test_list_run_output:
              logging.error('  %s', line)
          with tempfile_ext.NamedTemporaryDirectory() as host_dir:
            host_file = os.path.join(host_dir, 'list_tests.json')
            dev.PullFile(dev_test_list_json.name, host_file)
            with open(host_file, 'r') as host_file:
              return json.load(host_file)

      return crash_handler.RetryOnSystemCrash(_run, d)

    raw_test_lists = self._env.parallel_devices.pMap(list_tests).pGet(None)

    # If all devices failed to list tests, raise an exception.
    # Check that tl is not None and is not empty.
    if all(not tl for tl in raw_test_lists):
      raise device_errors.CommandFailedError(
          'Failed to list tests on any device')

    # Get the first viable list of raw tests
    raw_tests = [tl for tl in raw_test_lists if tl][0]

    instrumentation_test_instance.SaveTestsToPickle(pickle_path, raw_tests)
    return raw_tests

  @contextlib.contextmanager
  def _ArchiveLogcat(self, device, test_name):
    stream_name = 'logcat_%s_%s_%s' % (
        test_name.replace('#', '.'),
        time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime()),
        device.serial)

    logcat_file = None
    logmon = None
    try:
      with self._env.output_manager.ArchivedTempfile(
          stream_name, 'logcat') as logcat_file:
        with logcat_monitor.LogcatMonitor(
            device.adb,
            filter_specs=local_device_environment.LOGCAT_FILTERS,
            output_file=logcat_file.name,
            transform_func=self._test_instance.MaybeDeobfuscateLines,
            check_error=False) as logmon:
          with _LogTestEndpoints(device, test_name):
            with contextlib_ext.Optional(
                trace_event.trace(test_name),
                self._env.trace_output):
              yield logcat_file
    finally:
      if logmon:
        logmon.Close()
      if logcat_file and logcat_file.Link():
        logging.info('Logcat saved to %s', logcat_file.Link())

  def _SaveTraceData(self, trace_device_file, device, test_class):
    trace_host_file = self._env.trace_output

    if device.FileExists(trace_device_file.name):
      try:
        java_trace_json = device.ReadFile(trace_device_file.name)
      except IOError:
        raise Exception('error pulling trace file from device')
      finally:
        trace_device_file.close()

      process_name = '%s (device %s)' % (test_class, device.serial)
      process_hash = int(hashlib.md5(process_name).hexdigest()[:6], 16)

      java_trace = json.loads(java_trace_json)
      java_trace.sort(key=lambda event: event['ts'])

      get_date_command = 'echo $EPOCHREALTIME'
      device_time = device.RunShellCommand(get_date_command, single_line=True)
      device_time = float(device_time) * 1e6
      system_time = trace_time.Now()
      time_difference = system_time - device_time

      threads_to_add = set()
      for event in java_trace:
        # Ensure thread ID and thread name will be linked in the metadata.
        threads_to_add.add((event['tid'], event['name']))

        event['pid'] = process_hash

        # Adjust time stamp to align with Python trace times (from
        # trace_time.Now()).
        event['ts'] += time_difference

      for tid, thread_name in threads_to_add:
        thread_name_metadata = {'pid': process_hash, 'tid': tid,
                                'ts': 0, 'ph': 'M', 'cat': '__metadata',
                                'name': 'thread_name',
                                'args': {'name': thread_name}}
        java_trace.append(thread_name_metadata)

      process_name_metadata = {'pid': process_hash, 'tid': 0, 'ts': 0,
                               'ph': 'M', 'cat': '__metadata',
                               'name': 'process_name',
                               'args': {'name': process_name}}
      java_trace.append(process_name_metadata)

      java_trace_json = json.dumps(java_trace)
      java_trace_json = java_trace_json.rstrip(' ]')

      with open(trace_host_file, 'r') as host_handle:
        host_contents = host_handle.readline()

      if host_contents:
        java_trace_json = ',%s' % java_trace_json.lstrip(' [')

      with open(trace_host_file, 'a') as host_handle:
        host_handle.write(java_trace_json)

  def _SaveScreenshot(self, device, screenshot_device_file, test_name, results,
                      link_name):
    screenshot_filename = '%s-%s.png' % (
        test_name, time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime()))
    if device.FileExists(screenshot_device_file.name):
      with self._env.output_manager.ArchivedTempfile(
          screenshot_filename, 'screenshot',
          output_manager.Datatype.PNG) as screenshot_host_file:
        try:
          device.PullFile(screenshot_device_file.name,
                          screenshot_host_file.name)
        finally:
          screenshot_device_file.close()
      _SetLinkOnResults(results, link_name, screenshot_host_file.Link())

  def _ProcessRenderTestResults(self, device, results):
    if not self._render_tests_device_output_dir:
      return
    self._ProcessSkiaGoldRenderTestResults(device, results)

  def _ProcessSkiaGoldRenderTestResults(self, device, results):
    gold_dir = posixpath.join(self._render_tests_device_output_dir,
                              _DEVICE_GOLD_DIR)
    if not device.FileExists(gold_dir):
      return

    gold_properties = self._test_instance.skia_gold_properties
    with tempfile_ext.NamedTemporaryDirectory() as host_dir:
      use_luci = not (gold_properties.local_pixel_tests
                      or gold_properties.no_luci_auth)

      # Pull everything at once instead of pulling individually, as it's
      # slightly faster since each command over adb has some overhead compared
      # to doing the same thing locally.
      host_dir = os.path.join(host_dir, _DEVICE_GOLD_DIR)
      device.PullFile(gold_dir, host_dir)
      for image_name in os.listdir(host_dir):
        if not image_name.endswith('.png'):
          continue

        render_name = image_name[:-4]
        json_name = render_name + '.json'
        json_path = os.path.join(host_dir, json_name)
        image_path = os.path.join(host_dir, image_name)
        if not os.path.exists(json_path):
          _FailTestIfNecessary(results)
          _AppendToLog(
              results, 'Unable to find corresponding JSON file for image %s '
              'when doing Skia Gold comparison.' % image_name)
          continue

        # Add 'ignore': '1' if a comparison failure would not be surfaced, as
        # that implies that we aren't actively maintaining baselines for the
        # test. This helps prevent unrelated CLs from getting comments posted to
        # them.
        with open(json_path) as infile:
          # All the key/value pairs in the JSON file are strings, so convert
          # to a bool.
          json_dict = json.load(infile)
          fail_on_unsupported = json_dict.get('fail_on_unsupported_configs',
                                              'false')
          fail_on_unsupported = fail_on_unsupported.lower() == 'true'
        should_hide_failure = (
            device.build_version_sdk not in RENDER_TEST_MODEL_SDK_CONFIGS.get(
                device.product_model, []) and not fail_on_unsupported)
        if should_hide_failure:
          json_dict['ignore'] = '1'
          with open(json_path, 'w') as outfile:
            json.dump(json_dict, outfile)

        gold_session = self._skia_gold_session_manager.GetSkiaGoldSession(
            keys_input=json_path)

        try:
          status, error = gold_session.RunComparison(
              name=render_name,
              png_file=image_path,
              output_manager=self._env.output_manager,
              use_luci=use_luci)
        except Exception as e:  # pylint: disable=broad-except
          _FailTestIfNecessary(results)
          _AppendToLog(results, 'Skia Gold comparison raised exception: %s' % e)
          continue

        if not status:
          continue

        # Don't fail the test if we ran on an unsupported configuration unless
        # the test has explicitly opted in, as it's likely that baselines
        # aren't maintained for that configuration.
        if should_hide_failure:
          if self._test_instance.skia_gold_properties.local_pixel_tests:
            _AppendToLog(
                results, 'Gold comparison for %s failed, but model %s with SDK '
                '%d is not a supported configuration. This failure would be '
                'ignored on the bots, but failing since tests are being run '
                'locally.' % (render_name, device.product_model,
                              device.build_version_sdk))
          else:
            _AppendToLog(
                results, 'Gold comparison for %s failed, but model %s with SDK '
                '%d is not a supported configuration, so ignoring failure.' %
                (render_name, device.product_model, device.build_version_sdk))
            continue

        _FailTestIfNecessary(results)
        failure_log = (
            'Skia Gold reported failure for RenderTest %s. See '
            'RENDER_TESTS.md for how to fix this failure.' % render_name)
        status_codes = gold_utils.AndroidSkiaGoldSession.StatusCodes
        if status == status_codes.AUTH_FAILURE:
          _AppendToLog(results,
                       'Gold authentication failed with output %s' % error)
        elif status == status_codes.INIT_FAILURE:
          _AppendToLog(results,
                       'Gold initialization failed with output %s' % error)
        elif status == status_codes.COMPARISON_FAILURE_REMOTE:
          public_triage_link, internal_triage_link =\
              gold_session.GetTriageLinks(render_name)
          if not public_triage_link:
            _AppendToLog(
                results, 'Failed to get triage link for %s, raw output: %s' %
                (render_name, error))
            _AppendToLog(
                results, 'Reason for no triage link: %s' %
                gold_session.GetTriageLinkOmissionReason(render_name))
            continue
          if gold_properties.IsTryjobRun():
            _SetLinkOnResults(results,
                              'Public Skia Gold triage link for entire CL',
                              public_triage_link)
            _SetLinkOnResults(results,
                              'Internal Skia Gold triage link for entire CL',
                              internal_triage_link)
          else:
            _SetLinkOnResults(
                results, 'Public Skia Gold triage link for %s' % render_name,
                public_triage_link)
            _SetLinkOnResults(
                results, 'Internal Skia Gold triage link for %s' % render_name,
                internal_triage_link)
          _AppendToLog(results, failure_log)

        elif status == status_codes.COMPARISON_FAILURE_LOCAL:
          given_link = gold_session.GetGivenImageLink(render_name)
          closest_link = gold_session.GetClosestImageLink(render_name)
          diff_link = gold_session.GetDiffImageLink(render_name)

          processed_template_output = _GenerateRenderTestHtml(
              render_name, given_link, closest_link, diff_link)
          with self._env.output_manager.ArchivedTempfile(
              '%s.html' % render_name, 'gold_local_diffs',
              output_manager.Datatype.HTML) as html_results:
            html_results.write(processed_template_output)
          _SetLinkOnResults(results, render_name, html_results.Link())
          _AppendToLog(
              results,
              'See %s link for diff image with closest positive.' % render_name)
        elif status == status_codes.LOCAL_DIFF_FAILURE:
          _AppendToLog(results,
                       'Failed to generate diffs from Gold: %s' % error)
        else:
          logging.error(
              'Given unhandled SkiaGoldSession StatusCode %s with error %s',
              status, error)

  #override
  def _ShouldRetry(self, test, result):
    # We've tried to disable retries in the past with mixed results.
    # See crbug.com/619055 for historical context and crbug.com/797002
    # for ongoing efforts.
    del test, result
    return True

  #override
  def _ShouldShard(self):
    return True

  @classmethod
  def _GetTimeoutScaleFromAnnotations(cls, annotations):
    try:
      return int(annotations.get('TimeoutScale', {}).get('value', 1))
    except ValueError as e:
      logging.warning("Non-integer value of TimeoutScale ignored. (%s)", str(e))
      return 1

  @classmethod
  def _GetTimeoutFromAnnotations(cls, annotations, test_name):
    for k, v in TIMEOUT_ANNOTATIONS:
      if k in annotations:
        timeout = v
        break
    else:
      logging.warning('Using default 1 minute timeout for %s', test_name)
      timeout = 60

    timeout *= cls._GetTimeoutScaleFromAnnotations(annotations)

    return timeout


def _IsWPRRecordReplayTest(test):
  """Determines whether a test or a list of tests is a WPR RecordReplay Test."""
  if not isinstance(test, list):
    test = [test]
  return any([
      WPR_RECORD_REPLAY_TEST_FEATURE_ANNOTATION in t['annotations'].get(
          FEATURE_ANNOTATION, {}).get('value', ()) for t in test
  ])


def _GetWPRArchivePath(test):
  """Retrieves the archive path from the WPRArchiveDirectory annotation."""
  return test['annotations'].get(WPR_ARCHIVE_FILE_PATH_ANNOTATION,
                                 {}).get('value', ())


def _ReplaceUncommonChars(original):
  """Replaces uncommon characters with __."""
  if not original:
    raise ValueError('parameter should not be empty')

  uncommon_chars = ['#']
  for char in uncommon_chars:
    original = original.replace(char, '__')
  return original


def _IsRenderTest(test):
  """Determines if a test or list of tests has a RenderTest amongst them."""
  if not isinstance(test, list):
    test = [test]
  return any([RENDER_TEST_FEATURE_ANNOTATION in t['annotations'].get(
              FEATURE_ANNOTATION, {}).get('value', ()) for t in test])


def _GenerateRenderTestHtml(image_name, failure_link, golden_link, diff_link):
  """Generates a RenderTest results page.

  Displays the generated (failure) image, the golden image, and the diff
  between them.

  Args:
    image_name: The name of the image whose comparison failed.
    failure_link: The URL to the generated/failure image.
    golden_link: The URL to the golden image.
    diff_link: The URL to the diff image between the failure and golden images.

  Returns:
    A string containing the generated HTML.
  """
  jinja2_env = jinja2.Environment(
      loader=jinja2.FileSystemLoader(_JINJA_TEMPLATE_DIR), trim_blocks=True)
  template = jinja2_env.get_template(_JINJA_TEMPLATE_FILENAME)
  # pylint: disable=no-member
  return template.render(
      test_name=image_name,
      failure_link=failure_link,
      golden_link=golden_link,
      diff_link=diff_link)


def _FailTestIfNecessary(results):
  """Marks the given results as failed if it wasn't already.

  Marks the result types as ResultType.FAIL unless they were already some sort
  of failure type, e.g. ResultType.CRASH.

  Args:
    results: A list of base_test_result.BaseTestResult objects.
  """
  for result in results:
    if result.GetType() not in [
        base_test_result.ResultType.FAIL, base_test_result.ResultType.CRASH,
        base_test_result.ResultType.TIMEOUT, base_test_result.ResultType.UNKNOWN
    ]:
      result.SetType(base_test_result.ResultType.FAIL)


def _AppendToLog(results, line):
  """Appends the given line to the end of the logs of the given results.

  Args:
    results: A list of base_test_result.BaseTestResult objects.
    line: A string to be appended as a neww line to the log of |result|.
  """
  for result in results:
    result.SetLog(result.GetLog() + '\n' + line)


def _SetLinkOnResults(results, link_name, link):
  """Sets the given link on the given results.

  Args:
    results: A list of base_test_result.BaseTestResult objects.
    link_name: A string containing the name of the link being set.
    link: A string containing the lkink being set.
  """
  for result in results:
    result.SetLink(link_name, link)
