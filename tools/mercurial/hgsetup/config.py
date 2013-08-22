# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from configobj import ConfigObj


BUGZILLA_FINGERPRINT = '45:77:35:fd:6f:2c:1c:c2:90:4b:f7:b4:4d:60:c6:97:c5:5c:47:27'
HG_FINGERPRINT = '10:78:e8:57:2d:95:de:7c:de:90:bd:22:e1:38:17:67:c5:a7:9c:14'


class MercurialConfig(object):
    """Interface for manipulating a Mercurial config file."""

    def __init__(self, infile=None):
        """Create a new instance, optionally from an existing hgrc file."""

        # write_empty_values is necessary to prevent built-in extensions (which
        # have no value) from being dropped on write.
        # list_values aren't needed by Mercurial and disabling them prevents
        # quotes from being added.
        self._c = ConfigObj(infile=infile, encoding='utf-8',
            write_empty_values=True, list_values=False)

    @property
    def config(self):
        return self._c

    @property
    def extensions(self):
        """Returns the set of currently enabled extensions (by name)."""
        return set(self._c.get('extensions', {}).keys())

    def write(self, fh):
        return self._c.write(fh)

    def have_valid_username(self):
        if 'ui' not in self._c:
            return False

        if 'username' not in self._c['ui']:
            return False

        # TODO perform actual validation here.

        return True

    def add_mozilla_host_fingerprints(self):
        """Add host fingerprints so SSL connections don't warn."""
        if 'hostfingerprints' not in self._c:
            self._c['hostfingerprints'] = {}

        self._c['hostfingerprints']['bugzilla.mozilla.org'] = \
            BUGZILLA_FINGERPRINT
        self._c['hostfingerprints']['hg.mozilla.org'] = HG_FINGERPRINT

    def set_username(self, name, email):
        """Set the username to use for commits.

        The username consists of a name (typically <firstname> <lastname>) and
        a well-formed e-mail address.
        """
        if 'ui' not in self._c:
            self._c['ui'] = {}

        username = '%s <%s>' % (name, email)

        self._c['ui']['username'] = username.strip()

    def activate_extension(self, name, path=None):
        """Activate an extension.

        An extension is defined by its name (in the config) and a filesystem
        path). For built-in extensions, an empty path is specified.
        """
        if not path:
            path = ''

        if 'extensions' not in self._c:
            self._c['extensions'] = {}

        self._c['extensions'][name] = path

    def have_recommended_diff_settings(self):
        if 'diff' not in self._c:
            return False

        old = dict(self._c['diff'])
        try:
            self.ensure_recommended_diff_settings()
        finally:
            self._c['diff'].update(old)

        return self._c['diff'] == old

    def ensure_recommended_diff_settings(self):
        if 'diff' not in self._c:
            self._c['diff'] = {}

        d = self._c['diff']
        d['git'] = 1
        d['showfunc'] = 1
        d['unified'] = 8

    def autocommit_mq(self, value=True):
        if 'mqext' not in self._c:
            self._c['mqext'] = {}

        if value:
            self._c['mqext']['mqcommit'] = 'auto'
        else:
            try:
                del self._c['mqext']['mqcommit']
            except KeyError:
                pass
