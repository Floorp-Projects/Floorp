# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import sys
import tempfile
import urllib2
import zipfile
from xml.dom import minidom

import mozfile
from mozlog.unstructured import getLogger

# Needed for the AMO's rest API - https://developer.mozilla.org/en/addons.mozilla.org_%28AMO%29_API_Developers%27_Guide/The_generic_AMO_API
AMO_API_VERSION = "1.5"

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

        # Add-ons downloaded and which have to be removed from the file system
        self.downloaded_addons = []

        # Information needed for profile reset (see http://bit.ly/17JesUf)
        self.installed_addons = []
        self.installed_manifests = []

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

        # Remove all downloaded add-ons
        for addon in self.downloaded_addons:
            mozfile.remove(addon)

        # restore backups
        if self.backup_dir and os.path.isdir(self.backup_dir):
            extensions_path = os.path.join(self.profile, 'extensions', 'staged')

            for backup in os.listdir(self.backup_dir):
                backup_path = os.path.join(self.backup_dir, backup)
                shutil.move(backup_path, extensions_path)

            if not os.listdir(self.backup_dir):
                mozfile.remove(self.backup_dir)

        # reset instance variables to defaults
        self._internal_init()

    @classmethod
    def download(self, url, target_folder=None):
        """
        Downloads an add-on from the specified URL to the target folder

        :param url: URL of the add-on (XPI file)
        :param target_folder: Folder to store the XPI file in

        """
        response = urllib2.urlopen(url)
        fd, path = tempfile.mkstemp(suffix='.xpi')
        os.write(fd, response.read())
        os.close(fd)

        if not self.is_addon(path):
            mozfile.remove(path)
            raise AddonFormatError('Not a valid add-on: %s' % url)

        # Give the downloaded file a better name by using the add-on id
        details = self.addon_details(path)
        new_path = path.replace('.xpi', '_%s.xpi' % details.get('id'))

        # Move the add-on to the target folder if requested
        if target_folder:
            new_path = os.path.join(target_folder, os.path.basename(new_path))

        os.rename(path, new_path)

        return new_path

    def get_addon_path(self, addon_id):
        """Returns the path to the installed add-on

        :param addon_id: id of the add-on to retrieve the path from
        """
        # By default we should expect add-ons being located under the
        # extensions folder. Only if the application hasn't been run and
        # installed the add-ons yet, it will be located under 'staged'.
        # Also add-ons could have been unpacked by the application.
        extensions_path = os.path.join(self.profile, 'extensions')
        paths = [os.path.join(extensions_path, addon_id),
                 os.path.join(extensions_path, addon_id + '.xpi'),
                 os.path.join(extensions_path, 'staged', addon_id),
                 os.path.join(extensions_path, 'staged', addon_id + '.xpi')]
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

    def install_addons(self, addons=None, manifests=None):
        """
        Installs all types of addons

        :param addons: a list of addon paths to install
        :param manifest: a list of addon manifests to install
        """

        # install addon paths
        if addons:
            if isinstance(addons, basestring):
                addons = [addons]
            for addon in set(addons):
                self.install_from_path(addon)

        # install addon manifests
        if manifests:
            if isinstance(manifests, basestring):
                manifests = [manifests]
            for manifest in manifests:
                self.install_from_manifest(manifest)

    def install_from_manifest(self, filepath):
        """
        Installs addons from a manifest
        :param filepath: path to the manifest of addons to install
        """
        try:
            from manifestparser import ManifestParser
        except ImportError:
            module_logger.critical(
                "Installing addons from manifest requires the"
                " manifestparser package to be installed.")
            raise

        manifest = ManifestParser()
        manifest.read(filepath)
        addons = manifest.get()

        for addon in addons:
            if '://' in addon['path'] or os.path.exists(addon['path']):
                self.install_from_path(addon['path'])
                continue

            # No path specified, try to grab it off AMO
            locale = addon.get('amo_locale', 'en_US')
            query = 'https://services.addons.mozilla.org/' + locale + '/firefox/api/' + AMO_API_VERSION + '/'
            if 'amo_id' in addon:
                query += 'addon/' + addon['amo_id']                 # this query grabs information on the addon base on its id
            else:
                query += 'search/' + addon['name'] + '/default/1'   # this query grabs information on the first addon returned from a search
            install_path = AddonManager.get_amo_install_path(query)
            self.install_from_path(install_path)

        self.installed_manifests.append(filepath)

    @classmethod
    def get_amo_install_path(self, query):
        """
        Get the addon xpi install path for the specified AMO query.

        :param query: query-documentation_

        .. _query-documentation: https://developer.mozilla.org/en/addons.mozilla.org_%28AMO%29_API_Developers%27_Guide/The_generic_AMO_API
        """
        response = urllib2.urlopen(query)
        dom = minidom.parseString(response.read())
        for node in dom.getElementsByTagName('install')[0].childNodes:
            if node.nodeType == node.TEXT_NODE:
                return node.data

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

        try:
            if zipfile.is_zipfile(addon_path):
                # Bug 944361 - We cannot use 'with' together with zipFile because
                # it will cause an exception thrown in Python 2.6.
                try:
                    compressed_file = zipfile.ZipFile(addon_path, 'r')
                    manifest = compressed_file.read('install.rdf')
                finally:
                    compressed_file.close()
            elif os.path.isdir(addon_path):
                with open(os.path.join(addon_path, 'install.rdf'), 'r') as f:
                    manifest = f.read()
            else:
                raise IOError('Add-on path is neither an XPI nor a directory: %s' % addon_path)
        except (IOError, KeyError), e:
            raise AddonFormatError, str(e), sys.exc_info()[2]

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
        except Exception, e:
            raise AddonFormatError, str(e), sys.exc_info()[2]

        # turn unpack into a true/false value
        if isinstance(details['unpack'], basestring):
            details['unpack'] = details['unpack'].lower() == 'true'

        # If no ID is set, the add-on is invalid
        if details.get('id') is None:
            raise AddonFormatError('Add-on id could not be found.')

        return details

    def install_from_path(self, path, unpack=False):
        """
        Installs addon from a filepath, url or directory of addons in the profile.

        :param path: url, path to .xpi, or directory of addons
        :param unpack: whether to unpack unless specified otherwise in the install.rdf
        """

        # if the addon is a URL, download it
        # note that this won't work with protocols urllib2 doesn't support
        if mozfile.is_url(path):
            path = self.download(path)
            self.downloaded_addons.append(path)

        addons = [path]

        # if path is not an add-on, try to install all contained add-ons
        try:
            self.addon_details(path)
        except AddonFormatError, e:
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
            extensions_path = os.path.join(self.profile, 'extensions', 'staged')
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

    def remove_addon(self, addon_id):
        """Remove the add-on as specified by the id

        :param addon_id: id of the add-on to be removed
        """
        path = self.get_addon_path(addon_id)
        mozfile.remove(path)
