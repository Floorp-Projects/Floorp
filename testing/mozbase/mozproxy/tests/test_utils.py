#!/usr/bin/env python
from __future__ import absolute_import, print_function

import os
import shutil
import mock
import mozunit

from mozproxy.utils import download_file_from_url
from support import tempdir

here = os.path.dirname(__file__)


def urlretrieve(*args, **kw):
    def _urlretrieve(url, local_dest):
        # simply copy over our tarball
        shutil.copyfile(os.path.join(here, "archive.tar.gz"), local_dest)
        return local_dest, {}

    return _urlretrieve


@mock.patch("mozproxy.utils.urlretrieve", new_callable=urlretrieve)
def test_download_file(*args):
    with tempdir() as dest_dir:
        dest = os.path.join(dest_dir, "archive.tar.gz")
        download_file_from_url("http://example.com/archive.tar.gz", dest, extract=True)
        # archive.tar.gz contains hey.txt, if it worked we should see it
        assert os.path.exists(os.path.join(dest_dir, "hey.txt"))


if __name__ == "__main__":
    mozunit.main(runwith="pytest")
