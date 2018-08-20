#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import mozhttpd
import urllib2
import os
import re

import pytest

import mozunit


@pytest.fixture(name="docroot")
def fixture_docroot():
    """Returns a docroot path."""
    return os.path.dirname(os.path.abspath(__file__))


@pytest.fixture(name="httpd")
def fixture_httpd(docroot):
    """Yields a started MozHttpd server."""
    httpd = mozhttpd.MozHttpd(port=0, docroot=docroot)
    httpd.start(block=False)
    yield httpd
    httpd.stop()


@pytest.mark.parametrize(
    "path",
    [
        pytest.param("", id="no_params"),
        pytest.param("?foo=bar&fleem=&foo=fleem", id="with_params"),
    ],
)
def test_filelist(httpd, docroot, path):
    f = urllib2.urlopen(
        "http://{host}:{port}/{path}".format(
            host="127.0.0.1", port=httpd.httpd.server_port, path=path
        )
    )

    filelist = os.listdir(docroot)

    pattern = "\<[a-zA-Z0-9\-\_\.\=\"'\/\\\%\!\@\#\$\^\&\*\(\) ]*\>"

    for line in f.readlines():
        subbed_lined = re.sub(pattern, "", line.strip("\n"))
        webline = subbed_lined.strip("/").strip().strip("@")

        if webline and not webline.startswith("Directory listing for"):
            msg = "File {} in dir listing corresponds to a file".format(
                webline
            )
            assert webline in filelist, msg
            filelist.remove(webline)

    msg = "Should have no items in filelist ({}) unaccounted for".format(
        filelist
    )
    assert len(filelist) == 0, msg


if __name__ == "__main__":
    mozunit.main()
