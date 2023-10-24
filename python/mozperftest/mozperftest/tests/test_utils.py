#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import shutil
import sys
from datetime import date, timedelta
from pathlib import Path
from subprocess import CalledProcessError
from unittest import mock

import mozunit
import pytest

from mozperftest.tests.support import EXAMPLE_TESTS_DIR, requests_content, temp_file
from mozperftest.utils import (
    build_test_list,
    checkout_python_script,
    convert_day,
    create_path,
    download_file,
    get_multi_tasks_url,
    get_output_dir,
    get_revision_namespace_url,
    host_platform,
    install_package,
    install_requirements_file,
    load_class,
    load_class_from_path,
    silence,
)


def test_silence():
    with silence():
        print("HIDDEN")


def test_host_platform():
    plat = host_platform()

    # a bit useless... :)
    if sys.platform.startswith("darwin"):
        assert plat == "darwin"
    else:
        if sys.maxsize > 2**32:
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
    with mock.patch("subprocess.check_call") as mock_check_call:
        assert install_package(vem, "foo")
        mock_check_call.assert_called_once_with(
            [
                vem.python_path,
                "-m",
                "pip",
                "install",
                "foo",
            ]
        )


def test_install_requirements_file():
    vem = mock.Mock()
    vem.bin_path = "someplace"
    with mock.patch("subprocess.check_call") as mock_check_call, mock.patch(
        "mozperftest.utils.os"
    ):
        assert install_requirements_file(vem, "foo")
        mock_check_call.assert_called_once_with(
            [
                vem.python_path,
                "-m",
                "pip",
                "install",
                "--no-deps",
                "-r",
                "foo",
                "--no-index",
                "--find-links",
                "https://pypi.pub.build.mozilla.org/pub/",
            ]
        )


@mock.patch("pip._internal.req.constructors.install_req_from_line", new=_req)
def test_install_package_failures():
    vem = mock.Mock()
    vem.bin_path = "someplace"

    def check_call(*args):
        raise CalledProcessError(1, "")

    with pytest.raises(CalledProcessError):
        with mock.patch("subprocess.check_call", new=check_call):
            install_package(vem, "foo")

    # we can also absorb the error, and just return False
    assert not install_package(vem, "foo", ignore_failure=True)


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


def test_revision_namespace_url():
    route = "FakeBuildRoute"
    day = "2020.06.08"
    buildurl = get_revision_namespace_url(route, day=day)
    assert day in buildurl and route in buildurl
    assert buildurl.endswith(".revision")


def test_multibuild_url():
    route = "FakeBuildRoute"
    day = "2020.06.08"
    revision = "deadbeef"
    buildurl = get_multi_tasks_url(route, revision, day=day)
    assert all(item in buildurl for item in (route, day, revision))

    with mock.patch("mozperftest.utils.date") as mockeddate:
        mockeddate.today.return_value = mockeddate
        mockeddate.strftime.return_value = "2020.07.09"
        buildurl = get_multi_tasks_url(route, revision, day="today")
        assert "2020.07.09" in buildurl and route in buildurl

        with mock.patch("mozperftest.utils.timedelta"):
            mockeddate.__sub__.return_value = mockeddate
            mockeddate.strftime.return_value = "2020.08.09"
            buildurl = get_multi_tasks_url(route, revision)
            assert "2020.08.09" in buildurl and route in buildurl


class ImportMe:
    pass


def test_load_class():
    with pytest.raises(ImportError):
        load_class("notimportable")

    with pytest.raises(ImportError):
        load_class("notim:por:table")

    with pytest.raises(ImportError):
        load_class("notim:portable")

    with pytest.raises(ImportError):
        load_class("mozperftest.tests.test_utils:NOEXIST")

    klass = load_class("mozperftest.tests.test_utils:ImportMe")
    assert klass is ImportMe


def test_load_class_from_path():
    with pytest.raises(ImportError) as exc:
        load_class_from_path("nonexistent", __file__)
    assert "found but it was not a valid class" in exc.value.args[0]

    with pytest.raises(ImportError) as exc:
        load_class_from_path("nonexistent", "nonexistent-file")
    assert "does not exist." in exc.value.args[0]

    klass = load_class_from_path("ImportMe", __file__)
    assert klass.__name__ is ImportMe.__name__


class _Venv:
    python_path = sys.executable


def test_checkout_python_script():
    with silence() as captured:
        assert checkout_python_script(_Venv(), "lib2to3", ["--help"])

    stdout, stderr = captured
    stdout.seek(0)
    assert stdout.read() == "=> lib2to3 [OK]\n"


def test_run_python_script_failed():
    with silence() as captured:
        assert not checkout_python_script(_Venv(), "nothing")

    stdout, stderr = captured
    stdout.seek(0)
    assert stdout.read().endswith("[FAILED]\n")


def test_get_output_dir():
    with temp_file() as temp_dir:
        output_dir = get_output_dir(temp_dir)
        assert output_dir.exists()
        assert output_dir.is_dir()

        output_dir = get_output_dir(output=temp_dir, folder="artifacts")
        assert output_dir.exists()
        assert output_dir.is_dir()
        assert "artifacts" == output_dir.parts[-1]


def test_create_path():
    path = Path("path/doesnt/exist").resolve()
    if path.exists():
        shutil.rmtree(path.parent.parent)
    try:
        path = create_path(path)

        assert path.exists()
    finally:
        shutil.rmtree(path.parent.parent)


if __name__ == "__main__":
    mozunit.main()
