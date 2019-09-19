---
layout: page
title: Versioning and release process
permalink: /contributing/versioning
---

The *Android components* project uses a similar [versioning and release process as Firefox](https://wiki.mozilla.org/Release_Management/Release_Process) (New major version releases at fixed intervals).

## Major releases

* Major releases happen at fixed intervals (currently on Tuesday every week)
* Every release increments the major version (e.g. 1.0.0 -> 2.0.0 -> 3.0.0)
* Major releases are tagged on and shipped from the `master` branch.

## Point releases

* Point releases happen on demand whenever an app project requires a new version but cannot update to the latest release version yet or whenever a new release version is not available yet.
* Point releases are created from a release branch, created from a previously tagged release.
* The minor or patch version is increased depending on whether the release introduces backported functionality or just fixes bugs (e.g. 1.0.0 -> 1.1.0 or 2.0.0 -> 2.0.1).

## Rationale and alternatives

#### Release cycle and stability

The *Android components* project follows a rapid release cycle to get new functionality into the hands of consuming apps as soon as possible. For this reason the project does not have a long release stability phase (e.g. 1.0.0 -> 1.0.1 -> 1.0.2 -> 1.1.0 -> 1.1.1).

App teams are encouraged to update to the latest release frequently. The *Android Components* team tries to minimize the amount of breaking API changes. Frequent updates minimize the amount of API changes when updating *Android Components*.

#### Versioning

All *Android Components* are released together using a unified version number. This makes it easy for the *Android Components* team to guarantee that this set of components is guaranteed and tested to work together.

The downside of a shared version is the loss of [semantic versioning](https://semver.org/) semantics on a per component level. It is entirely possible that a component is released using  a new major version and there's not a single code change in this component in the new release. Instead the [changelog](https://mozac.org/changelog/) is the primary mechanism to communicate major and minor changes in a component.

At this time individually semantically versioned components would introduce a too large maintenance overhead for the *Android Components* team (Individual releases, release manager/owner for every component, guaranteeing compatibility across many versions).
