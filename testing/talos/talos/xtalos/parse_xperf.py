#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import xtalos
import subprocess
import etlparser


def stop(xperf_path, debug=False):
    xperf_cmd = [xperf_path, '-stop', '-stop', 'talos_ses']
    if debug:
        print("executing '%s'" % subprocess.list2cmdline(xperf_cmd))
    subprocess.call(xperf_cmd)


def stop_from_config(config_file=None, debug=False, **kwargs):
    """start from a YAML config file"""

    # required options and associated error messages
    required = {'xperf_path': "xperf_path not given",
                "etl_filename": "No etl_filename given"}
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
    if not os.path.exists(kwargs['xperf_path']):
        raise xtalos.XTalosError("ERROR: xperf_path '%s' does not exist"
                                 % kwargs['xperf_path'])

    # make calling arguments
    stopargs = {}
    stopargs['xperf_path'] = kwargs['xperf_path']
    stopargs['debug'] = debug

    # call start
    stop(**stopargs)

    etlparser.etlparser_from_config(config_file,
                                    approot=kwargs['approot'],
                                    error_filename=kwargs['error_filename'],
                                    processID=kwargs['processID'])


def main(args=sys.argv[1:]):

    # parse command line arguments
    parser = xtalos.XtalosOptions()
    args = parser.parse_args(args)

    # start xperf
    try:
        stop_from_config(config_file=args.configFile,
                         debug=args.debug_level >= xtalos.DEBUG_INFO,
                         **args.__dict__)
    except xtalos.XTalosError as e:
        parser.error(str(e))

if __name__ == "__main__":
    main()
