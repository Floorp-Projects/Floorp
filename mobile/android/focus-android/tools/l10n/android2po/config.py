from os import path
import argparse

__all__ = ('Config',)


def percentage(string):
    errstr = "must be a float between 0 and 1, not %r" % string
    try:
        value = float(string)
    except ValueError:
        raise argparse.ArgumentTypeError(errstr)
    if value < 0 or value > 1:
        raise argparse.ArgumentTypeError(errstr)
    return value


class Config(object):
    """Defines all the options supported by our configuration system.
    """
    OPTIONS = (
        {
            'name': 'android',
            'help': 'Android resource directory ($PROJECT/res by default)',
            'dest': 'resource_dir',
            'kwargs': {'metavar': 'DIR'}
            # No default, and will not actually be stored on the config object.
        },
        {
            'name': 'gettext',
            'help': 'directory containing the .po files ($PROJECT/locale by default)',
            'dest': 'gettext_dir',
            'kwargs': {'metavar': 'DIR'}
            # No default, and will not actually be stored on the config object.
         },
        {
            'name': 'groups',
            'help': 'process the given default XML files (for example ' +
                    '"strings arrays"); by default all files which contain ' +
                    'string resources will be used',
            'dest': 'groups',
            'default': [],
            'kwargs': {'nargs': '+', 'metavar': 'GROUP'}
        },
        {
            'name': 'no-template',
            'help': 'do not generate a .pot template file on export',
            'dest': 'no_template',
            'default': False,
            'kwargs': {'action': 'store_true'}
        },
        {
            'name': 'template',
            'help': 'filename to use for the .pot file(s); may contain the ' +
                    '%%(domain)s and %%(group)s variables',
            'dest': 'template_name',
            'default': '',
            'kwargs': {'metavar': 'NAME'}
        },
        {
            'name': 'ignore',
            'help': 'ignore the given message; can be given multiple times; ' +
                    'regular expressions can be used if putting the value ' +
                    'inside slashes (/match/)',
            'dest': 'ignores',
            'default': [],
            'kwargs': {'metavar': 'MATCH', 'action': 'append', 'nargs': '+'}
        },
        {
            'name': 'ignore-fuzzy',
            'help': 'during import, ignore messages marked as fuzzy in .po files',
            'dest': 'ignore_fuzzy',
            'default': False,
            'kwargs': {'action': 'store_true'}
        },
        {
            'name': 'require-min-complete',
            'help': 'ignore a language\'s .po file(s) completely if there ' +
                    'aren\'t at least the given percentage of translations',
            'dest': 'min_completion',
            'default': 0,
            'kwargs': {'metavar': 'FLOAT', 'type': percentage}
        },
        {
            'name': 'domain',
            'help': 'gettext po domain to use, affects the .po filenames',
            'dest': 'domain',
            'default': None,
        },
        {
            'name': 'layout',
            'help': 'how and where .po files are stored; may be "default", ' +
                    '"gnu", or a custom path using the variables %%(locale)s ' +
                    '%%(domain)s and optionally %%(group)s. E.g., ' +
                    '"%%(group)s-%%(locale)s.po" will write to "strings-es.po" ' +
                    'for Spanish in strings.xml.',
            'dest': 'layout',
            'default': 'default',
        },
        {
            'name': 'enable-fuzzy-matching',
            'help': 'enable fuzzy matching during export command. When it is enabled ' +
                    'android2po will automatically add translations for new strings. ' +
                    'by default this behaviour is turned off',
            'dest': 'enable_fuzzy_matching',
            'default': False,
            'kwargs': {'action': 'store_true'}
        },
        {
            'name': 'clear-obsolete',
            'help': 'during export do not add obsolete strings to the generated .po files',
            'dest': 'clear_obsolete',
            'default': True,
            'kwargs': {'action': 'store_true'}
        }
    )

    def __init__(self):
        """Initialize all configuration values with a default.

        It is important that we do this here manually, rather than relying
        on the "default" mechanism of argparse, because we have multiple
        potential congiguration sources (command line, config file), and
        we don't want defaults to override actual values.

        The attributes we define here are also used to determine
        which command line options passed should be assigned to this
        object, and which should be exposed via a separate ``options``
        namespace.
        """
        for optdef in self.OPTIONS:
            if 'default' in optdef:
                setattr(self, optdef['dest'], optdef['default'])

    @classmethod
    def setup_arguments(cls, parser):
        """Setup our configuration values as arguments in the ``argparse``
        object in ``parser``.
        """
        for optdef in cls.OPTIONS:
            names = ('--%s' % optdef.get('name'),)
            kwargs = {
                'help': optdef.get('help', None),
                'dest': optdef.get('dest', None),
                # We handle defaults ourselves. This is actually important,
                # or defaults from one config source may override valid
                # values from another.
                'default': argparse.SUPPRESS,
            }
            kwargs.update(optdef.get('kwargs', {}))
            parser.add_argument(*names, **kwargs)

    @classmethod
    def rebase_paths(cls, config, base_path):
        """Make those config values that are paths relative to
        ``base_path``, because by default, paths are relative to
        the current working directory.
        """
        for name in ('gettext_dir', 'resource_dir'):
            value = getattr(config, name, None)
            if value is not None:
                setattr(config, name, path.normpath(path.join(base_path, value)))
