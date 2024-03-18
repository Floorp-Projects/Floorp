---
layout: page
title: RFC process
permalink: /rfc/0001-rfc-process
---

* Start date: 2020-07-07
* RFC PR: [#7651](https://github.com/mozilla-mobile/android-components/pull/7651)

## Summary

Introducing a lightweight RFC ("request for comments") process for proposing and discussing "substantial" changes and for building consensus.

## Motivation

The existing workflow of opening and reviewing pull requests is fully sufficient for many smaller changes.

For substantially larger changes (functionality, behavior, architecture), an RFC process prior to writing any code may help with:

* Publicly discussing a change proposal with other maintainers and consumers of components.
* Gathering and integrating feedback into a proposal.
* Documenting why specific changes and decisions were made.
* Building consensus among the team before potentially writing a lot of code.

A change is substantial if it

* affects multiple components;
* affects how components interact through either their public or internal APIs;
* fundamentally changes how a component is implemented, how it manages state or reacts to changes, in a way that isn't self-explanatory or a direct result of a bug fix.

## Guide-level explanation

The high-level process of creating an RFC is:

* Create an RFC document (like this one) using the template.
* Open a pull request for the RFC document.
* Ask for feedback on the pull request, via the [mailing list]() or in [chat](https://chat.mozilla.org/#/room/#android-components:mozilla.org).

During the lifetime of an RFC:

* Discussion happens asynchronously on the pull request. Anyone is allowed to engage in this discussion.
* Build consensus and integrate feedback.

After the discussion phase has concluded:

* If a consensus has been reached, then the RFC is considered "accepted" and gets merged into the repository for documentation purposes.
* If a consensus has not been reached, then the RFC is considered "rejected" and the pull request gets closed. The rejected RFC proposal may get revived should the requirements change in the future.

Once the RFC is accepted, then authors may implement it and submit the feature as a pull request.

## Drawbacks

* Writing an RFC is an additional overhead and may feel slower or cumbersome. The assumption is that the advantages still outnumber this drawback. In the hopes of better fitting the needs of the team at this time, this RFC process is simplified and deigned to be lightweight compared to other existing projects.

## Rationale and alternatives

Discussions about changes have been present without this process - mostly happening in more real-time means of communication such as Zoom, Slack or Riot. BThis resulted in forced synchronicity and closed platforms for those interested yet not involved parties. A slower RFC process allows more people to participate, avoids "secrets" and documents the discussion publicly.

## Prior art

Many other open-source projects are using an RFC process. Some examples are:

* [Rust RFCs - Active RFC List](https://rust-lang.github.io/rfcs/)
* [Bors: About the Draft RFCs category](https://forum.bors.tech/t/about-the-draft-rfcs-category/291)
* [seL4: The RFC Process](https://docs.sel4.systems/processes/rfc-process.html)
* [The TensorFlow RFC process](https://www.tensorflow.org/community/contribute/rfc_process?hl=en)

## Unresolved questions

* Is this process lightweight enough that it will be used?
