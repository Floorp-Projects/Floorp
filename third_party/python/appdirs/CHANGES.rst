appdirs Changelog
=================

appdirs 1.4.3
-------------
- [PR #76] Python 3.6 invalid escape sequence deprecation fixes
- Fix for Python 3.6 support

appdirs 1.4.2
-------------
- [PR #84] Allow installing without setuptools
- [PR #86] Fix string delimiters in setup.py description
- Add Python 3.6 support

appdirs 1.4.1
-------------
- [issue #38] Fix _winreg import on Windows Py3
- [issue #55] Make appname optional

appdirs 1.4.0
-------------
- [PR #42] AppAuthor is now optional on Windows
- [issue 41] Support Jython on Windows, Mac, and Unix-like platforms. Windows
  support requires `JNA <https://github.com/twall/jna>`_.
- [PR #44] Fix incorrect behaviour of the site_config_dir method

appdirs 1.3.0
-------------
- [Unix, issue 16] Conform to XDG standard, instead of breaking it for
  everybody
- [Unix] Removes gratuitous case mangling of the case, since \*nix-es are
  usually case sensitive, so mangling is not wise
- [Unix] Fixes the utterly wrong behaviour in ``site_data_dir``, return result
  based on XDG_DATA_DIRS and make room for respecting the standard which
  specifies XDG_DATA_DIRS is a multiple-value variable
- [Issue 6] Add ``*_config_dir`` which are distinct on nix-es, according to
  XDG specs; on Windows and Mac return the corresponding ``*_data_dir``

appdirs 1.2.0
-------------

- [Unix] Put ``user_log_dir`` under the *cache* dir on Unix. Seems to be more
  typical.
- [issue 9] Make ``unicode`` work on py3k.

appdirs 1.1.0
-------------

- [issue 4] Add ``AppDirs.user_log_dir``.
- [Unix, issue 2, issue 7] appdirs now conforms to `XDG base directory spec
  <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>`_.
- [Mac, issue 5] Fix ``site_data_dir()`` on Mac.
- [Mac] Drop use of 'Carbon' module in favour of hardcoded paths; supports
  Python3 now.
- [Windows] Append "Cache" to ``user_cache_dir`` on Windows by default. Use
  ``opinion=False`` option to disable this.
- Add ``appdirs.AppDirs`` convenience class. Usage:

        >>> dirs = AppDirs("SuperApp", "Acme", version="1.0")
        >>> dirs.user_data_dir
        '/Users/trentm/Library/Application Support/SuperApp/1.0'

- [Windows] Cherry-pick Komodo's change to downgrade paths to the Windows short
  paths if there are high bit chars.
- [Linux] Change default ``user_cache_dir()`` on Linux to be singular, e.g.
  "~/.superapp/cache".
- [Windows] Add ``roaming`` option to ``user_data_dir()`` (for use on Windows only)
  and change the default ``user_data_dir`` behaviour to use a *non*-roaming
  profile dir (``CSIDL_LOCAL_APPDATA`` instead of ``CSIDL_APPDATA``). Why? Because
  a large roaming profile can cause login speed issues. The "only syncs on
  logout" behaviour can cause surprises in appdata info.


appdirs 1.0.1 (never released)
------------------------------

Started this changelog 27 July 2010. Before that this module originated in the
`Komodo <http://www.activestate.com/komodo>`_ product as ``applib.py`` and then
as `applib/location.py
<http://github.com/ActiveState/applib/blob/master/applib/location.py>`_ (used by
`PyPM <http://code.activestate.com/pypm/>`_ in `ActivePython
<http://www.activestate.com/activepython>`_). This is basically a fork of
applib.py 1.0.1 and applib/location.py 1.0.1.

