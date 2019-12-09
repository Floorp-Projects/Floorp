Release History
===============

.. include:: _draft.rst

.. towncrier release notes start

v16.7.8 (2019-11-22)
--------------------

Bugfixes
^^^^^^^^

- upgrade setuptools from 41.4.0 to 41.6.0 (`#1442 <https://github.com/pypa/virtualenv/issues/1442>`_)


v16.7.7 (2019-10-22)
--------------------

Bugfixes
^^^^^^^^

- * fix virtualenv creation when ``--no-pip`` argument used. (`#1430 <https://github.com/pypa/virtualenv/issues/1430>`_)
- upgrade bundled pip from ``19.3`` to ``19.3.1`` (`#1433 <https://github.com/pypa/virtualenv/issues/1433>`_)


v16.7.6 (2019-10-16)
--------------------

Bugfixes
^^^^^^^^

- fix to support for Python 3 on MacOS 10.15 provided by Xcode (`#1402 <https://github.com/pypa/virtualenv/issues/1402>`_)
- bump bundled pip from ``19.2.3`` to ``19.3`` and setuptools from ``41.2.0`` to ``41.4.0`` (`#1428 <https://github.com/pypa/virtualenv/issues/1428>`_)


v16.7.5 (2019-09-03)
--------------------

Bugfixes
^^^^^^^^

- upgrade pip from ``19.2.2`` to ``19.2.3`` (`#1414 <https://github.com/pypa/virtualenv/issues/1414>`_)


v16.7.4 (2019-08-23)
--------------------

Bugfixes
^^^^^^^^

- * fix powershell activation when sourced (`#1398 <https://github.com/pypa/virtualenv/issues/1398>`_)
- * upgrade wheel from ``0.33.4`` to ``0.33.6`` and setuptools from ``41.1.0`` to ``41.2.0`` (`#1409 <https://github.com/pypa/virtualenv/issues/1409>`_)


v16.7.3 (2019-08-16)
--------------------

Bugfixes
^^^^^^^^

- upgrade pip from ``19.1.1`` to ``19.2.2`` and setuptools from ``41.0.1`` to ``41.1.0`` (`#1404 <https://github.com/pypa/virtualenv/issues/1404>`_)


v16.7.2 (2019-07-26)
--------------------

Bugfixes
^^^^^^^^

- fix regression - sh activation script not working under sh (only bash) (`#1396 <https://github.com/pypa/virtualenv/issues/1396>`_)


v16.7.1 (2019-07-25)
--------------------

Features
^^^^^^^^

- pip bumped to 19.2.1 (`#1392 <https://github.com/pypa/virtualenv/issues/1392>`_)


v16.7.0 (2019-07-23)
--------------------

Features
^^^^^^^^

- ``activate.ps1`` syntax and style updated to follow ``PSStyleAnalyzer`` rules (`#1371 <https://github.com/pypa/virtualenv/issues/1371>`_)
- Allow creating virtual environments for ``3.xy``. (`#1385 <https://github.com/pypa/virtualenv/issues/1385>`_)
- Report error when running activate scripts directly, instead of sourcing. By reporting an error instead of running silently, the user get immediate feedback that the script was not used correctly. Only Bash and PowerShell are supported for now. (`#1388 <https://github.com/pypa/virtualenv/issues/1388>`_)
- * add pip 19.2 (19.1.1 is kept to still support python 3.4 dropped by latest pip) (`#1389 <https://github.com/pypa/virtualenv/issues/1389>`_)


v16.6.2 (2019-07-14)
--------------------

Bugfixes
^^^^^^^^

- Extend the LICENSE search paths list by ``lib64/pythonX.Y`` to support Linux
  vendors who install their Python to ``/usr/lib64/pythonX.Y`` (Gentoo, Fedora,
  openSUSE, RHEL and others) - by ``hroncok`` (`#1382 <https://github.com/pypa/virtualenv/issues/1382>`_)


v16.6.1 (2019-06-16)
--------------------

Bugfixes
^^^^^^^^

- Raise an error if the target path contains the operating systems path separator (using this would break our activation scripts) - by @rrauenza. (`#395 <https://github.com/pypa/virtualenv/issues/395>`_)
- Fix an additional issue with #1339, where the user specifies ``--python``
  pointing to a venv redirector executable. (`#1364 <https://github.com/pypa/virtualenv/issues/1364>`_)


v16.6.0 (2019-05-15)
--------------------

Features
^^^^^^^^

- Drop Jython support. Jython became slower and slower in the last few months and significantly holds back our
  CI and development. As there's very little user base for it decided to drop support for it. If there are Jython
  developers reach out to us to see how we can add back support. (`#1354 <https://github.com/pypa/virtualenv/issues/1354>`_)
- Upgrade embedded packages:

      * upgrade wheel from ``0.33.1`` to ``0.33.4``
      * upgrade pip from ``19.1`` to ``19.1.1`` (`#1356 <https://github.com/pypa/virtualenv/issues/1356>`_)


v16.5.0 (2019-04-24)
--------------------

Bugfixes
^^^^^^^^

- Add tests covering prompt manipulation during activation/deactivation,
  and harmonize behavior of all supported shells - by ``bskinn`` (`#1330 <https://github.com/pypa/virtualenv/issues/1330>`_)
- Handle running virtualenv from within a virtual environment created
  using the stdlib ``venv`` module. Fixes #1339. (`#1345 <https://github.com/pypa/virtualenv/issues/1345>`_)


Features
^^^^^^^^

- ``-p`` option accepts Python version in following formats now: ``X``, ``X-ZZ``, ``X.Y`` and ``X.Y-ZZ``, where ``ZZ`` is ``32`` or ``64``. (Windows only) (`#1340 <https://github.com/pypa/virtualenv/issues/1340>`_)
- upgrade pip from ``19.0.3`` to ``19.1`` (`#1346 <https://github.com/pypa/virtualenv/issues/1346>`_)
- upgrade setuptools from ``40.8.0 to ``41.0.1`` (`#1346 <https://github.com/pypa/virtualenv/issues/1346>`_)


v16.4.3 (2019-03-01)
--------------------

Bugfixes
^^^^^^^^

- Revert the symlink fix, causing debian packaging issues. (`#1390 <https://github.com/pypa/virtualenv/issues/1390>`_)


v16.4.1 (2019-02-22)
--------------------

Bugfixes
^^^^^^^^

- Fix ``license()`` builtin by copying the ``LICENSE`` file into the virtualenv - by ``asottile``. (`#1317 <https://github.com/pypa/virtualenv/issues/1317>`_)


Features
^^^^^^^^

- bump vendored pip to ``19.0.3`` and wheel to ``0.33.1`` (`#1321 <https://github.com/pypa/virtualenv/issues/1321>`_)


v16.4.0 (2019-02-09)
--------------------

Bugfixes
^^^^^^^^

- fixes the scenario where the python base install is symlinked with relative symlinks (`#490 <https://github.com/pypa/virtualenv/issues/490>`_)
- Use ``importlib`` over ``imp`` in ``virtualenv.py`` for ``python >= 3.4`` - by Anthony Sottile (`#1293 <https://github.com/pypa/virtualenv/issues/1293>`_)
- Copy or link PyPy header files instead of include directory itself (`#1302 <https://github.com/pypa/virtualenv/issues/1302>`_)
- Allow virtualenv creation with older pip not having ``config`` command
  correspondingly disabling configuration related features (such as pip cert
  setting) in this case. (`#1303 <https://github.com/pypa/virtualenv/issues/1303>`_)


Features
^^^^^^^^

- upgrade to pip ``19.0.2`` and setuptools ``40.8.0`` (`#1312 <https://github.com/pypa/virtualenv/issues/1312>`_)


v16.3.0 (2019-01-25)
--------------------

Bugfixes
^^^^^^^^

- Use ``importlib`` over deprecated ``imp` in ``distutils/__init__.py`` for python 3 - by Anthony Sottile (`#955 <https://github.com/pypa/virtualenv/issues/955>`_)
- Preserve ``cert`` option defined in ``pip.conf`` or environment variable. (`#1273 <https://github.com/pypa/virtualenv/issues/1273>`_)
- fixed a ``ResourceWarning: unclosed file`` in ``call_subprocess()`` - by Mickaël Schoentgen (`#1277 <https://github.com/pypa/virtualenv/issues/1277>`_)
- pre-import some built-in modules in ``site.py`` on PyPy according to PyPy's ``site.py`` - by microdog (`#1281 <https://github.com/pypa/virtualenv/issues/1281>`_)
- Copy files from ``sys.exec_prefix`` only if it is really different path than
  used prefix, bugfix for #1270 (`#1282 <https://github.com/pypa/virtualenv/issues/1282>`_)


Features
^^^^^^^^

- Enable virtualenv to be distributed as a ``zipapp`` or to be run as a
  wheel with ``PYTHONPATH=virtualenv...any.whl python -mvirtualenv`` - by
  Anthony Sottile (`#1092 <https://github.com/pypa/virtualenv/issues/1092>`_)
- bump vendored pip from ``18.1`` to ``19.0.1`` (`#1291 <https://github.com/pypa/virtualenv/issues/1291>`_)


Documentation
^^^^^^^^^^^^^

- discourage installation as ``root``, including ``sudo`` - by ``altendky`` (`#1061 <https://github.com/pypa/virtualenv/issues/1061>`_)


v16.2.0 (2018-12-31)
--------------------

Bugfixes
^^^^^^^^

- ``copyfile`` handles relative symlinks and symlinks to symlinks, avoiding problems when Python was installed using ``stow`` or ``homebrew``. (`#268 <https://github.com/pypa/virtualenv/issues/268>`_)
- Fix preserving of original path when using fish and a subshell. (`#904 <https://github.com/pypa/virtualenv/issues/904>`_)
- Drop the source layout of the project, going back to how the source was laid out before ``16.1.0``. (`#1241 <https://github.com/pypa/virtualenv/issues/1241>`_)
- Fix bootstrap script generation broken with ``16.0.0``. Support now both ``CPython``, ``pypy``, ``jython``. (`#1244 <https://github.com/pypa/virtualenv/issues/1244>`_)
- ``lib64`` symlink is again relative (as was with ``< 16.1.0``). (`#1248 <https://github.com/pypa/virtualenv/issues/1248>`_)


Features
^^^^^^^^

- ``fish`` version 3 support for the activation script. (`#1275 <https://github.com/pypa/virtualenv/issues/1275>`_)
- ``powershell`` activator is no longer signed. (`#816 <https://github.com/pypa/virtualenv/issues/816>`_)
- ``pyproject.toml`` with ``PEP-517`` and ``PEP-518`` is now provided. ``tox.ini`` is now packaged with the ``sdist``. Distributions repackaging the library should use ``tox -e py`` to run the test suite on the ``sdist``. (`#909 <https://github.com/pypa/virtualenv/issues/909>`_)
- ``activate_this.py`` improvements: set ``VIRTUAL_ENV`` environment variable; ``pypy``, ``pypy3`` and ``jython`` support. (`#1057 <https://github.com/pypa/virtualenv/issues/1057>`_)
- The `xonsh <http://xon.sh/index.html>`_ shell is now supported by generating the ``xon.sh`` activation script. (`#1206 <https://github.com/pypa/virtualenv/issues/1206>`_)
- Support ``pip`` wheels with removed ``certifi's cacert.pem``. (`#1252 <https://github.com/pypa/virtualenv/issues/1252>`_)
- Upgrade setuptools from ``40.5.0`` to ``40.6.3`` and wheel from ``0.32.2`` to ``0.32.3``. (`#1257 <https://github.com/pypa/virtualenv/issues/1257>`_)
- ``powershell`` now also provides the ``pydoc`` function that uses the virtual environments ``pydoc``. (`#1258 <https://github.com/pypa/virtualenv/issues/1258>`_)
- Migrate to a ``setup.cfg`` based build. Minimum ``setuptools`` required to build is ``setuptools >= 40.6.3``, this is automatically acquired for all PEP-518 builders (recommended), or acquired via the old ``setup_requires`` method otherwise. Move exclusively to a ``setuptools`` generated console entry point script, this now does make ``setuptools >= 18.0.0`` a runtime dependency (install requires). Source and issue tracker now is shown on PyPi (supplied as package metadata) beside the homepage. (`#1259 <https://github.com/pypa/virtualenv/issues/1259>`_)


Deprecations (removal in next major release)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Using ``python setup.py test`` is now marked as deprecated and will be removed in next release. Use ``tox`` instead, always. (`#909 <https://github.com/pypa/virtualenv/issues/909>`_)
- Using the project directly from the source layout is now deprecated. Going ahead people wanting to use the project without installing the virtualenv are encouraged to download the wheel from PyPi and extract it to access the ``virtualenv.py`` file. We'll be switching to a ``src`` layout with next release. (`#1241 <https://github.com/pypa/virtualenv/issues/1241>`_)
- No longer support ``distutils`` build/installation, now ``setuptools >= 40.6.3`` is required. (`#1259 <https://github.com/pypa/virtualenv/issues/1259>`_)


Documentation
^^^^^^^^^^^^^

- ``activate_this.py`` recommend ``exec(open(this_file).read(), {'__file__': this_file})`` as it works both on Python 2 and 3. (`#1057 <https://github.com/pypa/virtualenv/issues/1057>`_)
- Clarify how this project relates to the standard libraries ``venv`` and when one would still want to use this tool. (`#1086 <https://github.com/pypa/virtualenv/issues/1086>`_)
- Move to a ``towncrier`` generated changelog to avoid merge conflicts, generate draft changelog documentation. Prefix version string in changelog with ``v`` to make the hyperlinks stable. (`#1234 <https://github.com/pypa/virtualenv/issues/1234>`_)


v16.1.0 (2018-10-31)
--------------------
* Fixed documentation to use pypi.org and correct curl options; :issue:`1042`
* bug fix: ensure prefix is absolute when creating a new virtual environment :issue:`1208`
* upgrade setuptools from ``39.1.0`` to ``40.5.0``
* upgrade wheel from ``0.31.1`` to ``0.32.2``
* upgrade pip from ``10.0.1`` to ``18.1``
* ``activate.csh`` does not use basename and handles newlines :issue:`1200`
* fix failure to copy on platforms that use lib64 :issue:`1189`
* enable tab-completion in the interactive interpreter by default, thanks to a new ``sys.__interactivehook__`` on Python 3 :issue:`967`
* suppress warning of usage of the deprecated ``imp`` module :issue:`1238`

v16.0.0 (2018-05-16)
--------------------

* Drop support for Python 2.6.
* Upgrade pip to 10.0.1.
* Upgrade setuptools to 39.1.0.
* Upgrade wheel to 0.31.1.


v15.2.0 (2018-03-21)
--------------------

* Upgrade setuptools to 39.0.1.

* Upgrade pip to 9.0.3.

* Upgrade wheel to 0.30.0.


v15.1.0 (2016-11-15)
--------------------

* Support Python 3.6.

* Upgrade setuptools to 28.0.0.

* Upgrade pip to 9.0.1.

* Don't install pre-release versions of pip, setuptools, or wheel from PyPI.


v15.0.3 (2016-08-05)
--------------------

* Test for given python path actually being an executable *file*, :issue:`939`

* Only search for copy actual existing Tcl/Tk directories (:pull:`937`)

* Generically search for correct Tcl/Tk version (:pull:`926`, :pull:`933`)

* Upgrade setuptools to 22.0.5

v15.0.2 (2016-05-28)
--------------------

* Copy Tcl/Tk libs on Windows to allow them to run,
  fixes :issue:`93` (:pull:`888`)

* Upgrade setuptools to 21.2.1.

* Upgrade pip to 8.1.2.


v15.0.1 (2016-03-17)
--------------------

* Print error message when DEST_DIR exists and is a file

* Upgrade setuptools to 20.3

* Upgrade pip to 8.1.1.


v15.0.0 (2016-03-05)
--------------------

* Remove the ``virtualenv-N.N`` script from the package; this can no longer be
  correctly created from a wheel installation.
  Resolves :issue:`851`, :issue:`692`

* Remove accidental runtime dependency on pip by extracting certificate in the
  subprocess.

* Upgrade setuptools 20.2.2.

* Upgrade pip to 8.1.0.


v14.0.6 (2016-02-07)
--------------------

* Upgrade setuptools to 20.0

* Upgrade wheel to 0.29.0

* Fix an error where virtualenv didn't pass in a working ssl certificate for
  pip, causing "weird" errors related to ssl.


v14.0.5 (2016-02-01)
--------------------

* Homogenize drive letter casing for both prefixes and filenames. :issue:`858`


v14.0.4 (2016-01-31)
--------------------

* Upgrade setuptools to 19.6.2

* Revert ac4ea65; only correct drive letter case.
  Fixes :issue:`856`, :issue:`815`


v14.0.3 (2016-01-28)
--------------------

* Upgrade setuptools to 19.6.1


v14.0.2 (2016-01-28)
--------------------

* Upgrade setuptools to 19.6

* Suppress any errors from ``unset`` on different shells (:pull:`843`)

* Normalize letter case for prefix path checking. Fixes :issue:`837`


v14.0.1 (2016-01-21)
--------------------

* Upgrade from pip 8.0.0 to 8.0.2.

* Fix the default of ``--(no-)download`` to default to downloading.


v14.0.0 (2016-01-19)
--------------------

* **BACKWARDS INCOMPATIBLE** Drop support for Python 3.2.

* Upgrade setuptools to 19.4

* Upgrade wheel to 0.26.0

* Upgrade pip to 8.0.0

* Upgrade argparse to 1.4.0

* Added support for ``python-config`` script (:pull:`798`)

* Updated activate.fish (:pull:`589`) (:pull:`799`)

* Account for a ``site.pyo`` correctly in some python implementations (:pull:`759`)

* Properly restore an empty PS1 (:issue:`407`)

* Properly remove ``pydoc`` when deactivating

* Remove workaround for very old Mageia / Mandriva linuxes (:pull:`472`)

* Added a space after virtualenv name in the prompt: ``(env) $PS1``

* Make sure not to run a --user install when creating the virtualenv (:pull:`803`)

* Remove virtualenv.py's path from sys.path when executing with a new
  python. Fixes issue :issue:`779`, :issue:`763` (:pull:`805`)

* Remove use of () in .bat files so ``Program Files (x86)`` works :issue:`35`

* Download new releases of the preinstalled software from PyPI when there are
  new releases available. This behavior can be disabled using
  ``--no-download``.

* Make ``--no-setuptools``, ``--no-pip``, and ``--no-wheel`` independent of
  each other.


v13.1.2 (2015-08-23)
--------------------

* Upgrade pip to 7.1.2.


v13.1.1 (2015-08-20)
--------------------

* Upgrade pip to 7.1.1.

* Upgrade setuptools to 18.2.

* Make the activate script safe to use when bash is running with ``-u``.


v13.1.0 (2015-06-30)
--------------------

* Upgrade pip to 7.1.0

* Upgrade setuptools to 18.0.1


v13.0.3 (2015-06-01)
--------------------

* Upgrade pip to 7.0.3


v13.0.2 (2015-06-01)
--------------------

* Upgrade pip to 7.0.2

* Upgrade setuptools to 17.0


v13.0.1 (2015-05-22)
--------------------

* Upgrade pip to 7.0.1


v13.0.0 (2015-05-21)
--------------------

* Automatically install wheel when creating a new virutalenv. This can be
  disabled by using the ``--no-wheel`` option.

* Don't trust the current directory as a location to discover files to install
  packages from.

* Upgrade setuptools to 16.0.

* Upgrade pip to 7.0.0.


v12.1.1 (2015-04-07)
--------------------

* Upgrade pip to 6.1.1


v12.1.0 (2015-04-07)
--------------------

* Upgrade setuptools to 15.0

* Upgrade pip to 6.1.0


v12.0.7 (2015-02-04)
--------------------

* Upgrade pip to 6.0.8


v12.0.6 (2015-01-28)
--------------------

* Upgrade pip to 6.0.7

* Upgrade setuptools to 12.0.5


v12.0.5 (2015-01-03)
--------------------

* Upgrade pip to 6.0.6

* Upgrade setuptools to 11.0


v12.0.4 (2014-12-23)
--------------------

* Revert the fix to ``-p`` on Debian based pythons as it was broken in other
  situations.

* Revert several sys.path changes new in 12.0 which were breaking virtualenv.

v12.0.3 (2014-12-23)
--------------------

* Fix an issue where Debian based Pythons would fail when using -p with the
  host Python.

* Upgrade pip to 6.0.3

v12.0.2 (2014-12-23)
--------------------

* Upgraded pip to 6.0.2

v12.0.1 (2014-12-22)
--------------------

* Upgraded pip to 6.0.1


v12.0 (2014-12-22)
------------------

* **PROCESS** Version numbers are now simply ``X.Y`` where the leading ``1``
  has been dropped.
* Split up documentation into structured pages
* Now using pytest framework
* Correct sys.path ordering for debian, issue #461
* Correctly throws error on older Pythons, issue #619
* Allow for empty $PATH, pull #601
* Don't set prompt if $env:VIRTUAL_ENV_DISABLE_PROMPT is set for Powershell
* Updated setuptools to 7.0

v1.11.6 (2014-05-16)
--------------------

* Updated setuptools to 3.6
* Updated pip to 1.5.6

v1.11.5 (2014-05-03)
--------------------

* Updated setuptools to 3.4.4
* Updated documentation to use https://virtualenv.pypa.io/
* Updated pip to 1.5.5

v1.11.4 (2014-02-21)
--------------------

* Updated pip to 1.5.4


v1.11.3 (2014-02-20)
--------------------

* Updated setuptools to 2.2
* Updated pip to 1.5.3


v1.11.2 (2014-01-26)
--------------------

* Fixed easy_install installed virtualenvs by updated pip to 1.5.2

v1.11.1 (2014-01-20)
--------------------

* Fixed an issue where pip and setuptools were not getting installed when using
  the ``--system-site-packages`` flag.
* Updated setuptools to fix an issue when installed with easy_install
* Fixed an issue with Python 3.4 and sys.stdout encoding being set to ascii
* Upgraded pip to v1.5.1
* Upgraded setuptools to v2.1

v1.11 (2014-01-02)
------------------

* **BACKWARDS INCOMPATIBLE** Switched to using wheels for the bundled copies of
  setuptools and pip. Using sdists is no longer supported - users supplying
  their own versions of pip/setuptools will need to provide wheels.
* **BACKWARDS INCOMPATIBLE** Modified the handling of ``--extra-search-dirs``.
  This option now works like pip's ``--find-links`` option, in that it adds
  extra directories to search for compatible wheels for pip and setuptools.
  The actual wheel selected is chosen based on version and compatibility, using
  the same algorithm as ``pip install setuptools``.
* Fixed #495, --always-copy was failing (#PR 511)
* Upgraded pip to v1.5
* Upgraded setuptools to v1.4

v1.10.1 (2013-08-07)
--------------------

* **New Signing Key** Release 1.10.1 is using a different key than normal with
  fingerprint: 7C6B 7C5D 5E2B 6356 A926 F04F 6E3C BCE9 3372 DCFA
* Upgraded pip to v1.4.1
* Upgraded setuptools to v0.9.8


v1.10 (2013-07-23)
------------------

* **BACKWARDS INCOMPATIBLE** Dropped support for Python 2.5. The minimum
  supported Python version is now Python 2.6.

* **BACKWARDS INCOMPATIBLE** Using ``virtualenv.py`` as an isolated script
  (i.e. without an associated ``virtualenv_support`` directory) is no longer
  supported for security reasons and will fail with an error.

  Along with this, ``--never-download`` is now always pinned to ``True``, and
  is only being maintained in the short term for backward compatibility
  (Pull #412).

* **IMPORTANT** Switched to the new setuptools (v0.9.7) which has been merged
  with Distribute_ again and works for Python 2 and 3 with one codebase.
  The ``--distribute`` and ``--setuptools`` options are now no-op.

* Updated to pip 1.4.

* Added support for PyPy3k

* Added ``--always-copy`` option to suppress use of symlinks (Pull #409)

* Added the option to use a version number with the ``-p`` option to get the
  system copy of that Python version (Windows only)

* Removed embedded ``ez_setup.py``, ``distribute_setup.py`` and
  ``distribute_from_egg.py`` files as part of switching to merged setuptools.

* Fixed ``--relocatable`` to work better on Windows.

* Fixed issue with readline on Windows.

.. _Distribute: https://pypi.org/project/distribute

v1.9.1 (2013-03-08)
-------------------

* Updated to pip 1.3.1 that fixed a major backward incompatible change of
  parsing URLs to externally hosted packages that got accidentily included
  in pip 1.3.

v1.9 (2013-03-07)
-----------------

* Unset VIRTUAL_ENV environment variable in deactivate.bat (Pull #364)
* Upgraded distribute to 0.6.34.
* Added ``--no-setuptools`` and ``--no-pip`` options (Pull #336).
* Fixed Issue #373. virtualenv-1.8.4 was failing in cygwin (Pull #382).
* Fixed Issue #378. virtualenv is now "multiarch" aware on debian/ubuntu (Pull #379).
* Fixed issue with readline module path on pypy and OSX (Pull #374).
* Made 64bit detection compatible with Python 2.5 (Pull #393).


v1.8.4 (2012-11-25)
-------------------

* Updated distribute to 0.6.31. This fixes #359 (numpy install regression) on
  UTF-8 platforms, and provides a workaround on other platforms:
  ``PYTHONIOENCODING=utf8 pip install numpy``.

* When installing virtualenv via curl, don't forget to filter out arguments
  the distribute setup script won't understand. Fixes #358.

* Added some more integration tests.

* Removed the unsupported embedded setuptools egg for Python 2.4 to reduce
  file size.

v1.8.3 (2012-11-21)
-------------------

* Fixed readline on OS X. Thanks minrk

* Updated distribute to 0.6.30 (improves our error reporting, plus new
  distribute features and fixes). Thanks Gabriel (g2p)

* Added compatibility with multiarch Python (Python 3.3 for example). Added an
  integration test. Thanks Gabriel (g2p)

* Added ability to install distribute from a user-provided egg, rather than the
  bundled sdist, for better speed. Thanks Paul Moore.

* Make the creation of lib64 symlink smarter about already-existing symlink,
  and more explicit about full paths. Fixes #334 and #330. Thanks Jeremy Orem.

* Give lib64 site-dir preference over lib on 64-bit systems, to avoid wrong
  32-bit compiles in the venv. Fixes #328. Thanks Damien Nozay.

* Fix a bug with prompt-handling in ``activate.csh`` in non-interactive csh
  shells. Fixes #332. Thanks Benjamin Root for report and patch.

* Make it possible to create a virtualenv from within a Python
  3.3. pyvenv. Thanks Chris McDonough for the report.

* Add optional --setuptools option to be able to switch to it in case
  distribute is the default (like in Debian).

v1.8.2 (2012-09-06)
-------------------

* Updated the included pip version to 1.2.1 to fix regressions introduced
  there in 1.2.


v1.8.1 (2012-09-03)
-------------------

* Fixed distribute version used with ``--never-download``. Thanks michr for
  report and patch.

* Fix creating Python 3.3 based virtualenvs by unsetting the
  ``__PYVENV_LAUNCHER__`` environment variable in subprocesses.


v1.8 (2012-09-01)
-----------------

* **Dropped support for Python 2.4** The minimum supported Python version is
  now Python 2.5.

* Fix ``--relocatable`` on systems that use lib64. Fixes #78. Thanks Branden
  Rolston.

* Symlink some additional modules under Python 3. Fixes #194. Thanks Vinay
  Sajip, Ian Clelland, and Stefan Holek for the report.

* Fix ``--relocatable`` when a script uses ``__future__`` imports. Thanks
  Branden Rolston.

* Fix a bug in the config option parser that prevented setting negative
  options with environment variables. Thanks Ralf Schmitt.

* Allow setting ``--no-site-packages`` from the config file.

* Use ``/usr/bin/multiarch-platform`` if available to figure out the include
  directory. Thanks for the patch, Mika Laitio.

* Fix ``install_name_tool`` replacement to work on Python 3.X.

* Handle paths of users' site-packages on Mac OS X correctly when changing
  the prefix.

* Updated the embedded version of distribute to 0.6.28 and pip to 1.2.


v1.7.2 (2012-06-22)
-------------------

* Updated to distribute 0.6.27.

* Fix activate.fish on OS X. Fixes #8. Thanks David Schoonover.

* Create a virtualenv-x.x script with the Python version when installing, so
  virtualenv for multiple Python versions can be installed to the same
  script location. Thanks Miki Tebeka.

* Restored ability to create a virtualenv with a path longer than 78
  characters, without breaking creation of virtualenvs with non-ASCII paths.
  Thanks, Bradley Ayers.

* Added ability to create virtualenvs without having installed Apple's
  developers tools (using an own implementation of ``install_name_tool``).
  Thanks Mike Hommey.

* Fixed PyPy and Jython support on Windows. Thanks Konstantin Zemlyak.

* Added pydoc script to ease use. Thanks Marc Abramowitz. Fixes #149.

* Fixed creating a bootstrap script on Python 3. Thanks Raul Leal. Fixes #280.

* Fixed inconsistency when having set the ``PYTHONDONTWRITEBYTECODE`` env var
  with the --distribute option or the ``VIRTUALENV_USE_DISTRIBUTE`` env var.
  ``VIRTUALENV_USE_DISTRIBUTE`` is now considered again as a legacy alias.


v1.7.1.2 (2012-02-17)
---------------------

* Fixed minor issue in ``--relocatable``. Thanks, Cap Petschulat.


v1.7.1.1 (2012-02-16)
---------------------

* Bumped the version string in ``virtualenv.py`` up, too.

* Fixed rST rendering bug of long description.


v1.7.1 (2012-02-16)
-------------------

* Update embedded pip to version 1.1.

* Fix ``--relocatable`` under Python 3. Thanks Doug Hellmann.

* Added environ PATH modification to activate_this.py. Thanks Doug
  Napoleone. Fixes #14.

* Support creating virtualenvs directly from a Python build directory on
  Windows. Thanks CBWhiz. Fixes #139.

* Use non-recursive symlinks to fix things up for posix_local install
  scheme. Thanks michr.

* Made activate script available for use with msys and cygwin on Windows.
  Thanks Greg Haskins, Cliff Xuan, Jonathan Griffin and Doug Napoleone.
  Fixes #176.

* Fixed creation of virtualenvs on Windows when Python is not installed for
  all users. Thanks Anatoly Techtonik for report and patch and Doug
  Napoleone for testing and confirmation. Fixes #87.

* Fixed creation of virtualenvs using -p in installs where some modules
  that ought to be in the standard library (e.g. ``readline``) are actually
  installed in ``site-packages`` next to ``virtualenv.py``. Thanks Greg Haskins
  for report and fix. Fixes #167.

* Added activation script for Powershell (signed by Jannis Leidel). Many
  thanks to Jason R. Coombs.


v1.7 (2011-11-30)
-----------------

* Gave user-provided ``--extra-search-dir`` priority over default dirs for
  finding setuptools/distribute (it already had priority for finding pip).
  Thanks Ethan Jucovy.

* Updated embedded Distribute release to 0.6.24. Thanks Alex Gronholm.

* Made ``--no-site-packages`` behavior the default behavior.  The
  ``--no-site-packages`` flag is still permitted, but displays a warning when
  used. Thanks Chris McDonough.

* New flag: ``--system-site-packages``; this flag should be passed to get the
  previous default global-site-package-including behavior back.

* Added ability to set command options as environment variables and options
  in a ``virtualenv.ini`` file.

* Fixed various encoding related issues with paths. Thanks Gunnlaugur Thor Briem.

* Made ``virtualenv.py`` script executable.


v1.6.4 (2011-07-21)
-------------------

* Restored ability to run on Python 2.4, too.


v1.6.3 (2011-07-16)
-------------------

* Restored ability to run on Python < 2.7.


v1.6.2 (2011-07-16)
-------------------

* Updated embedded distribute release to 0.6.19.

* Updated embedded pip release to 1.0.2.

* Fixed #141 - Be smarter about finding pkg_resources when using the
  non-default Python interpreter (by using the ``-p`` option).

* Fixed #112 - Fixed path in docs.

* Fixed #109 - Corrected doctests of a Logger method.

* Fixed #118 - Fixed creating virtualenvs on platforms that use the
  "posix_local" install scheme, such as Ubuntu with Python 2.7.

* Add missing library to Python 3 virtualenvs (``_dummy_thread``).


v1.6.1 (2011-04-30)
-------------------

* Start to use git-flow.

* Added support for PyPy 1.5

* Fixed #121 -- added sanity-checking of the -p argument. Thanks Paul Nasrat.

* Added progress meter for pip installation as well as setuptools. Thanks Ethan
  Jucovy.

* Added --never-download and --search-dir options. Thanks Ethan Jucovy.


v1.6
----

* Added Python 3 support! Huge thanks to Vinay Sajip and Vitaly Babiy.

* Fixed creation of virtualenvs on Mac OS X when standard library modules
  (readline) are installed outside the standard library.

* Updated bundled pip to 1.0.


v1.5.2
------

* Moved main repository to Github: https://github.com/pypa/virtualenv

* Transferred primary maintenance from Ian to Jannis Leidel, Carl Meyer and Brian Rosner

* Fixed a few more pypy related bugs.

* Updated bundled pip to 0.8.2.

* Handed project over to new team of maintainers.

* Moved virtualenv to Github at https://github.com/pypa/virtualenv


v1.5.1
------

* Added ``_weakrefset`` requirement for Python 2.7.1.

* Fixed Windows regression in 1.5


v1.5
----

* Include pip 0.8.1.

* Add support for PyPy.

* Uses a proper temporary dir when installing environment requirements.

* Add ``--prompt`` option to be able to override the default prompt prefix.

* Fix an issue with ``--relocatable`` on Windows.

* Fix issue with installing the wrong version of distribute.

* Add fish and csh activate scripts.


v1.4.9
------

* Include pip 0.7.2


v1.4.8
------

* Fix for Mac OS X Framework builds that use
  ``--universal-archs=intel``

* Fix ``activate_this.py`` on Windows.

* Allow ``$PYTHONHOME`` to be set, so long as you use ``source
  bin/activate`` it will get unset; if you leave it set and do not
  activate the environment it will still break the environment.

* Include pip 0.7.1


v1.4.7
------

* Include pip 0.7


v1.4.6
------

* Allow ``activate.sh`` to skip updating the prompt (by setting
  ``$VIRTUAL_ENV_DISABLE_PROMPT``).


v1.4.5
------

* Include pip 0.6.3

* Fix ``activate.bat`` and ``deactivate.bat`` under Windows when
  ``PATH`` contained a parenthesis


v1.4.4
------

* Include pip 0.6.2 and Distribute 0.6.10

* Create the ``virtualenv`` script even when Setuptools isn't
  installed

* Fix problem with ``virtualenv --relocate`` when ``bin/`` has
  subdirectories (e.g., ``bin/.svn/``); from Alan Franzoni.

* If you set ``$VIRTUALENV_DISTRIBUTE`` then virtualenv will use
  Distribute by default (so you don't have to remember to use
  ``--distribute``).


v1.4.3
------

* Include pip 0.6.1


v1.4.2
------

* Fix pip installation on Windows

* Fix use of stand-alone ``virtualenv.py`` (and boot scripts)

* Exclude ~/.local (user site-packages) from environments when using
  ``--no-site-packages``


v1.4.1
------

* Include pip 0.6


v1.4
----

* Updated setuptools to 0.6c11

* Added the --distribute option

* Fixed packaging problem of support-files


v1.3.4
------

* Virtualenv now copies the actual embedded Python binary on
  Mac OS X to fix a hang on Snow Leopard (10.6).

* Fail more gracefully on Windows when ``win32api`` is not installed.

* Fix site-packages taking precedent over Jython's ``__classpath__``
  and also specially handle the new ``__pyclasspath__`` entry in
  ``sys.path``.

* Now copies Jython's ``registry`` file to the virtualenv if it exists.

* Better find libraries when compiling extensions on Windows.

* Create ``Scripts\pythonw.exe`` on Windows.

* Added support for the Debian/Ubuntu
  ``/usr/lib/pythonX.Y/dist-packages`` directory.

* Set ``distutils.sysconfig.get_config_vars()['LIBDIR']`` (based on
  ``sys.real_prefix``) which is reported to help building on Windows.

* Make ``deactivate`` work on ksh

* Fixes for ``--python``: make it work with ``--relocatable`` and the
  symlink created to the exact Python version.


v1.3.3
------

* Use Windows newlines in ``activate.bat``, which has been reported to help
  when using non-ASCII directory names.

* Fixed compatibility with Jython 2.5b1.

* Added a function ``virtualenv.install_python`` for more fine-grained
  access to what ``virtualenv.create_environment`` does.

* Fix `a problem <https://bugs.launchpad.net/virtualenv/+bug/241581>`_
  with Windows and paths that contain spaces.

* If ``/path/to/env/.pydistutils.cfg`` exists (or
  ``/path/to/env/pydistutils.cfg`` on Windows systems) then ignore
  ``~/.pydistutils.cfg`` and use that other file instead.

* Fix ` a problem
  <https://bugs.launchpad.net/virtualenv/+bug/340050>`_ picking up
  some ``.so`` libraries in ``/usr/local``.


v1.3.2
------

* Remove the ``[install] prefix = ...`` setting from the virtualenv
  ``distutils.cfg`` -- this has been causing problems for a lot of
  people, in rather obscure ways.

* If you use a boot script it will attempt to import ``virtualenv``
  and find a pre-downloaded Setuptools egg using that.

* Added platform-specific paths, like ``/usr/lib/pythonX.Y/plat-linux2``


v1.3.1
------

* Real Python 2.6 compatibility.  Backported the Python 2.6 updates to
  ``site.py``, including `user directories
  <http://docs.python.org/dev/whatsnew/2.6.html#pep-370-per-user-site-packages-directory>`_
  (this means older versions of Python will support user directories,
  whether intended or not).

* Always set ``[install] prefix`` in ``distutils.cfg`` -- previously
  on some platforms where a system-wide ``distutils.cfg`` was present
  with a ``prefix`` setting, packages would be installed globally
  (usually in ``/usr/local/lib/pythonX.Y/site-packages``).

* Sometimes Cygwin seems to leave ``.exe`` off ``sys.executable``; a
  workaround is added.

* Fix ``--python`` option.

* Fixed handling of Jython environments that use a
  jython-complete.jar.


v1.3
----

* Update to Setuptools 0.6c9
* Added an option ``virtualenv --relocatable EXISTING_ENV``, which
  will make an existing environment "relocatable" -- the paths will
  not be absolute in scripts, ``.egg-info`` and ``.pth`` files.  This
  may assist in building environments that can be moved and copied.
  You have to run this *after* any new packages installed.
* Added ``bin/activate_this.py``, a file you can use like
  ``execfile("path_to/activate_this.py",
  dict(__file__="path_to/activate_this.py"))`` -- this will activate
  the environment in place, similar to what `the mod_wsgi example
  does <http://code.google.com/p/modwsgi/wiki/VirtualEnvironments>`_.
* For Mac framework builds of Python, the site-packages directory
  ``/Library/Python/X.Y/site-packages`` is added to ``sys.path``, from
  Andrea Rech.
* Some platform-specific modules in Macs are added to the path now
  (``plat-darwin/``, ``plat-mac/``, ``plat-mac/lib-scriptpackages``),
  from Andrea Rech.
* Fixed a small Bashism in the ``bin/activate`` shell script.
* Added ``__future__`` to the list of required modules, for Python
  2.3.  You'll still need to backport your own ``subprocess`` module.
* Fixed the ``__classpath__`` entry in Jython's ``sys.path`` taking
  precedent over virtualenv's libs.


v1.2
----

* Added a ``--python`` option to select the Python interpreter.
* Add ``warnings`` to the modules copied over, for Python 2.6 support.
* Add ``sets`` to the module copied over for Python 2.3 (though Python
  2.3 still probably doesn't work).


v1.1.1
------

* Added support for Jython 2.5.


v1.1
----

* Added support for Python 2.6.
* Fix a problem with missing ``DLLs/zlib.pyd`` on Windows.  Create
* ``bin/python`` (or ``bin/python.exe``) even when you run virtualenv
  with an interpreter named, e.g., ``python2.4``
* Fix MacPorts Python
* Added --unzip-setuptools option
* Update to Setuptools 0.6c8
* If the current directory is not writable, run ez_setup.py in ``/tmp``
* Copy or symlink over the ``include`` directory so that packages will
  more consistently compile.


v1.0
----

* Fix build on systems that use ``/usr/lib64``, distinct from
  ``/usr/lib`` (specifically CentOS x64).
* Fixed bug in ``--clear``.
* Fixed typos in ``deactivate.bat``.
* Preserve ``$PYTHONPATH`` when calling subprocesses.


v0.9.2
------

* Fix include dir copying on Windows (makes compiling possible).
* Include the main ``lib-tk`` in the path.
* Patch ``distutils.sysconfig``: ``get_python_inc`` and
  ``get_python_lib`` to point to the global locations.
* Install ``distutils.cfg`` before Setuptools, so that system
  customizations of ``distutils.cfg`` won't effect the installation.
* Add ``bin/pythonX.Y`` to the virtualenv (in addition to
  ``bin/python``).
* Fixed an issue with Mac Framework Python builds, and absolute paths
  (from Ronald Oussoren).


v0.9.1
------

* Improve ability to create a virtualenv from inside a virtualenv.
* Fix a little bug in ``bin/activate``.
* Actually get ``distutils.cfg`` to work reliably.


v0.9
----

* Added ``lib-dynload`` and ``config`` to things that need to be
  copied over in an environment.
* Copy over or symlink the ``include`` directory, so that you can
  build packages that need the C headers.
* Include a ``distutils`` package, so you can locally update
  ``distutils.cfg`` (in ``lib/pythonX.Y/distutils/distutils.cfg``).
* Better avoid downloading Setuptools, and hitting PyPI on environment
  creation.
* Fix a problem creating a ``lib64/`` directory.
* Should work on MacOSX Framework builds (the default Python
  installations on Mac).  Thanks to Ronald Oussoren.


v0.8.4
------

* Windows installs would sometimes give errors about ``sys.prefix`` that
  were inaccurate.
* Slightly prettier output.


v0.8.3
------

* Added support for Windows.


v0.8.2
------

* Give a better warning if you are on an unsupported platform (Mac
  Framework Pythons, and Windows).
* Give error about running while inside a workingenv.
* Give better error message about Python 2.3.


v0.8.1
------

Fixed packaging of the library.


v0.8
----

Initial release.  Everything is changed and new!
