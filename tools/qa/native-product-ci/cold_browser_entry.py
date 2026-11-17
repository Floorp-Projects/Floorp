# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# Closely follows Runtime qa3_browser_entry.py; bounded manifest instead of a
# one-test canary. No retries/repeat or change to Mozilla assertions.
import os
import sys
from pathlib import Path


def run(harness, selected=None):
    parser=harness.MochitestArgumentParser(app="generic")
    options=parser.parse_args();parser.validate(options)
    if (options.flavor!="browser" or not options.manifestFile or (options.test_paths != selected if selected is not None else bool(options.test_paths)) or options.repeat or options.verify
            or options.verify_fission or options.runUntilFailure or options.restartAfterFailure):
        raise ValueError("cold manifest requires one unfiltered, nonrepeating browser run")
    class SinglePassMochitest(harness.MochitestDesktop):
        pass
    logger_options={k:v for k,v in vars(options).items() if k.startswith("log") or k=="valgrind"}
    runner=SinglePassMochitest(options.flavor,logger_options,options.stagedAddons,quiet=options.quiet)
    options.runByManifest=True
    result=runner.runTests(options)
    runner.archiveMozLogs();runner.message_logger.finish()
    return result


if __name__=="__main__":
    root=Path(os.environ["COLD_MOCHITEST_ROOT"]).resolve(strict=True)
    if not (root/"runtests.py").is_file():raise ValueError("missing cold native harness")
    sys.path.insert(0,str(root))
    import runtests
    sys.exit(run(runtests))
