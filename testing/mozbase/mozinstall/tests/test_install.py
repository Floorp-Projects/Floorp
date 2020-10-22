from __future__ import absolute_import

import subprocess

import mozinfo
import mozinstall
import mozunit
import pytest


@pytest.mark.skipif(
    mozinfo.isWin, reason='Bug 1157352 - New firefox.exe needed for mozinstall 1.12 and higher.')
def test_is_installer(request, get_installer):
    """Test that we can identify a correct installer."""
    if mozinfo.isLinux:
        assert mozinstall.is_installer(get_installer('tar.bz2'))

    if mozinfo.isWin:
        # test zip installer
        assert mozinstall.is_installer(get_installer('zip'))

        # test exe installer
        assert mozinstall.is_installer(get_installer('exe'))

        try:
            # test stub browser file
            # without pefile on the system this test will fail
            import pefile  # noqa
            stub_exe = request.node.fspath.dirpath('build_stub').join('firefox.exe').strpath
            assert not mozinstall.is_installer(stub_exe)
        except ImportError:
            pass

    if mozinfo.isMac:
        assert mozinstall.is_installer(get_installer('dmg'))


def test_invalid_source_error(get_installer):
    """Test that InvalidSource error is raised with an incorrect installer."""
    if mozinfo.isLinux:
        with pytest.raises(mozinstall.InvalidSource):
            mozinstall.install(get_installer('dmg'), 'firefox')

    elif mozinfo.isWin:
        with pytest.raises(mozinstall.InvalidSource):
            mozinstall.install(get_installer('tar.bz2'), 'firefox')

    elif mozinfo.isMac:
        with pytest.raises(mozinstall.InvalidSource):
            mozinstall.install(get_installer('tar.bz2'), 'firefox')

    # Test an invalid url handler
    with pytest.raises(mozinstall.InvalidSource):
        mozinstall.install('file://foo.bar', 'firefox')


@pytest.mark.skipif(
    mozinfo.isWin, reason='Bug 1157352 - New firefox.exe needed for mozinstall 1.12 and higher.')
def test_install(tmpdir, get_installer):
    """Test to install an installer."""
    if mozinfo.isLinux:
        installdir = mozinstall.install(get_installer('tar.bz2'), tmpdir.strpath)
        assert installdir == tmpdir.join('firefox').strpath

    elif mozinfo.isWin:
        installdir_exe = mozinstall.install(get_installer('exe'), tmpdir.join('exe').strpath)
        assert installdir_exe == tmpdir.join('exe', 'firefox').strpath

        installdir_zip = mozinstall.install(get_installer('zip'), tmpdir.join('zip').strpath)
        assert installdir_zip == tmpdir.join('zip', 'firefox').strpath

    elif mozinfo.isMac:
        installdir = mozinstall.install(get_installer('dmg'), tmpdir.strpath)
        assert installdir == tmpdir.realpath().join('Firefox Stub.app').strpath

        mounted_images = subprocess.check_output(['hdiutil', 'info']).decode('ascii')
        assert get_installer('dmg') not in mounted_images


if __name__ == '__main__':
    mozunit.main()
