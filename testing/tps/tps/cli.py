# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import optparse
import os
import sys

from threading import RLock

from tps import TPSTestRunner

def main():
    parser = optparse.OptionParser()
    parser.add_option("--mobile",
                      action = "store_true", dest = "mobile",
                      default = False,
                      help = "run with mobile settings")
    parser.add_option("--testfile",
                      action = "store", type = "string", dest = "testfile",
                      default = '../../services/sync/tests/tps/test_sync.js',
                      help = "path to the test file to run "
                             "[default: %default]")
    parser.add_option("--logfile",
                      action = "store", type = "string", dest = "logfile",
                      default = 'tps.log',
                      help = "path to the log file [default: %default]")
    parser.add_option("--resultfile",
                      action = "store", type = "string", dest = "resultfile",
                      default = 'tps_result.json',
                      help = "path to the result file [default: %default]")
    parser.add_option("--binary",
                      action = "store", type = "string", dest = "binary",
                      default = None,
                      help = "path to the Firefox binary, specified either as "
                             "a local file or a url; if omitted, the PATH "
                             "will be searched;")
    parser.add_option("--configfile",
                      action = "store", type = "string", dest = "configfile",
                      default = None,
                      help = "path to the config file to use "
                             "[default: %default]")
    parser.add_option("--pulsefile",
                      action = "store", type = "string", dest = "pulsefile",
                      default = None,
                      help = "path to file containing a pulse message in "
                             "json format that you want to inject into the monitor")
    parser.add_option("--ignore-unused-engines",
                       default=False,
                       action="store_true",
                       dest="ignore_unused_engines",
                       help="If defined, don't load unused engines in individual tests."
                             " Has no effect for pulse monitor.")
    (options, args) = parser.parse_args()

    configfile = options.configfile
    if configfile is None:
        if os.environ.get('VIRTUAL_ENV'):
            configfile = os.path.join(os.path.dirname(__file__), 'config.json')
        if configfile is None or not os.access(configfile, os.F_OK):
            raise Exception("Unable to find config.json in a VIRTUAL_ENV; you must "
                            "specify a config file using the --configfile option")

    # load the config file
    f = open(configfile, 'r')
    configcontent = f.read()
    f.close()
    config = json.loads(configcontent)

    rlock = RLock()

    print 'using result file', options.resultfile

    extensionDir = config.get("extensiondir")
    if not extensionDir or extensionDir == '__EXTENSIONDIR__':
        extensionDir = os.path.join(os.getcwd(), "..", "..", "services", "sync", "tps", "extensions")
    else:
        if sys.platform == 'win32':
            # replace msys-style paths with proper Windows paths
            import re
            m = re.match('^\/\w\/', extensionDir)
            if m:
                extensionDir = "%s:/%s" % (m.group(0)[1:2], extensionDir[3:])
                extensionDir = extensionDir.replace("/", "\\")

    TPS = TPSTestRunner(extensionDir,
                        testfile=options.testfile,
                        logfile=options.logfile,
                        binary=options.binary,
                        config=config,
                        rlock=rlock,
                        mobile=options.mobile,
                        resultfile=options.resultfile,
                        ignore_unused_engines=options.ignore_unused_engines)
    TPS.run_tests()

if __name__ == "__main__":
    main()
