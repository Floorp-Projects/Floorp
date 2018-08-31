---
title: SessionUseCases.LoadDataUseCase.invoke - 
---

[mozilla.components.feature.session](../../index.html) / [SessionUseCases](../index.html) / [LoadDataUseCase](index.html) / [invoke](./invoke.html)

# invoke

`fun invoke(data: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, mimeType: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, encoding: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "UTF-8", session: Session = sessionManager.selectedSessionOrThrow): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Loads the provided data based on the mime type using the provided session (or the
currently selected session if none is provided).

