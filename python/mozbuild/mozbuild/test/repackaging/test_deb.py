# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
import pytest

from mozbuild.repackaging import deb


@pytest.mark.parametrize(
    "arch, does_dir_exist, expected_path, expected_result",
    (
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


if __name__ == "__main__":
    mozunit.main()
