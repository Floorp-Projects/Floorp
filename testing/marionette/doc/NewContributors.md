New contributors
================

This page is aimed at people who are new to Mozilla and want to contribute
to Mozilla source code related to Marionette Python tests, WebDriver
spec tests and related test harnesses and tools. Mozilla has both
git and Mercurial repositories, but this guide only describes Mercurial.

If you run into issues or have doubts, check out the [Resources](#resources)
section below and **don't hesitate to ask questions**. :) The goal of these
steps is to make sure you have the basics of your development environment
working. Once you do, we can get you started with working on an
actual bug, yay!


Accounts, communication
-----------------------

  1. Set up [IRC].

  2. Set up a [Bugzilla] account (and, if you like, a [Mozillians] profile).
     Please include your IRC nickname in both of these accounts
     so we can work with you more easily. For example, Eve Smith
     would set their Bugzilla name to "Eve Smith (:esmith)", where
     esmith is their IRC nick.

  3. Join #ateam on irc.mozilla.org and introduce
     yourself to the team. :ato, :AutomatedTester, :maja_zf and :whimboo are all familiar with Marionette. We're nice, I promise, but we might not
     answer right away (different time zones, time off, etc.).

  4. When you want to ask a question on IRC, just go ahead an ask it even if
     no one appears to be around/responding.
     Provide lots of detail so that we have a better chance of helping you.
     If you don't get an answer right away, check again in a few hours --
     someone may have answered you in the mean time.

  5. You can view IRC logs on [logbot] to check if anyone has answered your
     question while you were offline.

  6. If you're having trouble reaching us over IRC, you are welcome to send an
     email to our [mailing list](index.html#communication) instead. It's a good
     idea to include your IRC nick in your email message.

[IRC]: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Getting_Started_with_IRC
[Bugzilla]: https://bugzilla.mozilla.org/
[Mozillians]: https://mozillians.org/
[logbot]: https://mozilla.logbot.info/ateam/

Getting the code, running tests
-------------------------------

  1. Follow this [tutorial](http://areweeveryoneyet.org/onramp/desktop.html)
     to get a copy of Firefox source code and build Firefox for Desktop.

     If you're asked to run a 'bootstrap' script, choose the option
     "Firefox for Desktop Artifact Mode".  This significantly
     reduces the time it takes to build Firefox on your machine
     (from 30+ minutes to just 1-2 minutes).

  2. Check if you can run any marionette tests: Use [mach] to run
     [Marionette] unit tests against the Firefox binary you just
     built in the previous step: `./mach marionette test` -- see
     [Running Marionette tests] for details.  If you see tests
     running, that's good enough: **you don't have to run all the
     tests right now**, that takes a long time.

     Fun fact: the set of tests that you just ran on your machine is
     triggered automatically for every changeset in the Mozilla source
     tree!  For example, here are the [latest results on Treeherder].

  3. As an exercise, try out this different way of running the
     Marionette unit tests, this time against Firefox Nightly
     instead of the binary in your source tree:

     * Download and install [Firefox Nightly], then find
       the path to the executable binary that got installed.
       For example, on a macOS it might be something like
       `FF_NIGHTLY_PATH=/Applications/FirefoxNightly.app/Contents/MacOS/firefox-bin`.

     * Create and activate a [virtualenv] environment.

     * Within your checkout of the Mozilla source, cd to
       [testing/marionette/client].

     * Run `python setup.py develop` to install the marionette
       driver package in development mode in your virtualenv.

     * Next cd to [testing/marionette/harness].

     * Run `cd marionette && python runtests.py tests/unit-tests.ini
       --binary $FF_NIGHTLY_PATH`.

       * These are the same tests that you ran with `./mach
         marionette test`, but they are testing the Firefox Nightly
         that you just installed.  (`./mach marionette test`
         just calls code in runtests.py).

     * Configure Mercurial with helpful extensions for Mozilla
       development by running `./mach mercurial-setup`.

       * It should install extensions like firefox-trees and set
         you up to be able to use MozReview, our code-review tool.

       * If it asks you about activating the mq extension, I suggest
         you respond with 'No'.

[mach]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/mach
[Marionette]: index.html
[Running Marionette tests]: PythonTests.html
[latest results on Treeherder]: https://treeherder.mozilla.org/#/jobs?repo=mozilla-inbound&filter-job_type_symbol=Mn
[Firefox Nightly]: https://nightly.mozilla.org/
[virtualenv]: https://www.dabapps.com/blog/introduction-to-pip-and-virtualenv-python/
[testing/marionette/client]: https://searchfox.org/mozilla-central/source/testing/marionette/client
[testing/marionette/harness]: https://searchfox.org/mozilla-central/source/testing/marionette/harness


Work on bugs and get code review
--------------------------------

Once you've completed the above basics, find a bug to work on
at [Bugs Ahoy](https://www.joshmatthews.net/bugsahoy/?automation=1). If
you don't find any, ask :whimboo, :maja_zf
or :ato in #ateam for a good first bug to work on.

To work on the bug that is suggested to you and push a patch up
for review, follow the [Firefox Workflow] in hg.

After testing your code locally, you will push your patch to be
reviewed in MozReview.  To set up MozReview, see this [configuration]
page.  (Note: the only kind of account you need for MozReview is
Bugzilla (not LDAP) and you can only use HTTPS, not SSH.)

[Firefox Workflow]: https://mozilla-version-control-tools.readthedocs.org/en/latest/hgmozilla/firefoxworkflow.html
[configuration]: https://mozilla-version-control-tools.readthedocs.org/en/latest/mozreview/install-mercurial.html#configuring-mercurial-to-use-mozreview


Resources
---------

  * Sometimes (often?) documentation is out-of-date.  If something looks
    off, do ask us for help!

  * This document provides a one-track, simple flow for getting
    started with Marionette Test Runner development.
    The general guide to all Marionette development is at [Contributing](Contributing.html)

  * Search Mozilla's hg repositories with [searchfox].

  * Another [guide for new contributors].  It has not been updated in a long
    time but it's a good general
    resource if you ever get stuck on something.  The most relevant
    sections to you are about Bugzilla, Mercurial, Python and the
    Development Process.

  * [Mercurial for Mozillians]

  * More general resources are available in this little [guide] :maja_zf wrote
    in 2015 to help a student get started with open source contributions.

    * Textbook about general open source practices: [Practical
      Open Source Software Exploration]

  * If you'd rather use git instead of hg, see [git workflow for
    Gecko development] and/or [this blog post by :ato].

[searchfox]: https://searchfox.org/mozilla-central/source/testing/marionette/
[guide for new contributors]: https://ateam-bootcamp.readthedocs.org/en/latest/guide/index.html#new-contributor-guide
[Mercurial for Mozillians]: https://mozilla-version-control-tools.readthedocs.org/en/latest/hgmozilla/index.html
[guide]: https://gist.github.com/mjzffr/d2adef328a416081f543
[Practical Open Source Software Exploration]: https://quaid.fedorapeople.org/TOS/Practical_Open_Source_Software_Exploration/html/index.html
[git workflow for Gecko development]: https://github.com/glandium/git-cinnabar/wiki/Mozilla:-A-git-workflow-for-Gecko-development
[this blog post by :ato]: https://sny.no/2016/03/geckogit
[git workflow for Gecko development]: https://github.com/glandium/git-cinnabar/wiki/Mozilla:-A-git-workflow-for-Gecko-development
