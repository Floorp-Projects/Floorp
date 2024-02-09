# The RFC Process

An RFC (**R**equest **F**or **C**omments) is a process through which contributors can solicit buy-in
for proposed changes to the codebase and repository at-large. It was introduced in the first RFC,
[0001-rfc-process](./0001-rfc-process.md), which includes additional details about the reasoning
for including the process. 

This is an overview of what kind of changes benefit from or require the consensus-building that the 
RFC process provides, as well as a brief guide on how to draft them.

## What kinds of changes require an RFC?

1. Substantial changes to public APIs in Android Components, like the changes found in [0003 Concept Base Component](./0003-concept-base-component.md) and [0008 Tab Groups](docs/rfcs/0008-tab-groups.md). 
2. Changes to process that affect other teams, like the changes found in [0001 RFC Process](./0001-rfc-process.md), [0013 Add stakeholders to RFCs](./0013-rfc-process-updates.md), and [0007 Synchronized Releases](./0007-synchronized-releases.md).
3. Proposals for changes to areas of the codebase that are owned by CODEOWNERS outside the author's team.

## What kind of other changes can an RFC be useful for?

1. Announcing a rough plan for changes to a public API you own in order to solicit feedback.
2. Soliciting feedback for architectural changes that affect the entire codebase, like [0009 Remove Interactors and Controllers](./0009-remove-interactors-and-controllers.md).

## How to contribute an RFC

There is a [template](./0000-template.md) that can be a useful guide for structure.

While drafting a proposal, consider the scope of your changes. Generally, the level of detail should match the level of 
impact the changes will have on downstream consumers of APIs, other teams, or users.

Once a proposal is drafted:

1. Choose a deadline for general feedback.
2. Select and communicate with stakeholders.
3. Share the RFC more broadly through Slack and mailing lists for general feedback (like firefox-android-team@ and firefox-mobile@).
4. Build consensus and integrate feedback.

### Stakeholders

Stakeholders are required for each RFC. They will have the final say in acceptance and rejection. 
Include at least 2 people as stakeholders: a CODEOWNER of the affected area and another (preferably a Firefox for Android team member).

Stakeholders should be active in the RFC process - they should ask to be replaced if they do not have bandwidth to get the RFC finished in a short time span. This is to help the RFC process remain nimble and lightweight.

### Deadlines

A deadline for feedback should be included in each RFC. This should usually be at least a week, so plan accordingly. 
For more substantial changes, it can be useful to plan for 2 or 3 weeks so that there is more opportunity for feedback from people that are not stakeholders.
If a proposal is approved by all stakeholders earlier than the deadline, the proposal can be merged immediately.