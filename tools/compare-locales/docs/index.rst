============
Localization
============

.. toctree::
   :maxdepth: 1

   glossary

The documentation here is targeted at developers, writing localizable code
for Firefox and Firefox for Android, as well as Thunderbird and SeaMonkey.

If you haven't dealt with localization in gecko code before, it's a good
idea to check the :doc:`./glossary` for what localization is, and which terms
we use for what.

Exposing strings
----------------

Localizers only handle a few file formats in well-known locations in the
source tree.

The locations are specified by TOML files. They're part of the bigger
localization ecosystem at Mozilla, and `the documentation about the
file format <http://moz-l10n-config.readthedocs.io/en/latest/fileformat.html>`_
explains how to set them up, and what the entries mean. In short, you find

.. code-block:: toml

    [[paths]]
        reference = browser/locales/en-US/**
        l10n = {l}browser/**

to add a directory for all localizations. Changes to these files are best
submitted for review by :Pike or :flod.

These configuration files are the future, and right now, we still have
support for the previous way to configuring l10n, which is described below.

The locations are commonly in directories like

    :file:`browser/`\ ``locales/en-US/``\ :file:`subdir/file.ext`

The first thing to note is that only files beneath :file:`locales/en-US` are
exposed to localizers. The second thing to note is that only a few directories
are exposed. Which directories are exposed is defined in files called
``l10n.ini``, which are at a
`few places <https://dxr.mozilla.org/mozilla-central/search?q=path%3Al10n.ini&redirect=true>`_
in the source code.

An example looks like this

.. code-block:: ini

    [general]
    depth = ../..

    [compare]
    dirs = browser
        browser/branding/official

    [includes]
    toolkit = toolkit/locales/l10n.ini

This tells the l10n infrastructure three things: Resolve the paths against the
directory two levels up, include files in :file:`browser/locales/en-US` and
:file:`browser/branding/official/locales/en-US`, and load more data from
:file:`toolkit/locales/l10n.ini`.

For projects like Thunderbird and SeaMonkey in ``comm-central``, additional
data needs to be provided when including an ``l10n.ini`` from a different
repository:

.. code-block:: ini

    [include_toolkit]
    type = hg
    mozilla = mozilla-central
    repo = https://hg.mozilla.org/
    l10n.ini = toolkit/locales/l10n.ini

This tells the l10n pieces where to find the repository, and where inside
that repository the ``l10n.ini`` file is. This is needed because for local
builds, :file:`mail/locales/l10n.ini` references
:file:`mozilla/toolkit/locales/l10n.ini`, which is where the comm-central
build setup expects toolkit to be.

Now that the directories exposed to l10n are known, we can talk about the
supported file formats.

File formats
------------

This is just a quick overview, please check the
`XUL Tutorial <https://developer.mozilla.org/docs/Mozilla/Tech/XUL/Tutorial/Localization>`_
for an in-depth tour.

The following file formats are known to the l10n tool chains:

DTD
    Used in XUL and XHTML. Also for Android native strings.
Properties
    Used from JavaScript and C++. When used from js, also comes with
    `plural support <https://developer.mozilla.org/docs/Mozilla/Localization/Localization_and_Plurals>`_.
ini
    Used by the crashreporter and updater, avoid if possible.
foo.defines
    Used during builds, for example to create :file:`install.rdf` for
    language packs.

Adding new formats involves changing various different tools, and is strongly
discouraged.

Exceptions
----------
Generally, anything that exists in ``en-US`` needs a one-to-one mapping in
all localizations. There are a few cases where that's not wanted, notably
around search settings and spell-checking dictionaries.

To enable tools to adjust to those exceptions, there's a python-coded
:py:mod:`filter.py`, implementing :py:func:`test`, with the following
signature

.. code-block:: python

    def test(mod, path, entity = None):
        if does_not_matter:
            return "ignore"
        if show_but_do_not_merge:
            return "report"
        # default behavior, localizer or build need to do something
        return "error"

For any missing file, this function is called with ``mod`` being
the *module*, and ``path`` being the relative path inside
:file:`locales/en-US`. The module is the top-level dir as referenced in
:file:`l10n.ini`.

For missing strings, the :py:data:`entity` parameter is the key of the string
in the en-US file.

l10n-merge
----------

Gecko doesn't support fallback from a localization to ``en-US`` at runtime.
Thus, the build needs to ensure that the localization as it's built into
the package has all required strings, and that the strings don't contain
errors. To ensure that, we're *merging* the localization and ``en-US``
at build time, nick-named :term:`l10n-merge`.

The process can be manually triggered via

.. code-block:: bash

    $> ./mach build merge-de

It creates another directory in the object dir, :file:`merge-dir/ab-CD`, in
which the modified files are stored. The actual repackaging process looks for
the localized files in the merge dir first, then the localized file, and then
in ``en-US``. Thus, for the ``de`` localization of
:file:`browser/locales/en-US/chrome/browser/browser.dtd`, it checks

1. :file:`$objdir/browser/locales/merge-de/browser/chrome/browser/browser.dtd`
2. :file:`$(LOCALE_BASEDIR)/de/browser/chrome/browser/browser.dtd`
3. :file:`browser/locales/en-US/chrome/browser/browser.dtd`

and will include the first of those files it finds.

l10n-merge modifies a file if it supports the particular file type, and there
are missing strings which are not filtered out, or if an existing string
shows an error. See the Checks section below for details.

Checks
------

As part of the build and other localization tool chains, we run a variety
of source-based checks. Think of them as linters.

The suite of checks is usually determined by file type, i.e., there's a
suite of checks for DTD files and one for properties files, etc. An exception
are Android-specific checks.

Android
^^^^^^^

For Android, we need to localize :file:`strings.xml`. We're doing so via DTD
files, which is mostly OK. But the strings inside the XML file have to
satisfy additional constraints about quotes etc, that are not part of XML.
There's probably some historic background on why things are the way they are.

The Android-specific checks are enabled for DTD files that are in
:file:`mobile/android/base/locales/en-US/`.

Localizations
-------------

Now that we talked in-depth about how to expose content to localizers,
where are the localizations?

We host a mercurial repository per locale and per branch. All of our
localizations can be found on https://hg.mozilla.org/l10n-central/.

You can search inside our localized files on
`Transvision <https://transvision.mozfr.org/>`_ and
https://dxr.mozilla.org/l10n-central/source/.
