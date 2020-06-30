===========================
Telemetry review guidelines
===========================

General guidelines for reviewing changes
========================================

These are the general principles we follow when reviewing changes.

- *Be constructive.* Both reviewers and patch authors should be allies that aim to get the change landed together.
- *Consider the impact.* We deliver critical data that is processed and used by many systems and people. Any disruption should be planned and intentional.
- *Be diligent.* All changes should be tested under typical conditions.
- *Know your limits.* Defer to other peers or experts where sensible.

Main considerations for any change
========================================

For any change, these are the fundamental questions that we always need satisfactory answers for.

- Does this have a plan?

  - Is there a specific need to do this?
  - Does this change need `a proposal <https://github.com/mozilla/Fx-Data-Planning/blob/master/process/ProposalProcess.md>`_ first?
  - Do we need to announce this before we do this? (e.g. for `deprecations <https://github.com/mozilla/Fx-Data-Planning/blob/master/process/Deprecation.md>`_)

- Does this involve the right people?

  - Does this change need input from... A data engineer? A data scientist?
  - Does this change need data review?

- Is this change complete?

  - Does this change have sufficient test coverage?
  - Does this change have sufficient documentation?

- Do we need follow-ups?

  - Do we need to file validation bugs? Or other follow-up bugs?
  - Do we need to communicate this to our users? Or other groups?

Additional considerations
=========================

Besides the basic considerations above, these are additional detailed considerations that help with reviewing changes.

Considerations for all changes
------------------------------

- Follow our standards and best practices.

  - Firefox Desktop:

    - `The Mozilla coding style <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Coding_Style>`_
    - `The toolkit code review guidelines <https://wiki.mozilla.org/Toolkit/Code_Review>`_

  - Mobile:

    - `Android/Kotlin code style <https://kotlinlang.org/docs/reference/coding-conventions.html>`_
    - `iOS/Swift code style <https://github.com/mozilla-mobile/firefox-ios/wiki/Swift-Style-Guides>`_

- Does this impact performance significantly?:

  - Don't delay application initialisation (unless absolutely necessary).
  - Don't ever block on network requests.
  - Make longer tasks async whenever feasible.

- Does this affect products more broadly than expected?

  - Consider all our platforms: Windows, Mac, Linux, Android.
  - Consider all our products: Firefox, Fennec, GeckoView, Glean.

- Does this fall afoul of common architectural failures?

  - Prefer APIs that take non-String types unless writing a parser.

- Sanity checking:

  - How does this behave in a release build? Have you tested this?
  - Does this change contain test coverage? We require test coverage for every new code, changes and bug fixes.

- Does this need documentation updates?

  - To the :ref:`in-tree docs <Telemetry>`?
  - To the `firefox-data-docs <https://docs.telemetry.mozilla.org/>`_ (`repository <https://github.com/mozilla/firefox-data-docs>`_)?
  - To the `glean documentation <https://github.com/mozilla-mobile/android-components/tree/master/components/service/glean>`_?

- Following up:

  - Do we have a validation bug filed yet?
  - Do all TODOs have follow-up bugs filed?
  - Do we need to communicate this to our users?

    - `fx-data-dev <https://mail.mozilla.org/listinfo/fx-data-dev>`_ (Main Firefox data list)
    - `firefox-dev <https://mail.mozilla.org/listinfo/firefox-dev>`_ (Firefox application developers)
    - `dev-platform <https://lists.mozilla.org/listinfo/dev-platform>`_ (Gecko / platform developers)
    - `mobile-firefox-dev <https://mail.mozilla.org/listinfo/mobile-firefox-dev>`_ (Mobile developers)
    - fx-team (Firefox staff)

  - Do we need to communicate this to other groups?

    - Data engineering, data science, data stewards, ...?

Consider the impact on others
-----------------------------

- Could this change break upstream pipeline jobs?

  - Does this change the format of outgoing data in an unhandled way?

    - E.g. by adding, removing, or changing the type of a non-metric payload field.

  - Does this require ping schema updates?
  - Does this break jobs that parse the registry files for metrics? (Scalars.yaml, metrics.yaml, etc.)

- Do we need to involve others?

  - Changes to data formats, ping contents, ping semantics etc. require involving a data engineer.
  - Changes to any outgoing data that is in active use require involving the stakeholders (e.g. data scientists).

Considerations for Firefox Desktop
----------------------------------

- For profiles:

  - How does using different profiles affect this?
  - How does switching between profiles affect this?
  - What happens when users switch between different channels?

- Footguns:

  - Does this have side-effects on Fennec? (Unified Telemetry is off there, so behavior is pretty different.)
  - Is your code gated on prefs, build info, channels? Tests should cover that.
  - If test is gated on isUnified, code should be too (and vice-versa)

    - Test for the other case

  - Any code using `new Date()` should get additional scrutiny
    - Code using `new Date()` should be using Policy so it can be mocked
    - Tests using `new Date()` should use specific dates, not the current one

  - How does this impact Build Faster support/Artifact builds/Dynamic builtin scalars or events? Will this be testable by others on artifact builds?
  - We work in the open: Does the change include words that might scare end users?
  - How does this handle client id resets?
  - How does this handle users opting out of data collection?
