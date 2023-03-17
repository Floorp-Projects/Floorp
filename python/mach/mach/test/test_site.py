# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from unittest import mock

import pytest
from buildconfig import topsrcdir
from mozunit import main

from mach.site import (
    PIP_NETWORK_INSTALL_RESTRICTED_VIRTUALENVS,
    SitePackagesSource,
    resolve_requirements,
)


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
        assert SitePackagesSource.for_mach() == expected


def test_all_restricted_sites_dont_have_pypi_requirements():
    for site_name in PIP_NETWORK_INSTALL_RESTRICTED_VIRTUALENVS:
        requirements = resolve_requirements(topsrcdir, site_name)
        assert not requirements.pypi_requirements, (
            'Sites that must be able to operate without "pip install" must not have any '
            f'mandatory "pypi requirements". However, the "{site_name}" site depends on: '
            f"{requirements.pypi_requirements}"
        )


if __name__ == "__main__":
    main()
