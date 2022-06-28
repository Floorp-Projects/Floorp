Breaking changes in GeckoView
=============================

Agi sferro <agi@sferro.dev>

Abstract
--------

This document describes the reasoning behind the GeckoView deprecation policy,
where we are today and where we want to be in the future.

Background
----------

The following sections illustrate how breaking changes are expensive and
frustrating as a consumer of GeckoView, as a Gecko engineer and as an external
consumer, how they take away time from the Fenix team and reduce the average
testing time on Nightly up to 30%. And finally, how breaking changes negate the
very advantages that brought us to the current modularized architecture.

Introduction
------------

GeckoView is a library that provides consumers access to Gecko and is the main
way through which Gecko is consumed on Mozilla’s Android products.

GeckoView provides Nightly, Beta and Release channels which update with the
same cadence as Firefox Desktop does.

Firefox for Android (code name Fenix) is developed on a standalone repository
on GitHub and uses GeckoView through Android Components (AC for short), an
Android library also developed on its own standalone repository.

Fenix also provides Nightly, Beta and Release updates that mirror GeckoView and
Firefox Desktop’s.

Testing days
------------

All Firefox Gecko-based products release a new major version every 4 weeks.
Which means that, on average, a commit that lands on a random day during the
release cycle gets 2 weeks of testing time on the Nightly user base.

We try to increase the average testing time on Nightly by having a few “soft”
code-freeze days before each Merge day where engineers are not supposed to push
risky changes, but there’s no enforcement and it’s left to each engineer to
decide whether their change is risky or not.

Each day where the Nightly build is delayed, every change contained in the
current Nightly cycle gets 7% (1 out of 14 days) on average less testing that
it normally would during a build. That is assuming that a problem gets
immediately reported and the report is immediately referred to the right
Engineering team.

Assuming a 4 days report delay, each day where the Nightly build is delayed,
due to reasons such as breaking changes, reduces the average testing time by
10%.

Nightly update
--------------

Fenix Nightly consumes GeckoView indirectly through Android Components. Each
day, an automated script makes a change in Fenix’s codebase to update AC’s
version. This change is then submitted to Fenix’s CI and, if all tests pass, is
merged to the codebase automatically.

A new Fenix Nightly build is then generated and automatically published to
Google’s Play Store, from where it gets distributed to all Nightly users on
Android.

Android Components has a similar automated process which publishes new versions
every day, picking up the new GeckoView nightly build.

The update process fails from time to time. The cause of the failure largely
falls in one of the following three buckets.

- An intermittent test failure
- A bug introduced in the latest AC or GeckoView update which causes a test to
  fail
- A backward incompatible change has been made in AC or GeckoView that breaks
  the build.

The current mitigation for 1 is to disable or fix tests that fail
intermittently, similarly to what happens in mozilla-central.

2 and 3 are problems unique to Fenix and AC (as compared to Firefox Desktop)
and are a direct consequence of the multi-package infrastructure of Fenix.

Build breakages
---------------

When the automated Nightly update fails, an engineer on the Fenix team needs to
manually intervene to unblock the build.

The need for a manual intervention automatically adds a day of Nightly build
delay when the failure occurs outside of business hours, and 2 or 3 days of
delay when the failure happens on a Friday night.

Therefore, even assuming that a build breakage takes no time to fix, the
average testing time is reduced by 7-30% for each build breakage that occurs.

In the case where the breakage takes a few days or more to fix, the average
testing time can be reduced to as much as half of what it would be on a
breakage-free Nightly cycle.

Build breakages put undue burden on the Fenix team, who has to jump on the
breakage and has to drop their current work to avoid losing additional testing
days.

Reducing breakages
------------------

Breakages caused by upstream teams like GeckoView can be divided into 2 groups:

- Behavior changes that cause test failures downstream
- Breaking changes in the API that cause the build to fail.

To reduce breakages from group 1, the GeckoView team maintains an extensive set
of integration tests that operate solely on the GeckoView API, and therefore
rarely break because of refactoring.

For group 2, the GeckoView team instituted a deprecation policy which requires
each backward-incompatible change to keep the old code for 3 releases, allowing
downstream consumers, like Fenix, time to migrate asynchronously to the new
code without breaking the build.

Functional testing and prototyping
----------------------------------

GeckoView offers a test browser app called GeckoViewExample (or GVE) that is
developed in-tree and thus always available to test local changes.

GVE is the main testing vehicle for Gecko and GeckoView engineers that want to
develop new code, however, there frequently are issues or new features that
cannot be tested on GVE and need to be tested directly on Fenix.

To test new code in Fenix, the build system offers an easy way to swap
locally-build GeckoView in Fenix.

The process of testing new Gecko code in Fenix needs to be straightforward, as
it’s often used by platform engineers that are unfamiliar with Android and
Fenix itself, and are not likely to retain knowledge from running code on
Android and would likely need help to do so from the GeckoView or Fenix team.

Side-effects of build breakages
-------------------------------

When a breakage lands in mozilla-central and until the breakage is fixed in the
Fenix codebase, a locally built GeckoView is not compatible with the
most-recent tip of Fenix.

This can be confusing to an engineer that is unfamiliar to Fenix, and can cause
frustration and time lost trying to figure out why upstream code, without
modifications, fails to compile.

Beyond confusion, an incompatibility on the GeckoView/Fenix combined history
negates the primary advantage of building Fenix in a separate package:
decoupling Gecko from the Android front-end.

Building older versions from source is also harder, as the set of version
couples (GeckoView, Fenix) that are compatible with each other is not
explicitly documented anywhere.

External consumers
------------------

For apps interested in building a browser for Android, GeckoView provides the
unique combination of being a modern Web engine with a relatively stable API.

For comparison, alternatives to GeckoView include:

- WebView, Android’s way of embedding web pages on Android apps. WebView has
  has several drawbacks for browser developers, including:

  - having a limited API for building browsers, as it does not expose modern
    Web features or browser-specific APIs like bookmarks, passwords, etc;
  - not allowing developers to control the underlying Chromium version. WebView
    users will get whatever version of WebView is installed on the device.
  - On the other hand, using WebView has the advantage of providing a smaller
    download package, as the bulk of the engine is already installed on the
    device.

- Fork Chromium, which has the drawback of either having to rewrite the entire
  browser front-end or locally patching the Chrome front-end, which involves
  frequent changes and updates to be on top of. Using Chromium has the advantage
  of providing the most stable, performant and compatible Web Engine on the
  market.

If the cost of updating GeckoView becomes high enough because of frequent API
changes, the advantage of using GeckoView is negated.

Prior Art
---------

Many public libraries offer a deprecation policy similar or better than
GeckoView. For example, Android APIs need to be deprecated for a few releases
before being considered for removal, and completely removed only in exceptional
cases. Google products’ deprecated APIs are supported for a year before being
removed. Ebay requires deprecating an API before removal.

Status quo
----------

Making backward-incompatible changes to the GeckoView API is currently heavily
discouraged and requires approval by the GeckoView team.

We do, however, have breaking changes from time to time. The last breaking
change was in June 2021, a refactor of the permission API which we didn’t think
was worth executing in a backward compatible way. Before that, the last
breaking change was in September 2020.

Tracking breaking changes
-------------------------

Internally, GeckoView tracks the API using apilint. Each change that touches
the API requires an additional GeckoView peer to review the patch and a
description of the change in the changelog.

Apilint also tracks deprecated APIs and enforces their removal, so that old,
deprecated APIs don’t linger in the codebase for longer than necessary.

The future
----------

The ideal end state for GeckoView would be to not have any more backward
incompatible changes. Our experience is that supporting the old APIs for a
limited time is a small overhead in our development and that the benefits from
having a backward compatible API greatly outweigh the cost.

We cannot, however, predict all future needs of GeckoView and Firefox as a
whole, so we cannot exclude the possibility of having new breaking changes
going forward.
