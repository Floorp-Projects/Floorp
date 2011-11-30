# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozprofile.
#
# The Initial Developer of the Original Code is
#   The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Andrew Halberstadt <halbersa@gmail.com>
#   Mikeal Rogers <mikeal.rogers@gmail.com>
#   Clint Talbert <ctalbert@mozilla.com>
#   Jeff Hammel <jhammel@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import os
import shutil
import sys
import tempfile
import urllib2
import zipfile
from distutils import dir_util
from manifestparser import ManifestParser
from xml.dom import minidom

# Needed for the AMO's rest API - https://developer.mozilla.org/en/addons.mozilla.org_%28AMO%29_API_Developers%27_Guide/The_generic_AMO_API
AMO_API_VERSION = "1.5"

class AddonManager(object):
    """
    Handles all operations regarding addons including: installing and cleaning addons
    """

    def __init__(self, profile):
        """
        profile - the path to the profile for which we install addons
        """
        self.profile = profile
        self.installed_addons = []
        # keeps track of addons and manifests that were passed to install_addons
        self.addons = []
        self.manifests = []


    def install_addons(self, addons=None, manifests=None):
        """
        Installs all types of addons
        addons - a list of addon paths to install
        manifest - a list of addon manifests to install
        """
        # install addon paths
        if addons:
            if isinstance(addons, basestring):
                addons = [addons]
            for addon in addons:
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
        filepath - path to the manifest of addons to install
        """
        self.manifests.append(filepath)
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

    @classmethod
    def get_amo_install_path(self, query):
        """
        Return the addon xpi install path for the specified AMO query.
        See: https://developer.mozilla.org/en/addons.mozilla.org_%28AMO%29_API_Developers%27_Guide/The_generic_AMO_API
        for query documentation.
        """
        response = urllib2.urlopen(query)
        dom = minidom.parseString(response.read())
        for node in dom.getElementsByTagName('install')[0].childNodes:
            if node.nodeType == node.TEXT_NODE:
                return node.data

    @classmethod
    def addon_details(cls, addon_path):
        """
        returns a dictionary of details about the addon
        - addon_path : path to the addon directory
        Returns:
        {'id':      u'rainbow@colors.org', # id of the addon
         'version': u'1.4',                # version of the addon
         'name':    u'Rainbow',            # name of the addon
         'unpack': # whether to unpack the addon
        """

        # TODO: We don't use the unpack variable yet, but we should: bug 662683
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

        doc = minidom.parse(os.path.join(addon_path, 'install.rdf'))

        # Get the namespaces abbreviations
        em = get_namespace_id(doc, "http://www.mozilla.org/2004/em-rdf#")
        rdf = get_namespace_id(doc, "http://www.w3.org/1999/02/22-rdf-syntax-ns#")

        description = doc.getElementsByTagName(rdf + "Description").item(0)
        for node in description.childNodes:
            # Remove the namespace prefix from the tag for comparison
            entry = node.nodeName.replace(em, "")
            if entry in details.keys():
                details.update({ entry: get_text(node) })

        # turn unpack into a true/false value
        if isinstance(details['unpack'], basestring):
            details['unpack'] = details['unpack'].lower() == 'true'

        return details

    def install_from_path(self, path, unpack=False):
        """
        Installs addon from a filepath, url
        or directory of addons in the profile.
        - path: url, path to .xpi, or directory of addons
        - unpack: whether to unpack unless specified otherwise in the install.rdf
        """
        self.addons.append(path)

        # if the addon is a url, download it
        # note that this won't work with protocols urllib2 doesn't support
        if '://' in path:
            response = urllib2.urlopen(path)
            fd, path = tempfile.mkstemp(suffix='.xpi')
            os.write(fd, response.read())
            os.close(fd)
            tmpfile = path
        else:
            tmpfile = None

        # if the addon is a directory, install all addons in it
        addons = [path]
        if not path.endswith('.xpi') and not os.path.exists(os.path.join(path, 'install.rdf')):
            assert os.path.isdir(path), "Addon '%s' cannot be installed" % path
            addons = [os.path.join(path, x) for x in os.listdir(path)]

        # install each addon
        for addon in addons:
            tmpdir = None
            xpifile = None
            if addon.endswith('.xpi'):
                tmpdir = tempfile.mkdtemp(suffix = '.' + os.path.split(addon)[-1])
                compressed_file = zipfile.ZipFile(addon, 'r')
                for name in compressed_file.namelist():
                    if name.endswith('/'):
                        os.makedirs(os.path.join(tmpdir, name))
                    else:
                        if not os.path.isdir(os.path.dirname(os.path.join(tmpdir, name))):
                            os.makedirs(os.path.dirname(os.path.join(tmpdir, name)))
                        data = compressed_file.read(name)
                        f = open(os.path.join(tmpdir, name), 'wb')
                        f.write(data)
                        f.close()
                xpifile = addon
                addon = tmpdir

            # determine the addon id
            addon_details = AddonManager.addon_details(addon)
            addon_id = addon_details.get('id')
            assert addon_id, 'The addon id could not be found: %s' % addon

            # copy the addon to the profile
            extensions_path = os.path.join(self.profile, 'extensions')
            addon_path = os.path.join(extensions_path, addon_id)
            if not unpack and not addon_details['unpack'] and xpifile:
                if not os.path.exists(extensions_path):
                    os.makedirs(extensions_path)
                shutil.copy(xpifile, addon_path + '.xpi')
            else:
                dir_util.copy_tree(addon, addon_path, preserve_symlinks=1)
                self.installed_addons.append(addon_path)

            # remove the temporary directory, if any
            if tmpdir:
                dir_util.remove_tree(tmpdir)

        # remove temporary file, if any
        if tmpfile:
            os.remove(tmpfile)

    def clean_addons(self):
        """Cleans up addons in the profile."""
        for addon in self.installed_addons:
            if os.path.isdir(addon):
                dir_util.remove_tree(addon)
