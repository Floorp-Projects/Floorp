# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import argparse
import os
import re
from six import StringIO
from six.moves import configparser
import sys
import tempfile
import xml.dom.minidom
import zipfile

import mozfile
import mozlog

from mozversion import errors


INI_DATA_MAPPING = (('application', 'App'), ('platform', 'Build'))


class Version(object):

    def __init__(self):
        self._info = {}
        self._logger = mozlog.get_default_logger(component='mozversion')
        if not self._logger:
            self._logger = mozlog.unstructured.getLogger('mozversion')

    def get_gecko_info(self, path):
        for type, section in INI_DATA_MAPPING:
            config_file = os.path.join(path, "%s.ini" % type)
            if os.path.exists(config_file):
                self._parse_ini_file(open(config_file), type, section)
            else:
                self._logger.warning('Unable to find %s' % config_file)

    def _parse_ini_file(self, fp, type, section):
        config = configparser.RawConfigParser()
        config.readfp(fp)
        name_map = {'codename': 'display_name',
                    'milestone': 'version',
                    'sourcerepository': 'repository',
                    'sourcestamp': 'changeset'}
        for key, value in config.items(section):
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
        archive_list = archive.namelist()
        for type, section in INI_DATA_MAPPING:
            filename = "%s.ini" % type
            if filename in archive_list:
                self._parse_ini_file(archive.open(filename), type,
                                     section)
            else:
                self._logger.warning('Unable to find %s' % filename)

        if "package-name.txt" in archive_list:
            self._info["package_name"] = \
                archive.open("package-name.txt").readlines()[0].strip()


class LocalVersion(Version):

    def __init__(self, binary, **kwargs):
        Version.__init__(self, **kwargs)

        if binary:
            # on Windows, the binary may be specified with or without the
            # .exe extension
            if not os.path.exists(binary) and not os.path.exists(binary +
                                                                 '.exe'):
                raise IOError('Binary path does not exist: %s' % binary)
            path = os.path.dirname(os.path.realpath(binary))
        else:
            path = os.getcwd()

        if not self.check_location(path):
            if sys.platform == 'darwin':
                resources_path = os.path.join(os.path.dirname(path),
                                              'Resources')
                if self.check_location(resources_path):
                    path = resources_path
                else:
                    raise errors.LocalAppNotFoundError(path)

            else:
                raise errors.LocalAppNotFoundError(path)

        self.get_gecko_info(path)

    def check_location(self, path):
        return (os.path.exists(os.path.join(path, 'application.ini'))
                and os.path.exists(os.path.join(path, 'platform.ini')))


class B2GVersion(Version):

    def __init__(self, sources=None, **kwargs):
        Version.__init__(self, **kwargs)

        sources = sources or \
            os.path.exists(os.path.join(os.getcwd(), 'sources.xml')) and \
            os.path.join(os.getcwd(), 'sources.xml')

        if sources and os.path.exists(sources):
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
                self._logger.info('Unable to unzip application.zip, falling '
                                  'back to system unzip')
                from subprocess import call
                call(['unzip', '-j', app_zip.name, 'resources/gaia_commit.txt',
                      '-d', tempdir])

            with open(gaia_commit) as f:
                changeset, date = f.read().splitlines()
                self._info['gaia_changeset'] = re.match(
                    '^\w{40}$', changeset) and changeset or None
                self._info['gaia_date'] = date
        except KeyError:
            self._logger.warning(
                'Unable to find resources/gaia_commit.txt in '
                'application.zip')
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
            self._logger.warning('Error pulling gaia file')


class RemoteB2GVersion(B2GVersion):

    def __init__(self, sources=None, host=None,
                 device_serial=None, adb_host=None, adb_port=None,
                 **kwargs):
        B2GVersion.__init__(self, sources, **kwargs)

        try:
            import mozdevice
        except ImportError:
            self._logger.critical("mozdevice is required to get the version"
                                  " of a remote device")
            raise

        dm = mozdevice.DeviceManagerADB(deviceSerial=device_serial,
                                        serverHost=adb_host,
                                        serverPort=adb_port)

        if not sources:
            path = 'system/sources.xml'
            if dm.fileExists(path):
                sources = StringIO(dm.pullFile(path))
            else:
                self._logger.info('Unable to find %s' % path)

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
            self._logger.warning('Error pulling gaia file')

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

        if self._info.get('device_id', '').lower() == 'flame':
            for prop in ['ro.boot.bootloader', 't2m.sw.version']:
                value = dm.shellCheckOutput(['getprop', prop])
                if value:
                    self._info['device_firmware_version_base'] = value
                    break


def get_version(binary=None, sources=None, host=None,
                device_serial=None, adb_host=None, adb_port=None):
    """
    Returns the application version information as a dict. You can specify
    a path to the binary of the application or an Android APK file (to get
    version information for Firefox for Android). If this is omitted then the
    current directory is checked for the existance of an application.ini
    file. If not found and that the binary path was not specified, then it is
    assumed the target application is a remote Firefox OS instance.

    :param binary: Path to the binary for the application or Android APK file
    :param sources: Path to the sources.xml file (Firefox OS)
    :param host: Host address of remote Firefox OS instance (not used with ADB)
    :param device_serial: Serial identifier of Firefox OS device (ADB)
    :param adb_host: Host address of ADB server
    :param adb_port: Port of ADB server
    """
    try:
        if binary and zipfile.is_zipfile(binary) and 'AndroidManifest.xml' in \
           zipfile.ZipFile(binary, 'r').namelist():
            version = LocalFennecVersion(binary)
        else:
            version = LocalVersion(binary)
            if version._info.get('application_name') == 'B2G':
                version = LocalB2GVersion(binary, sources=sources)
    except errors.LocalAppNotFoundError:
        if binary:
            # we had a binary argument, do not search for remote B2G
            raise
        version = RemoteB2GVersion(sources=sources,
                                   host=host,
                                   adb_host=adb_host,
                                   adb_port=adb_port,
                                   device_serial=device_serial)

    for (key, value) in sorted(version._info.items()):
        if value:
            version._logger.info('%s: %s' % (key, value))

    return version._info


def cli(args=sys.argv[1:]):
    parser = argparse.ArgumentParser(
        description='Display version information for Mozilla applications')
    parser.add_argument(
        '--binary',
        help='path to application binary or apk')
    fxos = parser.add_argument_group('Firefox OS')
    fxos.add_argument(
        '--sources',
        help='path to sources.xml')
    fxos.add_argument(
        '--device',
        help='serial identifier of device to target')
    fxos.add_argument(
        '--adb-host',
        help='host running adb')
    fxos.add_argument(
        '--adb-port',
        help='port running adb')
    mozlog.commandline.add_logging_group(
        parser,
        include_formatters=mozlog.commandline.TEXT_FORMATTERS
    )

    args = parser.parse_args()
    host = os.environ.get('TEST_DEVICE')

    mozlog.commandline.setup_logging(
        'mozversion', args, {'mach': sys.stdout})

    get_version(binary=args.binary,
                sources=args.sources,
                host=host,
                device_serial=args.device,
                adb_host=args.adb_host,
                adb_port=args.adb_port)


if __name__ == '__main__':
    cli()
