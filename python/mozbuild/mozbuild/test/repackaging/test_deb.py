# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import json
import os
import tarfile
import tempfile
import zipfile
from contextlib import nullcontext as does_not_raise
from unittest.mock import MagicMock, call

import mozpack.path as mozpath
import mozunit
import pytest

from mozbuild.repackaging import deb

_APPLICATION_INI_CONTENT = """[App]
Vendor=Mozilla
Name=Firefox
RemotingName=firefox-nightly-try
CodeName=Firefox Nightly
BuildID=20230222000000
"""

_APPLICATION_INI_CONTENT_DATA = {
    "name": "Firefox",
    "display_name": "Firefox Nightly",
    "vendor": "Mozilla",
    "remoting_name": "firefox-nightly-try",
    "build_id": "20230222000000",
    "timestamp": datetime.datetime(2023, 2, 22),
}


@pytest.mark.parametrize(
    "number_of_application_ini_files, expectaction, expected_result",
    (
        (0, pytest.raises(ValueError), None),
        (1, does_not_raise(), _APPLICATION_INI_CONTENT_DATA),
        (2, pytest.raises(ValueError), None),
    ),
)
def test_extract_application_ini_data(
    number_of_application_ini_files, expectaction, expected_result
):
    with tempfile.TemporaryDirectory() as d:
        tar_path = os.path.join(d, "input.tar")
        with tarfile.open(tar_path, "w") as tar:
            application_ini_path = os.path.join(d, "application.ini")
            with open(application_ini_path, "w") as application_ini_file:
                application_ini_file.write(_APPLICATION_INI_CONTENT)

            for i in range(number_of_application_ini_files):
                tar.add(application_ini_path, f"{i}/application.ini")

        with expectaction:
            assert deb._extract_application_ini_data(tar_path) == expected_result


def test_extract_application_ini_data_from_directory():
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, "application.ini"), "w") as f:
            f.write(_APPLICATION_INI_CONTENT)

        assert (
            deb._extract_application_ini_data_from_directory(d)
            == _APPLICATION_INI_CONTENT_DATA
        )


@pytest.mark.parametrize(
    "version, build_number, package_name_suffix, description_suffix, expected",
    (
        (
            "112.0a1",
            1,
            "",
            "",
            {
                "DEB_DESCRIPTION": "Mozilla Firefox",
                "DEB_PKG_INSTALL_PATH": "usr/lib/firefox-nightly-try",
                "DEB_PKG_NAME": "firefox-nightly-try",
                "DEB_PKG_VERSION": "112.0a1~20230222000000",
            },
        ),
        (
            "112.0a1",
            1,
            "-l10n-fr",
            " - Language pack for Firefox Nightly for fr",
            {
                "DEB_DESCRIPTION": "Mozilla Firefox - Language pack for Firefox Nightly for fr",
                "DEB_PKG_INSTALL_PATH": "usr/lib/firefox-nightly-try",
                "DEB_PKG_NAME": "firefox-nightly-try-l10n-fr",
                "DEB_PKG_VERSION": "112.0a1~20230222000000",
            },
        ),
        (
            "112.0b1",
            1,
            "",
            "",
            {
                "DEB_DESCRIPTION": "Mozilla Firefox",
                "DEB_PKG_INSTALL_PATH": "usr/lib/firefox-nightly-try",
                "DEB_PKG_NAME": "firefox-nightly-try",
                "DEB_PKG_VERSION": "112.0b1~build1",
            },
        ),
        (
            "112.0",
            2,
            "",
            "",
            {
                "DEB_DESCRIPTION": "Mozilla Firefox",
                "DEB_PKG_INSTALL_PATH": "usr/lib/firefox-nightly-try",
                "DEB_PKG_NAME": "firefox-nightly-try",
                "DEB_PKG_VERSION": "112.0~build2",
            },
        ),
    ),
)
def test_get_build_variables(
    version, build_number, package_name_suffix, description_suffix, expected
):
    application_ini_data = {
        "name": "Firefox",
        "display_name": "Firefox",
        "vendor": "Mozilla",
        "remoting_name": "firefox-nightly-try",
        "build_id": "20230222000000",
        "timestamp": datetime.datetime(2023, 2, 22),
    }
    assert deb._get_build_variables(
        application_ini_data,
        "x86",
        version,
        build_number,
        depends="${shlibs:Depends},",
        package_name_suffix=package_name_suffix,
        description_suffix=description_suffix,
    ) == {
        **{
            "DEB_CHANGELOG_DATE": "Wed, 22 Feb 2023 00:00:00 -0000",
            "DEB_ARCH_NAME": "i386",
            "DEB_DEPENDS": "${shlibs:Depends},",
        },
        **expected,
    }


def test_copy_plain_deb_config(monkeypatch):
    def mock_listdir(dir):
        assert dir == "/template_dir"
        return [
            "/template_dir/debian_file1.in",
            "/template_dir/debian_file2.in",
            "/template_dir/debian_file3",
            "/template_dir/debian_file4",
        ]

    monkeypatch.setattr(deb.os, "listdir", mock_listdir)

    def mock_makedirs(dir, exist_ok):
        assert dir == "/source_dir/debian"
        assert exist_ok is True

    monkeypatch.setattr(deb.os, "makedirs", mock_makedirs)

    mock_copy = MagicMock()
    monkeypatch.setattr(deb.shutil, "copy", mock_copy)

    deb._copy_plain_deb_config("/template_dir", "/source_dir")
    assert mock_copy.call_args_list == [
        call("/template_dir/debian_file3", "/source_dir/debian/debian_file3"),
        call("/template_dir/debian_file4", "/source_dir/debian/debian_file4"),
    ]


def test_render_deb_templates():
    with tempfile.TemporaryDirectory() as template_dir, tempfile.TemporaryDirectory() as source_dir:
        with open(os.path.join(template_dir, "debian_file1.in"), "w") as f:
            f.write("${some_build_variable}")

        with open(os.path.join(template_dir, "debian_file2.in"), "w") as f:
            f.write("Some hardcoded value")

        with open(os.path.join(template_dir, "ignored_file.in"), "w") as f:
            f.write("Must not be copied")

        deb._render_deb_templates(
            template_dir,
            source_dir,
            {"some_build_variable": "some_value"},
            exclude_file_names=["ignored_file.in"],
        )

        with open(os.path.join(source_dir, "debian", "debian_file1")) as f:
            assert f.read() == "some_value"

        with open(os.path.join(source_dir, "debian", "debian_file2")) as f:
            assert f.read() == "Some hardcoded value"

        assert not os.path.exists(os.path.join(source_dir, "debian", "ignored_file"))
        assert not os.path.exists(os.path.join(source_dir, "debian", "ignored_file.in"))


def test_inject_deb_distribution_folder(monkeypatch):
    def mock_check_call(command):
        global clone_dir
        clone_dir = command[-1]
        os.makedirs(os.path.join(clone_dir, "desktop/deb/distribution"))

    monkeypatch.setattr(deb.subprocess, "check_call", mock_check_call)

    def mock_copytree(source_tree, destination_tree):
        global clone_dir
        assert source_tree == mozpath.join(clone_dir, "desktop/deb/distribution")
        assert destination_tree == "/source_dir/firefox/distribution"

    monkeypatch.setattr(deb.shutil, "copytree", mock_copytree)

    deb._inject_deb_distribution_folder("/source_dir", "Firefox")


@pytest.mark.parametrize(
    "does_path_exits, expectation",
    (
        (True, does_not_raise()),
        (False, pytest.raises(deb.NoDebPackageFound)),
    ),
)
def test_generate_deb_archive(
    monkeypatch,
    does_path_exits,
    expectation,
):
    monkeypatch.setattr(deb, "_get_command", lambda _: ["mock_command"])
    monkeypatch.setattr(deb.subprocess, "check_call", lambda *_, **__: None)

    def mock_exists(path):
        assert path == "/target_dir/firefox_111.0_amd64.deb"
        return does_path_exits

    monkeypatch.setattr(deb.os.path, "exists", mock_exists)

    def mock_move(source_path, destination_path):
        assert source_path == "/target_dir/firefox_111.0_amd64.deb"
        assert destination_path == "/output/target.deb"

    monkeypatch.setattr(deb.shutil, "move", mock_move)

    with expectation:
        deb._generate_deb_archive(
            source_dir="/source_dir",
            target_dir="/target_dir",
            output_file_path="/output/target.deb",
            build_variables={
                "DEB_PKG_NAME": "firefox",
                "DEB_PKG_VERSION": "111.0",
            },
            arch="x86_64",
        )


@pytest.mark.parametrize(
    "arch, is_chroot_available, expected",
    (
        (
            "all",
            True,
            [
                "chroot",
                "/srv/jessie-amd64",
                "bash",
                "-c",
                "cd /tmp/*/source; dpkg-buildpackage -us -uc -b",
            ],
        ),
        ("all", False, ["dpkg-buildpackage", "-us", "-uc", "-b"]),
        (
            "x86",
            True,
            [
                "chroot",
                "/srv/jessie-i386",
                "bash",
                "-c",
                "cd /tmp/*/source; dpkg-buildpackage -us -uc -b --host-arch=i386",
            ],
        ),
        ("x86", False, ["dpkg-buildpackage", "-us", "-uc", "-b", "--host-arch=i386"]),
        (
            "x86_64",
            True,
            [
                "chroot",
                "/srv/jessie-amd64",
                "bash",
                "-c",
                "cd /tmp/*/source; dpkg-buildpackage -us -uc -b --host-arch=amd64",
            ],
        ),
        (
            "x86_64",
            False,
            ["dpkg-buildpackage", "-us", "-uc", "-b", "--host-arch=amd64"],
        ),
    ),
)
def test_get_command(monkeypatch, arch, is_chroot_available, expected):
    monkeypatch.setattr(deb, "_is_chroot_available", lambda _: is_chroot_available)
    assert deb._get_command(arch) == expected


@pytest.mark.parametrize(
    "arch, does_dir_exist, expected_path, expected_result",
    (
        ("all", False, "/srv/jessie-amd64", False),
        ("all", True, "/srv/jessie-amd64", True),
        ("x86", False, "/srv/jessie-i386", False),
        ("x86_64", False, "/srv/jessie-amd64", False),
        ("x86", True, "/srv/jessie-i386", True),
        ("x86_64", True, "/srv/jessie-amd64", True),
    ),
)
def test_is_chroot_available(
    monkeypatch, arch, does_dir_exist, expected_path, expected_result
):
    def _mock_is_dir(path):
        assert path == expected_path
        return does_dir_exist

    monkeypatch.setattr(deb.os.path, "isdir", _mock_is_dir)
    assert deb._is_chroot_available(arch) == expected_result


@pytest.mark.parametrize(
    "arch, expected",
    (
        ("all", "/srv/jessie-amd64"),
        ("x86", "/srv/jessie-i386"),
        ("x86_64", "/srv/jessie-amd64"),
    ),
)
def test_get_chroot_path(arch, expected):
    assert deb._get_chroot_path(arch) == expected


_MANIFEST_JSON_DATA = {
    "langpack_id": "fr",
    "manifest_version": 2,
    "browser_specific_settings": {
        "gecko": {
            "id": "langpack-fr@devedition.mozilla.org",
            "strict_min_version": "112.0a1",
            "strict_max_version": "112.0a1",
        }
    },
    "name": "Language: Français (French)",
    "description": "Firefox Developer Edition Language Pack for Français (fr) – French",
    "version": "112.0.20230227.181253",
    "languages": {
        "fr": {
            "version": "20230223164410",
            "chrome_resources": {
                "app-marketplace-icons": "browser/chrome/browser/locale/fr/app-marketplace-icons/",
                "branding": "browser/chrome/fr/locale/branding/",
                "browser": "browser/chrome/fr/locale/browser/",
                "browser-region": "browser/chrome/fr/locale/browser-region/",
                "devtools": "browser/chrome/fr/locale/fr/devtools/client/",
                "devtools-shared": "browser/chrome/fr/locale/fr/devtools/shared/",
                "formautofill": "browser/features/formautofill@mozilla.org/fr/locale/fr/",
                "report-site-issue": "browser/features/webcompat-reporter@mozilla.org/fr/locale/fr/",
                "alerts": "chrome/fr/locale/fr/alerts/",
                "autoconfig": "chrome/fr/locale/fr/autoconfig/",
                "global": "chrome/fr/locale/fr/global/",
                "global-platform": {
                    "macosx": "chrome/fr/locale/fr/global-platform/mac/",
                    "linux": "chrome/fr/locale/fr/global-platform/unix/",
                    "android": "chrome/fr/locale/fr/global-platform/unix/",
                    "win": "chrome/fr/locale/fr/global-platform/win/",
                },
                "mozapps": "chrome/fr/locale/fr/mozapps/",
                "necko": "chrome/fr/locale/fr/necko/",
                "passwordmgr": "chrome/fr/locale/fr/passwordmgr/",
                "pdf.js": "chrome/fr/locale/pdfviewer/",
                "pipnss": "chrome/fr/locale/fr/pipnss/",
                "pippki": "chrome/fr/locale/fr/pippki/",
                "places": "chrome/fr/locale/fr/places/",
                "weave": "chrome/fr/locale/fr/services/",
            },
        }
    },
    "sources": {"browser": {"base_path": "browser/"}},
    "author": "mozfr.org (contributors: L’équipe francophone)",
}


def test_extract_langpack_metadata():
    with tempfile.TemporaryDirectory() as d:
        langpack_path = os.path.join(d, "langpack.xpi")
        with zipfile.ZipFile(langpack_path, "w") as zip:
            zip.writestr("manifest.json", json.dumps(_MANIFEST_JSON_DATA))

        assert deb._extract_langpack_metadata(langpack_path) == _MANIFEST_JSON_DATA


if __name__ == "__main__":
    mozunit.main()
