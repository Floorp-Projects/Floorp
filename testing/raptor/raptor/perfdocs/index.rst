######
Raptor
######

Raptor is a performance-testing framework for running browser pageload and browser benchmark tests. Raptor is cross-browser compatible and is currently running in production on Firefox Desktop, Firefox Android GeckoView, Fenix, Reference Browser, Chromium, and Chrome.

- Contact: Dave Hunt [:davehunt]
- Source code: https://searchfox.org/mozilla-central/source/testing/raptor
- Good first bugs: https://codetribute.mozilla.org/projects/automation?project%3DRaptor

Raptor currently supports three test types: 1) page-load performance tests, 2) standard benchmark-performance tests, and 3) "scenario"-based tests, such as power, CPU, and memory-usage measurements on Android (and desktop?).

Locally, Raptor can be invoked with the following command:

::

    ./mach raptor

**We're in the process of migrating away from webextension to browsertime. Currently, raptor supports both of them, but at the end of the migration, the support for webextension will be removed.**

.. toctree::
   :titlesonly:

   browsertime
   webextension
   debugging
   contributing
   test-list
   metrics

.. contents::
   :depth: 2
   :local:

Raptor tests
************

The following documents all testing we have for Raptor.

{documentation}
