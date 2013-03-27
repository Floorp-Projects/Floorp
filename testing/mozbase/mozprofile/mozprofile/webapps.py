# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Handles installing open webapps (https://developer.mozilla.org/en-US/docs/Apps)
to a profile. A webapp object is a dict that contains some metadata about
the webapp and must at least include a name, description and manifestURL.

Each webapp has a manifest (https://developer.mozilla.org/en-US/docs/Apps/Manifest).
Additionally there is a separate json manifest that keeps track of the installed
webapps, their manifestURLs and their permissions.
"""

__all__ = ["Webapp", "WebappCollection", "WebappFormatException", "APP_STATUS_NOT_INSTALLED",
           "APP_STATUS_INSTALLED", "APP_STATUS_PRIVILEGED", "APP_STATUS_CERTIFIED"]

from string import Template
import os
import shutil

try:
    import json
except ImportError:
    import simplejson as json

# from http://hg.mozilla.org/mozilla-central/file/add0b94c2c0b/caps/idl/nsIPrincipal.idl#l163
APP_STATUS_NOT_INSTALLED = 0
APP_STATUS_INSTALLED     = 1
APP_STATUS_PRIVILEGED    = 2
APP_STATUS_CERTIFIED     = 3

class WebappFormatException(Exception):
    """thrown for invalid webapp objects"""

class Webapp(dict):
    """A webapp definition"""

    required_keys = ('name', 'description', 'manifestURL')

    def __init__(self, *args, **kwargs):
        try:
            dict.__init__(self, *args, **kwargs)
        except (TypeError, ValueError):
            raise WebappFormatException("Webapp object should be an instance of type 'dict'")
        self.validate()

    def __eq__(self, other):
        """Webapps are considered equal if they have the same name"""
        if not isinstance(other, self.__class__):
            return False
        return self['name'] == other['name']

    def __ne__(self, other):
        """Webapps are considered not equal if they have different names"""
        return not self.__eq__(other)

    def validate(self):
        # TODO some keys are required if another key has a certain value
        for key in self.required_keys:
            if key not in self:
                raise WebappFormatException("Webapp object missing required key '%s'" % key)


class WebappCollection(object):
    """A list-like object that collects webapps and updates the webapp manifests"""

    json_template = Template(""""$name": {
  "origin": "$origin",
  "installOrigin": "$origin",
  "receipt": null,
  "installTime": 132333986000,
  "manifestURL": "$manifestURL",
  "localId": $localId,
  "id": "$name",
  "appStatus": $appStatus,
  "csp": "$csp"
}""")

    manifest_template = Template("""{
  "name": "$name",
  "csp": "$csp",
  "description": "$description",
  "launch_path": "/",
  "developer": {
    "name": "Mozilla",
    "url": "https://mozilla.org/"
  },
  "permissions": [
  ],
  "locales": {
    "en-US": {
      "name": "$name",
      "description": "$description"
    }
  },
  "default_locale": "en-US",
  "icons": {
  }
}
""")

    def __init__(self, profile, apps=None, json_template=None, manifest_template=None):
        """
        :param profile: the file path to a profile
        :param apps: [optional] a list of webapp objects or file paths to json files describing webapps
        :param json_template: [optional] string template describing the webapp json format
        :param manifest_template: [optional] string template describing the webapp manifest format
        """
        if not isinstance(profile, basestring):
            raise TypeError("Must provide path to a profile, received '%s'" % type(profile))
        self.profile = profile
        self.webapps_dir = os.path.join(self.profile, 'webapps')
        self.backup_dir = os.path.join(self.profile, '.mozprofile_backup', 'webapps')

        self._apps = []
        self._installed_apps = []
        if apps:
            if not isinstance(apps, (list, set, tuple)):
                apps = [apps]

            for app in apps:
                if isinstance(app, basestring) and os.path.isfile(app):
                    self.extend(self.read_json(app))
                else:
                    self.append(app)

        self.json_template = json_template or self.json_template
        self.manifest_template = manifest_template or self.manifest_template

    def __getitem__(self, index):
        return self._apps.__getitem__(index)

    def __setitem__(self, index, value):
        return self._apps.__setitem__(index, Webapp(value))

    def __delitem__(self, index):
        return self._apps.__delitem__(index)

    def __len__(self):
        return self._apps.__len__()

    def __contains__(self, value):
        return self._apps.__contains__(Webapp(value))

    def append(self, value):
        return self._apps.append(Webapp(value))

    def insert(self, index, value):
        return self._apps.insert(index, Webapp(value))

    def extend(self, values):
        return self._apps.extend([Webapp(v) for v in values])

    def remove(self, value):
        return self._apps.remove(Webapp(value))

    def _write_webapps_json(self, apps):
        contents = []
        for app in apps:
            contents.append(self.json_template.substitute(app))
        contents = '{\n' + ',\n'.join(contents) + '\n}\n'
        webapps_json_path = os.path.join(self.webapps_dir, 'webapps.json')
        webapps_json_file = open(webapps_json_path, "w")
        webapps_json_file.write(contents)
        webapps_json_file.close()

    def _write_webapp_manifests(self, write_apps=[], remove_apps=[]):
        # Write manifests for installed apps
        for app in write_apps:
            manifest_dir = os.path.join(self.webapps_dir, app['name'])
            manifest_path = os.path.join(manifest_dir, 'manifest.webapp')
            if not os.path.isfile(manifest_path):
                if not os.path.isdir(manifest_dir):
                    os.mkdir(manifest_dir)
                manifest = self.manifest_template.substitute(app)
                manifest_file = open(manifest_path, "a")
                manifest_file.write(manifest)
                manifest_file.close()
        # Remove manifests for removed apps
        for app in remove_apps:
            self._installed_apps.remove(app)
            manifest_dir = os.path.join(self.webapps_dir, app['name'])
            if os.path.isdir(manifest_dir):
                shutil.rmtree(manifest_dir)

    def update_manifests(self):
        """Updates the webapp manifests with the webapps represented in this collection

        If update_manifests is called a subsequent time, there could have been apps added or
        removed to the collection in the interim. The manifests will be adjusted accordingly
        """
        apps_to_install = [app for app in self._apps if app not in self._installed_apps]
        apps_to_remove = [app for app in self._installed_apps if app not in self._apps]
        if apps_to_install == apps_to_remove == []:
            # nothing to do
            return

        if not os.path.isdir(self.webapps_dir):
            os.makedirs(self.webapps_dir)
        elif not self._installed_apps:
            shutil.copytree(self.webapps_dir, self.backup_dir)

        webapps_json_path = os.path.join(self.webapps_dir, 'webapps.json')
        webapps_json = []
        if os.path.isfile(webapps_json_path):
            webapps_json = self.read_json(webapps_json_path, description="description")
            webapps_json = [a for a in webapps_json if a not in apps_to_remove]

        # Iterate over apps already in webapps.json to determine the starting local
        # id and to ensure apps are properly formatted
        start_id = 1
        for local_id, app in enumerate(webapps_json):
            app['localId'] = local_id + 1
            start_id += 1
            if not app.get('csp'):
                app['csp'] = ''
            if not app.get('appStatus'):
                app['appStatus'] = 3

        # Append apps_to_install to the pre-existent apps
        for local_id, app in enumerate(apps_to_install):
            app['localId'] = local_id + start_id
            # ignore if it's already installed
            if app in webapps_json:
                start_id -= 1
                continue
            webapps_json.append(app)
            self._installed_apps.append(app)

        # Write the full contents to webapps.json
        self._write_webapps_json(webapps_json)

        # Create/remove manifest file for each app.
        self._write_webapp_manifests(apps_to_install, apps_to_remove)

    def clean(self):
        """Remove all webapps that were installed and restore profile to previous state"""
        if self._installed_apps and os.path.isdir(self.webapps_dir):
            shutil.rmtree(self.webapps_dir)

        if os.path.isdir(self.backup_dir):
            shutil.copytree(self.backup_dir, self.webapps_dir)
            shutil.rmtree(self.backup_dir)

        self._apps = []
        self._installed_apps = []

    @classmethod
    def read_json(cls, path, **defaults):
        """Reads a json file which describes a set of webapps. The json format is either a
        dictionary where each key represents the name of a webapp (e.g B2G format) or a list
        of webapp objects.

        :param path: Path to a json file defining webapps
        :param defaults: Default key value pairs added to each webapp object if key doesn't exist

        Returns a list of Webapp objects
        """
        f = open(path, 'r')
        app_json = json.load(f)
        f.close()

        apps = []
        if isinstance(app_json, dict):
            for k, v in app_json.iteritems():
                v['name'] = k
                apps.append(v)
        else:
            apps = app_json
            if not isinstance(apps, list):
                apps = [apps]

        ret = []
        for app in apps:
            d = defaults.copy()
            d.update(app)
            ret.append(Webapp(**d))
        return ret
