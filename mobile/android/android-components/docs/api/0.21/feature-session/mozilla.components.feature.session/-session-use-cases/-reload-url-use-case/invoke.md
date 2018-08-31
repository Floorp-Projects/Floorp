---
title: SessionUseCases.ReloadUrlUseCase.invoke - 
---

[mozilla.components.feature.session](../../index.html) / [SessionUseCases](../index.html) / [ReloadUrlUseCase](index.html) / [invoke](./invoke.html)

# invoke

`fun invoke(session: Session = sessionManager.selectedSessionOrThrow): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Reloads the current URL of the provided session (or the currently
selected session if none is provided).

### Parameters

`session` - the session for which reload should be triggered.