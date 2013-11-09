# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This module produces a JSON file that provides basic build info and
# configuration metadata.

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
    required = ["TARGET_CPU", "OS_TARGET", "MOZ_WIDGET_TOOLKIT"]
    missing = [r for r in required if r not in substs]
    if missing:
        raise Exception("Missing required environment variables: %s" %
                        ', '.join(missing))

    d = {}
    d['topsrcdir'] = config.topsrcdir

    if 'MOZCONFIG' in env:
        mozconfig = env["MOZCONFIG"]
        mozconfig = os.path.join(config.topsrcdir, mozconfig)
        d['mozconfig'] = os.path.normpath(mozconfig)

    # os
    o = substs["OS_TARGET"]
    known_os = {"Linux": "linux",
                "WINNT": "win",
                "Darwin": "mac",
                "Android": "b2g" if substs["MOZ_WIDGET_TOOLKIT"] == "gonk" else "android"}
    if o in known_os:
        d["os"] = known_os[o]
    else:
        # Allow unknown values, just lowercase them.
        d["os"] = o.lower()

    # Widget toolkit, just pass the value directly through.
    d["toolkit"] = substs["MOZ_WIDGET_TOOLKIT"]

    # Application name
    if 'MOZ_APP_NAME' in substs:
        d["appname"] = substs["MOZ_APP_NAME"]

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
    d['crashreporter'] = bool(substs.get('MOZ_CRASHREPORTER'))
    d['datareporting'] = bool(substs.get('MOZ_DATA_REPORTING'))
    d['asan'] = substs.get('MOZ_ASAN') == '1'
    d['tests_enabled'] = substs.get('ENABLE_TESTS') == "1"
    d['bin_suffix'] = substs.get('BIN_SUFFIX', '')

    d['ogg'] = bool(substs.get('MOZ_OGG'))
    d['webm'] = bool(substs.get('MOZ_WEBM'))
    d['wave'] = bool(substs.get('MOZ_WAVE'))

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
