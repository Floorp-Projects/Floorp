---
layout: page
title: Hosting Android code in the Android Components repository
permalink: /contributing/hosting-android-code-in-repository
---

When developing a new (Mozilla) Android component - especially one that wraps **Rust** code for multiple platforms - one decision to make is where the Android (Kotlin/Java) code should live. Should it live in a project repository that also contains the Rust code or should it live in the *android-components* repository alongside the other Android code?

This document lists the advantages you get *for free* when hosting your Android code in the *android-components* repository (consuming pre-compiled binaries of your Rust code).

## Build

#### Managed build system

The *Android components* team maintains and updates the build process for local development and in automation.

This includes:
* Updating the build system and code to use the latest Android SDK, build tools and Gradle version
* Updating code across all components to avoid using deprecated APIs

## Testing

Having all components in the same repository means that they will be tested as a whole. We can more easily guarantee that all components work well together. We will be doing this with unit tests and integration tests.

This is specially important for service components, and becomes much more important once we are going to introduce more UI components that will need to use the service components.

## Releasing and Publishing

#### Frequent automated releases

The *Android components* team releases frequently (currently weekly) and the process is fully automated. Code changes will be part of the next release without any further steps.

#### Publishing

Release artifacts are automatically published on maven.mozilla.org and are immediately available to consumers.

The *Android components* team is planning to release SNAPSHOT builds for every successfull *main* merge. This will allow consuming apps to test approved changes immediately after merge without needing to wait for a release.

#### Consistent versioning and compatibility

All components are released together with a shared version number. This guarantees that all components with the same version number are compatible. With that a consuming app can use the components in the same setup as they were developed and tested in the *android-components* repository.

#### Less Fragmentation

All components will be together in the same repository in a clearly named and hierarchical structure.

## Documentation

When all components are in the same project, it is much simpler to generate consistent and linked documentation. As an example, see the [service-firefox-accounts](https://mozilla-mobile.github.io/android-components/api/0.19.1/service-firefox-accounts/index.html) API Reference that is automatically generated and published when we do a release.

This is not just a developer convenience, it is also highly convenient for users of the components (internal and external). It gives them a consistent view of the project as a whole.

## Tooling

The *Android components* team evaluates, integrates and maintains tools that run in automation for improving code quality, keeping a consistent code style, avoiding bugs and detecting performance issues. Those tools are getting run automatically for all pull requests and merges.

Currently we are using:
* [Android Lint](https://developer.android.com/studio/write/lint) - *"Android Lint is a tool which scans Android project sources for potential bugs."*
* [ktlint](https://github.com/shyiko/ktlint) - *"An anti-bikeshedding Kotlin linter with built-in formatter"*
* [detekt](https://github.com/arturbosch/detekt) - *"A static code analysis tool for the Kotlin programming language"*

In addition to that the *Android components* team started to write [custom lint rules](https://github.com/mozilla-mobile/android-components/tree/main/components/tooling/lint) that enforce component related rules (e.g. "Use the provided logging class in components instead of android.util.Log").

## Services

#### Integration of third-party services

The *Android components* team evaluates, integrates and maintains third-party services for code quality, testing and automation purposes.

Currently we are using:
* *Firebase Device Lab* for running tests on real devices.

The *Android components* team is currently evaluating [Gradle Enterprise](https://gradle.com/) to speed up builds and improve build reliability.

## Development and code quality

#### Consistent component APIs

The *Android components* team makes sure all components have consistent APIs, and use the same patterns and conventions so that consumers can rely on component interoperability.

#### Base component

The *Android components* team maintains a *base component* (`support-base`) that offers basic building blocks for components. This avoids code duplication ("Every components implements its own Observable") and allows the app to configure certain behaviors in a consistent way.

For example the *base component* offers components a logging class for logging inside components. The consuming app can configure how, if and when something gets logged for all components using this logging class. A similar setup is planned for telemetry and other functionality where the component needs to invoke something that the consuming app should be in control of.

## Dependencies

#### Optimized dependency graph

The *Android components* team ensures that the dependency graph is as minimal as possible by avoiding duplicates and version conflicts. This is much easier to do in a monorepo with a single buildchain. If fact, keeping dependency versions consistent across many different repositories is hard and somewhat unrealistic.

#### Consistent releases

The *Android components* team has a high release cadence. A release is cut at the end of every sprint. This makes updates of all components available to internal and external consumers at the end of every week.

## Ownership

You remain the owner of your components code. Each component can have a [CODEOWNERS](https://help.github.com/articles/about-codeowners/) file that allows us to specify who owns and reviews the code. This means individual teams can be easily responsible for a subtree of the components project that they own.

## Conclusion

The *Android components* team recommends hosting Android component code (Kotlin) in the *android-components* repository for the reasons mentioned above.
