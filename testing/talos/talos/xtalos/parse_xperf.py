#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function

import os
import subprocess
import sys

import etlparser
import mozfile
import xtalos
from xperf_analyzer import (
    ProcessStart,
    SessionStoreWindowRestored,
    XPerfAttribute,
    XPerfFile,
    XPerfInterval,
)


def run_session_restore_analysis(debug=False, **kwargs):
    required = ("csv_filename", "outputFile")
    for r in required:
        if r not in kwargs:
            raise xtalos.XTalosError("%s required" % r)

    final_output_file = "%s_session_restore_stats%s" % os.path.splitext(
        kwargs["outputFile"]
    )

    output = "time_to_session_store_window_restored_ms\n"

    with XPerfFile(csvfile=kwargs["csv_filename"], debug=debug) as xperf:
        fx_start = ProcessStart("firefox.exe")
        ss_window_restored = SessionStoreWindowRestored()

        interval = XPerfInterval(fx_start, ss_window_restored)
        xperf.add_attr(interval)

        xperf.analyze()

        output += "%.3f\n" % (interval.get_results()[XPerfAttribute.RESULT])

    with open(final_output_file, "w") as out:
        out.write(output)

    if debug:
        etlparser.uploadFile(final_output_file)


def stop(xperf_path, debug=False):
    xperf_cmd = [xperf_path, "-stop", "-stop", "talos_ses"]
    if debug:
        print("executing '%s'" % subprocess.list2cmdline(xperf_cmd))
    subprocess.call(xperf_cmd)


def stop_from_config(config_file=None, debug=False, **kwargs):
    """start from a YAML config file"""

    # required options and associated error messages
    required = {
        "xperf_path": "xperf_path not given",
        "etl_filename": "No etl_filename given",
    }
    for key in required:
        if key not in kwargs:
            kwargs[key] = None

    if config_file:
        # override options from YAML config file
        kwargs = xtalos.options_from_config(kwargs, config_file)

    # ensure the required options are given
    for key, msg in required.items():
        if not kwargs.get(key):
            raise xtalos.XTalosError(msg)

    # ensure path to xperf actually exists
    if not os.path.exists(kwargs["xperf_path"]):
        raise xtalos.XTalosError(
            "ERROR: xperf_path '%s' does not exist" % kwargs["xperf_path"]
        )

    # make calling arguments
    stopargs = {}
    stopargs["xperf_path"] = kwargs["xperf_path"]
    stopargs["debug"] = debug

    # call start
    stop(**stopargs)

    etlparser.etlparser_from_config(
        config_file,
        approot=kwargs["approot"],
        error_filename=kwargs["error_filename"],
        processID=kwargs["processID"],
    )

    csv_base = "%s.csv" % kwargs["etl_filename"]
    run_session_restore_analysis(csv_filename=csv_base, debug=debug, **kwargs)

    if not debug:
        mozfile.remove(csv_base)


def main(args=sys.argv[1:]):

    # parse command line arguments
    parser = xtalos.XtalosOptions()
    args = parser.parse_args(args)

    # stop xperf
    try:
        stop_from_config(
            config_file=args.configFile,
            debug=args.debug_level >= xtalos.DEBUG_INFO,
            **args.__dict__
        )
    except xtalos.XTalosError as e:
        parser.error(str(e))


if __name__ == "__main__":
    main()
