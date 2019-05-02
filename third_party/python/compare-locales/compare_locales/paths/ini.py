# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from six.moves.configparser import ConfigParser, NoSectionError, NoOptionError
from collections import defaultdict
from compare_locales import util, mozpath
from .project import ProjectConfig


class L10nConfigParser(object):
    '''Helper class to gather application information from ini files.

    This class is working on synchronous open to read files or web data.
    Subclass this and overwrite loadConfigs and addChild if you need async.
    '''
    def __init__(self, inipath, **kwargs):
        """Constructor for L10nConfigParsers

        inipath -- l10n.ini path
        Optional keyword arguments are fowarded to the inner ConfigParser as
        defaults.
        """
        self.inipath = mozpath.normpath(inipath)
        # l10n.ini files can import other l10n.ini files, store the
        # corresponding L10nConfigParsers
        self.children = []
        # we really only care about the l10n directories described in l10n.ini
        self.dirs = []
        # optional defaults to be passed to the inner ConfigParser (unused?)
        self.defaults = kwargs

    def getDepth(self, cp):
        '''Get the depth for the comparison from the parsed l10n.ini.
        '''
        try:
            depth = cp.get('general', 'depth')
        except (NoSectionError, NoOptionError):
            depth = '.'
        return depth

    def getFilters(self):
        '''Get the test functions from this ConfigParser and all children.

        Only works with synchronous loads, used by compare-locales, which
        is local anyway.
        '''
        filter_path = mozpath.join(mozpath.dirname(self.inipath), 'filter.py')
        try:
            local = {}
            with open(filter_path) as f:
                exec(compile(f.read(), filter_path, 'exec'), {}, local)
            if 'test' in local and callable(local['test']):
                filters = [local['test']]
            else:
                filters = []
        except BaseException:  # we really want to handle EVERYTHING here
            filters = []

        for c in self.children:
            filters += c.getFilters()

        return filters

    def loadConfigs(self):
        """Entry point to load the l10n.ini file this Parser refers to.

        This implementation uses synchronous loads, subclasses might overload
        this behaviour. If you do, make sure to pass a file-like object
        to onLoadConfig.
        """
        cp = ConfigParser(self.defaults)
        cp.read(self.inipath)
        depth = self.getDepth(cp)
        self.base = mozpath.join(mozpath.dirname(self.inipath), depth)
        # create child loaders for any other l10n.ini files to be included
        try:
            for title, path in cp.items('includes'):
                # skip default items
                if title in self.defaults:
                    continue
                # add child config parser
                self.addChild(title, path, cp)
        except NoSectionError:
            pass
        # try to load the "dirs" defined in the "compare" section
        try:
            self.dirs.extend(cp.get('compare', 'dirs').split())
        except (NoOptionError, NoSectionError):
            pass
        # try to set "all_path" and "all_url"
        try:
            self.all_path = mozpath.join(self.base, cp.get('general', 'all'))
        except (NoOptionError, NoSectionError):
            self.all_path = None
        return cp

    def addChild(self, title, path, orig_cp):
        """Create a child L10nConfigParser and load it.

        title -- indicates the module's name
        path -- indicates the path to the module's l10n.ini file
        orig_cp -- the configuration parser of this l10n.ini
        """
        cp = L10nConfigParser(mozpath.join(self.base, path), **self.defaults)
        cp.loadConfigs()
        self.children.append(cp)

    def dirsIter(self):
        """Iterate over all dirs and our base path for this l10n.ini"""
        for dir in self.dirs:
            yield dir, (self.base, dir)

    def directories(self):
        """Iterate over all dirs and base paths for this l10n.ini as well
        as the included ones.
        """
        for t in self.dirsIter():
            yield t
        for child in self.children:
            for t in child.directories():
                yield t

    def allLocales(self):
        """Return a list of all the locales of this project"""
        with open(self.all_path) as f:
            return util.parseLocales(f.read())


class SourceTreeConfigParser(L10nConfigParser):
    '''Subclassing L10nConfigParser to work with just the repos
    checked out next to each other instead of intermingled like
    we do for real builds.
    '''

    def __init__(self, inipath, base, redirects):
        '''Add additional arguments basepath.

        basepath is used to resolve local paths via branchnames.
        redirects is used in unified repository, mapping upstream
        repos to local clones.
        '''
        L10nConfigParser.__init__(self, inipath)
        self.base = base
        self.redirects = redirects

    def addChild(self, title, path, orig_cp):
        # check if there's a section with details for this include
        # we might have to check a different repo, or even VCS
        # for example, projects like "mail" indicate in
        # an "include_" section where to find the l10n.ini for "toolkit"
        details = 'include_' + title
        if orig_cp.has_section(details):
            branch = orig_cp.get(details, 'mozilla')
            branch = self.redirects.get(branch, branch)
            inipath = orig_cp.get(details, 'l10n.ini')
            path = mozpath.join(self.base, branch, inipath)
        else:
            path = mozpath.join(self.base, path)
        cp = SourceTreeConfigParser(path, self.base, self.redirects,
                                    **self.defaults)
        cp.loadConfigs()
        self.children.append(cp)


class EnumerateApp(object):
    reference = 'en-US'

    def __init__(self, inipath, l10nbase):
        self.setupConfigParser(inipath)
        self.modules = defaultdict(dict)
        self.l10nbase = mozpath.abspath(l10nbase)
        self.filters = []
        self.addFilters(*self.config.getFilters())

    def setupConfigParser(self, inipath):
        self.config = L10nConfigParser(inipath)
        self.config.loadConfigs()

    def addFilters(self, *args):
        self.filters += args

    def asConfig(self):
        # We've already normalized paths in the ini parsing.
        # Set the path and root to None to just keep our paths as is.
        config = ProjectConfig(None)
        config.set_root('.')  # sets to None because path is None
        config.add_environment(l10n_base=self.l10nbase)
        self._config_for_ini(config, self.config)
        filters = self.config.getFilters()
        if filters:
            config.set_filter_py(filters[0])
        config.set_locales(self.config.allLocales(), deep=True)
        return config

    def _config_for_ini(self, projectconfig, aConfig):
        for k, (basepath, module) in aConfig.dirsIter():
            paths = {
                'module': module,
                'reference': mozpath.normpath('%s/%s/locales/en-US/**' %
                                              (basepath, module)),
                'l10n': mozpath.normpath('{l10n_base}/{locale}/%s/**' %
                                         module)
            }
            if module == 'mobile/android/base':
                paths['test'] = ['android-dtd']
            projectconfig.add_paths(paths)
        for child in aConfig.children:
            self._config_for_ini(projectconfig, child)


class EnumerateSourceTreeApp(EnumerateApp):
    '''Subclass EnumerateApp to work on side-by-side checked out
    repos, and to no pay attention to how the source would actually
    be checked out for building.
    '''

    def __init__(self, inipath, basepath, l10nbase, redirects,
                 locales=None):
        self.basepath = basepath
        self.redirects = redirects
        EnumerateApp.__init__(self, inipath, l10nbase, locales)

    def setupConfigParser(self, inipath):
        self.config = SourceTreeConfigParser(inipath, self.basepath,
                                             self.redirects)
        self.config.loadConfigs()
