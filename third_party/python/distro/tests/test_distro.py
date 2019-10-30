# Copyright 2015,2016 Nir Cohen
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import sys
import ast
import subprocess
try:
    from StringIO import StringIO  # Python 2.x
except ImportError:
    from io import StringIO  # Python 3.x

import pytest


BASE = os.path.abspath(os.path.dirname(__file__))
RESOURCES = os.path.join(BASE, 'resources')
DISTROS_DIR = os.path.join(RESOURCES, 'distros')
TESTDISTROS = os.path.join(RESOURCES, 'testdistros')
SPECIAL = os.path.join(RESOURCES, 'special')
DISTROS = [dist for dist in os.listdir(DISTROS_DIR) if dist != '__shared__']


IS_LINUX = sys.platform.startswith('linux')
if IS_LINUX:
    import distro

    RELATIVE_UNIXCONFDIR = distro._UNIXCONFDIR[1:]
    MODULE_DISTRO = distro._distro


class TestNonLinuxPlatform:
    """Obviously, this only tests Windows. Will add OS X tests on Travis
    Later
    """

    def test_cant_use_on_windows(self):
        try:
            import distro  # NOQA
        except ImportError as ex:
            assert 'Unsupported platform' in str(ex)


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestCli:

    def _parse(self, command):
        sys.argv = command.split()
        distro.main()

    def _run(self, command):
        stdout, _ = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE).communicate()
        # Need to decode or we get bytes in Python 3.x
        return stdout.decode('utf-8')

    def test_cli_for_coverage_yuch(self):
        self._parse('distro')
        self._parse('distro -j')

    def test_cli(self):
        command = [sys.executable, '-m', 'distro']
        desired_output = 'Name: ' + distro.name(pretty=True)
        distro_version = distro.version(pretty=True)
        distro_codename = distro.codename()
        desired_output += '\n' + 'Version: ' + distro_version
        desired_output += '\n' + 'Codename: ' + distro_codename
        desired_output += '\n'
        assert self._run(command) == desired_output

    def test_cli_json(self):
        command = [sys.executable, '-m', 'distro', '-j']
        assert ast.literal_eval(self._run(command)) == distro.info()


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class DistroTestCase(object):
    """A base class for any testcase classes that test the distributions
    represented in the `DISTROS` subtree.
    """

    def setup_method(self, test_method):
        # The environment stays the same across all testcases, so we
        # save and restore the PATH env var in each test case that
        # changes it:
        self._saved_path = os.environ["PATH"]
        self._saved_UNIXCONFDIR = distro._UNIXCONFDIR

    def teardown_method(self, test_method):
        os.environ["PATH"] = self._saved_path
        distro._UNIXCONFDIR = self._saved_UNIXCONFDIR

    def _setup_for_distro(self, distro_root):
        distro_bin = os.path.join(distro_root, 'bin')
        # We don't want to pick up a possibly present lsb_release in the
        # distro that runs this test, so we use a PATH with only one entry:
        os.environ["PATH"] = distro_bin
        distro._UNIXCONFDIR = os.path.join(distro_root, RELATIVE_UNIXCONFDIR)


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestOSRelease:

    def setup_method(self, test_method):
        dist = test_method.__name__.split('_')[1]
        os_release = os.path.join(DISTROS_DIR, dist, 'etc', 'os-release')
        self.distro = distro.LinuxDistribution(False, os_release, 'non')

    def _test_outcome(self, outcome):
        assert self.distro.id() == outcome.get('id', '')
        assert self.distro.name() == outcome.get('name', '')
        assert self.distro.name(pretty=True) == outcome.get('pretty_name', '')
        assert self.distro.version() == outcome.get('version', '')
        assert self.distro.version(pretty=True) == \
            outcome.get('pretty_version', '')
        assert self.distro.version(best=True) == \
            outcome.get('best_version', '')
        assert self.distro.like() == outcome.get('like', '')
        assert self.distro.codename() == outcome.get('codename', '')

    def test_arch_os_release(self):
        desired_outcome = {
            'id': 'arch',
            'name': 'Arch Linux',
            'pretty_name': 'Arch Linux',
        }
        self._test_outcome(desired_outcome)

    def test_kali_os_release(self):
        desired_outcome = {
            'id': 'kali',
            'name': 'Kali GNU/Linux',
            'pretty_name': 'Kali GNU/Linux Rolling',
            'version': '2017.1',
            'pretty_version': '2017.1',
            'best_version': '2017.1',
            'like': 'debian'
        }
        self._test_outcome(desired_outcome)

    def test_centos7_os_release(self):
        desired_outcome = {
            'id': 'centos',
            'name': 'CentOS Linux',
            'pretty_name': 'CentOS Linux 7 (Core)',
            'version': '7',
            'pretty_version': '7 (Core)',
            'best_version': '7',
            'like': 'rhel fedora',
            'codename': 'Core'
        }
        self._test_outcome(desired_outcome)

    def test_coreos_os_release(self):
        desired_outcome = {
            'id': 'coreos',
            'name': 'CoreOS',
            'pretty_name': 'CoreOS 899.15.0',
            'version': '899.15.0',
            'pretty_version': '899.15.0',
            'best_version': '899.15.0'
        }
        self._test_outcome(desired_outcome)

    def test_debian8_os_release(self):
        desired_outcome = {
            'id': 'debian',
            'name': 'Debian GNU/Linux',
            'pretty_name': 'Debian GNU/Linux 8 (jessie)',
            'version': '8',
            'pretty_version': '8 (jessie)',
            'best_version': '8',
            'codename': 'jessie'
        }
        self._test_outcome(desired_outcome)

    def test_fedora19_os_release(self):
        desired_outcome = {
            'id': 'fedora',
            'name': 'Fedora',
            'pretty_name': u'Fedora 19 (Schr\u00F6dinger\u2019s Cat)',
            'version': '19',
            'pretty_version': u'19 (Schr\u00F6dinger\u2019s Cat)',
            'best_version': '19',
            'codename': u'Schr\u00F6dinger\u2019s Cat'
        }
        self._test_outcome(desired_outcome)

    def test_fedora23_os_release(self):
        desired_outcome = {
            'id': 'fedora',
            'name': 'Fedora',
            'pretty_name': 'Fedora 23 (Twenty Three)',
            'version': '23',
            'pretty_version': '23 (Twenty Three)',
            'best_version': '23',
            'codename': 'Twenty Three'
        }
        self._test_outcome(desired_outcome)

    def test_fedora30_os_release(self):
        # Fedora 21 and above no longer have code names but the metadata in os-release was only
        # changed in a detectable way in Fedora 30+.  The piece in parenthesis in the pretty_name
        # field contains the VARIANT and differs depending on the variant which was installed.
        desired_outcome = {
            'id': 'fedora',
            'name': 'Fedora',
            'pretty_name': 'Fedora 30 (Thirty)',
            'version': '30',
            'pretty_version': '30',
            'best_version': '30',
            'codename': ''
        }
        self._test_outcome(desired_outcome)

    def test_kvmibm1_os_release(self):
        desired_outcome = {
            'id': 'kvmibm',
            'name': 'KVM for IBM z Systems',
            'pretty_name': 'KVM for IBM z Systems 1.1.1 (Z)',
            'version': '1.1.1',
            'pretty_version': '1.1.1 (Z)',
            'best_version': '1.1.1',
            'like': 'rhel fedora',
            'codename': 'Z'
        }
        self._test_outcome(desired_outcome)

    def test_linuxmint17_os_release(self):
        # Note: LinuxMint 17 actually *does* have Ubuntu 14.04 data in its
        #       os-release file. See discussion in GitHub issue #78.
        desired_outcome = {
            'id': 'ubuntu',
            'name': 'Ubuntu',
            'pretty_name': 'Ubuntu 14.04.3 LTS',
            'version': '14.04',
            'pretty_version': '14.04 (Trusty Tahr)',
            'best_version': '14.04.3',
            'like': 'debian',
            'codename': 'Trusty Tahr'
        }
        self._test_outcome(desired_outcome)

    def test_mageia5_os_release(self):
        desired_outcome = {
            'id': 'mageia',
            'name': 'Mageia',
            'pretty_name': 'Mageia 5',
            'version': '5',
            'pretty_version': '5',
            'best_version': '5',
            'like': 'mandriva fedora',
        }
        self._test_outcome(desired_outcome)

    def test_manjaro1512_os_release(self):
        self._test_outcome({
            'id': 'manjaro',
            'name': 'Manjaro Linux',
            'pretty_name': 'Manjaro Linux',
        })

    def test_opensuse42_os_release(self):
        desired_outcome = {
            'id': 'opensuse',
            'name': 'openSUSE Leap',
            'pretty_name': 'openSUSE Leap 42.1 (x86_64)',
            'version': '42.1',
            'pretty_version': '42.1',
            'best_version': '42.1',
            'like': 'suse',
        }
        self._test_outcome(desired_outcome)

    def test_raspbian7_os_release(self):
        desired_outcome = {
            'id': 'raspbian',
            'name': 'Raspbian GNU/Linux',
            'pretty_name': 'Raspbian GNU/Linux 7 (wheezy)',
            'version': '7',
            'pretty_version': '7 (wheezy)',
            'best_version': '7',
            'like': 'debian',
            'codename': 'wheezy'
        }
        self._test_outcome(desired_outcome)

    def test_raspbian8_os_release(self):
        desired_outcome = {
            'id': 'raspbian',
            'name': 'Raspbian GNU/Linux',
            'pretty_name': 'Raspbian GNU/Linux 8 (jessie)',
            'version': '8',
            'pretty_version': '8 (jessie)',
            'best_version': '8',
            'like': 'debian',
            'codename': 'jessie'
        }
        self._test_outcome(desired_outcome)

    def test_rhel7_os_release(self):
        desired_outcome = {
            'id': 'rhel',
            'name': 'Red Hat Enterprise Linux Server',
            'pretty_name': 'Red Hat Enterprise Linux Server 7.0 (Maipo)',
            'version': '7.0',
            'pretty_version': '7.0 (Maipo)',
            'best_version': '7.0',
            'like': 'fedora',
            'codename': 'Maipo'
        }
        self._test_outcome(desired_outcome)

    def test_slackware14_os_release(self):
        desired_outcome = {
            'id': 'slackware',
            'name': 'Slackware',
            'pretty_name': 'Slackware 14.1',
            'version': '14.1',
            'pretty_version': '14.1',
            'best_version': '14.1'
        }
        self._test_outcome(desired_outcome)

    def test_sles12_os_release(self):
        desired_outcome = {
            'id': 'sles',
            'name': 'SLES',
            'pretty_name': 'SUSE Linux Enterprise Server 12 SP1',
            'version': '12.1',
            'pretty_version': '12.1',
            'best_version': '12.1'
        }
        self._test_outcome(desired_outcome)

    def test_ubuntu14_os_release(self):
        desired_outcome = {
            'id': 'ubuntu',
            'name': 'Ubuntu',
            'pretty_name': 'Ubuntu 14.04.3 LTS',
            'version': '14.04',
            'pretty_version': '14.04 (Trusty Tahr)',
            'best_version': '14.04.3',
            'like': 'debian',
            'codename': 'Trusty Tahr'
        }
        self._test_outcome(desired_outcome)

    def test_ubuntu16_os_release(self):
        desired_outcome = {
            'id': 'ubuntu',
            'name': 'Ubuntu',
            'pretty_name': 'Ubuntu 16.04.1 LTS',
            'version': '16.04',
            'pretty_version': '16.04 (xenial)',
            'best_version': '16.04.1',
            'like': 'debian',
            'codename': 'xenial'
        }
        self._test_outcome(desired_outcome)

    def test_amazon2016_os_release(self):
        desired_outcome = {
            'id': 'amzn',
            'name': 'Amazon Linux AMI',
            'pretty_name': 'Amazon Linux AMI 2016.03',
            'version': '2016.03',
            'pretty_version': '2016.03',
            'best_version': '2016.03',
            'like': 'rhel fedora'
        }
        self._test_outcome(desired_outcome)

    def test_scientific7_os_release(self):
        desired_outcome = {
            'id': 'rhel',
            'name': 'Scientific Linux',
            'pretty_name': 'Scientific Linux 7.2 (Nitrogen)',
            'version': '7.2',
            'pretty_version': '7.2 (Nitrogen)',
            'best_version': '7.2',
            'like': 'fedora',
            'codename': 'Nitrogen'
        }
        self._test_outcome(desired_outcome)

    def test_gentoo_os_release(self):
        desired_outcome = {
            'id': 'gentoo',
            'name': 'Gentoo',
            'pretty_name': 'Gentoo/Linux',
        }
        self._test_outcome(desired_outcome)

    def test_openelec6_os_release(self):
        desired_outcome = {
            'id': 'openelec',
            'name': 'OpenELEC',
            'pretty_name': 'OpenELEC (official) - Version: 6.0.3',
            'version': '6.0',
            'pretty_version': '6.0',
            'best_version': '6.0.3',
        }
        self._test_outcome(desired_outcome)

    def test_cloudlinux7_os_release(self):
        desired_outcome = {
            'id': 'cloudlinux',
            'codename': 'Yury Malyshev',
            'name': 'CloudLinux',
            'pretty_name': 'CloudLinux 7.3 (Yury Malyshev)',
            'like': 'rhel fedora centos',
            'version': '7.3',
            'pretty_version': '7.3 (Yury Malyshev)',
            'best_version': '7.3',
            'major_version': '7',
            'minor_version': '3'
        }
        self._test_outcome(desired_outcome)


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestLSBRelease(DistroTestCase):

    def setup_method(self, test_method):
        super(TestLSBRelease, self).setup_method(test_method)
        dist = test_method.__name__.split('_')[1]
        self._setup_for_distro(os.path.join(DISTROS_DIR, dist))
        self.distro = distro.LinuxDistribution(True, 'non', 'non')

    def _test_outcome(self, outcome):
        assert self.distro.id() == outcome.get('id', '')
        assert self.distro.name() == outcome.get('name', '')
        assert self.distro.name(pretty=True) == outcome.get('pretty_name', '')
        assert self.distro.version() == outcome.get('version', '')
        assert self.distro.version(pretty=True) == \
            outcome.get('pretty_version', '')
        assert self.distro.version(best=True) == \
            outcome.get('best_version', '')
        assert self.distro.like() == outcome.get('like', '')
        assert self.distro.codename() == outcome.get('codename', '')

    def test_linuxmint17_lsb_release(self):
        desired_outcome = {
            'id': 'linuxmint',
            'name': 'LinuxMint',
            'pretty_name': 'Linux Mint 17.3 Rosa',
            'version': '17.3',
            'pretty_version': '17.3 (rosa)',
            'best_version': '17.3',
            'codename': 'rosa'
        }
        self._test_outcome(desired_outcome)

    def test_manjaro1512_lsb_release(self):
        self._test_outcome({
            'id': 'manjarolinux',
            'name': 'ManjaroLinux',
            'pretty_name': 'Manjaro Linux',
            'version': '15.12',
            'pretty_version': '15.12 (Capella)',
            'best_version': '15.12',
            'codename': 'Capella'
        })

    # @pytest.mark.xfail
    # def test_openelec6_lsb_release(self):
    #     # TODO: This should be fixed as part of #109 when dealing
    #     # with distro inconsistencies
    #     desired_outcome = {
    #         'id': 'openelec',
    #         'name': 'OpenELEC',
    #         'pretty_name': 'OpenELEC (official) - Version: 6.0.3',
    #         'version': '6.0.3',
    #         'pretty_version': '6.0.3',
    #         'best_version': '6.0.3',
    #     }
    #     self._test_outcome(desired_outcome)

    def test_openbsd62_uname(self):
        self._test_outcome({
            'id': 'openbsd',
            'name': 'OpenBSD',
            'version': '6.2',
            'pretty_name': 'OpenBSD 6.2',
            'pretty_version': '6.2',
            'best_version': '6.2'
        })

    def test_netbsd711_uname(self):
        self._test_outcome({
            'id': 'netbsd',
            'name': 'NetBSD',
            'version': '7.1.1',
            'pretty_name': 'NetBSD 7.1.1',
            'pretty_version': '7.1.1',
            'best_version': '7.1.1'
        })

    def test_freebsd111_uname(self):
        self._test_outcome({
            'id': 'freebsd',
            'name': 'FreeBSD',
            'version': '11.1',
            'pretty_name': 'FreeBSD 11.1',
            'pretty_version': '11.1',
            'best_version': '11.1'
        })

    def test_ubuntu14normal_lsb_release(self):
        self._setup_for_distro(os.path.join(TESTDISTROS, 'lsb',
                                            'ubuntu14_normal'))

        self.distro = distro.LinuxDistribution(True, 'non', 'non')

        desired_outcome = {
            'id': 'ubuntu',
            'name': 'Ubuntu',
            'pretty_name': 'Ubuntu 14.04.3 LTS',
            'version': '14.04',
            'pretty_version': '14.04 (trusty)',
            'best_version': '14.04.3',
            'codename': 'trusty'
        }
        self._test_outcome(desired_outcome)

    def test_ubuntu14nomodules_lsb_release(self):
        self._setup_for_distro(os.path.join(TESTDISTROS, 'lsb',
                                            'ubuntu14_nomodules'))

        self.distro = distro.LinuxDistribution(True, 'non', 'non')

        desired_outcome = {
            'id': 'ubuntu',
            'name': 'Ubuntu',
            'pretty_name': 'Ubuntu 14.04.3 LTS',
            'version': '14.04',
            'pretty_version': '14.04 (trusty)',
            'best_version': '14.04.3',
            'codename': 'trusty'
        }
        self._test_outcome(desired_outcome)

    def test_trailingblanks_lsb_release(self):
        self._setup_for_distro(os.path.join(TESTDISTROS, 'lsb',
                                            'ubuntu14_trailingblanks'))

        self.distro = distro.LinuxDistribution(True, 'non', 'non')

        desired_outcome = {
            'id': 'ubuntu',
            'name': 'Ubuntu',
            'pretty_name': 'Ubuntu 14.04.3 LTS',
            'version': '14.04',
            'pretty_version': '14.04 (trusty)',
            'best_version': '14.04.3',
            'codename': 'trusty'
        }
        self._test_outcome(desired_outcome)

    @pytest.mark.parametrize('errnum', ('001', '002', '126', '130', '255'))
    def test_lsb_release_error_level(self, errnum):
        self._setup_for_distro(os.path.join(
            TESTDISTROS, 'lsb', 'lsb_rc{0}'.format(errnum)))
        with pytest.raises(subprocess.CalledProcessError) as excinfo:
            distro.LinuxDistribution(True, 'non', 'non')._lsb_release_info
        assert excinfo.value.returncode == int(errnum)


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestSpecialRelease(DistroTestCase):
    def _test_outcome(self, outcome):
        assert self.distro.id() == outcome.get('id', '')
        assert self.distro.name() == outcome.get('name', '')
        assert self.distro.name(pretty=True) == outcome.get('pretty_name', '')
        assert self.distro.version() == outcome.get('version', '')
        assert self.distro.version(pretty=True) == \
            outcome.get('pretty_version', '')
        assert self.distro.version(best=True) == \
            outcome.get('best_version', '')
        assert self.distro.like() == outcome.get('like', '')
        assert self.distro.codename() == outcome.get('codename', '')
        assert self.distro.major_version() == outcome.get('major_version', '')
        assert self.distro.minor_version() == outcome.get('minor_version', '')
        assert self.distro.build_number() == outcome.get('build_number', '')

    def test_empty_release(self):
        distro_release = os.path.join(SPECIAL, 'empty-release')
        self.distro = distro.LinuxDistribution(False, 'non', distro_release)

        desired_outcome = {
            'id': 'empty'
        }
        self._test_outcome(desired_outcome)

    def test_unknowndistro_release(self):
        self._setup_for_distro(os.path.join(TESTDISTROS, 'distro',
                                            'unknowndistro'))

        self.distro = distro.LinuxDistribution()

        desired_outcome = {
            'id': 'unknowndistro',
            'name': 'Unknown Distro',
            'pretty_name': 'Unknown Distro 1.0 (Unknown Codename)',
            'version': '1.0',
            'pretty_version': '1.0 (Unknown Codename)',
            'best_version': '1.0',
            'codename': 'Unknown Codename',
            'major_version': '1',
            'minor_version': '0'
        }
        self._test_outcome(desired_outcome)

    def test_bad_uname(self):
        self._setup_for_distro(os.path.join(TESTDISTROS, 'distro',
                                            'baduname'))
        self.distro = distro.LinuxDistribution()

        assert self.distro.uname_attr('id') == ''
        assert self.distro.uname_attr('name') == ''
        assert self.distro.uname_attr('release') == ''


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestDistroRelease:

    def _test_outcome(self,
                      outcome,
                      distro_name='',
                      version='',
                      release_file_id='',
                      release_file_suffix='release'):
        release_file_id = release_file_id or distro_name
        distro_release = os.path.join(
            DISTROS_DIR, distro_name + version, 'etc', '{0}-{1}'.format(
                release_file_id, release_file_suffix))
        self.distro = distro.LinuxDistribution(False, 'non', distro_release)

        assert self.distro.id() == outcome.get('id', '')
        assert self.distro.name() == outcome.get('name', '')
        assert self.distro.name(pretty=True) == outcome.get('pretty_name', '')
        assert self.distro.version() == outcome.get('version', '')
        assert self.distro.version(pretty=True) == \
            outcome.get('pretty_version', '')
        assert self.distro.version(best=True) == \
            outcome.get('best_version', '')
        assert self.distro.like() == outcome.get('like', '')
        assert self.distro.codename() == outcome.get('codename', '')
        assert self.distro.major_version() == outcome.get('major_version', '')
        assert self.distro.minor_version() == outcome.get('minor_version', '')
        assert self.distro.build_number() == outcome.get('build_number', '')

    def test_arch_dist_release(self):
        desired_outcome = {
            'id': 'arch'
        }
        self._test_outcome(desired_outcome, 'arch')

    def test_centos5_dist_release(self):
        desired_outcome = {
            'id': 'centos',
            'name': 'CentOS',
            'pretty_name': 'CentOS 5.11 (Final)',
            'version': '5.11',
            'pretty_version': '5.11 (Final)',
            'best_version': '5.11',
            'codename': 'Final',
            'major_version': '5',
            'minor_version': '11'
        }
        self._test_outcome(desired_outcome, 'centos', '5')

    def test_centos7_dist_release(self):
        desired_outcome = {
            'id': 'centos',
            'name': 'CentOS Linux',
            'pretty_name': 'CentOS Linux 7.1.1503 (Core)',
            'version': '7.1.1503',
            'pretty_version': '7.1.1503 (Core)',
            'best_version': '7.1.1503',
            'codename': 'Core',
            'major_version': '7',
            'minor_version': '1',
            'build_number': '1503'
        }
        self._test_outcome(desired_outcome, 'centos', '7')

    def test_fedora19_dist_release(self):
        desired_outcome = {
            'id': 'fedora',
            'name': 'Fedora',
            'pretty_name': u'Fedora 19 (Schr\u00F6dinger\u2019s Cat)',
            'version': '19',
            'pretty_version': u'19 (Schr\u00F6dinger\u2019s Cat)',
            'best_version': '19',
            'codename': u'Schr\u00F6dinger\u2019s Cat',
            'major_version': '19'
        }
        self._test_outcome(desired_outcome, 'fedora', '19')

    def test_fedora23_dist_release(self):
        desired_outcome = {
            'id': 'fedora',
            'name': 'Fedora',
            'pretty_name': 'Fedora 23 (Twenty Three)',
            'version': '23',
            'pretty_version': '23 (Twenty Three)',
            'best_version': '23',
            'codename': 'Twenty Three',
            'major_version': '23'
        }
        self._test_outcome(desired_outcome, 'fedora', '23')

    def test_fedora30_dist_release(self):
        desired_outcome = {
            'id': 'fedora',
            'name': 'Fedora',
            'pretty_name': 'Fedora 30 (Thirty)',
            'version': '30',
            'pretty_version': '30 (Thirty)',
            'best_version': '30',
            'codename': 'Thirty',
            'major_version': '30'
        }
        self._test_outcome(desired_outcome, 'fedora', '30')

    def test_gentoo_dist_release(self):
        desired_outcome = {
            'id': 'gentoo',
            'name': 'Gentoo Base System',
            'pretty_name': 'Gentoo Base System 2.2',
            'version': '2.2',
            'pretty_version': '2.2',
            'best_version': '2.2',
            'major_version': '2',
            'minor_version': '2',
        }
        self._test_outcome(desired_outcome, 'gentoo')

    def test_kvmibm1_dist_release(self):
        desired_outcome = {
            'id': 'base',
            'name': 'KVM for IBM z Systems',
            'pretty_name': 'KVM for IBM z Systems 1.1.1 (Z)',
            'version': '1.1.1',
            'pretty_version': '1.1.1 (Z)',
            'best_version': '1.1.1',
            'codename': 'Z',
            'major_version': '1',
            'minor_version': '1',
            'build_number': '1'
        }
        self._test_outcome(desired_outcome, 'kvmibm', '1', 'base')

    def test_mageia5_dist_release(self):
        desired_outcome = {
            'id': 'mageia',
            'name': 'Mageia',
            'pretty_name': 'Mageia 5 (Official)',
            'version': '5',
            'pretty_version': '5 (Official)',
            'best_version': '5',
            'codename': 'Official',
            'major_version': '5'
        }
        self._test_outcome(desired_outcome, 'mageia', '5')

    def test_manjaro1512_dist_release(self):
        self._test_outcome({
            'id': 'manjaro',
            'name': 'Manjaro Linux',
            'pretty_name': 'Manjaro Linux',
            'version': '',
            'codename': ''
        }, 'manjaro', '1512')

    def test_opensuse42_dist_release(self):
        desired_outcome = {
            'id': 'suse',
            'name': 'openSUSE',
            'pretty_name': 'openSUSE 42.1 (x86_64)',
            'version': '42.1',
            'pretty_version': '42.1 (x86_64)',
            'best_version': '42.1',
            'codename': 'x86_64',
            'major_version': '42',
            'minor_version': '1'
        }
        self._test_outcome(desired_outcome, 'opensuse', '42', 'SuSE')

    def test_oracle7_dist_release(self):
        desired_outcome = {
            'id': 'oracle',
            'name': 'Oracle Linux Server',
            'pretty_name': 'Oracle Linux Server 7.5',
            'version': '7.5',
            'pretty_version': '7.5',
            'best_version': '7.5',
            'major_version': '7',
            'minor_version': '5'
        }
        self._test_outcome(desired_outcome, 'oracle', '7')

    def test_rhel6_dist_release(self):
        desired_outcome = {
            'id': 'rhel',
            'name': 'Red Hat Enterprise Linux Server',
            'pretty_name': 'Red Hat Enterprise Linux Server 6.5 (Santiago)',
            'version': '6.5',
            'pretty_version': '6.5 (Santiago)',
            'best_version': '6.5',
            'codename': 'Santiago',
            'major_version': '6',
            'minor_version': '5'
        }
        self._test_outcome(desired_outcome, 'rhel', '6', 'redhat')

    def test_rhel7_dist_release(self):
        desired_outcome = {
            'id': 'rhel',
            'name': 'Red Hat Enterprise Linux Server',
            'pretty_name': 'Red Hat Enterprise Linux Server 7.0 (Maipo)',
            'version': '7.0',
            'pretty_version': '7.0 (Maipo)',
            'best_version': '7.0',
            'codename': 'Maipo',
            'major_version': '7',
            'minor_version': '0'
        }
        self._test_outcome(desired_outcome, 'rhel', '7', 'redhat')

    def test_slackware14_dist_release(self):
        desired_outcome = {
            'id': 'slackware',
            'name': 'Slackware',
            'pretty_name': 'Slackware 14.1',
            'version': '14.1',
            'pretty_version': '14.1',
            'best_version': '14.1',
            'major_version': '14',
            'minor_version': '1'
        }
        self._test_outcome(
            desired_outcome,
            'slackware',
            '14',
            release_file_suffix='version')

    def test_sles12_dist_release(self):
        desired_outcome = {
            'id': 'suse',
            'name': 'SUSE Linux Enterprise Server',
            'pretty_name': 'SUSE Linux Enterprise Server 12 (s390x)',
            'version': '12',
            'pretty_version': '12 (s390x)',
            'best_version': '12',
            'major_version': '12',
            'codename': 's390x'
        }
        self._test_outcome(desired_outcome, 'sles', '12', 'SuSE')

    def test_cloudlinux5_dist_release(self):
        # Uses redhat-release only to get information.
        # The id of 'rhel' can only be fixed with issue #109.
        desired_outcome = {
            'id': 'cloudlinux',
            'codename': 'Vladislav Volkov',
            'name': 'CloudLinux Server',
            'pretty_name': 'CloudLinux Server 5.11 (Vladislav Volkov)',
            'version': '5.11',
            'pretty_version': '5.11 (Vladislav Volkov)',
            'best_version': '5.11',
            'major_version': '5',
            'minor_version': '11'
        }
        self._test_outcome(desired_outcome, 'cloudlinux', '5', 'redhat')

    def test_cloudlinux6_dist_release(self):
        # Same as above, only has redhat-release.
        desired_outcome = {
            'id': 'cloudlinux',
            'codename': 'Oleg Makarov',
            'name': 'CloudLinux Server',
            'pretty_name': 'CloudLinux Server 6.8 (Oleg Makarov)',
            'version': '6.8',
            'pretty_version': '6.8 (Oleg Makarov)',
            'best_version': '6.8',
            'major_version': '6',
            'minor_version': '8'
        }
        self._test_outcome(desired_outcome, 'cloudlinux', '6', 'redhat')

    def test_cloudlinux7_dist_release(self):
        desired_outcome = {
            'id': 'cloudlinux',
            'codename': 'Yury Malyshev',
            'name': 'CloudLinux',
            'pretty_name': 'CloudLinux 7.3 (Yury Malyshev)',
            'version': '7.3',
            'pretty_version': '7.3 (Yury Malyshev)',
            'best_version': '7.3',
            'major_version': '7',
            'minor_version': '3'
        }
        self._test_outcome(desired_outcome, 'cloudlinux', '7', 'redhat')


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestOverall(DistroTestCase):
    """Test a LinuxDistribution object created with default arguments.

    The direct accessor functions on that object are tested (e.g. `id()`); they
    implement the precedence between the different sources of information.

    In addition, because the distro release file is searched when not
    specified, the information resulting from the distro release file is also
    tested. The LSB and os-release sources are not tested again, because their
    test is already done in TestLSBRelease and TestOSRelease, and their
    algorithm does not depend on whether or not the file is specified.

    TODO: This class should have testcases for all distros that are claimed
    to be reliably maintained w.r.t. to their ID (see `id()`). Testcases for
    the following distros are still missing:
      * `amazon` - Amazon Linux
      * `gentoo` - GenToo Linux
      * `ibm_powerkvm` - IBM PowerKVM
      * `parallels` - Parallels
      * `pidora` - Pidora (Fedora remix for Raspberry Pi)
      * `raspbian` - Raspbian
      * `scientific` - Scientific Linux
      * `xenserver` - XenServer
    """

    def setup_method(self, test_method):
        super(TestOverall, self).setup_method(test_method)
        dist = test_method.__name__.split('_')[1]
        self._setup_for_distro(os.path.join(DISTROS_DIR, dist))
        self.distro = distro.LinuxDistribution()

    def _test_outcome(self, outcome):
        assert self.distro.id() == outcome.get('id', '')
        assert self.distro.name() == outcome.get('name', '')
        assert self.distro.name(pretty=True) == outcome.get('pretty_name', '')
        assert self.distro.version() == outcome.get('version', '')
        assert self.distro.version(pretty=True) == \
            outcome.get('pretty_version', '')
        assert self.distro.version(best=True) == \
            outcome.get('best_version', '')
        assert self.distro.like() == outcome.get('like', '')
        assert self.distro.codename() == outcome.get('codename', '')
        assert self.distro.major_version() == outcome.get('major_version', '')
        assert self.distro.minor_version() == outcome.get('minor_version', '')
        assert self.distro.build_number() == outcome.get('build_number', '')

    def _test_non_existing_release_file(self):
        # Test the info from the searched distro release file
        # does not have one.
        assert self.distro.distro_release_file == ''
        assert len(self.distro.distro_release_info()) == 0

    def _test_release_file_info(self, filename, outcome):
        # Test the info from the searched distro release file
        assert os.path.basename(self.distro.distro_release_file) == filename
        distro_info = self.distro.distro_release_info()
        for key, value in outcome.items():
            assert distro_info[key] == value
        return distro_info

    def test_arch_release(self):
        desired_outcome = {
            'id': 'arch',
            'name': 'Arch Linux',
            'pretty_name': 'Arch Linux',
        }
        self._test_outcome(desired_outcome)

        # Test the info from the searched distro release file
        # Does not have one; The empty /etc/arch-release file is not
        # considered a valid distro release file:
        self._test_non_existing_release_file()

    def test_centos5_release(self):
        desired_outcome = {
            'id': 'centos',
            'name': 'CentOS',
            'pretty_name': 'CentOS 5.11 (Final)',
            'version': '5.11',
            'pretty_version': '5.11 (Final)',
            'best_version': '5.11',
            'codename': 'Final',
            'major_version': '5',
            'minor_version': '11'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'centos',
            'name': 'CentOS',
            'version_id': '5.11',
            'codename': 'Final'
        }
        self._test_release_file_info('centos-release', desired_info)

    def test_centos7_release(self):
        desired_outcome = {
            'id': 'centos',
            'name': 'CentOS Linux',
            'pretty_name': 'CentOS Linux 7 (Core)',
            'version': '7',
            'pretty_version': '7 (Core)',
            'best_version': '7.1.1503',
            'like': 'rhel fedora',
            'codename': 'Core',
            'major_version': '7'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'centos',
            'name': 'CentOS Linux',
            'version_id': '7.1.1503',
            'codename': 'Core'
        }
        self._test_release_file_info('centos-release', desired_info)

    def test_coreos_release(self):
        desired_outcome = {
            'id': 'coreos',
            'name': 'CoreOS',
            'pretty_name': 'CoreOS 899.15.0',
            'version': '899.15.0',
            'pretty_version': '899.15.0',
            'best_version': '899.15.0',
            'major_version': '899',
            'minor_version': '15',
            'build_number': '0'
        }
        self._test_outcome(desired_outcome)
        self._test_non_existing_release_file()

    def test_debian8_release(self):
        desired_outcome = {
            'id': 'debian',
            'name': 'Debian GNU/Linux',
            'pretty_name': 'Debian GNU/Linux 8 (jessie)',
            'version': '8',
            'pretty_version': '8 (jessie)',
            'best_version': '8.2',
            'codename': 'jessie',
            'major_version': '8'
        }
        self._test_outcome(desired_outcome)
        self._test_non_existing_release_file()

    def test_exherbo_release(self):
        desired_outcome = {
            'id': 'exherbo',
            'name': 'Exherbo',
            'pretty_name': 'Exherbo Linux',
        }
        self._test_outcome(desired_outcome)

    def test_fedora19_release(self):
        desired_outcome = {
            'id': 'fedora',
            'name': 'Fedora',
            'pretty_name': u'Fedora 19 (Schr\u00F6dinger\u2019s Cat)',
            'version': '19',
            'pretty_version': u'19 (Schr\u00F6dinger\u2019s Cat)',
            'best_version': '19',
            'codename': u'Schr\u00F6dinger\u2019s Cat',
            'major_version': '19'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'fedora',
            'name': 'Fedora',
            'version_id': '19',
            'codename': u'Schr\u00F6dinger\u2019s Cat'
        }
        self._test_release_file_info('fedora-release', desired_info)

    def test_fedora23_release(self):
        desired_outcome = {
            'id': 'fedora',
            'name': 'Fedora',
            'pretty_name': 'Fedora 23 (Twenty Three)',
            'version': '23',
            'pretty_version': '23 (Twenty Three)',
            'best_version': '23',
            'codename': 'Twenty Three',
            'major_version': '23'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'fedora',
            'name': 'Fedora',
            'version_id': '23',
            'codename': 'Twenty Three'
        }
        self._test_release_file_info('fedora-release', desired_info)

    def test_fedora30_release(self):
        desired_outcome = {
            'id': 'fedora',
            'name': 'Fedora',
            'pretty_name': 'Fedora 30 (Thirty)',
            'version': '30',
            'pretty_version': '30',
            'best_version': '30',
            'codename': '',
            'major_version': '30'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'fedora',
            'name': 'Fedora',
            'version_id': '30',
            'codename': 'Thirty'
        }
        self._test_release_file_info('fedora-release', desired_info)

    def test_kvmibm1_release(self):
        desired_outcome = {
            'id': 'kvmibm',
            'name': 'KVM for IBM z Systems',
            'pretty_name': 'KVM for IBM z Systems 1.1.1 (Z)',
            'version': '1.1.1',
            'pretty_version': '1.1.1 (Z)',
            'best_version': '1.1.1',
            'like': 'rhel fedora',
            'codename': 'Z',
            'major_version': '1',
            'minor_version': '1',
            'build_number': '1'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'base',
            'name': 'KVM for IBM z Systems',
            'version_id': '1.1.1',
            'codename': 'Z'
        }
        self._test_release_file_info('base-release', desired_info)

    def test_linuxmint17_release(self):
        desired_outcome = {
            'id': 'ubuntu',
            'name': 'Ubuntu',
            'pretty_name': 'Ubuntu 14.04.3 LTS',
            'version': '14.04',
            'pretty_version': '14.04 (Trusty Tahr)',
            'best_version': '14.04.3',
            'like': 'debian',
            'codename': 'Trusty Tahr',
            'major_version': '14',
            'minor_version': '04'
        }
        self._test_outcome(desired_outcome)
        self._test_non_existing_release_file()

    def test_mageia5_release(self):
        desired_outcome = {
            'id': 'mageia',
            'name': 'Mageia',
            'pretty_name': 'Mageia 5',
            'version': '5',
            'pretty_version': '5 (thornicroft)',
            'best_version': '5',
            'like': 'mandriva fedora',
            # TODO: Codename differs between distro release and lsb_release.
            'codename': 'thornicroft',
            'major_version': '5'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'mageia',
            'name': 'Mageia',
            'version_id': '5',
            'codename': 'Official'
        }
        self._test_release_file_info('mageia-release', desired_info)

    def test_manjaro1512_release(self):
        self._test_outcome({
            'id': 'manjaro',
            'name': 'Manjaro Linux',
            'pretty_name': 'Manjaro Linux',
            'version': '15.12',
            'pretty_version': '15.12 (Capella)',
            'best_version': '15.12',
            'major_version': '15',
            'minor_version': '12',
            'codename': 'Capella'
        })

        self._test_release_file_info(
            'manjaro-release',
            {'id': 'manjaro',
             'name': 'Manjaro Linux'})

    def test_opensuse42_release(self):
        desired_outcome = {
            'id': 'opensuse',
            'name': 'openSUSE Leap',
            'pretty_name': 'openSUSE Leap 42.1 (x86_64)',
            'version': '42.1',
            'pretty_version': '42.1 (x86_64)',
            'best_version': '42.1',
            'like': 'suse',
            'codename': 'x86_64',
            'major_version': '42',
            'minor_version': '1'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'SuSE',
            'name': 'openSUSE',
            'version_id': '42.1',
            'codename': 'x86_64'
        }
        self._test_release_file_info('SuSE-release', desired_info)

    def test_oracle7_release(self):
        desired_outcome = {
            'id': 'oracle',
            'name': 'Oracle Linux Server',
            'pretty_name': 'Oracle Linux Server 7.5',
            'version': '7.5',
            'pretty_version': '7.5',
            'best_version': '7.5',
            'major_version': '7',
            'minor_version': '5'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'oracle',
            'name': 'Oracle Linux Server',
            'version_id': '7.5',
        }
        distro_info = self._test_release_file_info(
            'oracle-release', desired_info)
        assert 'codename' not in distro_info

    def test_raspbian7_release(self):
        desired_outcome = {
            'id': 'raspbian',
            'name': 'Raspbian GNU/Linux',
            'pretty_name': 'Raspbian GNU/Linux 7 (wheezy)',
            'version': '7',
            'pretty_version': '7 (wheezy)',
            'best_version': '7',
            'like': 'debian',
            'codename': 'wheezy',
            'major_version': '7',
        }
        self._test_outcome(desired_outcome)
        self._test_non_existing_release_file()

    def test_raspbian8_release(self):
        desired_outcome = {
            'id': 'raspbian',
            'name': 'Raspbian GNU/Linux',
            'pretty_name': 'Raspbian GNU/Linux 8 (jessie)',
            'version': '8',
            'pretty_version': '8 (jessie)',
            'best_version': '8',
            'like': 'debian',
            'codename': 'jessie',
            'major_version': '8',
        }
        self._test_outcome(desired_outcome)
        self._test_non_existing_release_file()

    def test_rhel5_release(self):
        desired_outcome = {
            'id': 'rhel',
            'name': 'Red Hat Enterprise Linux Server',
            'pretty_name': 'Red Hat Enterprise Linux Server 5.11 (Tikanga)',
            'version': '5.11',
            'pretty_version': '5.11 (Tikanga)',
            'best_version': '5.11',
            'codename': 'Tikanga',
            'major_version': '5',
            'minor_version': '11'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'redhat',
            'name': 'Red Hat Enterprise Linux Server',
            'version_id': '5.11',
            'codename': 'Tikanga'
        }
        self._test_release_file_info('redhat-release', desired_info)

    def test_rhel6_release(self):
        desired_outcome = {
            'id': 'rhel',
            'name': 'Red Hat Enterprise Linux Server',
            'pretty_name': 'Red Hat Enterprise Linux Server 6.5 (Santiago)',
            'version': '6.5',
            'pretty_version': '6.5 (Santiago)',
            'best_version': '6.5',
            'codename': 'Santiago',
            'major_version': '6',
            'minor_version': '5'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'redhat',
            'name': 'Red Hat Enterprise Linux Server',
            'version_id': '6.5',
            'codename': 'Santiago'
        }
        self._test_release_file_info('redhat-release', desired_info)

    def test_rhel7_release(self):
        desired_outcome = {
            'id': 'rhel',
            'name': 'Red Hat Enterprise Linux Server',
            'pretty_name': 'Red Hat Enterprise Linux Server 7.0 (Maipo)',
            'version': '7.0',
            'pretty_version': '7.0 (Maipo)',
            'best_version': '7.0',
            'like': 'fedora',
            'codename': 'Maipo',
            'major_version': '7',
            'minor_version': '0'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'redhat',
            'name': 'Red Hat Enterprise Linux Server',
            'version_id': '7.0',
            'codename': 'Maipo'
        }
        self._test_release_file_info('redhat-release', desired_info)

    def test_slackware14_release(self):
        desired_outcome = {
            'id': 'slackware',
            'name': 'Slackware',
            'pretty_name': 'Slackware 14.1',
            'version': '14.1',
            'pretty_version': '14.1',
            'best_version': '14.1',
            'major_version': '14',
            'minor_version': '1'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'slackware',
            'name': 'Slackware',
            'version_id': '14.1',
        }
        distro_info = self._test_release_file_info(
            'slackware-version', desired_info)
        assert 'codename' not in distro_info

    def test_sles12_release(self):
        desired_outcome = {
            'id': 'sles',
            'name': 'SLES',
            'pretty_name': 'SUSE Linux Enterprise Server 12 SP1',
            'version': '12.1',
            'pretty_version': '12.1 (n/a)',
            'best_version': '12.1',
            'codename': 'n/a',
            'major_version': '12',
            'minor_version': '1'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'SuSE',
            'name': 'SUSE Linux Enterprise Server',
            'version_id': '12',
            'codename': 's390x'
        }
        self._test_release_file_info('SuSE-release', desired_info)

    def test_ubuntu14_release(self):
        desired_outcome = {
            'id': 'ubuntu',
            'name': 'Ubuntu',
            'pretty_name': 'Ubuntu 14.04.3 LTS',
            'version': '14.04',
            'pretty_version': '14.04 (Trusty Tahr)',
            'best_version': '14.04.3',
            'like': 'debian',
            'codename': 'Trusty Tahr',
            'major_version': '14',
            'minor_version': '04'
        }
        self._test_outcome(desired_outcome)

        # Test the info from the searched distro release file
        # Does not have one; /etc/debian_version is not considered a distro
        # release file:
        self._test_non_existing_release_file()

    def test_ubuntu16_release(self):
        desired_outcome = {
            'id': 'ubuntu',
            'name': 'Ubuntu',
            'pretty_name': 'Ubuntu 16.04.1 LTS',
            'version': '16.04',
            'pretty_version': '16.04 (xenial)',
            'best_version': '16.04.1',
            'like': 'debian',
            'codename': 'xenial',
            'major_version': '16',
            'minor_version': '04'
        }
        self._test_outcome(desired_outcome)

        # Test the info from the searched distro release file
        # Does not have one; /etc/debian_version is not considered a distro
        # release file:
        self._test_non_existing_release_file()

    def test_amazon2016_release(self):
        desired_outcome = {
            'id': 'amzn',
            'name': 'Amazon Linux AMI',
            'pretty_name': 'Amazon Linux AMI 2016.03',
            'version': '2016.03',
            'pretty_version': '2016.03',
            'best_version': '2016.03',
            'like': 'rhel fedora',
            'major_version': '2016',
            'minor_version': '03'
        }
        self._test_outcome(desired_outcome)

    def test_amazon2014_release(self):
        # Amazon Linux 2014 only contains a system-release file.
        # distro doesn't currently handle it.
        desired_outcome = {}
        self._test_outcome(desired_outcome)

    def test_scientific6_release(self):
        desired_outcome = {
            'id': 'rhel',
            'name': 'Scientific Linux',
            'pretty_name': 'Scientific Linux 6.4 (Carbon)',
            'version': '6.4',
            'pretty_version': '6.4 (Carbon)',
            'best_version': '6.4',
            'codename': 'Carbon',
            'major_version': '6',
            'minor_version': '4',

        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'redhat',
            'name': 'Scientific Linux',
            'version_id': '6.4',
            'codename': 'Carbon'
        }
        self._test_release_file_info('redhat-release', desired_info)

    def test_scientific7_release(self):
        desired_outcome = {
            'id': 'rhel',
            'name': 'Scientific Linux',
            'pretty_name': 'Scientific Linux 7.2 (Nitrogen)',
            'version': '7.2',
            'pretty_version': '7.2 (Nitrogen)',
            'best_version': '7.2',
            'like': 'fedora',
            'codename': 'Nitrogen',
            'major_version': '7',
            'minor_version': '2',
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'redhat',
            'name': 'Scientific Linux',
            'version_id': '7.2',
            'codename': 'Nitrogen'
        }
        self._test_release_file_info('redhat-release', desired_info)

    def test_gentoo_release(self):
        desired_outcome = {
            'id': 'gentoo',
            'name': 'Gentoo',
            'pretty_name': 'Gentoo/Linux',
            'version': '2.2',
            'pretty_version': '2.2',
            'best_version': '2.2',
            'major_version': '2',
            'minor_version': '2',
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'gentoo',
            'name': 'Gentoo Base System',
            'version_id': '2.2',
        }
        self._test_release_file_info('gentoo-release', desired_info)

    def test_openelec6_release(self):
        desired_outcome = {
            'id': 'openelec',
            'name': 'OpenELEC',
            'pretty_name': 'OpenELEC (official) - Version: 6.0.3',
            'version': '6.0',
            'pretty_version': '6.0',
            'best_version': '6.0.3',
            'major_version': '6',
            'minor_version': '0',
        }
        self._test_outcome(desired_outcome)

    def test_mandriva2011_release(self):
        desired_outcome = {
            'id': 'mandrivalinux',
            'name': 'MandrivaLinux',
            'pretty_name': 'Mandriva Linux 2011.0',
            'version': '2011.0',
            'pretty_version': '2011.0 (turtle)',
            'best_version': '2011.0',
            'major_version': '2011',
            'minor_version': '0',
            'codename': 'turtle'
        }
        self._test_outcome(desired_outcome)

        desired_info = {
            'id': 'mandrake',
            'name': 'Mandriva Linux',
            'version_id': '2011.0',
        }
        self._test_release_file_info('mandrake-release', desired_info)

    def test_cloudlinux5_release(self):
        # Uses redhat-release only to get information.
        # The id of 'rhel' can only be fixed with issue #109.
        desired_outcome = {
            'id': 'cloudlinux',
            'codename': 'Vladislav Volkov',
            'name': 'CloudLinux Server',
            'pretty_name': 'CloudLinux Server 5.11 (Vladislav Volkov)',
            'version': '5.11',
            'pretty_version': '5.11 (Vladislav Volkov)',
            'best_version': '5.11',
            'major_version': '5',
            'minor_version': '11'
        }
        self._test_outcome(desired_outcome)

    def test_cloudlinux6_release(self):
        # Same as above, only has redhat-release.
        desired_outcome = {
            'id': 'cloudlinux',
            'codename': 'Oleg Makarov',
            'name': 'CloudLinux Server',
            'pretty_name': 'CloudLinux Server 6.8 (Oleg Makarov)',
            'version': '6.8',
            'pretty_version': '6.8 (Oleg Makarov)',
            'best_version': '6.8',
            'major_version': '6',
            'minor_version': '8'
        }
        self._test_outcome(desired_outcome)

    def test_cloudlinux7_release(self):
        desired_outcome = {
            'id': 'cloudlinux',
            'codename': 'Yury Malyshev',
            'name': 'CloudLinux',
            'pretty_name': 'CloudLinux 7.3 (Yury Malyshev)',
            'like': 'rhel fedora centos',
            'version': '7.3',
            'pretty_version': '7.3 (Yury Malyshev)',
            'best_version': '7.3',
            'major_version': '7',
            'minor_version': '3'
        }
        self._test_outcome(desired_outcome)


def _bad_os_listdir(path='.'):
    """ This function is used by TestOverallWithEtcNotReadable to simulate
    a folder that cannot be called with os.listdir() but files are still
    readable. Forces distro to guess which *-release files are available. """
    raise OSError()


@pytest.mark.skipIf(not IS_LINUX, reason='Irrelevant on non-linx')
class TestOverallWithEtcNotReadable(TestOverall):
    def setup_method(self, test_method):
        self._old_listdir = os.listdir
        os.listdir = _bad_os_listdir
        super(TestOverallWithEtcNotReadable, self).setup_method(test_method)

    def teardown_method(self, test_method):
        super(TestOverallWithEtcNotReadable, self).teardown_method(test_method)
        if os.listdir is _bad_os_listdir:
            os.listdir = self._old_listdir


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestGetAttr(DistroTestCase):
    """Test the consistency between the results of
    `{source}_release_attr()` and `{source}_release_info()` for all
    distros in `DISTROS`.
    """

    def _test_attr(self, info_method, attr_method):
        for dist in DISTROS:
            self._setup_for_distro(os.path.join(DISTROS_DIR, dist))
            _distro = distro.LinuxDistribution()
            info = getattr(_distro, info_method)()
            for key in info.keys():
                try:
                    assert info[key] == getattr(_distro, attr_method)(key)
                except AssertionError:
                    print("distro: {0}, key: {1}".format(dist, key))

    def test_os_release_attr(self):
        self._test_attr('os_release_info', 'os_release_attr')

    def test_lsb_release_attr(self):
        self._test_attr('lsb_release_info', 'lsb_release_attr')

    def test_distro_release_attr(self):
        self._test_attr('distro_release_info', 'distro_release_attr')


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestInfo(DistroTestCase):

    def setup_method(self, test_method):
        super(TestInfo, self).setup_method(test_method)
        self.ubuntu14_os_release = os.path.join(
            DISTROS_DIR, 'ubuntu14', 'etc', 'os-release')

    def test_info(self):
        _distro = distro.LinuxDistribution(
            False, self.ubuntu14_os_release, 'non')

        desired_info = {
            'id': 'ubuntu',
            'version': '14.04',
            'like': 'debian',
            'version_parts': {
                'major': '14',
                'minor': '04',
                'build_number': ''
            },
            'codename': 'Trusty Tahr'
        }

        info = _distro.info()
        assert info == desired_info

        desired_info_diff = {
            'version': '14.04 (Trusty Tahr)'
        }
        desired_info.update(desired_info_diff)
        info = _distro.info(pretty=True)
        assert info == desired_info

        desired_info_diff = {
            'version': '14.04.3',
            'version_parts': {
                'major': '14',
                'minor': '04',
                'build_number': '3'
            }
        }
        desired_info.update(desired_info_diff)
        info = _distro.info(best=True)
        assert info == desired_info

        desired_info_diff = {
            'version': '14.04.3 (Trusty Tahr)'
        }
        desired_info.update(desired_info_diff)
        info = _distro.info(pretty=True, best=True)
        assert info == desired_info

    def test_none(self):

        def _test_none(info):
            assert info['id'] == ''
            assert info['version'] == ''
            assert info['like'] == ''
            assert info['version_parts']['major'] == ''
            assert info['version_parts']['minor'] == ''
            assert info['version_parts']['build_number'] == ''
            assert info['codename'] == ''

        _distro = distro.LinuxDistribution(False, 'non', 'non')

        info = _distro.info()
        _test_none(info)

        info = _distro.info(best=True)
        _test_none(info)

        info = _distro.info(pretty=True)
        _test_none(info)

        info = _distro.info(pretty=True, best=True)
        _test_none(info)

    def test_linux_distribution(self):
        _distro = distro.LinuxDistribution(False, self.ubuntu14_os_release)
        i = _distro.linux_distribution()
        assert i == ('Ubuntu', '14.04', 'Trusty Tahr')

    def test_linux_distribution_full_false(self):
        _distro = distro.LinuxDistribution(False, self.ubuntu14_os_release)
        i = _distro.linux_distribution(full_distribution_name=False)
        assert i == ('ubuntu', '14.04', 'Trusty Tahr')

    def test_all(self):
        """Test info() by comparing its results with the results of specific
        consolidated accessor functions.
        """
        def _test_all(info, best=False, pretty=False):
            assert info['id'] == _distro.id()
            assert info['version'] == _distro.version(pretty=pretty, best=best)
            assert info['version_parts']['major'] == \
                _distro.major_version(best=best)
            assert info['version_parts']['minor'] == \
                _distro.minor_version(best=best)
            assert info['version_parts']['build_number'] == \
                _distro.build_number(best=best)
            assert info['like'] == _distro.like()
            assert info['codename'] == _distro.codename()
            assert len(info['version_parts']) == 3
            assert len(info) == 5

        for dist in DISTROS:
            self._setup_for_distro(os.path.join(DISTROS_DIR, dist))

            _distro = distro.LinuxDistribution()

            info = _distro.info()
            _test_all(info)

            info = _distro.info(best=True)
            _test_all(info, best=True)

            info = _distro.info(pretty=True)
            _test_all(info, pretty=True)

            info = _distro.info(pretty=True, best=True)
            _test_all(info, pretty=True, best=True)


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestOSReleaseParsing:
    """Test the parsing of os-release files.
    """

    def setup_method(self, test_method):
        self.distro = distro.LinuxDistribution(False, None, None)
        self.distro.debug = True

    def _get_props(self, input):
        return self.distro._parse_os_release_content(StringIO(
            input,
        ))

    def _test_zero_length_props(self, input):
        props = self._get_props(input)
        assert len(props) == 0

    def _test_empty_value(self, input):
        props = self._get_props(input)
        assert props.get('key', None) == ''

    def _test_parsed_value(self, input):
        props = self._get_props(input)
        assert props.get('key', None) == 'value'

    def test_kv_01_empty_file(self):
        self._test_zero_length_props('')

    def test_kv_02_empty_line(self):
        self._test_zero_length_props('\n')

    def test_kv_03_empty_line_with_crlf(self):
        self._test_zero_length_props('\r\n')

    def test_kv_04_empty_line_with_just_cr(self):
        self._test_zero_length_props('\r')

    def test_kv_05_comment(self):
        self._test_zero_length_props('# KEY=value\n')

    def test_kv_06_empty_value(self):
        self._test_empty_value('KEY=\n')

    def test_kv_07_empty_value_single_quoted(self):
        self._test_empty_value('KEY=\'\'\n')

    def test_kv_08_empty_value_double_quoted(self):
        self._test_empty_value('KEY=""\n')

    def test_kv_09_word(self):
        self._test_parsed_value('KEY=value\n')

    def test_kv_10_word_no_newline(self):
        self._test_parsed_value('KEY=value')

    def test_kv_11_word_with_crlf(self):
        self._test_parsed_value('KEY=value\r\n')

    def test_kv_12_word_with_just_cr(self):
        self._test_parsed_value('KEY=value\r')

    def test_kv_13_word_with_multi_blanks(self):
        self._test_empty_value('KEY=  cmd   \n')
        # Note: Without quotes, this assigns the empty string, and 'cmd' is
        # a separate token that is being ignored (it would be a command
        # in the shell).

    def test_kv_14_unquoted_words(self):
        self._test_parsed_value('KEY=value cmd\n')

    def test_kv_15_double_quoted_words(self):
        props = self._get_props('KEY="a simple value" cmd\n')
        assert props.get('key', None) == 'a simple value'

    def test_kv_16_double_quoted_words_with_multi_blanks(self):
        props = self._get_props('KEY=" a  simple   value "\n')
        assert props.get('key', None) == ' a  simple   value '

    def test_kv_17_double_quoted_word_with_single_quote(self):
        props = self._get_props('KEY="it\'s value"\n')
        assert props.get('key', None) == 'it\'s value'

    def test_kv_18_double_quoted_word_with_double_quote(self):
        props = self._get_props('KEY="a \\"bold\\" move"\n')
        assert props.get('key', None) == 'a "bold" move'

    def test_kv_19_single_quoted_words(self):
        props = self._get_props('KEY=\'a simple value\'\n')
        assert props.get('key', None) == 'a simple value'

    def test_kv_20_single_quoted_words_with_multi_blanks(self):
        props = self._get_props('KEY=\' a  simple   value \'\n')
        assert props.get('key', None) == ' a  simple   value '

    def test_kv_21_single_quoted_word_with_double_quote(self):
        props = self._get_props('KEY=\'a "bold" move\'\n')
        assert props.get('key', None) == 'a "bold" move'

    def test_kv_22_quoted_unicode_wordchar(self):
        # "wordchar" means it is in the shlex.wordchars variable.
        props = self._get_props(u'KEY="wordchar: \u00CA (E accent grave)"\n')
        assert props.get('key', None) == u'wordchar: \u00CA (E accent grave)'

    def test_kv_23_quoted_unicode_non_wordchar(self):
        # "non-wordchar" means it is not in the shlex.wordchars variable.
        props = self._get_props(
            u'KEY="non-wordchar: \u00A1 (inverted exclamation mark)"\n')
        assert (props.get('key', None) ==
                u'non-wordchar: \u00A1 (inverted exclamation mark)')

    def test_kv_24_double_quoted_entire_single_quoted_word(self):
        props = self._get_props('KEY="\'value\'"\n')
        assert props.get('key', None) == "'value'"

    def test_kv_25_single_quoted_entire_double_quoted_word(self):
        props = self._get_props('KEY=\'"value"\'\n')
        assert props.get('key', None) == '"value"'

    def test_kv_26_double_quoted_multiline(self):
        props = self.distro._parse_os_release_content(StringIO(
            'KEY="a multi\n'
            'line value"\n'
        ))
        assert props.get('key', None) == 'a multi\nline value'
        # TODO: Find out why the result is not 'a multi line value'

    def test_kv_27_double_quoted_multiline_2(self):
        props = self._get_props('KEY=\' a  simple   value \'\n')
        props = self.distro._parse_os_release_content(StringIO(
            'KEY="a multi\n'
            'line=value"\n'
        ))
        assert props.get('key', None) == 'a multi\nline=value'
        # TODO: Find out why the result is not 'a multi line=value'

    def test_kv_28_double_quoted_word_with_equal(self):
        props = self._get_props('KEY="var=value"\n')
        assert props.get('key', None) == 'var=value'

    def test_kv_29_single_quoted_word_with_equal(self):
        props = self._get_props('KEY=\'var=value\'\n')
        assert props.get('key', None) == 'var=value'

    def test_kx_01(self):
        props = self.distro._parse_os_release_content(StringIO(
            'KEY1=value1\n'
            'KEY2="value  2"\n'
        ))
        assert props.get('key1', None) == 'value1'
        assert props.get('key2', None) == 'value  2'

    def test_kx_02(self):
        props = self.distro._parse_os_release_content(StringIO(
            '# KEY1=value1\n'
            'KEY2="value  2"\n'
        ))
        assert props.get('key1', None) is None
        assert props.get('key2', None) == 'value  2'


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestGlobal:
    """Test the global module-level functions, and default values of their
    arguments.
    """

    def setup_method(self, test_method):
        pass

    def test_global(self):
        # Because the module-level functions use the module-global
        # LinuxDistribution instance, it would influence the tested
        # code too much if we mocked that in order to use the distro
        # specific release files. Instead, we let the functions use
        # the release files of the distro this test runs on, and
        # compare the result of the global functions with the result
        # of the methods on the global LinuxDistribution object.

        def _test_consistency(function, kwargs=None):
            kwargs = kwargs or {}
            method_result = getattr(MODULE_DISTRO, function)(**kwargs)
            function_result = getattr(distro, function)(**kwargs)
            assert method_result == function_result

        kwargs = {'full_distribution_name': True}
        _test_consistency('linux_distribution', kwargs)
        kwargs = {'full_distribution_name': False}
        _test_consistency('linux_distribution', kwargs)

        kwargs = {'pretty': False}
        _test_consistency('name', kwargs)
        _test_consistency('version', kwargs)
        _test_consistency('info', kwargs)

        kwargs = {'pretty': True}
        _test_consistency('name', kwargs)
        _test_consistency('version', kwargs)
        _test_consistency('info', kwargs)

        kwargs = {'best': False}
        _test_consistency('version', kwargs)
        _test_consistency('version_parts', kwargs)
        _test_consistency('major_version', kwargs)
        _test_consistency('minor_version', kwargs)
        _test_consistency('build_number', kwargs)
        _test_consistency('info', kwargs)

        kwargs = {'best': True}
        _test_consistency('version', kwargs)
        _test_consistency('version_parts', kwargs)
        _test_consistency('major_version', kwargs)
        _test_consistency('minor_version', kwargs)
        _test_consistency('build_number', kwargs)
        _test_consistency('info', kwargs)

        _test_consistency('id')
        _test_consistency('like')
        _test_consistency('codename')
        _test_consistency('info')

        _test_consistency('os_release_info')
        _test_consistency('lsb_release_info')
        _test_consistency('distro_release_info')
        _test_consistency('uname_info')

        os_release_keys = [
            'name',
            'version',
            'id',
            'id_like',
            'pretty_name',
            'version_id',
            'codename',
        ]
        for key in os_release_keys:
            _test_consistency('os_release_attr', {'attribute': key})

        lsb_release_keys = [
            'distributor_id',
            'description',
            'release',
            'codename',
        ]
        for key in lsb_release_keys:
            _test_consistency('lsb_release_attr', {'attribute': key})

        distro_release_keys = [
            'id',
            'name',
            'version_id',
            'codename',
        ]
        for key in distro_release_keys:
            _test_consistency('distro_release_attr', {'attribute': key})

        uname_keys = [
            'id',
            'name',
            'release'
        ]
        for key in uname_keys:
            _test_consistency('uname_attr', {'attribute': key})


@pytest.mark.skipif(not IS_LINUX, reason='Irrelevant on non-linux')
class TestRepr:
    """Test the __repr__() method.
    """

    def test_repr(self):
        # We test that the class name and the names of all instance attributes
        # show up in the repr() string.
        repr_str = repr(distro._distro)
        assert "LinuxDistribution" in repr_str
        for attr in MODULE_DISTRO.__dict__.keys():
            assert attr + '=' in repr_str
