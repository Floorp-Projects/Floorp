# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
from unittest import mock

import pytest as pytest

from mozunit import main
from mach.site import SitePackagesSource


@pytest.mark.parametrize(
    "env_native_package_source,env_use_system_python,env_moz_automation,expected",
    [
        ("system", False, False, SitePackagesSource.SYSTEM),
        ("pip", False, False, SitePackagesSource.VENV),
        ("none", False, False, SitePackagesSource.NONE),
        (None, False, False, SitePackagesSource.VENV),
        (None, False, True, SitePackagesSource.NONE),
        (None, True, False, SitePackagesSource.NONE),
        (None, True, True, SitePackagesSource.NONE),
    ],
)
def test_resolve_package_source(
    env_native_package_source, env_use_system_python, env_moz_automation, expected
):
    with mock.patch.dict(
        os.environ,
        {
            "MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE": env_native_package_source or "",
            "MACH_USE_SYSTEM_PYTHON": "1" if env_use_system_python else "",
            "MOZ_AUTOMATION": "1" if env_moz_automation else "",
        },
    ):
        assert SitePackagesSource.from_environment("build") == expected


def test_resolve_package_source_always_venv_for_most_sites():
    # Only sites in PIP_NETWORK_INSTALL_RESTRICTED_VIRTUALENVS have to be able to function
    # using only vendored packages or system packages.
    # All others must have an associated virtualenv.
    assert SitePackagesSource.from_environment("python-test") == SitePackagesSource.VENV


if __name__ == "__main__":
    main()
