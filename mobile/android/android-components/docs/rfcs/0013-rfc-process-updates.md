---
layout: page
title: RFC process updates
permalink: /docs/rfcs/0013-rfc-process-updates
---

* RFC PR: https://github.com/mozilla-mobile/firefox-android/pull/5681
* Start date: 2024-02-20
* End date: 2024-03-22
* Stakeholders: @jonalmeida, @boek

## Summary

Clarify requirements for RFCs by introducing a README and a template.

## Motivation

- The scale of the Firefox Android team has grown substantially since the RFC process was first introduced.
- More external teams are using and contributing to Android Components.
- Implicit deadlines have encouraged a lack of engagement with RFCs, slowing follow-up work.
- There has been a low level of engagement with RFCs because of a lack of clarity around the process and when they are appropriate.

## Guide-level explanation

This proposal suggests improving the RFC process by introducing the following:

- An RFC template, to provide a clear starting point for proposals.
- A guide for when RFCs are appropriate and when they are not needed in the form of a README.
- A requirement for explicit stakeholders for RFCs.
- An initial deadline recorded in each proposal.
- A renaming of the the "Prior Art" section to "Resources and Docs" and wording to indicate that proposals can also include additional documentation.

## Rationale and alternatives

The RFC process has been successful in some cases, but has not been consistently followed. This proposal aims to make the process more accessible and clear, by introducing high-level concepts in the README and a clear template. These documents can be more easily treated as living documents, and updated with any future changes to the process.

### Amend RFC 0001 with additional template information
- This was not included in the proposal in order to keep past RFCs consistent and free of modern context, and so that past RFCs do not become treated as living documents.

## Resources and Docs

[README](./README.md), a new artifact that includes guidelines and advice on when RFCs are appropriate and how to contribute them.
[0000 RFC Template](./0000-template.md), a new artifact that provides a clear starting point for proposals.
[Follow-up bug for updating CODEOWNERS](https://bugzilla.mozilla.org/show_bug.cgi?id=1881373), so that stakeholders can be found more easily.

## Unresolved questions
- Will this result in an easier process for contributors to follow?
- Will this result in more engagement with RFCs?
