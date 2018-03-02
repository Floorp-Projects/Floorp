# -*- coding: utf-8 -*-
from __future__ import absolute_import
from __future__ import unicode_literals

import os
import re
import glob
from argparse import Namespace
from os import path
from babel import Locale
from babel.core import UnknownLocaleError

from convert import key_plural_keywords
from config import Config
from utils import Path, format_to_re
from convert import read_xml, InvalidResourceError


__all__ = ('EnvironmentError', 'IncompleteEnvironment',
           'Environment', 'Language', 'resolve_locale')


class EnvironmentError(Exception):
    pass


class IncompleteEnvironment(EnvironmentError):
    pass


ANDROID_LOCALE_MAPPING = {
    'from': {
        'in': 'id',
        'iw': 'he',
        'ji': 'yi',
        'zh_CN': 'zh_Hans_CN',
        'zh_HK': 'zh_Hant_HK',
        'zh_TW': 'zh_Hant_TW'
    },
    'to': {
        'id': 'in',
        'he': 'iw',
        'yi': 'ji',
        'zh_Hans_CN': 'zh_CN',
        'zh_Hant_HK': 'zh_HK',
        'zh_Hant_TW': 'zh_TW'
    }
}
"""
Android uses locale scheme that differs from one used inside Babel,
so we must provide a mapping between one another. This list is not
full and must be updated to include all such mappings.

We can not simply ignore middle element in transition from android
to Babel locale mapping.
"""

MISSING_LOCALES = {
    'ia': {
        'name': "Interlingua",
        'local_name': "Interlingua",
        'plural_rule': 'es',
        'team': 'ia <LL@li.org>\n'
    },
    'cak': {
        'name': "Kaqchikel",
        'local_name': "Kaqchikel",
        'plural_rule': 'az',
        'team': 'cak <LL@li.org>\n'
    },
    'zam': {
        'name': "Miahuatlán Zapotec",
        'local_name': "DíɁztè",
        'plural_rule': 'az',
        'team': 'zam <LL@li.org>\n'
    },
    'trs': {
        'name': "Chicahuaxtla Triqui",
        'local_name': "Triqui",
        'plural_rule': 'az',
        'team': 'trs <LL@li.org>\n'
    },
    'meh': {
        'name': "Mixteco Yucuhiti",
        'local_name': "Tu´un savi ñuu Yasi'í Yuku Iti",
        'plural_rule': 'id',
        'team': 'meh <LL@li.org>\n'
    },
    'mix': {
        'name': "Mixtepec Mixtec",
        'local_name': "Tu'un savi",
        'plural_rule': 'id',
        'team': 'mix <LL@li.org>\n'
    },
    'oc': {
        'name': 'Occitan',
        'local_name': 'occitan',
        'plural_rule': 'fi',
        'team': 'oc <LL@li.org>\n'
    },
    'an': {
        'name': 'Aragonese',
        'local_name': 'Aragonés',
        'plural_rule': 'fi',
        'team': 'an <LL@li.org>\n'
    },
    'wo': {
        'name': 'Wolof',
        'local_name': 'Wolof',
        'plural_rule': 'id',
        'team': 'wo <LL@li.org>\n'
    },
    'tt': {
        'name': 'Tatar',
        'local_name': 'татарча',
        'plural_rule': 'fi',
        'team': 'tt <LL@li.org>\n'
    },
    'anp': {
        'name': 'Angika',
        'local_name': 'अंगिका',
        'plural_rule': 'bg',
        'team': 'anp <LL@li.org>\n'
    },
}


class Language(object):
    """Represents a single language."""

    def __init__(self, code, env=None):
        self.code = code
        self.env = env
        if code and code in MISSING_LOCALES:
            self.locale = Locale.parse(MISSING_LOCALES[code]['plural_rule'], sep='-')
        elif code:
            self.locale = Locale.parse(code, sep='-')
        else:
            self.locale = None

    def __unicode__(self):  # pragma: no cover
        return str(self.code)

    def xml(self, kind):
        # Android uses a special language code format for the region part
        if self.code in ANDROID_LOCALE_MAPPING['to']:
            code = ANDROID_LOCALE_MAPPING['to'][self.code]
        else:
            code = self.code
        parts = tuple(code.split('_', 2))
        if len(parts) == 2:
            android_code = "%s-r%s" % parts
        else:
            android_code = "%s" % parts
        return self.env.path(self.env.resource_dir,
                             'values-%s/%s.xml' % (android_code, kind))

    def po(self, kind):
        filename = self.env.config.layout % {
            'group': kind,
            'domain': self.env.config.domain or 'android',
            'locale': self.code}
        return self.env.path(self.env.gettext_dir, filename)

    @property
    def plural_keywords(self):
        # Sort plural rules properly
        ret = list(self.locale.plural_form.rules.keys()) + ['other']
        return sorted(ret, key=key_plural_keywords)


class DefaultLanguage(Language):
    """A special version of ``Language``, representing the default
    language.

    For the Android side, this means the XML files in the values/
    directory. For the gettext side, it means the .pot file(s).
    """

    def __init__(self, env):
        super(DefaultLanguage, self).__init__(None, env)

    def __unicode__(self):  # pragma: no cover
        return '<def>'

    def xml(self, kind):
        return self.env.path(self.env.resource_dir, 'values/%s.xml' % kind)

    def po(self, kind):
        filename = self.env.config.template_name % {
            'group': kind,
            'domain': self.env.config.domain or 'android',
        }
        return self.env.path(self.env.gettext_dir, filename)


def resolve_locale(code, env):
    """Return a ``Language`` instance for a locale code.

    Deals with incorrect Babel locale values."""
    try:
        return Language(code, env)
    except UnknownLocaleError:
        env.w.action('failed', '%s is not a valid locale' % code)


def find_project_dir_and_config():
    """Goes upwards through the directory hierarchy and tries to find
    either an Android project directory, a config file for ours, or both.

    The latter case (both) can only happen if the config file is in the
    root of the Android directory, because once we have either, we stop
    searching.

    Note that the two are distinct, in that if a config file is found,
    it's directory is not considered a "project directory" from which
    default paths can be derived.

    Returns a 2-tuple (project_dir, config_file).
    """
    cur = os.getcwd()

    while True:
        project_dir = config_file = None

        manifest_path = path.join(cur, 'AndroidManifest.xml')
        if path.exists(manifest_path) and path.isfile(manifest_path):
            project_dir = cur

        config_path = path.join(cur, '.android2po')
        if path.exists(config_path) and path.isfile(config_path):
            config_file = config_path

        # Stop once we found either.
        if project_dir or config_file:
            return project_dir, config_file

        # Stop once we're at the root of the filesystem.
        old = cur
        cur = path.normpath(path.join(cur, path.pardir))
        if cur == old:
            # No further change, we are probably at root level.
            # TODO: Is there a better way? Is path.ismount suitable?
            # Or we could split the path into pieces by path.sep.
            break

    return None, None


def find_android_kinds(resource_dir, get_all=False):
    """Return a list of Android XML resource types that are in use.

    For this, we simply have a look which xml files exists in the
    default values/ resource directory, and return those which
    include string resources.

    If ``get_all`` is given, the test for string resources will be
    skipped.
    """
    kinds = []
    search_dir = path.join(resource_dir, 'values')
    for name in os.listdir(search_dir):
        filename = path.join(search_dir, name)
        if path.isfile(filename) and name.endswith('.xml'):
            # We want to support arbitrary xml resource file names, but
            # we also need to make sure we only return those which actually
            # contain string resources. More specifically, a file named
            # my-colors.xml, containing only color resources, should not
            # result in a my-colors.po catalog to be created.
            #
            # We thus attempt to read each file here, see if there are any
            # strings in it. If we fail to parse a file, we return it and
            # trust that whatever command the user selected will later also
            # stumble and show a proper error.
            #
            # TODO:
            # I'm not entirely happy about this. One obvious problem is that
            # we are likely to parse these xml files twice, which seems like
            # a code smell. One potential solution: Stores the parsed XML
            # result directly in memory, with the environment, rather than
            # parsing it a second time later.
            #
            # We could also opt to fail outright if we encounter an invalid
            # XML file here, since the error doesn't belong to any "action".
            kind = path.splitext(name)[0]
            if kind in ('strings', 'arrays') or get_all:
                # These kinds are special, they are always supposed to
                # contain something translatable, so always include them.
                kinds.append(kind)
            else:
                try:
                    strings = read_xml(filename)
                except InvalidResourceError as e:
                    raise EnvironmentError('Failed to parse "%s": %s' % (filename, e))
                else:
                    # If there are any strings in the file, detect as
                    # a kind of xml file.
                    if strings:
                        kinds.append(kind)
    return kinds


class Environment(object):
    """Environment is the main object that holds all the data with
    which we run.

    Usage:

        env = Environment()
        env.pop_from_config(config)
        env.init()
    """

    def __init__(self, writer):
        self.w = writer
        self.xmlfiles = []
        self.default = DefaultLanguage(self)
        self.config = Config()
        self.auto_gettext_dir = None
        self.auto_resource_dir = None
        self.resource_dir = None
        self.gettext_dir = None

        # Try to determine if we are inside a project; if so, we a) might
        # find a configuration file, and b) can potentially assume some
        # default directory names.
        self.project_dir, self.config_file = find_project_dir_and_config()

    def _pull_into(self, namespace, target):
        """If for a value ``namespace`` there exists a corresponding
        attribute on ``target``, then update that attribute with the
        values from ``namespace``, and then remove the value from
        ``namespace``.

        This is needed because certain options, if passed on the command
        line, need nevertheless to be stored in the ``self.config``
        object. We therefore **pull** those values in, and return the
        rest of the options.
        """
        for name in dir(namespace):
            if name.startswith('_'):
                continue
            if name in target.__dict__:
                setattr(target, name, getattr(namespace, name))
                delattr(namespace, name)
        return namespace

    def _pull_into_self(self, namespace):
        """This is essentially like ``self._pull_info``, but we pull
        values into the environment object itself, and in order to avoid
        conflicts between option values and attributes on the environment
        (for example ``config``), we explicitly specify the values we're
        interested in: It's the "big" ones which we would like to make
        available on the environment object directly.
        """
        for name in ('resource_dir', 'gettext_dir'):
            if hasattr(namespace, name):
                setattr(self, name, getattr(namespace, name))
                delattr(namespace, name)
        return namespace

    def pop_from_options(self, argparse_namespace):
        """Apply the set of options given on the command line.

        These means that we need those options that are "configuration"
        values to end up in ``self.config``. The normal options will
        be made available as ``self.options``.
        """
        rest = self._pull_into_self(argparse_namespace)
        rest = self._pull_into(rest, self.config)
        self.options = rest

    def pop_from_config(self, argparse_namespace):
        """Load the values we support into our attributes, remove them
        from the ``config`` namespace, and store whatever is left in
        ``self.config``.
        """
        rest = self._pull_into_self(argparse_namespace)
        rest = self._pull_into(rest, self.config)
        # At this point, there shouldn't be anything left, because
        # nothing should be included in the argparse result that we
        # don't consider a configuration option.
        ns = Namespace()
        assert rest == ns

    def auto_paths(self):
        """Try to auto-fill some path values that don't have values yet.
        """
        if self.project_dir:
            if not self.resource_dir:
                self.resource_dir = path.join(self.project_dir, 'res')
                self.auto_resource_dir = True
            if not self.gettext_dir:
                self.gettext_dir = path.join(self.project_dir, 'locale')
                self.auto_gettext_dir = True

    def path(self, *pargs):
        """Helper that constructs a Path object using the project dir
        as the base."""
        return Path(*pargs, base=self.project_dir)

    def init(self):
        """Initialize the environment.

        This entails finding the default Android language resource files,
        and in the process doing some basic validation.
        An ``EnvironmentError`` is thrown if there is something wrong.
        """
        # If either of those is not specified, we can't continue. Raise a
        # special exception that let's the caller display the proper steps
        # on how to proceed.
        if not self.resource_dir or not self.gettext_dir:
            raise IncompleteEnvironment()

        # It's not enough for directories to be specified; they really
        # should exist as well. In particular, the locale/ directory is
        # not part of the standard Android tree and thus likely to not
        # exist yet, so we create it automatically, but ONLY if it wasn't
        # specified explicitely. If the user gave a specific location,
        # it seems right to let him deal with it fully.
        if not path.exists(self.gettext_dir) and self.auto_gettext_dir:
            os.makedirs(self.gettext_dir)
        elif not path.exists(self.gettext_dir):
            raise EnvironmentError('Gettext directory at "%s" doesn\'t exist.' %
                                   self.gettext_dir)
        elif not path.exists(self.resource_dir):
            raise EnvironmentError('Android resource direcory at "%s" doesn\'t exist.' %
                                   self.resource_dir)

        # Find the Android XML resources that are our original source
        # files, i.e. for example the values/strings.xml file.
        groups_found = find_android_kinds(self.resource_dir,
                                          get_all=bool(self.config.groups))
        if self.config.groups:
            self.xmlfiles = self.config.groups
            _missing = set(self.config.groups) - set(groups_found)
            if _missing:
                raise EnvironmentError(
                    'Unable to find the default XML files for the following groups: %s' % (
                        ", ".join(["%s (%s)" % (
                            g, path.join(self.resource_dir, 'values', "%s.xml" % g)) for g in _missing])
                    ))
        else:
            self.xmlfiles = groups_found
        if not self.xmlfiles:
            raise EnvironmentError('no language-neutral string resources found in "values/".')

        # If regular expressions are used as ignore filters, precompile
        # those to help speed things along. For simplicity, we also
        # convert all static ignores to regexes.
        compiled_list = []
        for ignore_list in self.config.ignores:
            for ignore in ignore_list:
                if ignore.startswith('/') and ignore.endswith('/'):
                    compiled_list.append(re.compile(ignore[1:-1]))
                else:
                    compiled_list.append(re.compile("^%s$" % re.escape(ignore)))
        self.config.ignores = compiled_list

        # Validate the layout option, and resolve magic constants ("gnu")
        # to an actual format string.
        layout = self.config.layout
        multiple_pos = len(self.xmlfiles) > 1
        if not layout or layout == 'default':
            if self.config.domain and multiple_pos:
                layout = '%(domain)s-%(group)s-%(locale)s.po'
            elif self.config.domain:
                layout = '%(domain)s-%(locale)s.po'
            elif multiple_pos:
                layout = '%(group)s-%(locale)s.po'
            else:
                layout = '%(locale)s.po'
        elif layout == 'gnu':
            if multiple_pos:
                layout = '%(locale)s/LC_MESSAGES/%(group)s-%(domain)s.po'
            else:
                layout = '%(locale)s/LC_MESSAGES/%(domain)s.po'
        else:
            # TODO: These tests essentially disallow any advanced
            # formatting syntax. While that is unlikely to be used
            # or needed, a better way to test for the existance of
            # a placeholder would probably be to insert a unique string
            # and see if it comes out at the end; or, come up with
            # a proper regex to parse.
            if '%(locale)s' not in layout:
                raise EnvironmentError('--layout lacks %(locale)s variable')
            if self.config.domain and '%(domain)s' not in layout:
                raise EnvironmentError('--layout needs %(domain)s variable, ',
                                       'since you have set a --domain')
            if multiple_pos and '%(group)s' not in layout:
                raise EnvironmentError('--layout needs %%(group)s variable, '
                                       'since you have multiple groups: %s' % (
                                           ", ".join(self.xmlfiles)))
        self.config.layout = layout

        # The --template option needs similar processing:
        template = self.config.template_name
        if not template:
            if self.config.domain and multiple_pos:
                template = '%(domain)s-%(group)s.pot'
            elif self.config.domain:
                template = '%(domain)s.pot'
            elif multiple_pos:
                template = '%(group)s.pot'
            else:
                template = 'template.pot'
        elif '%s' in template and '%(group)s' not in template:
            # In an earlier version the --template option only
            # supported a %s placeholder for the XML kind. Make
            # sure we still support this.
            # TODO: Would be nice we if could raise a deprecation
            # warning here somehow. That means adding a callback
            # to this function. Or, probably we should just make the
            # environment aware of the writer object. This would
            # simplify other things as well.
            template = template.replace('%s', '%(group)s')
        else:
            # Note that we do not validate %(domain)s here; we expressively
            # allow the user to define a template without a domain.
            # TODO: See the same case above when handling --layout
            if multiple_pos and '%(group)s' not in template:
                raise EnvironmentError('--template needs %%(group)s variable, '
                                       'since you have multiple groups: %s' % (
                                           ", ".join(self.xmlfiles)))
        self.config.template_name = template

    LANG_DIR = re.compile(r'^values-(\w\w)(?:-r(\w\w))?$')

    def get_android_languages(self):
        """Finds the languages that already exist inside the Android
        resource directory.

        Return value is a list of ``Language`` instances.
        """
        languages = []
        for name in os.listdir(self.resource_dir):
            match = self.LANG_DIR.match(name)
            if not match:
                continue
            country, region = match.groups()
            pseudo_code = "%s" % country
            if region:
                pseudo_code += "_%s" % region
            if pseudo_code in ANDROID_LOCALE_MAPPING['from']:
                code = ANDROID_LOCALE_MAPPING['from'][pseudo_code]
            else:
                code = pseudo_code
            language = resolve_locale(code, self)
            if language:
                languages.append(language)
        return languages

    def get_gettext_languages(self):
        """Finds the languages that already exist inside the gettext
        directory.

        This is a little more though than on the Android side, since
        we give the user a lot of flexibility in configuring how the
        .po files are layed out.

        Return value is a list of ``Language`` instances.
        """

        # Build a glob pattern based on the layout. This will enable
        # us to easily get a list of files that match the pattern.
        glob_pattern = self.config.layout % {
            'domain': self.config.domain,
            'group': '*',
            'locale': '*',
        }

        # Temporarily switch to the gettext directory. This allows us
        # to simply call glob() using the relative pattern, rather than
        # having to deal with making a full path, and then later on
        # stripping the full path again for the regex matching, and
        # potentially even running into problems when, say, the pattern
        # contains references like ../ to a parent directory.
        old_dir = os.getcwd()
        os.chdir(self.gettext_dir)
        try:
            list = glob.glob(glob_pattern)

            # We now have a list of matching .po files, but now idea
            # which languages they represent, because we don't know
            # which part of the filename is the locale. To solve this,
            # we build a regular expression from the format string,
            # one with a capture group where the locale code should be.
            regex = re.compile(format_to_re(self.config.layout))

            # We then try to match every single file returned by glob.
            # In this way, we can build a list of unique locale codes.
            languages = {}
            for item in list:
                m = regex.match(item)
                if not m:
                    continue
                code = m.groupdict()['locale']
                if code not in languages:
                    language = resolve_locale(code, self)
                    if language:
                        languages[code] = language

            return languages.values()
        finally:
            os.chdir(old_dir)
