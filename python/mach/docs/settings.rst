.. _mach_settings:

========
Settings
========

Mach can read settings in from a set of configuration files. These
configuration files are either named ``machrc`` or ``.machrc`` and
are specified by the bootstrap script. In mozilla-central, these files
can live in ``~/.mozbuild`` and/or ``topsrcdir``.

Settings can be specified anywhere, and used both by mach core or
individual commands.


Core Settings
=============

These settings are implemented by mach core.

* alias - Create a command alias. This is useful if you want to alias a command to something else, optionally including some defaults. It can either be used to create an entire new command, or provide defaults for an existing one. For example:

.. parsed-literal::

    [alias]
    mochitest = mochitest -f browser
    browser-test = mochitest -f browser


Defining Settings
=================

Settings need to be explicitly defined, along with their type,
otherwise mach will throw when trying to access them.

To define settings, use the :func:`~decorators.SettingsProvider`
decorator in an existing mach command module. E.g:

.. code-block:: python

    from mach.decorators import SettingsProvider

    @SettingsProvider
    class ArbitraryClassName(object):
        config_settings = [
            ('foo.bar', 'string'),
            ('foo.baz', 'int', 0, set([0,1,2])),
        ]

``@SettingsProvider``'s must specify a variable called ``config_settings``
that returns a list of tuples. Alternatively, it can specify a function
called ``config_settings`` that returns a list of tuples.

Each tuple is of the form:

.. code-block:: python

    ('<section>.<option>', '<type>', default, extra)

``type`` is a string and can be one of:
string, boolean, int, pos_int, path

``default`` is optional, and provides a default value in case none was
specified by any of the configuration files.

``extra`` is also optional and is a dict containing additional key/value
pairs to add to the setting's metadata. The following keys may be specified
in the ``extra`` dict:
    * ``choices`` - A set of allowed values for the setting.

Wildcards
---------

Sometimes a section should allow arbitrarily defined options from the user, such
as the ``alias`` section mentioned above. To define a section like this, use ``*``
as the option name. For example:

.. parsed-literal::

    ('foo.*', 'string')

This allows configuration files like this:

.. parsed-literal::

    [foo]
    arbitrary1 = some string
    arbitrary2 = some other string


Documenting Settings
====================

All settings must at least be documented in the en_US locale. Otherwise,
running ``mach settings`` will raise. Mach uses gettext to perform localization.

A handy command exists to generate the localization files:

.. parsed-literal::

    mach settings locale-gen <section>

You'll be prompted to add documentation for all options in section with the
en_US locale. To add documentation in another locale, pass in ``--locale``.


Accessing Settings
==================

Now that the settings are defined and documented, they're accessible from
individual mach commands if the command receives a context in its constructor.
For example:

.. code-block:: python

    from mach.decorators import (
        Command,
        CommandProvider,
        SettingsProvider,
    )

    @SettingsProvider
    class ExampleSettings(object):
        config_settings = [
            ('a.b', 'string', 'default'),
            ('foo.bar', 'string'),
            ('foo.baz', 'int', 0, {'choices': set([0,1,2])}),
        ]

    @CommandProvider
    class Commands(object):
        def __init__(self, context):
            self.settings = context.settings

        @Command('command', category='misc',
                 description='Prints a setting')
        def command(self):
            print(self.settings.a.b)
            for option in self.settings.foo:
                print(self.settings.foo[option])
