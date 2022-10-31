---
layout: page
title: Architecture and Overview
permalink: /contributing/architecture
---

# Architecture and Overview

Our main design goal is to provide independently reusable Android components. We strive to keep dependencies between components as minimal as possible. However, standalone components aren't always feasible, which is why we have grouped components based on their interactions and dependencies.

On the lowest level, we provide standalone UI components (e.g. autocomplete, progressbar, colors) as well as independent service and suppport libraries (e.g. Telemetry, Kotlin extensions and Utilities).

The second level consist of so called `Concept` modules. These are abstractions that describe contracts for component implementations such as `Engine` or `Session Storage`, which may in turn have multiple implementations. The purpose of these concepts is to allow for customization and pluggability. Therefore, where available, components should always depend on concept modules (not their implementations) to support bringing in alternative implementations.

On top of `Concept` modules we provide `Browser` components. These components provide browser-specific functionality by implementing concepts and using lower level components.

On the highest level, we provide `Feature` components which provide use case implementations (e.g search, load URL). Features can connect multiple `Browser` components with concepts, and will therefore depend on other components.

The following diagram does not contain all available components. See [Components]({{ site.baseurl }}/components/) for a complete and up-to-date list.

```
    ┌─────────────────────┬───────────────────────────────────────────────────────────────────────┐
    │                     │ ┌───────────────────────────────────────────────────────────────────┐ │
    │                     │ │                              Feature                              │ │
    │  Features combine   │ │ ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─   ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─  │ │
    │ browser components  │ │       Session      │         Toolbar      │         Search      │ │ │
    │    with concepts    │ │ └ ─ ─ ─ ─ ─ ─ ─ ─ ─    └ ─ ─ ─ ─ ─ ─ ─ ─ ─   └ ─ ─ ─ ─ ─ ─ ─ ─ ─  │ │
    │                     │ └───────────────────────────────────────────────────────────────────┘ │
    ├─────────────────────┼───────────────────────────────────────────────────────────────────────┤
    │                     │ ┌───────────────────────────────────────────────────────────────────┐ │
    │                     │ │                              Browser                              │ │
    │ Browser components  │ │ ┌ ─ ─ ─ ─ ─ ─ ┐ ┌ ─ ─ ─ ─ ─ ─ ┐  ┌ ─ ─ ─ ─ ─ ─ ┐  ┌ ─ ─ ─ ─ ─ ─ ┐ │ │
    │   may implement     │ │  Engine-Gecko       Search           Toolbar        Errorpages    │ │
    │  concepts and use   │ │ └ ─ ─ ─ ─ ─ ─ ┘ └ ─ ─ ─ ─ ─ ─ ┘  └ ─ ─ ─ ─ ─ ─ ┘  └ ─ ─ ─ ─ ─ ─ ┘ │ │
    │    lower level      │ │ ┌ ─ ─ ─ ─ ─ ─ ┐ ┌ ─ ─ ─ ─ ─ ─ ┐  ┌ ─ ─ ─ ─ ─ ─ ┐  ┌ ─ ─ ─ ─ ─ ─ ┐ │ │
    │     components      │ │  Engine-System      Session          Domains           Menu       │ │
    │                     │ │ └ ─ ─ ─ ─ ─ ─ ┘ └ ─ ─ ─ ─ ─ ─ ┘  └ ─ ─ ─ ─ ─ ─ ┘  └ ─ ─ ─ ─ ─ ─ ┘ │ │
    │                     │ └───────────────────────────────────────────────────────────────────┘ │
    ├─────────────────────┼───────────────────────────────────────────────────────────────────────┤
    │                     │ ┌───────────────────────────────────────────────────────────────────┐ │
    │  Abstractions and   │ │                              Concept                              │ │
    │   contracts for     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │     component       │ │               Engine          │             Toolbar          │    │ │
    │  implementations    │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ └───────────────────────────────────────────────────────────────────┘ │
    ├─────────────────────├───────────────────────────────────────────────────────────────────────┤
    │                     │ ┌────────────────────────────────┐ ┌────────────────────────────────┐ │
    │                     │ │               UI               │ │            Service             │ │
    │     Standalone      │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │     components      │ │          Autocomplete     │    │ │           Telemetry       │    │ │
    │                     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ └────────────────────────────────┘ │
    │                     │ │            Progress       │    │ ┌────────────────────────────────┐ │
    │                     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │            Support             │ │
    │                     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ │             Colors        │    │ │       Kotlin extensions   │    │ │
    │                     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ │             Fonts         │    │ │           Utilities       │    │ │
    │                     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │
    │                     │ │    ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │                                │ │
    │                     │ │             Icons         │    │ │                                │ │
    │                     │ │    └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─     │ │                                │ │
    │                     │ └────────────────────────────────┘ └────────────────────────────────┘ │
    └─────────────────────┴───────────────────────────────────────────────────────────────────────┘
```
