# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.


"""
This module contains a set of shortcut methods that create runners for commonly
used Mozilla applications, such as Firefox, Firefox for Android or Thunderbird.
"""

from __future__ import absolute_import

from .application import get_app_context
from .base import GeckoRuntimeRunner, FennecRunner, BlinkRuntimeRunner
from .devices import EmulatorAVD


def Runner(*args, **kwargs):
    """
    Create a generic GeckoRuntime runner.

    :param binary: Path to binary.
    :param cmdargs: Arguments to pass into binary.
    :param profile: Profile object to use.
    :param env: Environment variables to pass into the gecko process.
    :param clean_profile: If True, restores profile back to original state.
    :param process_class: Class used to launch the binary.
    :param process_args: Arguments to pass into process_class.
    :param symbols_path: Path to symbol files used for crash analysis.
    :param show_crash_reporter: allow the crash reporter window to pop up.
        Defaults to False.
    :returns: A generic GeckoRuntimeRunner.
    """
    return GeckoRuntimeRunner(*args, **kwargs)


def FirefoxRunner(*args, **kwargs):
    """
    Create a desktop Firefox runner.

    :param binary: Path to Firefox binary.
    :param cmdargs: Arguments to pass into binary.
    :param profile: Profile object to use.
    :param env: Environment variables to pass into the gecko process.
    :param clean_profile: If True, restores profile back to original state.
    :param process_class: Class used to launch the binary.
    :param process_args: Arguments to pass into process_class.
    :param symbols_path: Path to symbol files used for crash analysis.
    :param show_crash_reporter: allow the crash reporter window to pop up.
        Defaults to False.
    :returns: A GeckoRuntimeRunner for Firefox.
    """
    kwargs['app_ctx'] = get_app_context('firefox')()
    return GeckoRuntimeRunner(*args, **kwargs)


def ThunderbirdRunner(*args, **kwargs):
    """
    Create a desktop Thunderbird runner.

    :param binary: Path to Thunderbird binary.
    :param cmdargs: Arguments to pass into binary.
    :param profile: Profile object to use.
    :param env: Environment variables to pass into the gecko process.
    :param clean_profile: If True, restores profile back to original state.
    :param process_class: Class used to launch the binary.
    :param process_args: Arguments to pass into process_class.
    :param symbols_path: Path to symbol files used for crash analysis.
    :param show_crash_reporter: allow the crash reporter window to pop up.
        Defaults to False.
    :returns: A GeckoRuntimeRunner for Thunderbird.
    """
    kwargs['app_ctx'] = get_app_context('thunderbird')()
    return GeckoRuntimeRunner(*args, **kwargs)


def ChromeRunner(*args, **kwargs):
    """
    Create a desktop Google Chrome runner.

    :param binary: Path to Chrome binary.
    :param cmdargs: Arguments to pass into the binary.
    """
    kwargs['app_ctx'] = get_app_context('chrome')()
    return BlinkRuntimeRunner(*args, **kwargs)


def FennecEmulatorRunner(avd='mozemulator-4.3',
                         adb_path=None,
                         avd_home=None,
                         logdir=None,
                         serial=None,
                         binary=None,
                         app='org.mozilla.fennec',
                         **kwargs):
    """
    Create a Fennec emulator runner. This can either start a new emulator
    (which will use an avd), or connect to  an already-running emulator.

    :param avd: name of an AVD available in your environment.
        Typically obtained via tooltool: either 'mozemulator-4.3' or 'mozemulator-x86'.
        Defaults to 'mozemulator-4.3'
    :param avd_home: Path to avd parent directory
    :param logdir: Path to save logfiles such as logcat and qemu output.
    :param serial: Serial of emulator to connect to as seen in `adb devices`.
        Defaults to the first entry in `adb devices`.
    :param binary: Path to emulator binary.
        Defaults to None, which causes the device_class to guess based on PATH.
    :param app: Name of Fennec app (often org.mozilla.fennec_$USER)
        Defaults to 'org.mozilla.fennec'
    :param cmdargs: Arguments to pass into binary.
    :returns: A DeviceRunner for Android emulators.
    """
    kwargs['app_ctx'] = get_app_context('fennec')(app, adb_path=adb_path,
                                                  avd_home=avd_home,
                                                  device_serial=serial)
    device_args = {'app_ctx': kwargs['app_ctx'],
                   'avd': avd,
                   'binary': binary,
                   'logdir': logdir}
    return FennecRunner(device_class=EmulatorAVD,
                        device_args=device_args,
                        **kwargs)


runners = {
    'chrome': ChromeRunner,
    'default': Runner,
    'firefox': FirefoxRunner,
    'fennec': FennecEmulatorRunner,
    'thunderbird': ThunderbirdRunner,
}
