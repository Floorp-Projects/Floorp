# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

from configobj import ConfigObj
import codecs
import re
import os


HOST_FINGERPRINTS = {
    'bitbucket.org': '46:de:34:e7:9b:18:cd:7f:ae:fd:8b:e3:bc:f4:1a:5e:38:d7:ac:24',
    'bugzilla.mozilla.org': '7c:7a:c4:6c:91:3b:6b:89:cf:f2:8c:13:b8:02:c4:25:bd:1e:25:17',
    'hg.mozilla.org': 'af:27:b9:34:47:4e:e5:98:01:f6:83:2b:51:c9:aa:d8:df:fb:1a:27',
}


def config_file(files):
    """Select the most appropriate config file from a list."""
    if not files:
        return None

    if len(files) > 1:
        picky = [(os.path.getsize(f), f) for f in files if os.path.isfile(f)]
        if picky:
            return max(picky)[1]

    return files[0]


class ParseException(Exception):
    def __init__(self, line, msg):
        self.line = line
        super(Exception, self).__init__(msg)


class MercurialConfig(object):
    """Interface for manipulating a Mercurial config file."""

    def __init__(self, path=None):
        """Create a new instance, optionally from an existing hgrc file."""

        self.config_path = path

        # Mercurial configuration files allow an %include directive to include
        # other files, this is not supported by ConfigObj, so throw a useful
        # error saying this.
        if os.path.exists(path):
            with codecs.open(path, 'r', encoding='utf-8') as f:
                for i, line in enumerate(f):
                    if line.startswith('%include'):
                        raise ParseException(i + 1,
                            '%include directive is not supported by MercurialConfig')
                    if line.startswith(';'):
                        raise ParseException(i + 1,
                            'semicolon (;) comments are not supported; '
                            'use # instead')

        # write_empty_values is necessary to prevent built-in extensions (which
        # have no value) from being dropped on write.
        # list_values aren't needed by Mercurial and disabling them prevents
        # quotes from being added.
        self._c = ConfigObj(infile=path, encoding='utf-8',
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

        for k, v in HOST_FINGERPRINTS.items():
            self._c['hostfingerprints'][k] = v

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

    def have_mqext_autocommit_mq(self):
        if 'mqext' not in self._c:
            return False
        v = self._c['mqext'].get('mqcommit')
        return v == 'auto' or v == 'yes'

    def ensure_mqext_autocommit_mq(self):
        if self.have_mqext_autocommit_mq():
            return
        if 'mqext' not in self._c:
            self._c['mqext'] = {}
        self._c['mqext']['mqcommit'] = 'auto'

    def have_qnew_currentuser_default(self):
        if 'defaults' not in self._c:
            return False
        d = self._c['defaults']
        if 'qnew' not in d:
            return False
        argv = d['qnew'].split(' ')
        for arg in argv:
            if arg == '--currentuser' or re.match("-[^-]*U.*", arg):
                return True
        return False

    def ensure_qnew_currentuser_default(self):
        if self.have_qnew_currentuser_default():
            return
        if 'defaults' not in self._c:
            self._c['defaults'] = {}

        d = self._c['defaults']
        if 'qnew' not in d:
            d['qnew'] = '-U'
        else:
            d['qnew'] = '-U ' + d['qnew']

    def get_bugzilla_credentials(self):
        if 'bugzilla' not in self._c:
            return None, None, None, None, None

        b = self._c['bugzilla']
        return (
            b.get('username', None),
            b.get('password', None),
            b.get('userid', None),
            b.get('cookie', None),
            b.get('apikey', None),
        )

    def set_bugzilla_credentials(self, username, api_key):
        b = self._c.setdefault('bugzilla', {})
        if username:
            b['username'] = username
        if api_key:
            b['apikey'] = api_key

    def clear_legacy_bugzilla_credentials(self):
        if 'bugzilla' not in self._c:
            return

        b = self._c['bugzilla']
        for k in ('password', 'userid', 'cookie'):
            if k in b:
                del b[k]

    def have_clonebundles(self):
        return 'clonebundles' in self._c.get('experimental', {})

    def activate_clonebundles(self):
        exp = self._c.setdefault('experimental', {})
        exp['clonebundles'] = 'true'

        # bundleclone is redundant with clonebundles. Remove it if it
        # is installed.
        ext = self._c.get('extensions', {})
        try:
            del ext['bundleclone']
        except KeyError:
            pass

    def have_wip(self):
        return 'wip' in self._c.get('alias', {})

    def install_wip_alias(self):
        """hg wip shows a concise view of work in progress."""
        alias = self._c.setdefault('alias', {})
        alias['wip'] = 'log --graph --rev=wip --template=wip'

        revsetalias = self._c.setdefault('revsetalias', {})
        revsetalias['wip'] = ('('
                'parents(not public()) '
                'or not public() '
                'or . '
                'or (head() and branch(default))'
            ') and (not obsolete() or unstable()^) '
            'and not closed()')

        templates = self._c.setdefault('templates', {})
        templates['wip'] = ("'"
            # prefix with branch name
            '{label("log.branch", branches)} '
            # rev:node
            '{label("changeset.{phase}", rev)}'
            '{label("changeset.{phase}", ":")}'
            '{label("changeset.{phase}", short(node))} '
            # just the username part of the author, for brevity
            '{label("grep.user", author|user)}'
            # tags and bookmarks
            '{label("log.tag", if(tags," {tags}"))} '
            '{label("log.bookmark", if(bookmarks," {bookmarks}"))}'
            '\\n'
            # first line of commit message
            '{label(ifcontains(rev, revset("."), "desc.here"),desc|firstline)}'
            "'"
        )
