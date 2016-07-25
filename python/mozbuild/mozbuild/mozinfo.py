# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This module produces a JSON file that provides basic build info and
# configuration metadata.

from __future__ import absolute_import

import os
import re
import json


def build_dict(config, env=os.environ):
    """
    Build a dict containing data about the build configuration from
    the environment.
    """
    substs = config.substs

    # Check that all required variables are present first.
    required = ["TARGET_CPU", "OS_TARGET"]
    missing = [r for r in required if r not in substs]
    if missing:
        raise Exception("Missing required environment variables: %s" %
                        ', '.join(missing))

    d = {}
    d['topsrcdir'] = config.topsrcdir

    if config.mozconfig:
        d['mozconfig'] = config.mozconfig

    # os
    o = substs["OS_TARGET"]
    known_os = {"Linux": "linux",
                "WINNT": "win",
                "Darwin": "mac",
                "Android": "b2g" if substs.get("MOZ_WIDGET_TOOLKIT") == "gonk" else "android"}
    if o in known_os:
        d["os"] = known_os[o]
    else:
        # Allow unknown values, just lowercase them.
        d["os"] = o.lower()

    # Widget toolkit, just pass the value directly through.
    d["toolkit"] = substs.get("MOZ_WIDGET_TOOLKIT")

    # Application name
    if 'MOZ_APP_NAME' in substs:
        d["appname"] = substs["MOZ_APP_NAME"]

    # Build app name
    if 'MOZ_MULET' in substs and substs.get('MOZ_MULET') == "1":
        d["buildapp"] = "mulet"
    elif 'MOZ_BUILD_APP' in substs:
        d["buildapp"] = substs["MOZ_BUILD_APP"]

    # processor
    p = substs["TARGET_CPU"]
    # for universal mac builds, put in a special value
    if d["os"] == "mac" and "UNIVERSAL_BINARY" in substs and substs["UNIVERSAL_BINARY"] == "1":
        p = "universal-x86-x86_64"
    else:
        # do some slight massaging for some values
        #TODO: retain specific values in case someone wants them?
        if p.startswith("arm"):
            p = "arm"
        elif re.match("i[3-9]86", p):
            p = "x86"
    d["processor"] = p
    # hardcoded list of 64-bit CPUs
    if p in ["x86_64", "ppc64"]:
        d["bits"] = 64
    # hardcoded list of known 32-bit CPUs
    elif p in ["x86", "arm", "ppc"]:
        d["bits"] = 32
    # other CPUs will wind up with unknown bits

    d['debug'] = substs.get('MOZ_DEBUG') == '1'
    d['nightly_build'] = substs.get('NIGHTLY_BUILD') == '1'
    d['release_build'] = substs.get('RELEASE_BUILD') == '1'
    d['pgo'] = substs.get('MOZ_PGO') == '1'
    d['crashreporter'] = bool(substs.get('MOZ_CRASHREPORTER'))
    d['datareporting'] = bool(substs.get('MOZ_DATA_REPORTING'))
    d['healthreport'] = substs.get('MOZ_SERVICES_HEALTHREPORT') == '1'
    d['sync'] = substs.get('MOZ_SERVICES_SYNC') == '1'
    d['asan'] = substs.get('MOZ_ASAN') == '1'
    d['tsan'] = substs.get('MOZ_TSAN') == '1'
    d['telemetry'] = substs.get('MOZ_TELEMETRY_REPORTING') == '1'
    d['tests_enabled'] = substs.get('ENABLE_TESTS') == "1"
    d['bin_suffix'] = substs.get('BIN_SUFFIX', '')
    d['addon_signing'] = substs.get('MOZ_ADDON_SIGNING') == '1'
    d['require_signing'] = substs.get('MOZ_REQUIRE_SIGNING') == '1'
    d['official'] = bool(substs.get('MOZILLA_OFFICIAL'))
    d['sm_promise'] = bool(substs.get('SPIDERMONKEY_PROMISE'))

    def guess_platform():
        if d['buildapp'] in ('browser', 'mulet'):
            p = d['os']
            if p == 'mac':
                p = 'macosx64'
            elif d['bits'] == 64:
                p = '{}64'.format(p)
            elif p in ('win',):
                p = '{}32'.format(p)

            if d['buildapp'] == 'mulet':
                p = '{}-mulet'.format(p)

            if d['asan']:
                p = '{}-asan'.format(p)

            return p

        if d['buildapp'] == 'b2g':
            if d['toolkit'] == 'gonk':
                return 'emulator'

            if d['bits'] == 64:
                return 'linux64_gecko'
            return 'linux32_gecko'

        if d['buildapp'] == 'mobile/android':
            if d['processor'] == 'x86':
                return 'android-x86'
            return 'android-arm'

    def guess_buildtype():
        if d['debug']:
            return 'debug'
        if d['pgo']:
            return 'pgo'
        return 'opt'

    # if buildapp or bits are unknown, we don't have a configuration similar to
    # any in automation and the guesses are useless.
    if 'buildapp' in d and (d['os'] == 'mac' or 'bits' in d):
        d['platform_guess'] = guess_platform()
        d['buildtype_guess'] = guess_buildtype()

    if 'buildapp' in d and d['buildapp'] == 'mobile/android' and 'MOZ_ANDROID_MIN_SDK_VERSION' in substs:
        d['android_min_sdk'] = substs['MOZ_ANDROID_MIN_SDK_VERSION']

    return d


def write_mozinfo(file, config, env=os.environ):
    """Write JSON data about the configuration specified in config and an
    environment variable dict to |file|, which may be a filename or file-like
    object.
    See build_dict for information about what  environment variables are used,
    and what keys are produced.
    """
    build_conf = build_dict(config, env)
    if isinstance(file, basestring):
        with open(file, "w") as f:
            json.dump(build_conf, f)
    else:
        json.dump(build_conf, file)
