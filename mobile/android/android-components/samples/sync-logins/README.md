# [Android Components](../../README.md) > Samples > Firefox Sync - Logins

![](src/main/res/mipmap-xhdpi/ic_launcher.png)

A simple app showcasing the service-sync-logins component.

## Concepts

The main concepts shown in the sample app are:

* Usage of the asynchronous result type `SyncResult`.
* Login to Firefox Accounts that provides the necessary information to fetch the Logins from Firefox Sync.
* Getting the list of Logins from Firefox Sync.

## `SyncResult` usage

`SyncResult` represents a chainable asynchronous result, and is used as a convenient method of running potentially long-running tasks (eg. network requests, crypto operations) on threads outside of the UI thread.

A value or exception can be wrapped in an SyncResult:

```kotlin
val syncValue = SyncResult.fromValue(42)
val syncException = SyncResult.fromException(Exception("Something went wrong"))
```

One can attach `OnValueListener`s or `OnExceptionListener`s to an `SyncResult`. There are a few ways of chaining results in Kotlin:

* Passing the listeners directly via `then`, with object expressions or otherwise:

	```kotlin
	SyncResult.fromValue(42).then(object : OnValueListener<Integer, Void> {
		override fun onValue(value: Integer): SyncResult<Void>? {
			// handle the value
			return SyncResult<Void>()
		}
	}, object : OnExceptionListener<Void> {
		override fun onException(exception: Exception): SyncResult<Void>? {
			// handle the exception
			return SyncResult<Void>()
		}
	})
	```

	Since Java 6 does not support simple lambda syntax, this is one of the main ways to chain `SyncResult`s in Java.

* Passing lambdas via `then`:

	```kotlin
	SyncResult.fromValue(42).then({ value: Int -> // valueListener
		// handle the value
		return SyncResult<Void>()
	}, { exception: Exception ->
		// handle the exception
		return SyncResult<Void>()
	})
	```

* Completing a chain via `whenComplete`:

	```kotlin
	SyncResult.fromValue(42).whenComplete { value: Integer ->
		// handle the value
	}
	```

	Since `whenComplete` implies that the chain of promises has come to an end, there is no need to return another SyncResult at the end.


## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
