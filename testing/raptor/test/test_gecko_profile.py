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

from gecko_profile import GeckoProfile


def test_browsertime_profiling():
    result_dir = tempfile.mkdtemp()
    # untar geckoProfile.tar
    with tarfile.open(os.path.join(here, "geckoProfileTest.tar")) as f:
        f.extractall(path=result_dir)

    # Makes sure we can run the profile process against a browsertime-generated
    # profile (geckoProfile-1.json in this test dir)
    upload_dir = tempfile.mkdtemp()
    symbols_path = tempfile.mkdtemp()
    raptor_config = {
        "symbols_path": symbols_path,
        "browsertime": True,
        "browsertime_result_dir": os.path.join(result_dir, "amazon"),
    }
    test_config = {"name": "tp6"}
    try:
        profile = GeckoProfile(upload_dir, raptor_config, test_config)
        profile.symbolicate()
        profile.clean()
        arcname = os.environ["RAPTOR_LATEST_GECKO_PROFILE_ARCHIVE"]
        assert os.stat(arcname).st_size > 1000000, "We got a 1mb+ zip"
    except Exception:
        assert False, "Symbolication failed!"
        raise
    finally:
        shutil.rmtree(upload_dir)
        shutil.rmtree(symbols_path)
        shutil.rmtree(result_dir)


if __name__ == "__main__":
    mozunit.main()
