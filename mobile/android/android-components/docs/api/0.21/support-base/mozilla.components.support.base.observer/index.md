---
title: mozilla.components.support.base.observer - 
---

[mozilla.components.support.base.observer](./index.html)

## Package mozilla.components.support.base.observer

### Types

| [Consumable](-consumable/index.html) | `class Consumable<T>`<br>A generic wrapper for values that can get consumed. |
| [Observable](-observable/index.html) | `interface Observable<T>`<br>Interface for observables. This interface is implemented by ObserverRegistry so that classes that want to be observable can implement the interface by delegation: |
| [ObserverRegistry](-observer-registry/index.html) | `class ObserverRegistry<T> : `[`Observable`](-observable/index.html)`<`[`T`](-observer-registry/index.html#T)`>`<br>A helper for classes that want to get observed. This class keeps track of registered observers and can automatically unregister observers if a LifecycleOwner is provided. |

