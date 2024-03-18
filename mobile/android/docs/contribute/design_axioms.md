---
layout: page
title: Design Axioms
permalink: /contributing/design-axioms
---

## 💚 100% Kotlin

Kotlin is the language of choice for components we develop, and it is the recommended language for application projects integrating components. We make use of Kotlin idioms and official Kotlin extension libraries (e.g. Coroutines).

Java interoperability is not a goal of this project. We may add annotations for Java interoperability (e.g. `@JvmStatic`) at the request of a consumer to facilitate integration, but we are not considering replacing Kotlin idioms for the sake of operability.

## 🔄 Third-party dependencies

As providers of a framework, we strive to be independent of other third-party libraries unless strictly needed (e.g. Android Jetpack libraries).

It is our goal to let our consumers decide what third-party libraries fit their needs, without our components making this choice or introducing an additional, duplicated stack of third-party frameworks.

Additionally, by limiting external dependencies, we intent to keep the byte size of our components and the consumer apps as small as possible.

## 🧰 Simple and applicable

We are building components that directly satisfy the needs of our consumers. We tend to not design components in a vacuum without a defined use case. Components are preferred to be simple and "to the point". Additional functionality is added as needed, never in advance without knowing if it will ever be used (or how).

## 🎨 Customizability

Different consumers have different requirements. If possible, we try to provide customization options (e.g. styling) as needed by the consumers. We aim to balance customizability and complexity. When too complex of customization options are required, we prefer alternative component implementations.

## 🧩 Pluggability

Even with customization options, not all components will be able to satisfy all needs of all consumers. As such, we strive to depend on interfaces (allowing different implementations) instead of concrete classes. At a component level we prefer the same abstraction and try to depend on "concept components" (e.g. `concept-toolbar`) and their interfaces instead of components providing a specific implementation (e.g. `browser-toolbar`).

## ✅ Testing

We strive to have a high code coverage with a high quality suite of tests. Tests are meant to prevent regressions, exercise various parts of the code base, prove correctness and assert an always shippable state.

## 📓 API Documentation

While we strive to keep the public API surface as simple and self-explanatory as possible, we provide KDocs for all public API methods to describe the methods purpose, parameters, return types, and possible exceptions, if applicable.
