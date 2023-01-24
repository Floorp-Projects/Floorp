For an overview of our current architecture, please see [this document](https://github.com/mozilla-mobile/fenix/blob/master/docs/architecture-overview.md)

--- 

These are some of the major architecture decisions we've made so far in Fenix. [Why?](http://thinkrelevance.com/blog/2011/11/15/documenting-architecture-decisions)

---

# Overview

Several apps have suffered from insufficient attention to appropriate Android application architecture. This leads to spaghetti code, God objects, and leaky abstractions that make testing and maintaining our apps significantly more costly and challenging.

---

# Goals

Our architecture should:

* Have classes which aim to fulfill a single responsibility, thereby separating concerns.
* Improve the ability of developers to write effective automated unit and UI tests.
* Make the bulk of application code more readable, easing onboarding of contributors.
* Improve the ability to debug application errors with useful stack traces.
* Be amenable to code reuse when desirable.
* Handle A/B experimentation without vast code branches or confusion.

---

# Special Concerns

As a browser, Fenix will need to interface with the Android Components and the GeckoView rendering engine. This means any architecture we choose must not require that all components of the app are implemented similarly. Also, our application state will need to synchronize with the engine’s state as the two will not always be in perfect sync due to hidden internals.

---

# Component Architecture

## Context

A/B testing of layout and UX is a fundamental necessity of Fenix. We have a lot of hypotheses needing validation and actual usage data about what might make a browser better to acquire and retain more users. A lot of questionable decisions have been made in older mobile browsers that do not seem ideal for mobile devices, but we need data to tell us if our assumptions are correct.

## Decision

We did a review of modern app architectures used by companies throughout the tech industry and came across the Netflix "componentization" architecture. Netflix had a special desire to A/B test a lot of different layouts for their app's user interface and built their architecture for this express purpose.

Netflix's architecture moves all UX-related code away from activities and fragments. Instead, fragments subscribe to components through RxJava and components inflate themselves.

The actual components' inflated views are dropped into ConstraintLayouts and are tied together by applying programmatic ConstraintSets. ConstraintLayouts and ConstraintSets also tie nicely into the new and ultra-powerful MotionLayouts to empower rich animations.

Here are some videos of Juliano Moraes of Netflix describing their architecture:

[DroidCon NYC part 1 video](https://www.youtube.com/watch?v=dS9gho9Rxn4)

[DroidCon SF part 2 video](https://www.youtube.com/watch?v=1cWwfh_5ZQs)

Here's a [sample repository](https://github.com/julianomoraes/componentizationArch) demonstrating in a very simple form how it functions.

The goal of software architecture is to minimize the cost of change. This decision is possibly the biggest factor for reducing the cost of changes to Fenix. It also plays well with the Android Components project, which provides so many of the components that will make up this project.

## Consequences

We will package the UI into components which are reusable and can be remixed for A/B layout tests.

We will keep all UI/UX code out of activities and fragments to obey the [Single Responsibility Principle](https://blog.cleancoder.com/uncle-bob/2014/05/08/SingleReponsibilityPrinciple.html). These classes will exist to handle OS business logic and to bind components.

One downside of this is the extra cost of getting contributors to avoid making UI/UX changes directly from activities and fragments. Another is programmatic layouts don't play well with previewing layouts. We'll all need to learn how to use ConstraintLayouts and ConstraintSets effectively.

---

# Localized, MVI (Model-View-Intent) Unidirectional Architecture

## Context

Race conditions are the bane of Android apps everywhere. They often happen because multiple systems cannot agree about state.

## Decision

The best solution is to have all state changes flow in a functional, reactive manner out from a single source of truth with careful thread locking as required. This solution will be familiar to anyone who has worked with Redux or Flux.

Within each component, we'd like state changes to occur in a cycle. The user interface's contents are rendered by presenting an initial ViewState to the screen. When the user interacts with the app, a Change object is passed to a state reducer. The reducer function copies the current, immutable state with the requested changes and passes it to the Model. The View subscribes to these state changes and updates itself reactively.

In this manner, the UX of the app only flows in a single direction from a source of truth. Because all changes happen in a serialized RxKotlin Observable, they will be applied in the order they happen.

However, unlike some MVI architectures, we will not focus on keeping a global ViewState that encompasses all state. There are times when we want OS calls, NDK calls, and third party component calls to be the source of truth. Rather than trying to make a single ViewState that contains all state, we'll be able to observe a merged observable of all actions and state changes. This will be invaluable for debugging the app.

Here's a [state diagram of the MVI architecture](https://staltz.com/img/mvi-unidir-ui-arch.jpg).

## Consequences

We will experiment with writing new components using MVI unidirectional principles. We will need to take care when passing data between components that we do not override a more authoritative source of truth.

Because all changes can be represented by a single, merged and serialized Observable or Flowable, we should be able to use this for debugging. All ViewStates, Changes, and Actions/Intents will be easily loggable to observe the causes of state issues.

---

