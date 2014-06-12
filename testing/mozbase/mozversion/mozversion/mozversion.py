# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import ConfigParser
from StringIO import StringIO
from optparse import OptionParser
import os
import re
import sys
import tempfile
import xml.dom.minidom
import zipfile

import mozdevice
import mozlog
import mozfile


class VersionError(Exception):
    def __init__(self, message):
        Exception.__init__(self, message)


class LocalAppNotFoundError(VersionError):
    """Exception for local application not found"""
    def __init__(self):
        VersionError.__init__(
            self, 'No binary path or application.ini found in working '
            'directory. Specify a binary path or run from the directory '
            'containing the binary.')


INI_DATA_MAPPING = (('application', 'App'), ('platform', 'Build'))


class Version(mozlog.LoggingMixin):

    def __init__(self):
        self._info = {}

    def get_gecko_info(self, path):
        for type, section in INI_DATA_MAPPING:
            config_file = os.path.join(path, "%s.ini" % type)
            if os.path.exists(config_file):
                self._parse_ini_file(open(config_file), type, section)
            else:
                self.warn('Unable to find %s' % config_file)

    def _parse_ini_file(self, fp, type, section):
        config = ConfigParser.RawConfigParser()
        config.readfp(fp)
        name_map = {'CodeName': 'display_name',
                    'SourceRepository': 'repository',
                    'SourceStamp': 'changeset'}
        for key in ('BuildID', 'Name', 'CodeName', 'Version',
                    'SourceRepository', 'SourceStamp'):
            name = name_map.get(key, key).lower()
            self._info['%s_%s' % (type, name)] = config.has_option(
                section, key) and config.get(section, key) or None

        if not self._info.get('application_display_name'):
            self._info['application_display_name'] = \
                self._info.get('application_name')


class LocalFennecVersion(Version):

    def __init__(self, path, **kwargs):
        Version.__init__(self, **kwargs)
        self.get_gecko_info(path)

    def get_gecko_info(self, path):
        archive = zipfile.ZipFile(path, 'r')
        for type, section in INI_DATA_MAPPING:
            filename = "%s.ini" % type
            if filename in archive.namelist():
                self._parse_ini_file(archive.open(filename), type,
                                     section)
            else:
                self.warn('Unable to find %s' % filename)


class LocalVersion(Version):

    def __init__(self, binary, **kwargs):
        Version.__init__(self, **kwargs)
        path = None

        if binary:
            if not os.path.exists(binary):
                raise IOError('Binary path does not exist: %s' % binary)
            path = os.path.dirname(binary)
        else:
            if os.path.exists(os.path.join(os.getcwd(), 'application.ini')):
                path = os.getcwd()

        if not path:
            raise LocalAppNotFoundError()

        self.get_gecko_info(path)


class B2GVersion(Version):

    def __init__(self, sources=None, **kwargs):
        Version.__init__(self, **kwargs)

        sources = sources or \
            os.path.exists(os.path.join(os.getcwd(), 'sources.xml')) and \
            os.path.join(os.getcwd(), 'sources.xml')

        if sources:
            sources_xml = xml.dom.minidom.parse(sources)
            for element in sources_xml.getElementsByTagName('project'):
                path = element.getAttribute('path')
                changeset = element.getAttribute('revision')
                if path in ['gaia', 'gecko', 'build']:
                    if path == 'gaia' and self._info.get('gaia_changeset'):
                        break
                    self._info['_'.join([path, 'changeset'])] = changeset

    def get_gaia_info(self, app_zip):
        tempdir = tempfile.mkdtemp()
        try:
            gaia_commit = os.path.join(tempdir, 'gaia_commit.txt')
            try:
                zip_file = zipfile.ZipFile(app_zip.name)
                with open(gaia_commit, 'w') as f:
                    f.write(zip_file.read('resources/gaia_commit.txt'))
            except zipfile.BadZipfile:
                self.info('Unable to unzip application.zip, falling back to '
                          'system unzip')
                from subprocess import call
                call(['unzip', '-j', app_zip.name, 'resources/gaia_commit.txt',
                      '-d', tempdir])

            with open(gaia_commit) as f:
                changeset, date = f.read().splitlines()
                self._info['gaia_changeset'] = re.match(
                    '^\w{40}$', changeset) and changeset or None
                self._info['gaia_date'] = date
        finally:
            mozfile.remove(tempdir)


class LocalB2GVersion(B2GVersion):

    def __init__(self, binary, sources=None, **kwargs):
        B2GVersion.__init__(self, sources, **kwargs)

        if binary:
            if not os.path.exists(binary):
                raise IOError('Binary path does not exist: %s' % binary)
            path = os.path.dirname(binary)
        else:
            if os.path.exists(os.path.join(os.getcwd(), 'application.ini')):
                path = os.getcwd()

        self.get_gecko_info(path)

        zip_path = os.path.join(
            path, 'gaia', 'profile', 'webapps',
            'settings.gaiamobile.org', 'application.zip')
        if os.path.exists(zip_path):
            with open(zip_path, 'rb') as zip_file:
                self.get_gaia_info(zip_file)
        else:
            self.warn('Error pulling gaia file')


class RemoteB2GVersion(B2GVersion):

    def __init__(self, sources=None, dm_type='adb', host=None,
                 device_serial=None, **kwargs):
        B2GVersion.__init__(self, sources, **kwargs)

        if dm_type == 'adb':
            dm = mozdevice.DeviceManagerADB(deviceSerial=device_serial)
        elif dm_type == 'sut':
            if not host:
                raise Exception('A host for SUT must be supplied.')
            dm = mozdevice.DeviceManagerSUT(host=host)
        else:
            raise Exception('Unknown device manager type: %s' % dm_type)

        if not sources:
            path = 'system/sources.xml'
            if dm.fileExists(path):
                sources = StringIO(dm.pullFile(path))
            else:
                self.info('Unable to find %s' % path)

        tempdir = tempfile.mkdtemp()
        for ini in ('application', 'platform'):
            with open(os.path.join(tempdir, '%s.ini' % ini), 'w') as f:
                f.write(dm.pullFile('/system/b2g/%s.ini' % ini))
                f.flush()
        self.get_gecko_info(tempdir)
        mozfile.remove(tempdir)

        for path in ['/system/b2g', '/data/local']:
            path += '/webapps/settings.gaiamobile.org/application.zip'
            if dm.fileExists(path):
                with tempfile.NamedTemporaryFile() as f:
                    dm.getFile(path, f.name)
                    self.get_gaia_info(f)
                break
        else:
            self.warn('Error pulling gaia file')

        build_props = dm.pullFile('/system/build.prop')
        desired_props = {
            'ro.build.version.incremental': 'device_firmware_version_incremental',
            'ro.build.version.release': 'device_firmware_version_release',
            'ro.build.date.utc': 'device_firmware_date',
            'ro.product.device': 'device_id'}
        for line in build_props.split('\n'):
            if not line.strip().startswith('#') and '=' in line:
                key, value = [s.strip() for s in line.split('=', 1)]
                if key in desired_props.keys():
                    self._info[desired_props[key]] = value


def get_version(binary=None, sources=None, dm_type=None, host=None,
                device_serial=None):
    """
    Returns the application version information as a dict. You can specify
    a path to the binary of the application or an Android APK file (to get
    version information for Firefox for Android). If this is omitted then the
    current directory is checked for the existance of an application.ini
    file. If not found, then it is assumed the target application is a remote
    Firefox OS instance.

    :param binary: Path to the binary for the application or Android APK file
    :param sources: Path to the sources.xml file (Firefox OS)
    :param dm_type: Device manager type. Must be 'adb' or 'sut' (Firefox OS)
    :param host: Host address of remote Firefox OS instance (SUT)
    :param device_serial: Serial identifier of Firefox OS device (ADB)
    """
    try:
        if binary and zipfile.is_zipfile(binary) and 'AndroidManifest.xml' in \
           zipfile.ZipFile(binary, 'r').namelist():
            version = LocalFennecVersion(binary)
        else:
            version = LocalVersion(binary)
            if version._info.get('application_name') == 'B2G':
                version = LocalB2GVersion(binary, sources=sources)
    except LocalAppNotFoundError:
        version = RemoteB2GVersion(sources=sources, dm_type=dm_type, host=host,
                                   device_serial=device_serial)
    return version._info


def cli(args=sys.argv[1:]):
    parser = OptionParser()
    parser.add_option('--binary',
                      dest='binary',
                      help='path to application binary or apk')
    parser.add_option('--sources',
                      dest='sources',
                      help='path to sources.xml (Firefox OS only)')
    parser.add_option('--device',
                      help='serial identifier of device to target (Firefox OS '
                           'only)')
    (options, args) = parser.parse_args(args)

    dm_type = os.environ.get('DM_TRANS', 'adb')
    host = os.environ.get('TEST_DEVICE')

    version = get_version(binary=options.binary, sources=options.sources,
                          dm_type=dm_type, host=host,
                          device_serial=options.device)
    for (key, value) in sorted(version.items()):
        if value:
            print '%s: %s' % (key, value)

if __name__ == '__main__':
    cli()
