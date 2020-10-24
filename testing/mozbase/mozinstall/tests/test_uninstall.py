from __future__ import absolute_import

import mozinfo
import mozinstall
import mozunit
import py
import pytest


@pytest.mark.skipif(
    mozinfo.isWin, reason='Bug 1157352 - New firefox.exe needed for mozinstall 1.12 and higher.')
def test_uninstall(tmpdir, get_installer):
    """Test to uninstall an installed binary."""
    if mozinfo.isLinux:
        installdir = mozinstall.install(get_installer('tar.bz2'), tmpdir.strpath)
        mozinstall.uninstall(installdir)
        assert not py.path.local(installdir).check()

    elif mozinfo.isWin:
        installdir_exe = mozinstall.install(get_installer('exe'), tmpdir.join('exe').strpath)
        mozinstall.uninstall(installdir_exe)
        assert not py.path.local(installdir).check()

        installdir_zip = mozinstall.install(get_installer('zip'), tmpdir.join('zip').strpath)
        mozinstall.uninstall(installdir_zip)
        assert not py.path.local(installdir).check()

    elif mozinfo.isMac:
        installdir = mozinstall.install(get_installer('dmg'), tmpdir.strpath)
        mozinstall.uninstall(installdir)
        assert not py.path.local(installdir).check()


if __name__ == '__main__':
    mozunit.main()
