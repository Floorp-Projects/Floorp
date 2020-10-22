from __future__ import absolute_import

import pytest


@pytest.fixture
def get_installer(request):
    def _get_installer(extension):
        """Get path to the installer for the specified extension."""
        stub_dir = request.node.fspath.dirpath('installer_stubs')

        # We had to remove firefox.exe since it is not valid for mozinstall 1.12 and higher
        # Bug 1157352 - We should grab a firefox.exe from the build process or download it
        return stub_dir.join('firefox.{}'.format(extension)).strpath

    return _get_installer
