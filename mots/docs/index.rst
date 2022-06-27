..
    This file was automatically generated using `mots export`.
.. note::
    To add, remove, or update module information, see the `mots documentation <https://mots.readthedocs.io/en/latest/#adding-a-module>`_.

=======
Modules
=======

Mozilla operates under a `module ownership governance system <http://www.mozilla.org/hacking/module-ownership.html>`_. A module is a discrete unit of code or activity. An owner is the person in charge of a module or sub-module. A peer is a person whom the owner has appointed to help them. A module may have multiple peers and, very occasionally, multiple owners.

The system is overseen by the owner and peers of the Module Ownership module. For the modules that make up Firefox, oversight is provided by the Firefox Technical Leadership module. Owners may add and remove peers from their modules as they wish, without reference to anyone else.

Mots config
~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Zeid Zabaneh (zeid) <https://people.mozilla.org/s?query=zeid>`__
    * - Includes
      - 
        | `mots.yaml <https://searchfox.org/mozilla-central/search?q=&path=mots.yaml>`__

Code Coverage
~~~~~~~~~~~~~
Tools for code coverage instrumentation, and coverage data parsing and management.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Marco Castelluccio (marco) <https://people.mozilla.org/s?query=marco>`__
    * - Peer(s)
      - 
        | `Calixte Denizet (calixte) <https://people.mozilla.org/s?query=calixte>`__
        | `Joel Maher (jmaher) <https://people.mozilla.org/s?query=jmaher>`__
    * - Includes
      - 
        | `tools/code-coverage/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/code-coverage/\*\*/\*>`__
        | `python/mozbuild/mozbuild/codecoverage/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=python/mozbuild/mozbuild/codecoverage/\*\*/\*>`__
        | `testing/mozharness/mozharness/mozilla/testing/codecoverage.py <https://searchfox.org/mozilla-central/search?q=&path=testing/mozharness/mozharness/mozilla/testing/codecoverage.py>`__
    * - Bugzilla Components
      - Testing :: Code Coverage

Core: Accessibility
~~~~~~~~~~~~~~~~~~~
Support for platform accessibility APIs. Accessibility APIs are used by 3rd party software like screen readers, screen magnifiers, and voice dictation software, which need information about document content and UI controls, as well as important events like changes of focus.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `James Teh (Jamie) <https://people.mozilla.org/s?query=Jamie>`__
    * - Peer(s)
      - 
        | `Eitan Isaacson (eeejay) <https://people.mozilla.org/s?query=eeejay>`__
        | `Marco Zehe (MarcoZ) <https://people.mozilla.org/s?query=MarcoZ>`__
        | `Morgan Reschenberg (morgan) <https://people.mozilla.org/s?query=morgan>`__
    * - Includes
      - 
        | `accessible/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=accessible/\*\*/\*>`__
    * - Group
      - dev-accessibility
    * - URL
      - http://www.mozilla.org/access/
    * - Bugzilla Components
      - Core::Disability Access APIs

Core: Animation
~~~~~~~~~~~~~~~
Declarative animations: CSS animations, CSS transitions, Web Animations API, and off-main thread animations.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Brian Birtles (birtles) <https://people.mozilla.org/s?query=birtles>`__
    * - Peer(s)
      - 
        | `Boris Chiou (boris) <https://people.mozilla.org/s?query=boris>`__
        | `Hiroyuki Ikezoe (hiro) <https://people.mozilla.org/s?query=hiro>`__
    * - Includes
      - 
        | `dom/animations/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/animations/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::DOM::Animation, Core::CSS Transitions and Animations

Core: Anti-Tracking
~~~~~~~~~~~~~~~~~~~
Tracking detection and content-blocking.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Tim Huang (timhuang) <https://people.mozilla.org/s?query=timhuang>`__
    * - Peer(s)
      - 
        | `Andrea Marchesini (baku) <https://people.mozilla.org/s?query=baku>`__
        | `Dimi Lee (dimi) <https://people.mozilla.org/s?query=dimi>`__
        | `Paul Zühlcke (pbz) <https://people.mozilla.org/s?query=pbz>`__
        | `Johann Hofmann (johannh) <https://people.mozilla.org/s?query=johannh>`__
    * - Includes
      - 
        | `toolkit/components/antitracking/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/antitracking/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Privacy: Anti-Tracking

Core: APZ (Graphics submodule)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Asynchronous panning and zooming


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Botond Ballo (botond) <https://people.mozilla.org/s?query=botond>`__
    * - Peer(s)
      - 
        | `Timothy Nikkel (tnikkel) <https://people.mozilla.org/s?query=tnikkel>`__
        | `Ryan Hunt (rhunt) <https://people.mozilla.org/s?query=rhunt>`__
        | `Markus Stange (mstange) <https://people.mozilla.org/s?query=mstange>`__
    * - Includes
      - 
        | `gfx/layers/apz/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=gfx/layers/apz/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://wiki.mozilla.org/Platform/GFX/APZ
    * - Bugzilla Components
      - Core::Panning and Zooming

Core: Browser WebAPI
~~~~~~~~~~~~~~~~~~~~
Web API for rendering apps, browser windows and widgets.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Olli Pettay (smaug) <https://people.mozilla.org/s?query=smaug>`__
    * - Includes
      - 
        | `dom/browser-element/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/browser-element/\*\*/\*>`__
    * - Group
      - dev-webapi
    * - Bugzilla Components
      - Core::DOM

Core: Build and Release Tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Tools related to build and release automation and configuration of release builds.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Aki Sasaki (aki) <https://people.mozilla.org/s?query=aki>`__
    * - Includes
      - 
        | `tools/update-packaging/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/update-packaging/\*\*/\*>`__
        | `tools/update-verify/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/update-verify/\*\*/\*>`__
    * - Group
      - release-engineering
    * - URL
      - https://wiki.mozilla.org/ReleaseEngineering
    * - Bugzilla Components
      - Release Engineering::*

Core: Build Config
~~~~~~~~~~~~~~~~~~
The build system for Gecko and several mozilla.org hosted Gecko-based applications.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Mike Hommey (glandium) <https://people.mozilla.org/s?query=glandium>`__
    * - Peer(s)
      - 
        | `Mitchell Hentges (mhentges) <https://people.mozilla.org/s?query=mhentges>`__
        | `Andi (andi) <https://people.mozilla.org/s?query=andi>`__
    * - Includes
      - 
        | `build/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=build/\*\*/\*>`__
        | `config/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=config/\*\*/\*>`__
        | `python/mozbuild/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=python/mozbuild/\*\*/\*>`__
        | `tools/cross-commit/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/cross-commit/\*\*/\*>`__
        | `tools/cvs2hg-import.py <https://searchfox.org/mozilla-central/search?q=&path=tools/cvs2hg-import.py>`__
        | `tools/cvsmgmt/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/cvsmgmt/\*\*/\*>`__
        | `tools/elf-dynstr-gc/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/elf-dynstr-gc/\*\*/\*>`__
        | `tools/trees.pl <https://searchfox.org/mozilla-central/search?q=&path=tools/trees.pl>`__
        | `browser/config/mozconfigs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/config/mozconfigs/\*\*/\*>`__
        | `mobile/config/mozconfigs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=mobile/config/mozconfigs/\*\*/\*>`__
        | `xulrunner/config/mozconfigs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xulrunner/config/mozconfigs/\*\*/\*>`__
    * - Group
      - dev-builds
    * - URL
      - http://www.mozilla.org/build/
    * - Bugzilla Components
      - Core::Build Config

Core: Build Config - Fennec
===========================
Submodule of the build config covering Fennec's build system in mobile/android.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Nick Alexander (nalexander) <https://people.mozilla.org/s?query=nalexander>`__
    * - Peer(s)
      - 
        | `Mitchell Hentges (mhentges) <https://people.mozilla.org/s?query=mhentges>`__
        | `Andi (andi) <https://people.mozilla.org/s?query=andi>`__
    * - Includes
      - 
        | `build/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=build/\*\*/\*>`__
        | `config/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=config/\*\*/\*>`__
        | `python/mozbuild/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=python/mozbuild/\*\*/\*>`__
        | `tools/cross-commit/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/cross-commit/\*\*/\*>`__
        | `tools/cvs2hg-import.py <https://searchfox.org/mozilla-central/search?q=&path=tools/cvs2hg-import.py>`__
        | `tools/cvsmgmt/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/cvsmgmt/\*\*/\*>`__
        | `tools/elf-dynstr-gc/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/elf-dynstr-gc/\*\*/\*>`__
        | `tools/trees.pl <https://searchfox.org/mozilla-central/search?q=&path=tools/trees.pl>`__
        | `browser/config/mozconfigs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/config/mozconfigs/\*\*/\*>`__
        | `mobile/config/mozconfigs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=mobile/config/mozconfigs/\*\*/\*>`__
        | `xulrunner/config/mozconfigs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xulrunner/config/mozconfigs/\*\*/\*>`__
    * - Group
      - dev-builds
    * - Bugzilla Components
      - Core::Build Config

Core: Build Config - Taskgraph
==============================
Support for task-graph generation in decision, action, and cron tasks, including configuration of all tasks including those for CI, nightlies, and releases; as well as Docker and VM images used to execute those tasks.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Tom Prince (tomprince) <https://people.mozilla.org/s?query=tomprince>`__
    * - Peer(s)
      - 
        | `Andrew Halberstadt (ahal) <https://people.mozilla.org/s?query=ahal>`__
        | `Aki Sasaki (aki) <https://people.mozilla.org/s?query=aki>`__
        | `Brian Stack (bstack) <https://people.mozilla.org/s?query=bstack>`__
        | `Mike Hommey (glandium) <https://people.mozilla.org/s?query=glandium>`__
        | `Gregory Szorc (gps) <https://people.mozilla.org/s?query=gps>`__
        | `Joel Maher (jmaher) <https://people.mozilla.org/s?query=jmaher>`__
        | `Justin Wood (Callek) <https://people.mozilla.org/s?query=Callek>`__
    * - Includes
      - 
        | `build/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=build/\*\*/\*>`__
        | `config/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=config/\*\*/\*>`__
        | `python/mozbuild/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=python/mozbuild/\*\*/\*>`__
        | `tools/cross-commit/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/cross-commit/\*\*/\*>`__
        | `tools/cvs2hg-import.py <https://searchfox.org/mozilla-central/search?q=&path=tools/cvs2hg-import.py>`__
        | `tools/cvsmgmt/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/cvsmgmt/\*\*/\*>`__
        | `tools/elf-dynstr-gc/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/elf-dynstr-gc/\*\*/\*>`__
        | `tools/trees.pl <https://searchfox.org/mozilla-central/search?q=&path=tools/trees.pl>`__
        | `browser/config/mozconfigs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/config/mozconfigs/\*\*/\*>`__
        | `mobile/config/mozconfigs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=mobile/config/mozconfigs/\*\*/\*>`__
        | `xulrunner/config/mozconfigs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xulrunner/config/mozconfigs/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox Build System::Task Configuration

Core: Code Analysis and Debugging Tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Tools for debugging Mozilla code or for analyzing speed, memory use, and other characteristics of it.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `David Baron (dbaron) <https://people.mozilla.org/s?query=dbaron>`__
    * - Includes
      - 
        | `tools/codesighs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/codesighs/\*\*/\*>`__
        | `tools/debug/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/debug/\*\*/\*>`__
        | `tools/dreftool/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/dreftool/\*\*/\*>`__
        | `tools/dumpdeps/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/dumpdeps/\*\*/\*>`__
        | `tools/jprof/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/jprof/\*\*/\*>`__
        | `tools/leak-gauge/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/leak-gauge/\*\*/\*>`__
        | `tools/leaky/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/leaky/\*\*/\*>`__
        | `tools/memory/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/memory/\*\*/\*>`__
        | `tools/module-deps/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/module-deps/\*\*/\*>`__
        | `tools/performance/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/performance/\*\*/\*>`__
        | `tools/post_compile/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/post_compile/\*\*/\*>`__
        | `tools/preloader/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/preloader/\*\*/\*>`__
        | `tools/rb/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/rb/\*\*/\*>`__
        | `tools/reorder/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/reorder/\*\*/\*>`__
        | `tools/trace-malloc/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/trace-malloc/\*\*/\*>`__
        | `tools/uuiddeps/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/uuiddeps/\*\*/\*>`__
    * - Group
      - dev-performance

Core: Content Security
~~~~~~~~~~~~~~~~~~~~~~
Native content-based security features enforced in the ContentSecurityManager, including: Content Security Policy (CSP), Mixed Content Blocker (MCB), Referrer Policy, Subresource Integrity (SRI), Cross-Origin Resource Sharing (CORS), X-Frame-Options, X-Content-Type-Options: nosniff, HTTPS-Only-Mode, Sanitizer API, Sec-Fetch Metadata, and top-level data: URI blocking.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `ckerschbaumer <https://people.mozilla.org/s?query=ckerschbaumer>`__
    * - Peer(s)
      - 
        | `Tanvi Vyas (tanvi) <https://people.mozilla.org/s?query=tanvi>`__
        | `Daniel Veditz (dveditz) <https://people.mozilla.org/s?query=dveditz>`__
        | `Andrea Marchesini (baku) <https://people.mozilla.org/s?query=baku>`__
        | `Frederik Braun (freddy) <https://people.mozilla.org/s?query=freddy>`__
    * - Includes
      - 
        | `dom/security/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/security/\*\*/\*>`__
    * - Group
      - dev-security
    * - Bugzilla Components
      - Core::DOM: Security

Core: Cookies
~~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Andrea Marchesini (baku) <https://people.mozilla.org/s?query=baku>`__
    * - Peer(s)
      - 
        | `Honza Bambas (mayhemer) <https://people.mozilla.org/s?query=mayhemer>`__
    * - Includes
      - 
        | `netwerk/cookie/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/cookie/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Networking: Cookies

Core: Crash reporting
~~~~~~~~~~~~~~~~~~~~~
Infrastructure and tools used to generate, submit and process crash reports. This includes the in-tree google-breakpad fork, the crash report generation machinery as well as the host tools used to dump symbols, analyse minidumps and generate stack traces.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Gabriele Svelto (gsvelto) <https://people.mozilla.org/s?query=gsvelto>`__
    * - Peer(s)
      - 
        | `Kris Wright (KrisWright) <https://people.mozilla.org/s?query=KrisWright>`__
        | `Calixte Denizet (calixte) <https://people.mozilla.org/s?query=calixte>`__
        | `Aria Beingessner (Gankra) <https://people.mozilla.org/s?query=Gankra>`__
    * - Includes
      - 
        | `toolkit/crashreporter/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/crashreporter/\*\*/\*>`__
        | `toolkit/components/crashes/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/crashes/\*\*/\*>`__
        | `tools/crashreporter/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/crashreporter/\*\*/\*>`__
        | `ipc/glue/CrashReporter/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=ipc/glue/CrashReporter/\*\*/\*>`__
        | `mobile/android/geckoview/src/main/java/org/mozilla/geckoview/CrashReporter.java <https://searchfox.org/mozilla-central/search?q=&path=mobile/android/geckoview/src/main/java/org/mozilla/geckoview/CrashReporter.java>`__
    * - Group
      - dev-platform
    * - URL
      - https://firefox-source-docs.mozilla.org/toolkit/crashreporter/crashreporter/index.html
    * - Bugzilla Components
      - Toolkit::Crash Reporting

Core: C++/Rust usage, tools, and style
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Aspects of C++ use such as language feature usage, standard library versions/usage, compiler/toolchain versions, formatting and naming style, and aspects of Rust use as needs arise


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Bobby Holley (bholley) <https://people.mozilla.org/s?query=bholley>`__
    * - Peer(s)
      - 
        | `Botond Ballo (botond) <https://people.mozilla.org/s?query=botond>`__
        | `Mike Hommey (glandium) <https://people.mozilla.org/s?query=glandium>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Various

Core: Cycle Collector
~~~~~~~~~~~~~~~~~~~~~
Code to break and collect objects within reference cycles


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Andrew McCreight (mccr8) <https://people.mozilla.org/s?query=mccr8>`__
    * - Peer(s)
      - 
        | `André Bargull (anba) <https://people.mozilla.org/s?query=anba>`__
        | `Olli Pettay (smaug) <https://people.mozilla.org/s?query=smaug>`__
    * - Includes
      - 
        | `xpcom/base/nsCycleCollector.\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/base/nsCycleCollector.\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::XPCOM

Core: DLL Services
~~~~~~~~~~~~~~~~~~
Windows dynamic linker instrumentation and blocking


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Toshihito Kikuchi (toshi) <https://people.mozilla.org/s?query=toshi>`__
    * - Peer(s)
      - 
        | `David Parks (handyman) <https://people.mozilla.org/s?query=handyman>`__
        | `Molly Howell (mhowell) <https://people.mozilla.org/s?query=mhowell>`__
    * - Includes
      - 
        | `toolkit/xre/dllservices/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/xre/dllservices/\*\*/\*>`__
    * - Bugzilla Components
      - Core::DLL Services

Core: docshell
~~~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Olli Pettay (smaug) <https://people.mozilla.org/s?query=smaug>`__
        | `Nika Layzell (nika) <https://people.mozilla.org/s?query=nika>`__
    * - Peer(s)
      - 
        | `Peter Van der Beken (peterv) <https://people.mozilla.org/s?query=peterv>`__
        | `Andreas Farre (farre) <https://people.mozilla.org/s?query=farre>`__
    * - Includes
      - 
        | `docshell/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=docshell/\*\*/\*>`__
        | `uriloader/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=uriloader/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Document Navigation

Core: Document Object Model
~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Peter Van der Beken (peterv) <https://people.mozilla.org/s?query=peterv>`__
    * - Peer(s)
      - 
        | `Olli Pettay (smaug) <https://people.mozilla.org/s?query=smaug>`__
        | `Henri Sivonen (hsivonen) <https://people.mozilla.org/s?query=hsivonen>`__
        | `Bobby Holley (bholley) <https://people.mozilla.org/s?query=bholley>`__
        | `Andrea Marchesini (baku) <https://people.mozilla.org/s?query=baku>`__
        | `Andrew McCreight (mccr8) <https://people.mozilla.org/s?query=mccr8>`__
        | `Nika Layzell (nika) <https://people.mozilla.org/s?query=nika>`__
        | `Andreas Farre (farre) <https://people.mozilla.org/s?query=farre>`__
        | `Emilio Cobos Álvarez (emilio) <https://people.mozilla.org/s?query=emilio>`__
        | `Andrew Sutherland (asuth) <https://people.mozilla.org/s?query=asuth>`__
        | `Edgar Chen (edgar) <https://people.mozilla.org/s?query=edgar>`__
    * - Includes
      - 
        | `dom/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/\*\*/\*>`__
    * - Group
      - dev-tech-dom
    * - URL
      - http://developer.mozilla.org/en/docs/DOM
    * - Bugzilla Components
      - Core::DOM, Core::DOM: CSS Object Model, Core::DOM: Core & HTML

Core: DOM File
~~~~~~~~~~~~~~
DOM Blob, File and FileSystem APIs 


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Andrea Marchesini (baku) <https://people.mozilla.org/s?query=baku>`__
    * - Peer(s)
      - 
        | `Olli Pettay (smaug) <https://people.mozilla.org/s?query=smaug>`__
    * - Includes
      - 
        | `dom/file/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/file/\*\*/\*>`__
        | `dom/filesystem/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/filesystem/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - http://developer.mozilla.org/en/docs/DOM
    * - Bugzilla Components
      - Core::DOM: File

Core: DOM Streams
~~~~~~~~~~~~~~~~~
Streams Specification implementation


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Matthew Gaudet (mgaudet) <https://people.mozilla.org/s?query=mgaudet>`__
    * - Peer(s)
      - 
        | `Olli Pettay (smaug) <https://people.mozilla.org/s?query=smaug>`__
        | `Tom Schuster (evilpie) <https://people.mozilla.org/s?query=evilpie>`__
        | `Kagami (saschanaz) <https://people.mozilla.org/s?query=saschanaz>`__
    * - Includes
      - 
        | `dom/streams/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/streams/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - http://developer.mozilla.org/en/docs/DOM
    * - Bugzilla Components
      - Core::DOM: Streams

Core: Editor
~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Masayuki Nakano (masayuki) <https://people.mozilla.org/s?query=masayuki>`__
    * - Peer(s)
      - 
        | `Makoto Kato (m_kato) <https://people.mozilla.org/s?query=m_kato>`__
    * - Includes
      - 
        | `editor/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=editor/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - http://www.mozilla.org/editor/
    * - Bugzilla Components
      - Core::Editor

Core: Event Handling
~~~~~~~~~~~~~~~~~~~~
DOM Events and Event Handling 


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Olli Pettay (smaug) <https://people.mozilla.org/s?query=smaug>`__
        | `Masayuki Nakano (masayuki) <https://people.mozilla.org/s?query=masayuki>`__
    * - Peer(s)
      - 
        | `Edgar Chen (edgar) <https://people.mozilla.org/s?query=edgar>`__
    * - Includes
      - 
        | `dom/events/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/events/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - http://developer.mozilla.org/en/docs/DOM
    * - Bugzilla Components
      - Core::DOM: Events, Core::DOM: UI Events & Focus Handling

Core: Gecko Profiler
~~~~~~~~~~~~~~~~~~~~
Gecko's built-in profiler


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Markus Stange (mstange) <https://people.mozilla.org/s?query=mstange>`__
    * - Peer(s)
      - 
        | `Gerald Squelart (gerald) <https://people.mozilla.org/s?query=gerald>`__
        | `Greg Tatum (gregtatum) <https://people.mozilla.org/s?query=gregtatum>`__
        | `Nazım Can Altınova (canova) <https://people.mozilla.org/s?query=canova>`__
        | `Julian Seward (jseward) <https://people.mozilla.org/s?query=jseward>`__
        | `Kannan Vijayan (djvj) <https://people.mozilla.org/s?query=djvj>`__
        | `Barret Rennie (barret) <https://people.mozilla.org/s?query=barret>`__
    * - Includes
      - 
        | `tools/profiler/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/profiler/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://developer.mozilla.org/en-US/docs/Mozilla/Performance/Profiling_with_the_Built-in_Profiler
    * - Bugzilla Components
      - Core::Gecko Profiler

Core: GeckoView
~~~~~~~~~~~~~~~
Framework for embedding Gecko into Android applications


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Agi Sferro (agi) <https://people.mozilla.org/s?query=agi>`__
    * - Peer(s)
      - 
        | `Makoto Kato (m_kato) <https://people.mozilla.org/s?query=m_kato>`__
        | `owlish <https://people.mozilla.org/s?query=owlish>`__
    * - Includes
      - 
        | `mobile/android/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=mobile/android/\*\*/\*>`__
        | `widget/android/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=widget/android/\*\*/\*>`__
        | `hal/android/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=hal/android/\*\*/\*>`__
    * - URL
      - https://wiki.mozilla.org/Mobile/GeckoView
    * - Bugzilla Components
      - GeckoView::General

Core: Global Key Bindings
~~~~~~~~~~~~~~~~~~~~~~~~~
Global hot keys in Mozilla for the browser, editor, mail-news and widgets. Does not include underlined menu accelerators and the like, as those are part of i18n.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Masayuki Nakano (masayuki) <https://people.mozilla.org/s?query=masayuki>`__
    * - Includes
      - 
        | `dom/events/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/events/\*\*/\*>`__
    * - Group
      - dev-accessibility
    * - URL
      - http://www.mozilla.org/projects/ui/accessibility/mozkeyintro.html
    * - Bugzilla Components
      - Core::Keyboard: Navigation

Core: Graphics
~~~~~~~~~~~~~~
Mozilla graphics API


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jeff Muizelaar (jrmuizel) <https://people.mozilla.org/s?query=jrmuizel>`__
    * - Peer(s)
      - 
        | `Nicolas Silva (nical) <https://people.mozilla.org/s?query=nical>`__
        | `Kelsey Gilbert (jgilbert) <https://people.mozilla.org/s?query=jgilbert>`__
        | `Markus Stange (mstange) <https://people.mozilla.org/s?query=mstange>`__
        | `Bas Schouten (bas.schouten) <https://people.mozilla.org/s?query=bas.schouten>`__
        | `Jonathan Kew (jfkthame) <https://people.mozilla.org/s?query=jfkthame>`__
        | `Sotaro Ikeda (sotaro) <https://people.mozilla.org/s?query=sotaro>`__
        | `Jamie Nicol (jnicol) <https://people.mozilla.org/s?query=jnicol>`__
        | `Ryan Hunt (rhunt) <https://people.mozilla.org/s?query=rhunt>`__
    * - Includes
      - 
        | `gfx/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=gfx/\*\*/\*>`__
        | `dom/canvas/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/canvas/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://wiki.mozilla.org/Platform/GFX https://wiki.mozilla.org/Gecko:Layers https://wiki.mozilla.org/Gecko:2DGraphicsSketch
    * - Bugzilla Components
      - Core::Graphics, Core::Graphics: Layers, Core::Graphics: Text, Core::Graphics: WebRender, Core::GFX: Color Management, Core::Canvas: 2D, Core::Canvas: WebGL

Core: HAL
~~~~~~~~~
Hardware Abstraction Layer


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Gabriele Svelto (gsvelto) <https://people.mozilla.org/s?query=gsvelto>`__
    * - Includes
      - 
        | `hal/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=hal/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Hardware Abstraction Layer (HAL)

Core: HTML Parser
~~~~~~~~~~~~~~~~~
The HTML Parser transforms HTML source code into a DOM. It conforms to the HTML specification, and is mostly translated automatically from Java to C++.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Henri Sivonen (hsivonen) <https://people.mozilla.org/s?query=hsivonen>`__
    * - Peer(s)
      - 
        | `William Chen (wchen) <https://people.mozilla.org/s?query=wchen>`__
    * - Includes
      - 
        | `parser/html/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=parser/html/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - http://about.validator.nu/
    * - Bugzilla Components
      - Core::HTML: Parser

Core: I18N Library
~~~~~~~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Henri Sivonen (hsivonen) <https://people.mozilla.org/s?query=hsivonen>`__
        | `Jonathan Kew (jfkthame) <https://people.mozilla.org/s?query=jfkthame>`__
    * - Peer(s)
      - 
        | `Masatoshi Kimura (emk) <https://people.mozilla.org/s?query=emk>`__
        | `Zibi Braniecki (zbraniecki) <https://people.mozilla.org/s?query=zbraniecki>`__
        | `Makoto Kato (m_kato) <https://people.mozilla.org/s?query=m_kato>`__
    * - Includes
      - 
        | `intl/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=intl/\*\*/\*>`__
    * - Group
      - dev-i18n
    * - URL
      - http://mozilla.org/projects/intl/index.html
    * - Bugzilla Components
      - Core::Internationalization

Core: ImageLib
~~~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Timothy Nikkel (tnikkel) <https://people.mozilla.org/s?query=tnikkel>`__
    * - Peer(s)
      - 
        | `Andrew Osmond (aosmond) <https://people.mozilla.org/s?query=aosmond>`__
        | `Jeff Muizelaar (jrmuizel) <https://people.mozilla.org/s?query=jrmuizel>`__
    * - Includes
      - 
        | `media/libjpeg/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libjpeg/\*\*/\*>`__
        | `media/libpng/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libpng/\*\*/\*>`__
        | `image/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=image/\*\*/\*>`__
        | `modules/zlib/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=modules/zlib/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::ImageLib

Core: IndexedDB
~~~~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jan Varga (janv) <https://people.mozilla.org/s?query=janv>`__
    * - Peer(s)
      - 
        | `Bevis Tseng (bevis) <https://people.mozilla.org/s?query=bevis>`__
        | `Andrew Sutherland (asuth) <https://people.mozilla.org/s?query=asuth>`__
        | `Andrea Marchesini (baku) <https://people.mozilla.org/s?query=baku>`__
    * - Includes
      - 
        | `dom/indexedDB/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/indexedDB/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://developer.mozilla.org/en/IndexedDB
    * - Bugzilla Components
      - Core::DOM: IndexedDB

Core: IPC
~~~~~~~~~
Native message-passing between threads and processes


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Nika Layzell (nika) <https://people.mozilla.org/s?query=nika>`__
    * - Peer(s)
      - 
        | `Jim Mathies (jimm) <https://people.mozilla.org/s?query=jimm>`__
        | `Jed Davis (jld) <https://people.mozilla.org/s?query=jld>`__
        | `Andrew McCreight (mccr8) <https://people.mozilla.org/s?query=mccr8>`__
        | `David Parks (handyman) <https://people.mozilla.org/s?query=handyman>`__
    * - Includes
      - 
        | `ipc/glue/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=ipc/glue/\*\*/\*>`__
        | `ipc/ipdl/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=ipc/ipdl/\*\*/\*>`__
        | `ipc/chromium/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=ipc/chromium/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::IPC

Core: JavaScript
~~~~~~~~~~~~~~~~
JavaScript engine (SpiderMonkey)


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jan de Mooij (jandem) <https://people.mozilla.org/s?query=jandem>`__
    * - Peer(s)
      - 
        | `André Bargull (anba) <https://people.mozilla.org/s?query=anba>`__
        | `Ted Campbell (tcampbell) <https://people.mozilla.org/s?query=tcampbell>`__
        | `Jon Coppeard (jonco) <https://people.mozilla.org/s?query=jonco>`__
        | `sfink <https://people.mozilla.org/s?query=sfink>`__
        | `Tooru Fujisawa (arai) <https://people.mozilla.org/s?query=arai>`__
        | `Tom Schuster (evilpie) <https://people.mozilla.org/s?query=evilpie>`__
        | `Kannan Vijayan (djvj) <https://people.mozilla.org/s?query=djvj>`__
        | `Jeff Walden (Waldo) <https://people.mozilla.org/s?query=Waldo>`__
        | `Yulia Startsev (yulia) <https://people.mozilla.org/s?query=yulia>`__
        | `Matthew Gaudet (mgaudet) <https://people.mozilla.org/s?query=mgaudet>`__
        | `Iain Ireland (iain) <https://people.mozilla.org/s?query=iain>`__
        | `Nicolas B (nbp) <https://people.mozilla.org/s?query=nbp>`__
        | `Caroline Cullen (caroline) <https://people.mozilla.org/s?query=caroline>`__
        | `Nick Fitzgerald (fitzgen) <https://people.mozilla.org/s?query=fitzgen>`__
        | `Luke Wagner (luke) <https://people.mozilla.org/s?query=luke>`__
        | `Dan Gohman (sunfish) <https://people.mozilla.org/s?query=sunfish>`__
        | `Bobby Holley (bholley) <https://people.mozilla.org/s?query=bholley>`__
    * - Includes
      - 
        | `js/src/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=js/src/\*\*/\*>`__
    * - Group
      - dev-tech-js-engine
    * - URL
      - http://www.mozilla.org/js/spidermonkey, http://developer.mozilla.org/en/docs/About_JavaScript
    * - Bugzilla Components
      - Core::JavaScript Engine

Core: JavaScript JIT
~~~~~~~~~~~~~~~~~~~~
JavaScript engine's JIT compilers (IonMonkey, Baseline)


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jan de Mooij (jandem) <https://people.mozilla.org/s?query=jandem>`__
    * - Peer(s)
      - 
        | `André Bargull (anba) <https://people.mozilla.org/s?query=anba>`__
        | `Benjamin Bouvier (bbouvier) <https://people.mozilla.org/s?query=bbouvier>`__
        | `Ted Campbell (tcampbell) <https://people.mozilla.org/s?query=tcampbell>`__
        | `Caroline Cullen (caroline) <https://people.mozilla.org/s?query=caroline>`__
        | `Matthew Gaudet (mgaudet) <https://people.mozilla.org/s?query=mgaudet>`__
        | `Brian Hackett (bhackett1024) <https://people.mozilla.org/s?query=bhackett1024>`__
        | `Iain Ireland (iain) <https://people.mozilla.org/s?query=iain>`__
        | `Nicolas B (nbp) <https://people.mozilla.org/s?query=nbp>`__
        | `Tom Schuster (evilpie) <https://people.mozilla.org/s?query=evilpie>`__
        | `Sean Stangl (sstangl) <https://people.mozilla.org/s?query=sstangl>`__
        | `Kannan Vijayan (djvj) <https://people.mozilla.org/s?query=djvj>`__
        | `Luke Wagner (luke) <https://people.mozilla.org/s?query=luke>`__
    * - Includes
      - 
        | `js/src/jit/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=js/src/jit/\*\*/\*>`__
    * - Group
      - dev-tech-js-engine-internals
    * - URL
      - http://www.mozilla.org/js/spidermonkey
    * - Bugzilla Components
      - Core::JavaScript Engine: JIT

Core: jsat
~~~~~~~~~~
Javascript screen reader that is used in Android and B2G


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Eitan Isaacson (eeejay) <https://people.mozilla.org/s?query=eeejay>`__
    * - Peer(s)
      - 
        | `Yura Zenevich (yzen) <https://people.mozilla.org/s?query=yzen>`__
    * - Includes
      - 
        | `accessible/jsat/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=accessible/jsat/\*\*/\*>`__
    * - Group
      - dev-accessibility
    * - URL
      - http://www.mozilla.org/access/
    * - Bugzilla Components
      - Core::Disability Access APIs

Core: js-tests
~~~~~~~~~~~~~~
JavaScript test suite


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Bob Clary (bc) <https://people.mozilla.org/s?query=bc>`__
    * - Includes
      - 
        | `js/tests/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=js/tests/\*\*/\*>`__
    * - Group
      - dev-tech-js-engine
    * - URL
      - http://www.mozilla.org/js/tests/library.html

Core: Layout Engine
~~~~~~~~~~~~~~~~~~~
rendering tree construction, layout (reflow), etc.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Daniel Holbert (dholbert) <https://people.mozilla.org/s?query=dholbert>`__
    * - Peer(s)
      - 
        | `Robert O (roc) <https://people.mozilla.org/s?query=roc>`__
        | `Jonathan Kew (jfkthame) <https://people.mozilla.org/s?query=jfkthame>`__
        | `Timothy Nikkel (tnikkel) <https://people.mozilla.org/s?query=tnikkel>`__
        | `Xidorn Quan (xidorn) <https://people.mozilla.org/s?query=xidorn>`__
        | `Emilio Cobos Álvarez (emilio) <https://people.mozilla.org/s?query=emilio>`__
        | `Mats Palmgren (MatsPalmgren_bugz) <https://people.mozilla.org/s?query=MatsPalmgren_bugz>`__
        | `Ting-Yu Lin (TYLin) <https://people.mozilla.org/s?query=TYLin>`__
        | `Jonathan Watt (jwatt) <https://people.mozilla.org/s?query=jwatt>`__
    * - Includes
      - 
        | `layout/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/\*\*/\*>`__
        | `layout/base/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/base/\*\*/\*>`__
        | `layout/build/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/build/\*\*/\*>`__
        | `layout/doc/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/doc/\*\*/\*>`__
        | `layout/forms/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/forms/\*\*/\*>`__
        | `layout/generic/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/generic/\*\*/\*>`__
        | `layout/html/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/html/\*\*/\*>`__
        | `layout/printing/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/printing/\*\*/\*>`__
        | `layout/tables/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/tables/\*\*/\*>`__
        | `layout/tools/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/tools/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://wiki.mozilla.org/Gecko:Overview#Layout
    * - Bugzilla Components
      - Core::Layout, Core::Layout: Block and Inline, Core::Layout: Columns, Core::Layout: Flexbox, Core::Layout: Floats, Core::Layout: Form Controls, Core::Layout: Generated Content, Lists, and Counters, Core::Layout: Grid, Core::Layout: Images, Video, and HTML Frames, Core::Layout: Positioned, Core::Layout: Ruby, Core::Layout: Scrolling and Overflow, Core::Layout: Tables, Core::Layout: Text and Fonts, Core::Print Preview, Core::Printing: Output

Core: Legacy HTML Parser
~~~~~~~~~~~~~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Blake Kaplan (mrbkap) <https://people.mozilla.org/s?query=mrbkap>`__
    * - Peer(s)
      - 
        | `David Baron (dbaron) <https://people.mozilla.org/s?query=dbaron>`__
        | `Peter Van der Beken (peterv) <https://people.mozilla.org/s?query=peterv>`__
        | `rbs <https://people.mozilla.org/s?query=rbs>`__
    * - Includes
      - 
        | `parser/htmlparser/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=parser/htmlparser/\*\*/\*>`__
    * - URL
      - http://www.mozilla.org/newlayout/doc/parser.html
    * - Bugzilla Components
      - Core::HTML: Parser

Core: libjar
~~~~~~~~~~~~
The JAR handling code (protocol handler, stream implementation, and zipreader/zipwriter).


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `vgosu <https://people.mozilla.org/s?query=vgosu>`__
    * - Peer(s)
      - 
        | `Kershaw Chang (kershaw) <https://people.mozilla.org/s?query=kershaw>`__
    * - Includes
      - 
        | `modules/libjar/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=modules/libjar/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Networking: JAR

Core: MathML
~~~~~~~~~~~~
MathML is a low-level specification for describing mathematics which provides a foundation for the inclusion of mathematical expressions in Web pages.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Karl Tomlinson (karlt) <https://people.mozilla.org/s?query=karlt>`__
    * - Peer(s)
      - 
        | `Robert O (roc) <https://people.mozilla.org/s?query=roc>`__
    * - Includes
      - 
        | `layout/mathml/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/mathml/\*\*/\*>`__
    * - Group
      - dev-tech-mathml
    * - URL
      - http://www.mozilla.org/projects/mathml/
    * - Bugzilla Components
      - Core::MathML

Core: Media Playback
~~~~~~~~~~~~~~~~~~~~
HTML Media APIs, including Media Source Extensions and non-MSE video/audio element playback, and Encrypted Media Extensions. (WebRTC and WebAudio not included).


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jean-Yves Avenard (jya) <https://people.mozilla.org/s?query=jya>`__
    * - Peer(s)
      - 
        | `Matthew Gregan (kinetik) <https://people.mozilla.org/s?query=kinetik>`__
        | `Bryce Seager van Dyk (bryce) <https://people.mozilla.org/s?query=bryce>`__
        | `John Lin (jhlin) <https://people.mozilla.org/s?query=jhlin>`__
        | `Alastor Wu (alwu) <https://people.mozilla.org/s?query=alwu>`__
    * - Includes
      - 
        | `dom/media/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/media/\*\*/\*>`__
        | `media/gmp-clearkey/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/gmp-clearkey/\*\*/\*>`__
        | `media/libcubeb/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libcubeb/\*\*/\*>`__
        | `media/libnestegg/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libnestegg/\*\*/\*>`__
        | `media/libogg/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libogg/\*\*/\*>`__
        | `media/libopus/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libopus/\*\*/\*>`__
        | `media/libstagefright/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libstagefright/\*\*/\*>`__
        | `media/libtheora/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libtheora/\*\*/\*>`__
        | `media/libtremor/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libtremor/\*\*/\*>`__
        | `media/libvorbis/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libvorbis/\*\*/\*>`__
        | `media/libvpx/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/libvpx/\*\*/\*>`__
        | `media/omx-plugin/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/omx-plugin/\*\*/\*>`__
        | `media/rlz/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/rlz/\*\*/\*>`__
    * - Group
      - dev-media
    * - Bugzilla Components
      - Core::Audio/Video

Core: Media Transport
~~~~~~~~~~~~~~~~~~~~~
Pluggable transport for real-time media


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Eric Rescorla (ekr) <https://people.mozilla.org/s?query=ekr>`__
    * - Peer(s)
      - 
        | `Byron Campen (bwc) <https://people.mozilla.org/s?query=bwc>`__
        | `Adam Roach (abr) <https://people.mozilla.org/s?query=abr>`__
        | `nohlmeier <https://people.mozilla.org/s?query=nohlmeier>`__
    * - Includes
      - 
        | `media/mtransport/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/mtransport/\*\*/\*>`__
    * - Group
      - dev-media
    * - Bugzilla Components
      - Core::WebRTC::Networking

Core: Memory Allocator
~~~~~~~~~~~~~~~~~~~~~~
Most things related to memory allocation in Gecko, including jemalloc, replace-malloc, DMD (dark matter detector), logalloc, etc.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Mike Hommey (glandium) <https://people.mozilla.org/s?query=glandium>`__
    * - Includes
      - 
        | `memory/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=memory/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::DMD, Core::jemalloc

Core: mfbt
~~~~~~~~~~
mfbt is a collection of headers, macros, data structures, methods, and other functionality available for use and reuse throughout all Mozilla code (including SpiderMonkey and Gecko more broadly).


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jeff Walden (Waldo) <https://people.mozilla.org/s?query=Waldo>`__
    * - Peer(s)
      - 
        | `Ms2ger <https://people.mozilla.org/s?query=Ms2ger>`__
        | `Mike Hommey (glandium) <https://people.mozilla.org/s?query=glandium>`__
    * - Includes
      - 
        | `mfbt/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=mfbt/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::MFBT

Core: Moz2D (Graphics submodule)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Platform independent 2D graphics API


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Bas Schouten (bas.schouten) <https://people.mozilla.org/s?query=bas.schouten>`__
    * - Peer(s)
      - 
        | `Jeff Muizelaar (jrmuizel) <https://people.mozilla.org/s?query=jrmuizel>`__
        | `Jonathan Watt (jwatt) <https://people.mozilla.org/s?query=jwatt>`__
    * - Includes
      - 
        | `gfx/2d/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=gfx/2d/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://wiki.mozilla.org/Platform/GFX/Moz2D
    * - Bugzilla Components
      - Core::Graphics

Core: Mozglue
~~~~~~~~~~~~~
Glue library containing various low-level functionality, including a dynamic linker for Android, a DLL block list for Windows, etc.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Mike Hommey (glandium) <https://people.mozilla.org/s?query=glandium>`__
    * - Peer(s)
      - 
        | `jchen <https://people.mozilla.org/s?query=jchen>`__
    * - Includes
      - 
        | `mozglue/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=mozglue/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::mozglue

Core: mozilla-toplevel
~~~~~~~~~~~~~~~~~~~~~~
The top level directory for the mozilla tree.

.. warning::
    This module does not have any owners specified.

.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Includes
      - 
        | `README <https://searchfox.org/mozilla-central/search?q=&path=README>`__

Core: MSCOM
~~~~~~~~~~~
Integration with Microsoft Distributed COM


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `James Teh (Jamie) <https://people.mozilla.org/s?query=Jamie>`__
    * - Peer(s)
      - 
        | `Jim Mathies (jimm) <https://people.mozilla.org/s?query=jimm>`__
    * - Includes
      - 
        | `ipc/mscom/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=ipc/mscom/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::IPC: MSCOM

Core: Necko
~~~~~~~~~~~
The Mozilla Networking Library


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Dragana Damjanovic (dragana) <https://people.mozilla.org/s?query=dragana>`__
    * - Peer(s)
      - 
        | `Honza Bambas (mayhemer) <https://people.mozilla.org/s?query=mayhemer>`__
        | `Michal Novotny (michal) <https://people.mozilla.org/s?query=michal>`__
        | `Valentin Gosu (valentin) <https://people.mozilla.org/s?query=valentin>`__
        | `Kershaw Chang (kershaw) <https://people.mozilla.org/s?query=kershaw>`__
    * - Includes
      - 
        | `netwerk/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/\*\*/\*>`__
        | `netwerk/base/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/base/\*\*/\*>`__
        | `netwerk/build/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/build/\*\*/\*>`__
        | `netwerk/cache/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/cache/\*\*/\*>`__
        | `netwerk/dns/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/dns/\*\*/\*>`__
        | `netwerk/locales/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/locales/\*\*/\*>`__
        | `netwerk/mime/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/mime/\*\*/\*>`__
        | `netwerk/protocol/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/protocol/\*\*/\*>`__
        | `netwerk/resources/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/resources/\*\*/\*>`__
        | `netwerk/socket/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/socket/\*\*/\*>`__
        | `netwerk/streamconv/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/streamconv/\*\*/\*>`__
        | `netwerk/system/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/system/\*\*/\*>`__
        | `netwerk/test/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/test/\*\*/\*>`__
        | `netwerk/testserver/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/testserver/\*\*/\*>`__
    * - Group
      - dev-tech-network
    * - URL
      - http://www.mozilla.org/projects/netlib/, https://developer.mozilla.org/en/Necko
    * - Bugzilla Components
      - Core::Networking, Core::Networking: Cache, Core::Networking: Cookies, Core::Networking: FTP, Core::Networking: File, Core::Networking: HTTP, Core::Networking: JAR, Core::Networking: Websockets

Core: NodeJS usage, tools, and style
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Advises on the use of NodeJS and npm packages at build and runtime. Reviews additions/upgrades/removals of vendored npm packages. Works with appropriate teams to maintain automated license and security audits of npm packages. Works with the security team and relevant developers to respond to vulnerabilities in NodeJS and vendored npm packages.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Dan Mosedale (dmosedale) <https://people.mozilla.org/s?query=dmosedale>`__
    * - Peer(s)
      - 
        | `Mark Banner (standard8) <https://people.mozilla.org/s?query=standard8>`__
        | `Danny Coates (dcoates) <https://people.mozilla.org/s?query=dcoates>`__
        | `Kate Hudson (k88hudson) <https://people.mozilla.org/s?query=k88hudson>`__
        | `Ed Lee (ed) <https://people.mozilla.org/s?query=ed>`__
        | `Dave Townsend (mossop) <https://people.mozilla.org/s?query=mossop>`__
    * - Includes
      - 
        | `package.json <https://searchfox.org/mozilla-central/search?q=&path=package.json>`__
        | `package-lock.json <https://searchfox.org/mozilla-central/search?q=&path=package-lock.json>`__
        | `node_modules/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=node_modules/\*\*/\*>`__
    * - URL
      - [https://wiki.mozilla.org/Firefox/firefox-dev firefox-dev], #nodejs on slack
    * - Bugzilla Components
      - Various

Core: NSPR
~~~~~~~~~~
Netscape Portable Runtime


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Kai Engert (KaiE) <https://people.mozilla.org/s?query=KaiE>`__
    * - Peer(s)
      - 
        | `Mike Hommey (glandium) <https://people.mozilla.org/s?query=glandium>`__
    * - Includes
      - 
        | `nsprpub/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=nsprpub/\*\*/\*>`__
    * - Group
      - dev-tech-nspr
    * - URL
      - http://www.mozilla.org/projects/nspr/, http://www.mozilla.org/projects/nspr/reference/html/, http://www.mozilla.org/projects/nspr/release-notes/
    * - Bugzilla Components
      - NSPR

Core: PDF
~~~~~~~~~
Rendering code to display documents encoded in the ISO 32000-1 \`PDF' format.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Calixte Denizet (calixte) <https://people.mozilla.org/s?query=calixte>`__
    * - Peer(s)
      - 
        | `Marco Castelluccio (marco) <https://people.mozilla.org/s?query=marco>`__
    * - Includes
      - 
        | `toolkit/components/pdfjs/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/pdfjs/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://github.com/mozilla/pdf.js
    * - Bugzilla Components
      - Core::PDF

Core: Permissions
~~~~~~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Tim Huang (timhuang) <https://people.mozilla.org/s?query=timhuang>`__
    * - Peer(s)
      - 
        | `Andrea Marchesini (baku) <https://people.mozilla.org/s?query=baku>`__
        | `Johann Hofmann (johannh) <https://people.mozilla.org/s?query=johannh>`__
        | `Paul Zühlcke (pbz) <https://people.mozilla.org/s?query=pbz>`__
    * - Includes
      - 
        | `extensions/permissions/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=extensions/permissions/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core :: Permission Manager

Core: Plugins
~~~~~~~~~~~~~
 NPAPI Plugin support.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jim Mathies (jimm) <https://people.mozilla.org/s?query=jimm>`__
    * - Peer(s)
      - 
        | `David Parks (handyman) <https://people.mozilla.org/s?query=handyman>`__
    * - Includes
      - 
        | `dom/plugins/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/plugins/\*\*/\*>`__
        | `modules/plugin/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=modules/plugin/\*\*/\*>`__
    * - URL
      - https://wiki.mozilla.org/Plugins
    * - Bugzilla Components
      - Core::Plug-ins

Core: Preferences
~~~~~~~~~~~~~~~~~
Preference library


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Kris Wright (KrisWright) <https://people.mozilla.org/s?query=KrisWright>`__
    * - Peer(s)
      - 
        | `Mike Hommey (glandium) <https://people.mozilla.org/s?query=glandium>`__
        | `Kris Wright (KrisWright) <https://people.mozilla.org/s?query=KrisWright>`__
    * - Includes
      - 
        | `modules/libpref/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=modules/libpref/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Preferences: Backend

Core: Private Browsing
~~~~~~~~~~~~~~~~~~~~~~
Implementation of the Private Browsing mode, and the integration of other modules with Private Browsing APIs.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Tim Huang (timhuang) <https://people.mozilla.org/s?query=timhuang>`__
    * - Peer(s)
      - 
        | `Tim Huang (timhuang) <https://people.mozilla.org/s?query=timhuang>`__
    * - Group
      - dev-platform
    * - URL
      - https://wiki.mozilla.org/Private_Browsing
    * - Bugzilla Components
      - Firefox::Private Browsing

Core: Privilege Manager
~~~~~~~~~~~~~~~~~~~~~~~
"caps"


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Bobby Holley (bholley) <https://people.mozilla.org/s?query=bholley>`__
    * - Peer(s)
      - 
        | `Boris Zbarsky (bzbarsky) <https://people.mozilla.org/s?query=bzbarsky>`__
        | `ckerschbaumer <https://people.mozilla.org/s?query=ckerschbaumer>`__
    * - Includes
      - 
        | `caps/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=caps/\*\*/\*>`__
    * - Group
      - dev-tech-dom
    * - URL
      - http://www.mozilla.org/projects/security/components/index.html
    * - Bugzilla Components
      - Core::Security: CAPS

Core: Push Notifications
~~~~~~~~~~~~~~~~~~~~~~~~
Push is a way for application developers to send messages to their web applications.

.. warning::
    This module does not have any owners specified.

.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Peer(s)
      - 
        | `Martin Thomson (mt) <https://people.mozilla.org/s?query=mt>`__
        | `Dragana Damjanovic (dragana) <https://people.mozilla.org/s?query=dragana>`__
    * - Includes
      - 
        | `dom/push/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/push/\*\*/\*>`__
    * - Bugzilla Components
      - Core::DOM: Push Notifications

Core: Sandboxing (Linux)
~~~~~~~~~~~~~~~~~~~~~~~~
Sandboxing for the Linux platform


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jed Davis (jld) <https://people.mozilla.org/s?query=jld>`__
    * - Peer(s)
      - 
        | `Gian-Carlo Pascutto (gcp) <https://people.mozilla.org/s?query=gcp>`__
    * - Includes
      - 
        | `security/sandbox/linux/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=security/sandbox/linux/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://wiki.mozilla.org/Security/Sandbox 
    * - Bugzilla Components
      - Core::Security: Process Sandboxing

Core: Sandboxing (OSX)
~~~~~~~~~~~~~~~~~~~~~~
Sandboxing for the OSX platform 


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Haik Aftandilian (haik) <https://people.mozilla.org/s?query=haik>`__
    * - Includes
      - 
        | `security/sandbox/mac/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=security/sandbox/mac/\*\*/\*>`__
    * - Group
      - dev-platform 
    * - URL
      - https://wiki.mozilla.org/Security/Sandbox 
    * - Bugzilla Components
      - Core::Security: Process Sandboxing

Core: Sandboxing (Windows)
~~~~~~~~~~~~~~~~~~~~~~~~~~
Sandboxing for the Windows platform 


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Bob Owen (bobowen) <https://people.mozilla.org/s?query=bobowen>`__
    * - Peer(s)
      - 
        | `David Parks (handyman) <https://people.mozilla.org/s?query=handyman>`__
        | `Jim Mathies (jimm) <https://people.mozilla.org/s?query=jimm>`__
        | `Toshihito Kikuchi (toshi) <https://people.mozilla.org/s?query=toshi>`__
    * - Includes
      - 
        | `security/sandbox/win/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=security/sandbox/win/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://wiki.mozilla.org/Security/Sandbox 
    * - Bugzilla Components
      - Core::Security: Process Sandboxing

Core: security
~~~~~~~~~~~~~~
Crypto/PKI code, including NSS (Network Security Services) and JSS (NSS for Java)


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Benjamin Beurdouche (beurdouche) <https://people.mozilla.org/s?query=beurdouche>`__
        | `Robert Relyea (rrelyea) <https://people.mozilla.org/s?query=rrelyea>`__
        | `Martin Thomson (mt) <https://people.mozilla.org/s?query=mt>`__
    * - Peer(s)
      - 
        | `Kai Engert (KaiE) <https://people.mozilla.org/s?query=KaiE>`__
        | `Ryan Sleevi (ryan.sleevi) <https://people.mozilla.org/s?query=ryan.sleevi>`__
        | `Eric Rescorla (ekr) <https://people.mozilla.org/s?query=ekr>`__
        | `Daiki Ueno (ueno) <https://people.mozilla.org/s?query=ueno>`__
        | `nkulatova <https://people.mozilla.org/s?query=nkulatova>`__
        | `Dennis Jackson (djackson) <https://people.mozilla.org/s?query=djackson>`__
        | `John Schanck (jschanck) <https://people.mozilla.org/s?query=jschanck>`__
    * - Includes
      - 
        | `dbm/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dbm/\*\*/\*>`__
        | `security/coreconf/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=security/coreconf/\*\*/\*>`__
        | `security/dbm/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=security/dbm/\*\*/\*>`__
        | `security/jss/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=security/jss/\*\*/\*>`__
        | `security/nss/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=security/nss/\*\*/\*>`__
        | `security/tinderbox/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=security/tinderbox/\*\*/\*>`__
        | `security/tinderlight/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=security/tinderlight/\*\*/\*>`__
    * - Group
      - dev-tech-crypto
    * - URL
      - http://mozilla.org/projects/security/pki/
    * - Bugzilla Components
      - NSS, JSS, Core::Security, Core::Security: S/MIME

Core: Security - Mozilla PSM Glue
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Personal Security Manager


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Dana Keeler (keeler) <https://people.mozilla.org/s?query=keeler>`__
    * - Peer(s)
      - 
        | `John Schanck (jschanck) <https://people.mozilla.org/s?query=jschanck>`__
    * - Includes
      - 
        | `security/manager/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=security/manager/\*\*/\*>`__
    * - Group
      - dev-tech-crypto
    * - Bugzilla Components
      - Core::Security: PSM

Security - RLBox
~~~~~~~~~~~~~~~~
Sandboxing using WASM/RLBox libraries.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Shravan Narayan (shravanrn) <https://people.mozilla.org/s?query=shravanrn>`__
    * - Peer(s)
      - 
        | `Mike Hommey (glandium) <https://people.mozilla.org/s?query=glandium>`__
        | `Tom Ritter (tjr) <https://people.mozilla.org/s?query=tjr>`__
        | `Deian Stefan (deian) <https://people.mozilla.org/s?query=deian>`__
    * - Includes
      - 
        | `security/rlbox <https://searchfox.org/mozilla-central/search?q=&path=security/rlbox>`__
        | `third_party/rlbox <https://searchfox.org/mozilla-central/search?q=&path=third_party/rlbox>`__
        | `third_party/rlbox_wasm2c_sandbox <https://searchfox.org/mozilla-central/search?q=&path=third_party/rlbox_wasm2c_sandbox>`__
    * - Bugzilla Components
      - ordereddict([('Core::Security', 'RLBox')])

Core: Static analysis & rewriting for C++
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Tools for checking C++ code looking for problems at compile time, plus tools for automated rewriting of C++ code.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Andi (andi) <https://people.mozilla.org/s?query=andi>`__
    * - Peer(s)
      - 
        | `Nika Layzell (nika) <https://people.mozilla.org/s?query=nika>`__
        | `sfink <https://people.mozilla.org/s?query=sfink>`__
        | `Jeff Muizelaar (jrmuizel) <https://people.mozilla.org/s?query=jrmuizel>`__
    * - Includes
      - 
        | `build/clang-plugin/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=build/clang-plugin/\*\*/\*>`__
        | `tools/rewriting/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/rewriting/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Rewriting & Analysis

Core: storage
~~~~~~~~~~~~~
Storage APIs with a SQLite backend


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Marco Bonardo (mak) <https://people.mozilla.org/s?query=mak>`__
    * - Peer(s)
      - 
        | `Andrew Sutherland (asuth) <https://people.mozilla.org/s?query=asuth>`__
        | `Jan Varga (janv) <https://people.mozilla.org/s?query=janv>`__
    * - Includes
      - 
        | `db/sqlite3/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=db/sqlite3/\*\*/\*>`__
        | `storage/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=storage/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - http://developer.mozilla.org/en/docs/Storage
    * - Bugzilla Components
      - Toolkit::Storage, Core::SQL

Core: String
~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `David Baron (dbaron) <https://people.mozilla.org/s?query=dbaron>`__
    * - Peer(s)
      - 
        | `Eric Rahm (erahm) <https://people.mozilla.org/s?query=erahm>`__
    * - Includes
      - 
        | `string/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=string/\*\*/\*>`__
        | `xpcom/string/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/string/\*\*/\*>`__
    * - Group
      - dev-tech-xpcom
    * - URL
      - https://developer.mozilla.org/En/Mozilla_internal_string_guide
    * - Bugzilla Components
      - Core::String

Core: Style System
~~~~~~~~~~~~~~~~~~
CSS style sheet handling; style data computation


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Emilio Cobos Álvarez (emilio) <https://people.mozilla.org/s?query=emilio>`__
    * - Peer(s)
      - 
        | `Bobby Holley (bholley) <https://people.mozilla.org/s?query=bholley>`__
        | `Xidorn Quan (xidorn) <https://people.mozilla.org/s?query=xidorn>`__
    * - Includes
      - 
        | `layout/style/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/style/\*\*/\*>`__
        | `servo/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=servo/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://wiki.mozilla.org/Gecko:Overview#Style_System
    * - Bugzilla Components
      - Core::CSS Parsing and Computation

Core: SVG
~~~~~~~~~
Scalable Vector Graphics


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jonathan Watt (jwatt) <https://people.mozilla.org/s?query=jwatt>`__
    * - Peer(s)
      - 
        | `Robert Longson (longsonr) <https://people.mozilla.org/s?query=longsonr>`__
        | `Robert O (roc) <https://people.mozilla.org/s?query=roc>`__
        | `Daniel Holbert (dholbert) <https://people.mozilla.org/s?query=dholbert>`__
        | `Brian Birtles (birtles) <https://people.mozilla.org/s?query=birtles>`__
    * - Includes
      - 
        | `dom/svg/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/svg/\*\*/\*>`__
        | `layout/svg/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/svg/\*\*/\*>`__
        | `dom/smil/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/smil/\*\*/\*>`__
    * - Group
      - dev-tech-svg
    * - URL
      - https://developer.mozilla.org/en-US/docs/Web/SVG
    * - Bugzilla Components
      - Core::SVG

Core: UA String
~~~~~~~~~~~~~~~
User Agent String


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Tantek Çelik (tantek) <https://people.mozilla.org/s?query=tantek>`__
    * - Peer(s)
      - 
        | `Chris Peterson (cpeterson) <https://people.mozilla.org/s?query=cpeterson>`__
        | `Henri Sivonen (hsivonen) <https://people.mozilla.org/s?query=hsivonen>`__
    * - Includes
      - 
        | `netwerk/protocol/http/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/protocol/http/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/User-Agent/Firefox
    * - Bugzilla Components
      - Core::Networking: HTTP

Core: View System
~~~~~~~~~~~~~~~~~
The View Manager is responsible for handling "heavyweight" rendering (some clipping, compositing) and event handling tasks.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Markus Stange (mstange) <https://people.mozilla.org/s?query=mstange>`__
    * - Peer(s)
      - 
        | `Boris Zbarsky (bzbarsky) <https://people.mozilla.org/s?query=bzbarsky>`__
        | `David Baron (dbaron) <https://people.mozilla.org/s?query=dbaron>`__
    * - Includes
      - 
        | `view/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=view/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Layout: View Rendering

Core: Web Audio
~~~~~~~~~~~~~~~
Support for the W3C Web Audio API specification.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Paul Adenot (padenot) <https://people.mozilla.org/s?query=padenot>`__
    * - Peer(s)
      - 
        | `Robert O (roc) <https://people.mozilla.org/s?query=roc>`__
        | `Karl Tomlinson (karlt) <https://people.mozilla.org/s?query=karlt>`__
    * - Includes
      - 
        | `dom/media/webaudio/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/media/webaudio/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://wiki.mozilla.org/Web_Audio_API
    * - Bugzilla Components
      - Core::Web Audio

Core: Web Painting
~~~~~~~~~~~~~~~~~~
painting, display lists, and layer construction

.. warning::
    This module does not have any owners specified.

.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Peer(s)
      - 
        | `Robert O (roc) <https://people.mozilla.org/s?query=roc>`__
        | `David Baron (dbaron) <https://people.mozilla.org/s?query=dbaron>`__
        | `Timothy Nikkel (tnikkel) <https://people.mozilla.org/s?query=tnikkel>`__
        | `Markus Stange (mstange) <https://people.mozilla.org/s?query=mstange>`__
        | `Miko Mynttinen (miko) <https://people.mozilla.org/s?query=miko>`__
        | `Jamie Nicol (jnicol) <https://people.mozilla.org/s?query=jnicol>`__
    * - Includes
      - 
        | `layout/painting/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/painting/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - http://mozilla.org/newlayout/doc/, http://lxr.mozilla.org/mozilla/source/layout/doc/
    * - Bugzilla Components
      - Core::Layout: Web Painting

Core: Web Workers
~~~~~~~~~~~~~~~~~


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Andrew Sutherland (asuth) <https://people.mozilla.org/s?query=asuth>`__
    * - Peer(s)
      - 
        | `Andrea Marchesini (baku) <https://people.mozilla.org/s?query=baku>`__
        | `Yaron Tausky (ytausky) <https://people.mozilla.org/s?query=ytausky>`__
    * - Includes
      - 
        | `dom/workers/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/workers/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://developer.mozilla.org/En/Using_web_workers
    * - Bugzilla Components
      - Core::DOM: Workers

Core: WebGPU (Graphics submodule)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
WebGPU implementation


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Dzmitry Malyshau (kvark) <https://people.mozilla.org/s?query=kvark>`__
    * - Peer(s)
      - 
        | `Josh Groves (josh) <https://people.mozilla.org/s?query=josh>`__
        | `Kelsey Gilbert (jgilbert) <https://people.mozilla.org/s?query=jgilbert>`__
    * - Includes
      - 
        | `dom/webgpu/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/webgpu/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://wiki.mozilla.org/Platform/GFX/WebGPU
    * - Bugzilla Components
      - Core::Graphics::WebGPU

Core: WebRTC
~~~~~~~~~~~~
WebRTC is responsible for realtime audio and video communication, as well as related issues like low-level camera and microphone access


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Randell Jesup (jesup) <https://people.mozilla.org/s?query=jesup>`__
    * - Peer(s)
      - 
        | `Eric Rescorla (ekr) <https://people.mozilla.org/s?query=ekr>`__
        | `Adam Roach (abr) <https://people.mozilla.org/s?query=abr>`__
        | `Byron Campen (bwc) <https://people.mozilla.org/s?query=bwc>`__
    * - Includes
      - 
        | `netwerk/sctp/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=netwerk/sctp/\*\*/\*>`__
    * - Group
      - dev-media
    * - URL
      - https://wiki.mozilla.org/Media/webrtc
    * - Bugzilla Components
      - Core::WebRTC, Core::WebRTC Networking

Core: WebVR
~~~~~~~~~~~
Gecko's implementation of WebVR (Virtual Reality) functionality, including API, devices, graphics and integration


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `kip <https://people.mozilla.org/s?query=kip>`__
    * - Peer(s)
      - 
        | `Daosheng Mu (daoshengmu) <https://people.mozilla.org/s?query=daoshengmu>`__
    * - Includes
      - 
        | `dom/vr/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/vr/\*\*/\*>`__
        | `gfx/vr/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=gfx/vr/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - https://mozvr.com/
    * - Bugzilla Components
      - Core::WebVR

Core: WebRTC Media
==================
Submodule of WebRTC responsible for access to media input devices (microphones, cameras, screen capture), as well as realtime audiovisual codecs and packetization.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Randell Jesup (jesup) <https://people.mozilla.org/s?query=jesup>`__
    * - Peer(s)
      - 
        | `Jan-Ivar Bruaroey (jib) <https://people.mozilla.org/s?query=jib>`__
        | `Dan Minor (dminor) <https://people.mozilla.org/s?query=dminor>`__
        | `Andreas Pehrson (pehrsons) <https://people.mozilla.org/s?query=pehrsons>`__
    * - Includes
      - 
        | `media/webrtc/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/webrtc/\*\*/\*>`__
        | `dom/media/webrtc/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/media/webrtc/\*\*/\*>`__
        | `dom/media/systemservices/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/media/systemservices/\*\*/\*>`__
    * - Group
      - dev-media
    * - URL
      - https://wiki.mozilla.org/Media/webrtc
    * - Bugzilla Components
      - Core::WebRTC (Audio/Video)

Core: WebRTC Signaling
======================
Submodule of WebRTC responsible for implementation of PeerConnection API, WebRTC identity, and SDP/JSEP handling


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Byron Campen (bwc) <https://people.mozilla.org/s?query=bwc>`__
    * - Peer(s)
      - 
        | `Eric Rescorla (ekr) <https://people.mozilla.org/s?query=ekr>`__
        | `Adam Roach (abr) <https://people.mozilla.org/s?query=abr>`__
        | `Randell Jesup (jesup) <https://people.mozilla.org/s?query=jesup>`__
        | `nohlmeier <https://people.mozilla.org/s?query=nohlmeier>`__
    * - Includes
      - 
        | `media/webrtc/signaling/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=media/webrtc/signaling/\*\*/\*>`__
    * - Group
      - dev-media
    * - URL
      - https://wiki.mozilla.org/Media/webrtc
    * - Bugzilla Components
      - Core::WebRTC (Signaling)

Core: Widget
~~~~~~~~~~~~
Top level Widget


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jim Mathies (jimm) <https://people.mozilla.org/s?query=jimm>`__
    * - Includes
      - 
        | `widget/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=widget/\*\*/\*>`__
        | `widget/xpwidgets/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=widget/xpwidgets/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Drag and Drop, Core::Widget, Core::Printing: Setup

Core: Widget - Android
~~~~~~~~~~~~~~~~~~~~~~
This is part of the [https://wiki.mozilla.org/Modules/Core#GeckoView GeckoView] module.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Agi Sferro (agi) <https://people.mozilla.org/s?query=agi>`__

Core: Widget - GTK
~~~~~~~~~~~~~~~~~~
GTK widget support


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Karl Tomlinson (karlt) <https://people.mozilla.org/s?query=karlt>`__
    * - Peer(s)
      - 
        | `Martin Stránský (stransky) <https://people.mozilla.org/s?query=stransky>`__
    * - Includes
      - 
        | `widget/gtk/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=widget/gtk/\*\*/\*>`__
        | `widget/gtk2/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=widget/gtk2/\*\*/\*>`__
        | `widget/gtksuperwin/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=widget/gtksuperwin/\*\*/\*>`__
        | `widget/gtkxtbin/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=widget/gtkxtbin/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - http://www.mozilla.org/unix/, http://www.gtk.org, http://www.mozilla.org/ports/gtk/
    * - Bugzilla Components
      - Core::Widget: Gtk

Core: Widget - Headless
~~~~~~~~~~~~~~~~~~~~~~~
Headless widget support

.. warning::
    This module does not have any owners specified.

.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Includes
      - 
        | `widget/headless/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=widget/headless/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Firefox::Headless

Core: Widget - macOS
~~~~~~~~~~~~~~~~~~~~
 macOS widget support


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Not active (spohl) <https://people.mozilla.org/s?query=spohl>`__
    * - Peer(s)
      - 
        | `Markus Stange (mstange) <https://people.mozilla.org/s?query=mstange>`__
        | `Haik Aftandilian (haik) <https://people.mozilla.org/s?query=haik>`__
    * - Includes
      - 
        | `widget/cocoa/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=widget/cocoa/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Widget: Cocoa

Core: Widget - Windows
~~~~~~~~~~~~~~~~~~~~~~
Windows widget support


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Jim Mathies (jimm) <https://people.mozilla.org/s?query=jimm>`__
    * - Peer(s)
      - 
        | `Chris Martin (cmartin) <https://people.mozilla.org/s?query=cmartin>`__
        | `Toshihito Kikuchi (toshi) <https://people.mozilla.org/s?query=toshi>`__
        | `Molly Howell (mhowell) <https://people.mozilla.org/s?query=mhowell>`__
    * - Includes
      - 
        | `widget/windows/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=widget/windows/\*\*/\*>`__
    * - Group
      - dev-platform
    * - Bugzilla Components
      - Core::Widget: Win32

Core: XML
~~~~~~~~~
XML in Mozilla, including XML, XHTML, Namespaces in XML, Associating Style Sheets with XML Documents, XML Linking and XML Extras. XML-related things that are not covered by more specific projects.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Peter Van der Beken (peterv) <https://people.mozilla.org/s?query=peterv>`__
    * - Includes
      - 
        | `dom/xml/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/xml/\*\*/\*>`__
        | `extensions/xmlextras/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=extensions/xmlextras/\*\*/\*>`__
        | `parser/expat/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=parser/expat/\*\*/\*>`__
    * - Group
      - dev-tech-xml
    * - URL
      - http://www.mozilla.org/newlayout/xml/
    * - Bugzilla Components
      - Core::XML

Core: XPApps
~~~~~~~~~~~~
Cross-Platform Applications, mostly Navigator front end and application shell.

.. warning::
    This module does not have any owners specified.

.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Peer(s)
      - 
        | `Dean Tessman (dean_tessman) <https://people.mozilla.org/s?query=dean_tessman>`__
    * - Includes
      - 
        | `xpfe/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpfe/\*\*/\*>`__
    * - Group
      - dev-apps-seamonkey
    * - URL
      - http://www.mozilla.org/xpapps/

Core: XPCOM
~~~~~~~~~~~
The cross-platform object model and core data structures.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Nika Layzell (nika) <https://people.mozilla.org/s?query=nika>`__
    * - Peer(s)
      - 
        | `Kris Maglione (kmag) <https://people.mozilla.org/s?query=kmag>`__
        | `Barret Rennie (barret) <https://people.mozilla.org/s?query=barret>`__
        | `Jens Stutte (jstutte) <https://people.mozilla.org/s?query=jstutte>`__
        | `Kris Wright (KrisWright) <https://people.mozilla.org/s?query=KrisWright>`__
    * - Includes
      - 
        | `startupcache/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=startupcache/\*\*/\*>`__
        | `xpcom/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/\*\*/\*>`__
        | `xpcom/base/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/base/\*\*/\*>`__
        | `xpcom/build/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/build/\*\*/\*>`__
        | `xpcom/components/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/components/\*\*/\*>`__
        | `xpcom/ds/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/ds/\*\*/\*>`__
        | `xpcom/glue/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/glue/\*\*/\*>`__
        | `xpcom/proxy/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/proxy/\*\*/\*>`__
        | `xpcom/sample/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/sample/\*\*/\*>`__
        | `xpcom/stub/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/stub/\*\*/\*>`__
        | `xpcom/tests/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/tests/\*\*/\*>`__
        | `xpcom/threads/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/threads/\*\*/\*>`__
        | `xpcom/tools/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/tools/\*\*/\*>`__
        | `xpcom/windbgdlg/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/windbgdlg/\*\*/\*>`__
    * - Group
      - dev-platform
    * - URL
      - http://developer.mozilla.org/en/XPCOM
    * - Bugzilla Components
      - Core::XPCOM

Core: XPConnect
~~~~~~~~~~~~~~~
Deep Magic


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Bobby Holley (bholley) <https://people.mozilla.org/s?query=bholley>`__
    * - Peer(s)
      - 
        | `Boris Zbarsky (bzbarsky) <https://people.mozilla.org/s?query=bzbarsky>`__
        | `Peter Van der Beken (peterv) <https://people.mozilla.org/s?query=peterv>`__
        | `Blake Kaplan (mrbkap) <https://people.mozilla.org/s?query=mrbkap>`__
        | `Andrew McCreight (mccr8) <https://people.mozilla.org/s?query=mccr8>`__
        | `Kris Maglione (kmag) <https://people.mozilla.org/s?query=kmag>`__
        | `Nika Layzell (nika) <https://people.mozilla.org/s?query=nika>`__
    * - Includes
      - 
        | `js/xpconnect/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=js/xpconnect/\*\*/\*>`__
    * - Bugzilla Components
      - Core::XPConnect

Core: XPIDL
~~~~~~~~~~~
Cross-platform IDL compiler; produces .h C++ header files and .xpt runtime type description files from .idl interface description files.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Nika Layzell (nika) <https://people.mozilla.org/s?query=nika>`__
    * - Peer(s)
      - 
        | `Andrew McCreight (mccr8) <https://people.mozilla.org/s?query=mccr8>`__
    * - Includes
      - 
        | `xpcom/typelib/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=xpcom/typelib/\*\*/\*>`__
    * - Group
      - dev-tech-xpcom
    * - URL
      - http://www.mozilla.org/scriptable/xpidl, http://www.mozilla.org/scriptable

Core: XSLT Processor
~~~~~~~~~~~~~~~~~~~~
XSLT transformations processor


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Peter Van der Beken (peterv) <https://people.mozilla.org/s?query=peterv>`__
    * - Includes
      - 
        | `dom/xslt/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=dom/xslt/\*\*/\*>`__
    * - Group
      - dev-tech-xslt
    * - URL
      - http://www.mozilla.org/projects/xslt/, http://www.w3.org/TR/xslt.html
    * - Bugzilla Components
      - Core::XSLT

Desktop Firefox
~~~~~~~~~~~~~~~
Standalone Web Browser.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Dave Townsend (mossop) <https://people.mozilla.org/s?query=mossop>`__
        | `Gijs <https://people.mozilla.org/s?query=Gijs>`__
    * - Peer(s)
      - 
        | `Dão Gottwald (dao) <https://people.mozilla.org/s?query=dao>`__
        | `jaws <https://people.mozilla.org/s?query=jaws>`__
        | `Marco Bonardo (mak) <https://people.mozilla.org/s?query=mak>`__
        | `Matthew N (MattN) <https://people.mozilla.org/s?query=MattN>`__
    * - Includes
      - 
        | `browser/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/\*\*/\*>`__
        | `toolkit/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/\*\*/\*>`__
    * - Group
      - firefox-dev
    * - Bugzilla Components
      - Firefox, Toolkit

Add-ons Manager
===============
Extension management back-end.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `scaraveo <https://people.mozilla.org/s?query=scaraveo>`__
        | `Luca Greco (rpl) <https://people.mozilla.org/s?query=rpl>`__
    * - Peer(s)
      - 
        | `Luca Greco (rpl) <https://people.mozilla.org/s?query=rpl>`__
        | `Tomislav Jovanovic (zombie) <https://people.mozilla.org/s?query=zombie>`__
        | `Rob Wu (robwu) <https://people.mozilla.org/s?query=robwu>`__
    * - Includes
      - 
        | `toolkit/mozapps/extensions/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/mozapps/extensions/\*\*/\*>`__

Add-ons Manager UI
==================
about:addons.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `scaraveo <https://people.mozilla.org/s?query=scaraveo>`__
        | `Mark Striemer (mstriemer) <https://people.mozilla.org/s?query=mstriemer>`__
    * - Peer(s)
      - 
        | `Luca Greco (rpl) <https://people.mozilla.org/s?query=rpl>`__
        | `Tomislav Jovanovic (zombie) <https://people.mozilla.org/s?query=zombie>`__
        | `Rob Wu (robwu) <https://people.mozilla.org/s?query=robwu>`__
        | `William Durand (willdurand) <https://people.mozilla.org/s?query=willdurand>`__
    * - Includes
      - 
        | `toolkit/mozapps/extensions/content/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/mozapps/extensions/content/\*\*/\*>`__

Application Update
==================
The application update services.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Kirk Steuber (bytesized) <https://people.mozilla.org/s?query=bytesized>`__
    * - Peer(s)
      - 
        | `Molly Howell (mhowell) <https://people.mozilla.org/s?query=mhowell>`__
        | `Adam Gashlin (agashlin) <https://people.mozilla.org/s?query=agashlin>`__
    * - Includes
      - 
        | `toolkit/mozapps/update/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/mozapps/update/\*\*/\*>`__

Bookmarks & History
===================
The bookmarks and history services (Places).


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Marco Bonardo (mak) <https://people.mozilla.org/s?query=mak>`__
    * - Peer(s)
      - 
        | `Mark Banner (standard8) <https://people.mozilla.org/s?query=standard8>`__
        | `Drew Willcoxon (adw) <https://people.mozilla.org/s?query=adw>`__
    * - Includes
      - 
        | `browser/components/places/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/places/\*\*/\*>`__
        | `toolkit/components/places/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/places/\*\*/\*>`__

Desktop Theme
=============
The style rules used in the desktop UI.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Dão Gottwald (dao) <https://people.mozilla.org/s?query=dao>`__
    * - Peer(s)
      - 
        | `Itiel (itiel_yn8) <https://people.mozilla.org/s?query=itiel_yn8>`__
        | `Sam Foster (sfoster) <https://people.mozilla.org/s?query=sfoster>`__
    * - Includes
      - 
        | `browser/themes/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/themes/\*\*/\*>`__
        | `toolkit/themes/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/themes/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox::Theme, Toolkit::Themes

Desktop UI
==========
The main browser UI except where covered by more specific submodules.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `jaws <https://people.mozilla.org/s?query=jaws>`__
    * - Peer(s)
      - 
        | `Mike Conley (mconley) <https://people.mozilla.org/s?query=mconley>`__
        | `Florian Quèze (bmo) <https://people.mozilla.org/s?query=bmo>`__
    * - Includes
      - 
        | `browser/base/content/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/base/content/\*\*/\*>`__

Download Manager
================
The downloads UI and service.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Marco Bonardo (mak) <https://people.mozilla.org/s?query=mak>`__
    * - Peer(s)
      - 
        | `Gijs <https://people.mozilla.org/s?query=Gijs>`__
        | `Micah (mtigley) <https://people.mozilla.org/s?query=mtigley>`__
    * - Includes
      - 
        | `browser/components/downloads/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/downloads/\*\*/\*>`__
        | `toolkit/mozapps/downloads/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/mozapps/downloads/\*\*/\*>`__

Enterprise Policies
===================
System policies for controlling Firefox.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Please use mozilla (mkaply) <https://people.mozilla.org/s?query=mkaply>`__
    * - Peer(s)
      - 
        | `Dão Gottwald (dao) <https://people.mozilla.org/s?query=dao>`__
        | `jaws <https://people.mozilla.org/s?query=jaws>`__
        | `Marco Bonardo (mak) <https://people.mozilla.org/s?query=mak>`__
        | `Matthew N (MattN) <https://people.mozilla.org/s?query=MattN>`__
    * - Includes
      - 
        | `browser/components/enterprisepolicies/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/enterprisepolicies/\*\*/\*>`__

Experiments/Rollouts
====================
Desktop clients for our experiments and off-train deployments systems.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Barret Rennie (barret) <https://people.mozilla.org/s?query=barret>`__
    * - Peer(s)
      - 
        | `Gijs <https://people.mozilla.org/s?query=Gijs>`__
        | `Emily McMinn (emcminn) <https://people.mozilla.org/s?query=emcminn>`__
    * - Includes
      - 
        | `toolkit/components/normandy/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/normandy/\*\*/\*>`__
        | `toolkit/components/nimbus/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/nimbus/\*\*/\*>`__
    * - URL
      - https://experimenter.info/
    * - Bugzilla Components
      - Firefox::Normandy, Firefox::Nimbus Desktop Client

Form Autofill
=============
Form detection and autocomplete.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Sergey Galich (serg) <https://people.mozilla.org/s?query=serg>`__
    * - Peer(s)
      - 
        | `Tim Giles (tgiles) <https://people.mozilla.org/s?query=tgiles>`__
        | `Dimi Lee (dimi) <https://people.mozilla.org/s?query=dimi>`__
    * - Includes
      - 
        | `browser/extensions/formautofill/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/extensions/formautofill/\*\*/\*>`__
        | `toolkit/components/satchel/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/satchel/\*\*/\*>`__

In-product Messaging
====================
The system for delivering in-product messaging.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Andrei Oprea (aoprea) <https://people.mozilla.org/s?query=aoprea>`__
    * - Peer(s)
      - 
        | `Nan Jiang (nanj) <https://people.mozilla.org/s?query=nanj>`__
        | `Punam Dahiya (pdahiya) <https://people.mozilla.org/s?query=pdahiya>`__
        | `Ed Lee (Mardak) <https://people.mozilla.org/s?query=Mardak>`__
        | `Kate Hudson (k88hudson) <https://people.mozilla.org/s?query=k88hudson>`__
    * - Includes
      - 
        | `toolkit/components/messaging-system/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/messaging-system/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox::Messaging System

Launcher Process
================
Windows process for bootstrapping the browser process.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Toshihito Kikuchi (toshi) <https://people.mozilla.org/s?query=toshi>`__
    * - Peer(s)
      - 
        | `Molly Howell (mhowell) <https://people.mozilla.org/s?query=mhowell>`__
    * - Includes
      - 
        | `browser/app/winlauncher/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/app/winlauncher/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox::Launcher Process

Localization
============
Tooling to enable translation and facilitate localization.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Francesco Lodolo (flod) <https://people.mozilla.org/s?query=flod>`__
    * - Peer(s)
      - 
        | `Matjaz Horvat (mathjazz) <https://people.mozilla.org/s?query=mathjazz>`__
        | `Eemeli Aro (eemeli) <https://people.mozilla.org/s?query=eemeli>`__
    * - Includes
      - 
        | `browser/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/\*\*/\*>`__
        | `toolkit/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/\*\*/\*>`__

New Tab Page
============
The new tab/home page.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Ed Lee (ed) <https://people.mozilla.org/s?query=ed>`__
    * - Peer(s)
      - 
        | `Kate Hudson (k88hudson) <https://people.mozilla.org/s?query=k88hudson>`__
        | `Andrei Oprea (aoprea) <https://people.mozilla.org/s?query=aoprea>`__
        | `Scott (thecount) <https://people.mozilla.org/s?query=thecount>`__
    * - Includes
      - 
        | `browser/components/newtab/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/newtab/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox::New Tab Page

Onboarding
==========
The onboarding experience including UI tours.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Ed Lee (ed) <https://people.mozilla.org/s?query=ed>`__
    * - Peer(s)
      - 
        | `Matthew N (MattN) <https://people.mozilla.org/s?query=MattN>`__
    * - Includes
      - 
        | `browser/components/uitour/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/uitour/\*\*/\*>`__
    * - Bugzilla Components
      - F, i, r, e, f, o, x, :, :, T, o, u, r, s

Password Manager
================
Managing, saving and filling logins.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Sergey Galich (serg) <https://people.mozilla.org/s?query=serg>`__
    * - Peer(s)
      - 
        | `jaws <https://people.mozilla.org/s?query=jaws>`__
        | `Tim Giles (tgiles) <https://people.mozilla.org/s?query=tgiles>`__
        | `Dimi Lee (dimi) <https://people.mozilla.org/s?query=dimi>`__
        | `Sam Foster (sfoster) <https://people.mozilla.org/s?query=sfoster>`__
    * - Includes
      - 
        | `toolkit/components/passwordmgr/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/passwordmgr/\*\*/\*>`__
        | `browser/components/aboutlogins/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/aboutlogins/\*\*/\*>`__
    * - URL
      - https://wiki.mozilla.org/Toolkit:Password_Manager
    * - Bugzilla Components
      - Toolkit::Password Manager, Toolkit::Password Manager: Site, Compatibility, Firefox::about:logins

Preferences
===========
The front-end preferences system.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `jaws <https://people.mozilla.org/s?query=jaws>`__
    * - Peer(s)
      - 
        | `Mark Striemer (mstriemer) <https://people.mozilla.org/s?query=mstriemer>`__
        | `Gijs <https://people.mozilla.org/s?query=Gijs>`__
    * - Includes
      - 
        | `browser/components/preferences/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/preferences/\*\*/\*>`__
        | `browser/themes/\*/preferences <https://searchfox.org/mozilla-central/search?q=&path=browser/themes/\*/preferences>`__
        | `toolkit/mozapps/preferences <https://searchfox.org/mozilla-central/search?q=&path=toolkit/mozapps/preferences>`__

Profile Migration
=================
Migrating data from other browsers.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Gijs <https://people.mozilla.org/s?query=Gijs>`__
    * - Peer(s)
      - 
        | `Marco Bonardo (mak) <https://people.mozilla.org/s?query=mak>`__
        | `Matthew N (MattN) <https://people.mozilla.org/s?query=MattN>`__
    * - Includes
      - 
        | `browser/components/migration/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/migration/\*\*/\*>`__

Screenshots
===========
Code relating to Screenshots functionality


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Sam Foster (sfoster) <https://people.mozilla.org/s?query=sfoster>`__
    * - Peer(s)
      - 
        | `Jared Hirsch (jhirsch) <https://people.mozilla.org/s?query=jhirsch>`__
        | `Niklas Baumgardner (niklas) <https://people.mozilla.org/s?query=niklas>`__
    * - Includes
      - 
        | `browser/extensions/screenshots/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/extensions/screenshots/\*\*/\*>`__
        | `browser/components/screenshots/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/screenshots/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox::Screenshots

Search and Address Bar
======================
The search service, address bar and address bar autocomplete.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Mark Banner (standard8) <https://people.mozilla.org/s?query=standard8>`__
        | `Drew Willcoxon (adw) <https://people.mozilla.org/s?query=adw>`__
    * - Peer(s)
      - 
        | `Dale Harvey (daleharvey) <https://people.mozilla.org/s?query=daleharvey>`__
        | `Marco Bonardo (mak) <https://people.mozilla.org/s?query=mak>`__
        | `Dão Gottwald (dao) <https://people.mozilla.org/s?query=dao>`__
    * - Includes
      - 
        | `browser/components/search/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/search/\*\*/\*>`__
        | `browser/components/urlbar/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/urlbar/\*\*/\*>`__
        | `toolkit/components/search/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/search/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox::Address Bar, Firefox::Search

Security and Privacy UI
=======================
The front-end to our security and privacy features, including Protections UI, Site Identity, Site Permissions and Certificate Errors


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Paul Zühlcke (pbz) <https://people.mozilla.org/s?query=pbz>`__
    * - Peer(s)
      - 
        | `prathiksha <https://people.mozilla.org/s?query=prathiksha>`__
    * - Includes
      - 
        | `browser/components/protections/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/protections/\*\*/\*>`__
        | `browser/components/controlcenter/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/controlcenter/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox::Security, Firefox::Protections UI, Firefox::Site Identity, Firefox::Site Permissions

Session Restore
===============
Restoring a user's session after starting Firefox.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Dão Gottwald (dao) <https://people.mozilla.org/s?query=dao>`__
        | `Dale Harvey (daleharvey) <https://people.mozilla.org/s?query=daleharvey>`__
    * - Peer(s)
      - 
        | `Andreas Farre (farre) <https://people.mozilla.org/s?query=farre>`__
    * - Includes
      - 
        | `browser/components/sessionstore/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/sessionstore/\*\*/\*>`__
        | `toolkit/components/sessionstore/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/sessionstore/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox::Session Restore

Tabbed Browser
==============
The UI component controlling browser tabs.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Dão Gottwald (dao) <https://people.mozilla.org/s?query=dao>`__
    * - Peer(s)
      - 
        | `Dão Gottwald (dao) <https://people.mozilla.org/s?query=dao>`__
        | `jaws <https://people.mozilla.org/s?query=jaws>`__
        | `Marco Bonardo (mak) <https://people.mozilla.org/s?query=mak>`__
        | `Matthew N (MattN) <https://people.mozilla.org/s?query=MattN>`__
    * - Includes
      - 
        | `browser/base/content/tabbrowser\* <https://searchfox.org/mozilla-central/search?q=&path=browser/base/content/tabbrowser\*>`__
        | `browser/modules/AsyncTabSwitcher.jsm <https://searchfox.org/mozilla-central/search?q=&path=browser/modules/AsyncTabSwitcher.jsm>`__
    * - Bugzilla Components
      - Firefox::Tabbed Browser

Windows Installer
=================
The installer for Windows.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Molly Howell (mhowell) <https://people.mozilla.org/s?query=mhowell>`__
    * - Peer(s)
      - 
        | `Adam Gashlin (agashlin) <https://people.mozilla.org/s?query=agashlin>`__
        | `Nick Alexander (nalexander) <https://people.mozilla.org/s?query=nalexander>`__
    * - Includes
      - 
        | `browser/installer/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/installer/\*\*/\*>`__
        | `toolkit/mozapps/installer/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/mozapps/installer/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox::Installer

Marionette
~~~~~~~~~~
Marionette is a remote protocol that lets out-of-process programs communicate with, instrument, and control Gecko-based browsers. Combined with geckodriver, this forms our WebDriver implementation.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Henrik Skupin (whimboo) <https://people.mozilla.org/s?query=whimboo>`__
    * - Peer(s)
      - 
        | `Julian Descottes (jdescottes) <https://people.mozilla.org/s?query=jdescottes>`__
        | `James Graham (jgraham) <https://people.mozilla.org/s?query=jgraham>`__
    * - Includes
      - 
        | `testing/marionette/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/marionette/\*\*/\*>`__
    * - Bugzilla Components
      - Testing :: Marionette

mozharness
~~~~~~~~~~
Configuration-driven script harness.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Aki Sasaki (aki) <https://people.mozilla.org/s?query=aki>`__
    * - Peer(s)
      - 
        | `Justin Wood (Callek) <https://people.mozilla.org/s?query=Callek>`__
        | `Tom Prince (tomprince) <https://people.mozilla.org/s?query=tomprince>`__
    * - Includes
      - 
        | `testing/mozharness/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/mozharness/\*\*/\*>`__
    * - Bugzilla Components
      - Release Engineering :: Applications: MozharnessCore

firefox-ui
~~~~~~~~~~
Firefox UI test framework.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Henrik Skupin (whimboo) <https://people.mozilla.org/s?query=whimboo>`__
    * - Peer(s)
      - 
        | `Julian Descottes (jdescottes) <https://people.mozilla.org/s?query=jdescottes>`__
    * - Includes
      - 
        | `testing/firefox-ui/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/firefox-ui/\*\*/\*>`__
    * - Bugzilla Components
      - Testing :: Firefox UI

geckodriver
~~~~~~~~~~~
Proxy for using W3C WebDriver-compatible clients to interact with Gecko-based browsers.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `James Graham (jgraham) <https://people.mozilla.org/s?query=jgraham>`__
    * - Peer(s)
      - 
        | `Henrik Skupin (whimboo) <https://people.mozilla.org/s?query=whimboo>`__
    * - Includes
      - 
        | `testing/geckodriver/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/geckodriver/\*\*/\*>`__
    * - Bugzilla Components
      - Testing :: geckodriver

gtest
~~~~~
GTest test harness.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Geoff Brown (gbrown) <https://people.mozilla.org/s?query=gbrown>`__
    * - Includes
      - 
        | `testing/gtest/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/gtest/\*\*/\*>`__
    * - Bugzilla Components
      - Testing :: GTest

Mochitest
~~~~~~~~~
Mochitest test framework


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Andrew Halberstadt (ahal) <https://people.mozilla.org/s?query=ahal>`__
    * - Peer(s)
      - 
        | `Joel Maher (jmaher) <https://people.mozilla.org/s?query=jmaher>`__
        | `Geoff Brown (gbrown) <https://people.mozilla.org/s?query=gbrown>`__
    * - Includes
      - 
        | `testing/mochitest/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/mochitest/\*\*/\*>`__
    * - Bugzilla Components
      - Testing :: Mochitest

Mozbase
~~~~~~~
Base modules used for implementing test components.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Geoff Brown (gbrown) <https://people.mozilla.org/s?query=gbrown>`__
    * - Peer(s)
      - 
        | `Andrew Halberstadt (ahal) <https://people.mozilla.org/s?query=ahal>`__
        | `Andreas Tolfsen (ato) <https://people.mozilla.org/s?query=ato>`__
        | `Bob Clary (bc) <https://people.mozilla.org/s?query=bc>`__
        | `James Graham (jgraham) <https://people.mozilla.org/s?query=jgraham>`__
        | `Henrik Skupin (whimboo) <https://people.mozilla.org/s?query=whimboo>`__
    * - Bugzilla Components
      - Testing :: Mozbase, Testing :: Mozbase Rust

Performance Testing
~~~~~~~~~~~~~~~~~~~
This module encompasses all of our performance test frameworks: Raptor, Talos, MozPerfTest, AWSY.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Greg Mierzwinski (sparky) <https://people.mozilla.org/s?query=sparky>`__
    * - Peer(s)
      - 
        | `Kimberly Sereduck (kimberlythegeek) <https://people.mozilla.org/s?query=kimberlythegeek>`__
    * - Includes
      - 
        | `testing/raptor/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/raptor/\*\*/\*>`__
        | `testing/talos/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/talos/\*\*/\*>`__
        | `python/mozperftest/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=python/mozperftest/\*\*/\*>`__
        | `testing/awsy/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/awsy/\*\*/\*>`__
    * - Bugzilla Components
      - Testing :: Raptor, Testing :: Talos, Testing :: AWSY, Testing :: Performance

Reftest (+ jsreftest + crashtest)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Reftest test framework


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Timothy Nikkel (tnikkel) <https://people.mozilla.org/s?query=tnikkel>`__
    * - Peer(s)
      - 
        | `Andrew Halberstadt (ahal) <https://people.mozilla.org/s?query=ahal>`__
        | `Joel Maher (jmaher) <https://people.mozilla.org/s?query=jmaher>`__
    * - Includes
      - 
        | `layout/tools/reftest/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=layout/tools/reftest/\*\*/\*>`__
    * - Bugzilla Components
      - Testing :: Reftest

Tryselect
~~~~~~~~~
Frontend for selecting jobs on the try server.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Andrew Halberstadt (ahal) <https://people.mozilla.org/s?query=ahal>`__
    * - Peer(s)
      - 
        | `James Graham (jgraham) <https://people.mozilla.org/s?query=jgraham>`__
    * - Includes
      - 
        | `tools/tryselect/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=tools/tryselect/\*\*/\*>`__
    * - Bugzilla Components
      - Firefox Build System :: Try

web-platform-tests infrastructure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Infrastructure for running the cross-browser web-platform-tests


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `James Graham (jgraham) <https://people.mozilla.org/s?query=jgraham>`__
    * - Includes
      - 
        | `testing/web-platform/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/web-platform/\*\*/\*>`__
        | `testing/web-platform/tests/tools/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/web-platform/tests/tools/\*\*/\*>`__
    * - Excludes
      - 
        | `testing/web-platform/tests/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/web-platform/tests/\*\*/\*>`__
        | `testing/web-platform/meta/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/web-platform/meta/\*\*/\*>`__
        | `testing/web-platform/mozilla/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/web-platform/mozilla/\*\*/\*>`__
    * - Bugzilla Components
      - Testing :: web-platform-tests

XPCShell
~~~~~~~~
XPCShell test harness.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Joel Maher (jmaher) <https://people.mozilla.org/s?query=jmaher>`__
    * - Peer(s)
      - 
        | `Geoff Brown (gbrown) <https://people.mozilla.org/s?query=gbrown>`__
    * - Includes
      - 
        | `testing/xpcshell/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=testing/xpcshell/\*\*/\*>`__
    * - Bugzilla Components
      - Testing::XPCShell Harness

Toolkit
~~~~~~~
Components shared between desktop and mobile browsers.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Dave Townsend (mossop) <https://people.mozilla.org/s?query=mossop>`__
        | `Gijs <https://people.mozilla.org/s?query=Gijs>`__
    * - Peer(s)
      - 
        | `jaws <https://people.mozilla.org/s?query=jaws>`__
        | `Marco Bonardo (mak) <https://people.mozilla.org/s?query=mak>`__
        | `Matthew N (MattN) <https://people.mozilla.org/s?query=MattN>`__
    * - Includes
      - 
        | `toolkit/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/\*\*/\*>`__
    * - Group
      - firefox-dev
    * - URL
      - [[Firefox/Code_Review|Code Review Guidelines]]
    * - Bugzilla Components
      - Firefox, Toolkit

Application Startup
===================
The profile system and startup process before the front-end launches.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Dave Townsend (mossop) <https://people.mozilla.org/s?query=mossop>`__
    * - Peer(s)
      - 
        | `Nathan Froyd (froydnj) <https://people.mozilla.org/s?query=froydnj>`__
    * - Includes
      - 
        | `toolkit/profile/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/profile/\*\*/\*>`__
        | `toolkit/components/remote/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/remote/\*\*/\*>`__
        | `toolkit/xre/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/xre/\*\*/\*>`__

Telemetry
=========
The core infrastructure in the Firefox client to send back Telemetry and FHR data. Includes the common mechanism to record, view and submit data like histograms, the Telemetry environment and custom pings. This module does ''not'' include responsibility for every piece of submitted Telemetry data. Each team/module is responsible for their own measurements (histograms, scalars, other ping submissions, etc.).


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Chris H-C (chutten) <https://people.mozilla.org/s?query=chutten>`__
    * - Peer(s)
      - 
        | `aplacitelli <https://people.mozilla.org/s?query=aplacitelli>`__
        | `Jan-Erik Rediger (janerik) <https://people.mozilla.org/s?query=janerik>`__
    * - Includes
      - 
        | `toolkit/components/telemetry/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/telemetry/\*\*/\*>`__
        | `toolkit/content/aboutTelemetry.\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/content/aboutTelemetry.\*>`__
    * - Group
      - fx-data-dev
    * - URL
      - https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/index.html

UI Widgets
==========
The base widgets used throughout the UI.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `Neil Deakin (enndeakin) <https://people.mozilla.org/s?query=enndeakin>`__
    * - Peer(s)
      - 
        | `jaws <https://people.mozilla.org/s?query=jaws>`__
        | `Marco Bonardo (mak) <https://people.mozilla.org/s?query=mak>`__
        | `Matthew N (MattN) <https://people.mozilla.org/s?query=MattN>`__
    * - Includes
      - 
        | `toolkit/content/widgets/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/content/widgets/\*\*/\*>`__

Webextensions
=============
Webextension APIs and integration.


.. list-table::
    :stub-columns: 1
    :widths: 30 70

    * - Owner(s)
      - 
        | `scaraveo <https://people.mozilla.org/s?query=scaraveo>`__
        | `Tomislav Jovanovic (zombie) <https://people.mozilla.org/s?query=zombie>`__
    * - Peer(s)
      - 
        | `Luca Greco (rpl) <https://people.mozilla.org/s?query=rpl>`__
        | `Kris Maglione (kmag) <https://people.mozilla.org/s?query=kmag>`__
        | `Rob Wu (robwu) <https://people.mozilla.org/s?query=robwu>`__
        | `William Durand (willdurand) <https://people.mozilla.org/s?query=willdurand>`__
    * - Includes
      - 
        | `browser/components/extensions/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=browser/components/extensions/\*\*/\*>`__
        | `toolkit/components/extensions/\*\*/\* <https://searchfox.org/mozilla-central/search?q=&path=toolkit/components/extensions/\*\*/\*>`__

