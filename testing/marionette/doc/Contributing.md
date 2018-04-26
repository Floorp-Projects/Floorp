Contributing
============

If you are new to open source or to Mozilla, you might like this
[tutorial for new Marionette contributors](NewContributors.html).

We are delighted that you want to help improve Marionette!
‘Marionette’ means different a few different things, depending
on who you talk to, but the overall scope of the project involves
these components:

  * [_Marionette_] is a Firefox remote protocol to communicate with,
    instrument, and control Gecko-based browsers such as Firefox
    and Fennec.  It is built in to Firefox and written in [XPCOM]
    flavoured JavaScript.

    It serves as the backend for the geckodriver WebDriver implementation,
    and is used in the context of Firefox UI tests, reftesting,
    Web Platform Tests, test harness bootstrapping, and in many
    other far-reaching places where browser instrumentation is required.

  * [_geckodriver_] provides the HTTP API described by the [WebDriver
    protocol] to communicate with Gecko-based browsers such as
    Firefox and Fennec.  It is a standalone executable written in
    Rust, and can be used with compatible W3C WebDriver clients.

  * [_webdriver_] is a Rust crate providing interfaces, traits
    and types, errors, type- and bounds checks, and JSON marshaling
    for correctly parsing and emitting the [WebDriver protocol].

By participating in this project, you agree to abide by the Mozilla
[Community Participation Guidelines].  Here are some guidelines
for contributing high-quality and actionable bugs and code.

[_Marionette_]: ./index.html
[_geckodriver_]: ../geckodriver
[_webdriver_]: https://searchfox.org/mozilla-central/source/testing/webdriver/README.md
[WebDriver protocol]: https://w3c.github.io/webdriver/webdriver-spec.html#protocol
[XPCOM]: https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Guide
[Community Participation Guidelines]: https://www.mozilla.org/en-US/about/governance/policies/participation/


Writing code
------------

Because there are many moving parts involved remote controlling
a web browser, it can be challenging to a new contributor to know
where to start.  Please don’t hesitate to [ask questions]!

The canonical source code repository is [mozilla-central].  Bugs are
filed in the `Testing :: Marionette` component on Bugzilla.  We also
have a curated set of [good first bugs] you may consider attempting first.

We have collected a lot of good advice for working on Marionette
code in our [code style document], which we highly recommend you read.

[ask questions]: ./index.html#communication
[reach out to us]: ./index.html#communication
[mozilla-central]: https://searchfox.org/mozilla-central/source/testing/marionette/
[good first bugs]: https://www.joshmatthews.net/bugsahoy/?automation=1
[code style document]: CodeStyle.html


Next steps
----------

  * [Building](Building.html)
  * [Debugging](Debugging.html)
  * [Testing](Testing.html)
  * [Patching](Patches.html)


Other resources
---------------

  * [Code style](CodeStyle.html)
  * [Internals](internals/)
  * [New Contributor Tutorial](NewContributors.html)
