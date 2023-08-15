# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import sys
import tarfile
import tempfile

import mozunit

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))

raptor_dir = os.path.join(os.path.dirname(here), "raptor")
sys.path.insert(0, raptor_dir)

from chrome_trace import ChromeTrace


def test_browsertime_trace_collection():
    """Test the ability to collect existing trace files into a zip archive
    for viewing in the firefox profiler
    """
    result_dir = tempfile.mkdtemp()
    # untar chrome trace tar file, which contains a chimera run of wikipedia
    with tarfile.open(os.path.join(here, "chromeTraceTest.tar")) as f:
        f.extractall(path=result_dir)

    # Makes sure we can run the profile process against a browsertime-generated
    # trace (trace-1.json in this test dir)
    upload_dir = tempfile.mkdtemp()
    raptor_config = {
        "browsertime": True,
        "browsertime_result_dir": os.path.join(result_dir, "wikipedia"),
        "extra_profiler_run": True,
        "chimera": True,
    }
    test_config = {"name": "tp6", "type": "pageload"}
    try:
        profile = ChromeTrace(upload_dir, raptor_config, test_config)
        assert (
            len(profile.collect_profiles()) == 2
        ), "We have two profiles for a cold & warm run"
        profile.output_trace()
        profile.clean()
        arcname = os.environ["RAPTOR_LATEST_GECKO_PROFILE_ARCHIVE"]
        assert os.stat(arcname).st_size > 900000, "We got a ~0.9mb+ zip"
    except:
        assert False, "Failed to collect Traces"
        raise
    finally:
        shutil.rmtree(upload_dir)
        shutil.rmtree(result_dir)


if __name__ == "__main__":
    mozunit.main()
