# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
from ConfigParser import ConfigParser, NoSectionError, NoOptionError
from collections import defaultdict
import errno
import itertools
import logging
from compare_locales import util, mozpath
import pytoml as toml


class Matcher(object):
    '''Path pattern matcher
    Supports path matching similar to mozpath.match(), but does
    not match trailing file paths without trailing wildcards.
    Also gets a prefix, which is the path before the first wildcard,
    which is good for filesystem iterations, and allows to replace
    the own matches in a path on a different Matcher. compare-locales
    uses that to transform l10n and en-US paths back and forth.
    '''

    def __init__(self, pattern):
        '''Create regular expression similar to mozpath.match().
        '''
        prefix = pattern.split("*", 1)[0]
        p = re.escape(pattern)
        p = re.sub(r'(^|\\\/)\\\*\\\*\\\/', r'\1(.+/)?', p)
        p = re.sub(r'(^|\\\/)\\\*\\\*$', r'(\1.+)?', p)
        p = p.replace(r'\*', '([^/]*)') + '$'
        r = re.escape(pattern)
        r = re.sub(r'(^|\\\/)\\\*\\\*\\\/', r'\\\\0', r)
        r = re.sub(r'(^|\\\/)\\\*\\\*$', r'\\\\0', r)
        r = r.replace(r'\*', r'\\0')
        backref = itertools.count(1)
        r = re.sub(r'\\0', lambda m: '\\%s' % backref.next(), r)
        r = re.sub(r'\\(.)', r'\1', r)
        self.prefix = prefix
        self.regex = re.compile(p)
        self.placable = r

    def match(self, path):
        '''
        True if the given path matches the file pattern.
        '''
        return self.regex.match(path) is not None

    def sub(self, other, path):
        '''
        Replace the wildcard matches in this pattern into the
        pattern of the other Match object.
        '''
        if not self.match(path):
            return None
        return self.regex.sub(other.placable, path)


class ProjectConfig(object):
    '''Abstraction of l10n project configuration data.
    '''

    def __init__(self):
        self.filter_py = None  # legacy filter code
        # {
        #  'l10n': pattern,
        #  'reference': pattern,  # optional
        #  'locales': [],  # optional
        #  'test': [],  # optional
        # }
        self.paths = []
        self.rules = []
        self.locales = []
        self.environ = {}
        self.children = []
        self._cache = None

    variable = re.compile('{\s*([\w]+)\s*}')

    def expand(self, path, env=None):
        if env is None:
            env = {}

        def _expand(m):
            _var = m.group(1)
            for _env in (env, self.environ):
                if _var in _env:
                    return self.expand(_env[_var], env)
            return '{{{}}}'.format(_var)
        return self.variable.sub(_expand, path)

    def lazy_expand(self, pattern):
        def lazy_l10n_expanded_pattern(env):
            return Matcher(self.expand(pattern, env))
        return lazy_l10n_expanded_pattern

    def add_global_environment(self, **kwargs):
        self.add_environment(**kwargs)
        for child in self.children:
            child.add_global_environment(**kwargs)

    def add_environment(self, **kwargs):
        self.environ.update(kwargs)

    def add_paths(self, *paths):
        '''Add path dictionaries to this config.
        The dictionaries must have a `l10n` key. For monolingual files,
        `reference` is also required.
        An optional key `test` is allowed to enable additional tests for this
        path pattern.
        '''

        for d in paths:
            rv = {
                'l10n': self.lazy_expand(d['l10n']),
                'module': d.get('module')
            }
            if 'reference' in d:
                rv['reference'] = Matcher(d['reference'])
            if 'test' in d:
                rv['test'] = d['test']
            if 'locales' in d:
                rv['locales'] = d['locales'][:]
            self.paths.append(rv)

    def set_filter_py(self, filter):
        '''Set legacy filter.py code.
        Assert that no rules are set.
        Also, normalize output already here.
        '''
        assert not self.rules

        def filter_(module, path, entity=None):
            try:
                rv = filter(module, path, entity=entity)
            except BaseException:  # we really want to handle EVERYTHING here
                return 'error'
            rv = {
                True: 'error',
                False: 'ignore',
                'report': 'warning'
            }.get(rv, rv)
            assert rv in ('error', 'ignore', 'warning', None)
            return rv
        self.filter_py = filter_

    def add_rules(self, *rules):
        '''Add rules to filter on.
        Assert that there's no legacy filter.py code hooked up.
        '''
        assert self.filter_py is None
        for rule in rules:
            self.rules.extend(self._compile_rule(rule))

    def add_child(self, child):
        self.children.append(child)

    def set_locales(self, locales, deep=False):
        self.locales = locales
        for child in self.children:
            if not child.locales or deep:
                child.set_locales(locales, deep=True)
            else:
                locs = [loc for loc in locales if loc in child.locales]
                child.set_locales(locs)

    @property
    def configs(self):
        'Recursively get all configs in this project and its children'
        yield self
        for child in self.children:
            for config in child.configs:
                yield config

    def filter(self, l10n_file, entity=None):
        '''Filter a localization file or entities within, according to
        this configuration file.'''
        if self.filter_py is not None:
            return self.filter_py(l10n_file.module, l10n_file.file,
                                  entity=entity)
        rv = self._filter(l10n_file, entity=entity)
        if rv is None:
            return 'ignore'
        return rv

    class FilterCache(object):
        def __init__(self, locale):
            self.locale = locale
            self.rules = []
            self.l10n_paths = []

    def cache(self, locale):
        if self._cache and self._cache.locale == locale:
            return self._cache
        self._cache = self.FilterCache(locale)
        for paths in self.paths:
            self._cache.l10n_paths.append(paths['l10n']({
                "locale": locale
            }))
        for rule in self.rules:
            cached_rule = rule.copy()
            cached_rule['path'] = rule['path']({
                "locale": locale
            })
            self._cache.rules.append(cached_rule)
        return self._cache

    def _filter(self, l10n_file, entity=None):
        actions = set(
            child._filter(l10n_file, entity=entity)
            for child in self.children)
        if 'error' in actions:
            # return early if we know we'll error
            return 'error'

        cached = self.cache(l10n_file.locale)
        if any(p.match(l10n_file.fullpath) for p in cached.l10n_paths):
            action = 'error'
            for rule in reversed(cached.rules):
                if not rule['path'].match(l10n_file.fullpath):
                    continue
                if ('key' in rule) ^ (entity is not None):
                    # key/file mismatch, not a matching rule
                    continue
                if 'key' in rule and not rule['key'].match(entity):
                    continue
                action = rule['action']
                break
            actions.add(action)
        if 'error' in actions:
            return 'error'
        if 'warning' in actions:
            return 'warning'
        if 'ignore' in actions:
            return 'ignore'

    def _compile_rule(self, rule):
        assert 'path' in rule
        if isinstance(rule['path'], list):
            for path in rule['path']:
                _rule = rule.copy()
                _rule['path'] = self.lazy_expand(path)
                for __rule in self._compile_rule(_rule):
                    yield __rule
            return
        if isinstance(rule['path'], basestring):
            rule['path'] = self.lazy_expand(rule['path'])
        if 'key' not in rule:
            yield rule
            return
        if not isinstance(rule['key'], basestring):
            for key in rule['key']:
                _rule = rule.copy()
                _rule['key'] = key
                for __rule in self._compile_rule(_rule):
                    yield __rule
            return
        rule = rule.copy()
        key = rule['key']
        if key.startswith('re:'):
            key = key[3:]
        else:
            key = re.escape(key) + '$'
        rule['key'] = re.compile(key)
        yield rule


class ProjectFiles(object):
    '''Iterable object to get all files and tests for a locale and a
    list of ProjectConfigs.
    '''
    def __init__(self, locale, projects, mergebase=None):
        self.locale = locale
        self.matchers = []
        self.mergebase = mergebase
        configs = []
        for project in projects:
            configs.extend(project.configs)
        for pc in configs:
            if locale not in pc.locales:
                continue
            for paths in pc.paths:
                if 'locales' in paths and locale not in paths['locales']:
                    continue
                m = {
                    'l10n': paths['l10n']({
                        "locale": locale
                    }),
                    'module': paths.get('module'),
                }
                if 'reference' in paths:
                    m['reference'] = paths['reference']
                if self.mergebase is not None:
                    m['merge'] = paths['l10n']({
                        "locale": locale,
                        "l10n_base": self.mergebase
                    })
                m['test'] = set(paths.get('test', []))
                if 'locales' in paths:
                    m['locales'] = paths['locales'][:]
                self.matchers.append(m)
        self.matchers.reverse()  # we always iterate last first
        # Remove duplicate patterns, comparing each matcher
        # against all other matchers.
        # Avoid n^2 comparisons by only scanning the upper triangle
        # of a n x n matrix of all possible combinations.
        # Using enumerate and keeping track of indexes, as we can't
        # modify the list while iterating over it.
        drops = set()  # duplicate matchers to remove
        for i, m in enumerate(self.matchers[:-1]):
            if i in drops:
                continue  # we're dropping this anyway, don't search again
            for i_, m_ in enumerate(self.matchers[(i+1):]):
                if (mozpath.realpath(m['l10n'].prefix) !=
                        mozpath.realpath(m_['l10n'].prefix)):
                    # ok, not the same thing, continue
                    continue
                # check that we're comparing the same thing
                if 'reference' in m:
                    if (mozpath.realpath(m['reference'].prefix) !=
                            mozpath.realpath(m_.get('reference').prefix)):
                        raise RuntimeError('Mismatch in reference for ' +
                                           mozpath.realpath(m['l10n'].prefix))
                drops.add(i_ + i + 1)
                m['test'] |= m_['test']
        drops = sorted(drops, reverse=True)
        for i in drops:
            del self.matchers[i]

    def __iter__(self):
        known = {}
        for matchers in self.matchers:
            matcher = matchers['l10n']
            for path in self._files(matcher):
                if path not in known:
                    known[path] = {'test': matchers.get('test')}
                    if 'reference' in matchers:
                        known[path]['reference'] = matcher.sub(
                            matchers['reference'], path)
                    if 'merge' in matchers:
                        known[path]['merge'] = matcher.sub(
                            matchers['merge'], path)
            if 'reference' not in matchers:
                continue
            matcher = matchers['reference']
            for path in self._files(matcher):
                l10npath = matcher.sub(matchers['l10n'], path)
                if l10npath not in known:
                    known[l10npath] = {
                        'reference': path,
                        'test': matchers.get('test')
                    }
                    if 'merge' in matchers:
                        known[l10npath]['merge'] = \
                            matcher.sub(matchers['merge'], path)
        for path, d in sorted(known.items()):
            yield (path, d.get('reference'), d.get('merge'), d['test'])

    def _files(self, matcher):
        '''Base implementation of getting all files in a hierarchy
        using the file system.
        Subclasses might replace this method to support different IO
        patterns.
        '''
        base = matcher.prefix
        if os.path.isfile(base):
            if matcher.match(base):
                yield base
            return
        for d, dirs, files in os.walk(base):
            for f in files:
                p = mozpath.join(d, f)
                if matcher.match(p):
                    yield p

    def match(self, path):
        '''Return the tuple of l10n_path, reference, mergepath, tests
        if the given path matches any config, otherwise None.

        This routine doesn't check that the files actually exist.
        '''
        for matchers in self.matchers:
            matcher = matchers['l10n']
            if matcher.match(path):
                ref = merge = None
                if 'reference' in matchers:
                    ref = matcher.sub(matchers['reference'], path)
                if 'merge' in matchers:
                    merge = matcher.sub(matchers['merge'], path)
                return path, ref, merge, matchers.get('test')
            if 'reference' not in matchers:
                continue
            matcher = matchers['reference']
            if matcher.match(path):
                merge = None
                l10n = matcher.sub(matchers['l10n'], path)
                if 'merge' in matchers:
                    merge = matcher.sub(matchers['merge'], path)
                return l10n, path, merge, matchers.get('test')


class ConfigNotFound(EnvironmentError):
    def __init__(self, path):
        super(ConfigNotFound, self).__init__(
            errno.ENOENT,
            'Configuration file not found',
            path)


class TOMLParser(object):
    @classmethod
    def parse(cls, path, env=None, ignore_missing_includes=False):
        parser = cls(path, env=env,
                     ignore_missing_includes=ignore_missing_includes)
        parser.load()
        parser.processEnv()
        parser.processPaths()
        parser.processFilters()
        parser.processIncludes()
        parser.processLocales()
        return parser.asConfig()

    def __init__(self, path, env=None, ignore_missing_includes=False):
        self.path = path
        self.env = env if env is not None else {}
        self.ignore_missing_includes = ignore_missing_includes
        self.data = None
        self.pc = ProjectConfig()
        self.pc.PATH = path

    def load(self):
        try:
            with open(self.path, 'rb') as fin:
                self.data = toml.load(fin)
        except (toml.TomlError, IOError):
            raise ConfigNotFound(self.path)

    def processEnv(self):
        assert self.data is not None
        self.pc.add_environment(**self.data.get('env', {}))

    def processLocales(self):
        assert self.data is not None
        if 'locales' in self.data:
            self.pc.set_locales(self.data['locales'])

    def processPaths(self):
        assert self.data is not None
        for data in self.data.get('paths', []):
            l10n = data['l10n']
            if not l10n.startswith('{'):
                # l10n isn't relative to a variable, expand
                l10n = self.resolvepath(l10n)
            paths = {
                "l10n": l10n,
            }
            if 'locales' in data:
                paths['locales'] = data['locales']
            if 'reference' in data:
                paths['reference'] = self.resolvepath(data['reference'])
            self.pc.add_paths(paths)

    def processFilters(self):
        assert self.data is not None
        for data in self.data.get('filters', []):
            paths = data['path']
            if isinstance(paths, basestring):
                paths = [paths]
            # expand if path isn't relative to a variable
            paths = [
                self.resolvepath(path) if not path.startswith('{')
                else path
                for path in paths
            ]
            rule = {
                "path": paths,
                "action": data['action']
            }
            if 'key' in data:
                rule['key'] = data['key']
            self.pc.add_rules(rule)

    def processIncludes(self):
        assert self.data is not None
        if 'includes' not in self.data:
            return
        for include in self.data['includes']:
            p = include['path']
            p = self.resolvepath(p)
            try:
                child = self.parse(
                    p, env=self.env,
                    ignore_missing_includes=self.ignore_missing_includes
                )
            except ConfigNotFound as e:
                if not self.ignore_missing_includes:
                    raise
                (logging
                    .getLogger('compare-locales.io')
                    .error('%s: %s', e.strerror, e.filename))
                continue
            self.pc.add_child(child)

    def resolvepath(self, path):
        path = self.pc.expand(path, env=self.env)
        path = mozpath.join(
            mozpath.dirname(self.path),
            self.data.get('basepath', '.'),
            path)
        return mozpath.normpath(path)

    def asConfig(self):
        return self.pc


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
            execfile(filter_path, {}, local)
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
        return util.parseLocales(open(self.all_path).read())


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


class File(object):

    def __init__(self, fullpath, file, module=None, locale=None):
        self.fullpath = fullpath
        self.file = file
        self.module = module
        self.locale = locale
        pass

    def getContents(self):
        # open with universal line ending support and read
        return open(self.fullpath, 'rU').read()

    @property
    def localpath(self):
        f = self.file
        if self.module:
            f = mozpath.join(self.module, f)
        return f

    def __hash__(self):
        return hash(self.localpath)

    def __str__(self):
        return self.fullpath

    def __cmp__(self, other):
        if not isinstance(other, File):
            raise NotImplementedError
        rv = cmp(self.module, other.module)
        if rv != 0:
            return rv
        return cmp(self.file, other.file)


class EnumerateApp(object):
    reference = 'en-US'

    def __init__(self, inipath, l10nbase, locales=None):
        self.setupConfigParser(inipath)
        self.modules = defaultdict(dict)
        self.l10nbase = mozpath.abspath(l10nbase)
        self.filters = []
        self.addFilters(*self.config.getFilters())
        self.locales = locales or self.config.allLocales()
        self.locales.sort()

    def setupConfigParser(self, inipath):
        self.config = L10nConfigParser(inipath)
        self.config.loadConfigs()

    def addFilters(self, *args):
        self.filters += args

    def asConfig(self):
        config = ProjectConfig()
        self._config_for_ini(config, self.config)
        filters = self.config.getFilters()
        if filters:
            config.set_filter_py(filters[0])
        config.locales += self.locales
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
            projectconfig.add_global_environment(l10n_base=self.l10nbase)
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
