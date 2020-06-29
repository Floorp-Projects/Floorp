#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import sys
import mozunit
from unittest import mock
import pytest
from pathlib import Path
import shutil
from datetime import date, timedelta

from mozperftest.utils import (
    host_platform,
    silence,
    download_file,
    install_package,
    build_test_list,
    get_multi_tasks_url,
    convert_day,
)
from mozperftest.tests.support import temp_file, requests_content, EXAMPLE_TESTS_DIR


def test_silence():
    with silence():
        print("HIDDEN")


def test_host_platform():
    plat = host_platform()

    # a bit useless... :)
    if sys.platform.startswith("darwin"):
        assert plat == "darwin"
    else:
        if sys.maxsize > 2 ** 32:
            assert "64" in plat
        else:
            assert "64" not in plat


def get_raise(*args, **kw):
    raise Exception()


@mock.patch("mozperftest.utils.requests.get", new=get_raise)
def test_download_file_fails():
    with temp_file() as target, silence(), pytest.raises(Exception):
        download_file("http://I don't exist", Path(target), retry_sleep=0.1)


@mock.patch("mozperftest.utils.requests.get", new=requests_content())
def test_download_file_success():
    with temp_file() as target:
        download_file("http://content", Path(target), retry_sleep=0.1)
        with open(target) as f:
            assert f.read() == "some content"


def _req(package):
    class Req:
        location = "nowhere"

        @property
        def satisfied_by(self):
            return self

        def check_if_exists(self, **kw):
            pass

    return Req()


@mock.patch("pip._internal.req.constructors.install_req_from_line", new=_req)
def test_install_package():
    vem = mock.Mock()
    vem.bin_path = "someplace"
    install_package(vem, "foo")
    vem._run_pip.assert_called()


@mock.patch("mozperftest.utils.requests.get", requests_content())
def test_build_test_list():
    tests = [EXAMPLE_TESTS_DIR, "https://some/location/perftest_one.js"]
    try:
        files, tmp_dir = build_test_list(tests)
        assert len(files) == 2
    finally:
        shutil.rmtree(tmp_dir)


def test_convert_day():
    day = "2020.06.08"
    assert convert_day(day) == day
    with pytest.raises(ValueError):
        convert_day("2020-06-08")
    today = date.today()
    assert convert_day("today"), today.strftime("%Y.%m.%d")
    yesterday = today - timedelta(days=1)
    assert convert_day("yesterday") == yesterday.strftime("%Y.%m.%d")


def test_multibuild_url():
    route = "FakeBuildRoute"
    day = "2020.06.08"
    buildurl = get_multi_tasks_url(route, day=day)
    assert day in buildurl and route in buildurl

    with mock.patch("mozperftest.utils.date") as mockeddate:
        mockeddate.today.return_value = mockeddate
        mockeddate.strftime.return_value = "2020.07.09"
        buildurl = get_multi_tasks_url(route, day="today")
        assert "2020.07.09" in buildurl and route in buildurl

        with mock.patch("mozperftest.utils.timedelta"):
            mockeddate.__sub__.return_value = mockeddate
            mockeddate.strftime.return_value = "2020.08.09"
            buildurl = get_multi_tasks_url(route)
            assert "2020.08.09" in buildurl and route in buildurl


if __name__ == "__main__":
    mozunit.main()
