# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json
import os
import sys
import shutil
import tempfile
import zipfile
import hashlib
import binascii
from xml.dom import minidom
from six import reraise, string_types

import mozfile
from mozlog.unstructured import getLogger

_SALT = binascii.hexlify(os.urandom(32))
_TEMPORARY_ADDON_SUFFIX = "@temporary-addon"

# Logger for 'mozprofile.addons' module
module_logger = getLogger(__name__)


class AddonFormatError(Exception):
    """Exception for not well-formed add-on manifest files"""


class AddonManager(object):
    """
    Handles all operations regarding addons in a profile including:
    installing and cleaning addons
    """

    def __init__(self, profile, restore=True):
        """
        :param profile: the path to the profile for which we install addons
        :param restore: whether to reset to the previous state on instance garbage collection
        """
        self.profile = profile
        self.restore = restore

        # Initialize all class members
        self._internal_init()

    def _internal_init(self):
        """Internal: Initialize all class members to their default value"""

        # Add-ons installed; needed for cleanup
        self._addons = []

        # Backup folder for already existing addons
        self.backup_dir = None

        # Information needed for profile reset (see http://bit.ly/17JesUf)
        self.installed_addons = []

    def __del__(self):
        # reset to pre-instance state
        if self.restore:
            self.clean()

    def clean(self):
        """Clean up addons in the profile."""

        # Remove all add-ons installed
        for addon in self._addons:
            # TODO (bug 934642)
            # Once we have a proper handling of add-ons we should kill the id
            # from self._addons once the add-on is removed. For now lets forget
            # about the exception
            try:
                self.remove_addon(addon)
            except IOError:
                pass

        # restore backups
        if self.backup_dir and os.path.isdir(self.backup_dir):
            extensions_path = os.path.join(self.profile, 'extensions')

            for backup in os.listdir(self.backup_dir):
                backup_path = os.path.join(self.backup_dir, backup)
                shutil.move(backup_path, extensions_path)

            if not os.listdir(self.backup_dir):
                mozfile.remove(self.backup_dir)

        # reset instance variables to defaults
        self._internal_init()

    def get_addon_path(self, addon_id):
        """Returns the path to the installed add-on

        :param addon_id: id of the add-on to retrieve the path from
        """
        # By default we should expect add-ons being located under the
        # extensions folder.
        extensions_path = os.path.join(self.profile, 'extensions')
        paths = [os.path.join(extensions_path, addon_id),
                 os.path.join(extensions_path, addon_id + '.xpi')]
        for path in paths:
            if os.path.exists(path):
                return path

        raise IOError('Add-on not found: %s' % addon_id)

    @classmethod
    def is_addon(self, addon_path):
        """
        Checks if the given path is a valid addon

        :param addon_path: path to the add-on directory or XPI
        """
        try:
            self.addon_details(addon_path)
            return True
        except AddonFormatError:
            return False

    def _install_addon(self, path, unpack=False):
        addons = [path]

        # if path is not an add-on, try to install all contained add-ons
        try:
            self.addon_details(path)
        except AddonFormatError as e:
            module_logger.warning('Could not install %s: %s' % (path, str(e)))

            # If the path doesn't exist, then we don't really care, just return
            if not os.path.isdir(path):
                return

            addons = [os.path.join(path, x) for x in os.listdir(path) if
                      self.is_addon(os.path.join(path, x))]
            addons.sort()

        # install each addon
        for addon in addons:
            # determine the addon id
            addon_details = self.addon_details(addon)
            addon_id = addon_details.get('id')

            # if the add-on has to be unpacked force it now
            # note: we might want to let Firefox do it in case of addon details
            orig_path = None
            if os.path.isfile(addon) and (unpack or addon_details['unpack']):
                orig_path = addon
                addon = tempfile.mkdtemp()
                mozfile.extract(orig_path, addon)

            # copy the addon to the profile
            extensions_path = os.path.join(self.profile, 'extensions')
            addon_path = os.path.join(extensions_path, addon_id)

            if os.path.isfile(addon):
                addon_path += '.xpi'

                # move existing xpi file to backup location to restore later
                if os.path.exists(addon_path):
                    self.backup_dir = self.backup_dir or tempfile.mkdtemp()
                    shutil.move(addon_path, self.backup_dir)

                # copy new add-on to the extension folder
                if not os.path.exists(extensions_path):
                    os.makedirs(extensions_path)
                shutil.copy(addon, addon_path)
            else:
                # move existing folder to backup location to restore later
                if os.path.exists(addon_path):
                    self.backup_dir = self.backup_dir or tempfile.mkdtemp()
                    shutil.move(addon_path, self.backup_dir)

                # copy new add-on to the extension folder
                shutil.copytree(addon, addon_path, symlinks=True)

            # if we had to extract the addon, remove the temporary directory
            if orig_path:
                mozfile.remove(addon)
                addon = orig_path

            self._addons.append(addon_id)
            self.installed_addons.append(addon)

    def install(self, addons, **kwargs):
        """
        Installs addons from a filepath or directory of addons in the profile.

        :param addons: paths to .xpi or addon directories
        :param unpack: whether to unpack unless specified otherwise in the install.rdf
        """
        if not addons:
            return

        # install addon paths
        if isinstance(addons, string_types):
            addons = [addons]
        for addon in set(addons):
            self._install_addon(addon, **kwargs)

    @classmethod
    def _gen_iid(cls, addon_path):
        hash = hashlib.sha1(_SALT)
        hash.update(addon_path.encode())
        return hash.hexdigest() + _TEMPORARY_ADDON_SUFFIX

    @classmethod
    def addon_details(cls, addon_path):
        """
        Returns a dictionary of details about the addon.

        :param addon_path: path to the add-on directory or XPI

        Returns::

            {'id':      u'rainbow@colors.org', # id of the addon
             'version': u'1.4',                # version of the addon
             'name':    u'Rainbow',            # name of the addon
             'unpack':  False }                # whether to unpack the addon
        """

        details = {
            'id': None,
            'unpack': False,
            'name': None,
            'version': None
        }

        def get_namespace_id(doc, url):
            attributes = doc.documentElement.attributes
            namespace = ""
            for i in range(attributes.length):
                if attributes.item(i).value == url:
                    if ":" in attributes.item(i).name:
                        # If the namespace is not the default one remove 'xlmns:'
                        namespace = attributes.item(i).name.split(':')[1] + ":"
                        break
            return namespace

        def get_text(element):
            """Retrieve the text value of a given node"""
            rc = []
            for node in element.childNodes:
                if node.nodeType == node.TEXT_NODE:
                    rc.append(node.data)
            return ''.join(rc).strip()

        if not os.path.exists(addon_path):
            raise IOError('Add-on path does not exist: %s' % addon_path)

        is_webext = False
        try:
            if zipfile.is_zipfile(addon_path):
                # Bug 944361 - We cannot use 'with' together with zipFile because
                # it will cause an exception thrown in Python 2.6.
                try:
                    compressed_file = zipfile.ZipFile(addon_path, 'r')
                    filenames = [f.filename for f in (compressed_file).filelist]
                    if 'install.rdf' in filenames:
                        manifest = compressed_file.read('install.rdf')
                    elif 'manifest.json' in filenames:
                        is_webext = True
                        manifest = compressed_file.read('manifest.json').decode()
                        manifest = json.loads(manifest)
                    else:
                        raise KeyError("No manifest")
                finally:
                    compressed_file.close()
            elif os.path.isdir(addon_path):
                try:
                    with open(os.path.join(addon_path, 'install.rdf')) as f:
                        manifest = f.read()
                except IOError:
                    with open(os.path.join(addon_path, 'manifest.json')) as f:
                        manifest = json.loads(f.read())
                        is_webext = True
            else:
                raise IOError('Add-on path is neither an XPI nor a directory: %s' % addon_path)
        except (IOError, KeyError) as e:
            reraise(AddonFormatError, AddonFormatError(str(e)), sys.exc_info()[2])

        if is_webext:
            details['version'] = manifest['version']
            details['name'] = manifest['name']
            # Bug 1572404 - we support two locations for gecko-specific
            # metadata.
            for location in ('applications', 'browser_specific_settings'):
                try:
                    details['id'] = manifest[location]['gecko']['id']
                    break
                except KeyError:
                    pass
            if details['id'] is None:
                details['id'] = cls._gen_iid(addon_path)
            details['unpack'] = False
        else:
            try:
                doc = minidom.parseString(manifest)

                # Get the namespaces abbreviations
                em = get_namespace_id(doc, 'http://www.mozilla.org/2004/em-rdf#')
                rdf = get_namespace_id(doc, 'http://www.w3.org/1999/02/22-rdf-syntax-ns#')

                description = doc.getElementsByTagName(rdf + 'Description').item(0)
                for entry, value in description.attributes.items():
                    # Remove the namespace prefix from the tag for comparison
                    entry = entry.replace(em, "")
                    if entry in details.keys():
                        details.update({entry: value})
                for node in description.childNodes:
                    # Remove the namespace prefix from the tag for comparison
                    entry = node.nodeName.replace(em, "")
                    if entry in details.keys():
                        details.update({entry: get_text(node)})
            except Exception as e:
                reraise(AddonFormatError, AddonFormatError(str(e)), sys.exc_info()[2])

        # turn unpack into a true/false value
        if isinstance(details['unpack'], string_types):
            details['unpack'] = details['unpack'].lower() == 'true'

        # If no ID is set, the add-on is invalid
        if details.get('id') is None and not is_webext:
            raise AddonFormatError('Add-on id could not be found.')

        return details

    def remove_addon(self, addon_id):
        """Remove the add-on as specified by the id

        :param addon_id: id of the add-on to be removed
        """
        path = self.get_addon_path(addon_id)
        mozfile.remove(path)
