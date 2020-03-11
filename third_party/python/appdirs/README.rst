.. image:: https://secure.travis-ci.org/ActiveState/appdirs.png
    :target: http://travis-ci.org/ActiveState/appdirs

the problem
===========

What directory should your app use for storing user data? If running on Mac OS X, you
should use::

    ~/Library/Application Support/<AppName>

If on Windows (at least English Win XP) that should be::

    C:\Documents and Settings\<User>\Application Data\Local Settings\<AppAuthor>\<AppName>

or possibly::

    C:\Documents and Settings\<User>\Application Data\<AppAuthor>\<AppName>

for `roaming profiles <http://bit.ly/9yl3b6>`_ but that is another story.

On Linux (and other Unices) the dir, according to the `XDG
spec <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>`_, is::

    ~/.local/share/<AppName>


``appdirs`` to the rescue
=========================

This kind of thing is what the ``appdirs`` module is for. ``appdirs`` will
help you choose an appropriate:

- user data dir (``user_data_dir``)
- user config dir (``user_config_dir``)
- user cache dir (``user_cache_dir``)
- site data dir (``site_data_dir``)
- site config dir (``site_config_dir``)
- user log dir (``user_log_dir``)

and also:

- is a single module so other Python packages can include their own private copy
- is slightly opinionated on the directory names used. Look for "OPINION" in
  documentation and code for when an opinion is being applied.


some example output
===================

On Mac OS X::

    >>> from appdirs import *
    >>> appname = "SuperApp"
    >>> appauthor = "Acme"
    >>> user_data_dir(appname, appauthor)
    '/Users/trentm/Library/Application Support/SuperApp'
    >>> site_data_dir(appname, appauthor)
    '/Library/Application Support/SuperApp'
    >>> user_cache_dir(appname, appauthor)
    '/Users/trentm/Library/Caches/SuperApp'
    >>> user_log_dir(appname, appauthor)
    '/Users/trentm/Library/Logs/SuperApp'

On Windows 7::

    >>> from appdirs import *
    >>> appname = "SuperApp"
    >>> appauthor = "Acme"
    >>> user_data_dir(appname, appauthor)
    'C:\\Users\\trentm\\AppData\\Local\\Acme\\SuperApp'
    >>> user_data_dir(appname, appauthor, roaming=True)
    'C:\\Users\\trentm\\AppData\\Roaming\\Acme\\SuperApp'
    >>> user_cache_dir(appname, appauthor)
    'C:\\Users\\trentm\\AppData\\Local\\Acme\\SuperApp\\Cache'
    >>> user_log_dir(appname, appauthor)
    'C:\\Users\\trentm\\AppData\\Local\\Acme\\SuperApp\\Logs'

On Linux::

    >>> from appdirs import *
    >>> appname = "SuperApp"
    >>> appauthor = "Acme"
    >>> user_data_dir(appname, appauthor)
    '/home/trentm/.local/share/SuperApp
    >>> site_data_dir(appname, appauthor)
    '/usr/local/share/SuperApp'
    >>> site_data_dir(appname, appauthor, multipath=True)
    '/usr/local/share/SuperApp:/usr/share/SuperApp'
    >>> user_cache_dir(appname, appauthor)
    '/home/trentm/.cache/SuperApp'
    >>> user_log_dir(appname, appauthor)
    '/home/trentm/.cache/SuperApp/log'
    >>> user_config_dir(appname)
    '/home/trentm/.config/SuperApp'
    >>> site_config_dir(appname)
    '/etc/xdg/SuperApp'
    >>> os.environ['XDG_CONFIG_DIRS'] = '/etc:/usr/local/etc'
    >>> site_config_dir(appname, multipath=True)
    '/etc/SuperApp:/usr/local/etc/SuperApp'


``AppDirs`` for convenience
===========================

::

    >>> from appdirs import AppDirs
    >>> dirs = AppDirs("SuperApp", "Acme")
    >>> dirs.user_data_dir
    '/Users/trentm/Library/Application Support/SuperApp'
    >>> dirs.site_data_dir
    '/Library/Application Support/SuperApp'
    >>> dirs.user_cache_dir
    '/Users/trentm/Library/Caches/SuperApp'
    >>> dirs.user_log_dir
    '/Users/trentm/Library/Logs/SuperApp'


    
Per-version isolation
=====================

If you have multiple versions of your app in use that you want to be
able to run side-by-side, then you may want version-isolation for these
dirs::

    >>> from appdirs import AppDirs
    >>> dirs = AppDirs("SuperApp", "Acme", version="1.0")
    >>> dirs.user_data_dir
    '/Users/trentm/Library/Application Support/SuperApp/1.0'
    >>> dirs.site_data_dir
    '/Library/Application Support/SuperApp/1.0'
    >>> dirs.user_cache_dir
    '/Users/trentm/Library/Caches/SuperApp/1.0'
    >>> dirs.user_log_dir
    '/Users/trentm/Library/Logs/SuperApp/1.0'

