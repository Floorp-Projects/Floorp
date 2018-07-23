# [Android Components](../../README.md) > Samples > Firefox Accounts (FxA)

![](src/main/res/mipmap-xhdpi/ic_launcher.png)

A simple app showcasing the service-firefox-account component.

## Concepts

The main concepts shown in the sample app are:

* Usage of the asynchronous result type `FxaResult`
* Setting up a`FirefoxAccount` object, from a previous session or from scratch
* Spawning a custom tab or a WebView to handle the user's authentication flow

A minimal walkthrough is also provided in the [component README](https://github.com/mozilla-mobile/android-components/tree/master/components/service/firefox-accounts).

## `FxaResult` usage

`FxaResult` represents a chainable asynchronous result, and is used as a convenient method of running potentially long-running tasks (eg. network requests, crypto operations) on threads outside of the UI thread.

A value or exception can be wrapped in an FxaResult:

```kotlin
val fxaValue = FxaResult.fromValue(42)
val fxaException = FxaResult.fromException(Exception("Something went wrong"))
```

One can attach `OnValueListener`s or `OnExceptionListener`s to an `FxaResult`. There are a few ways of chaining results in Kotlin:

* Passing the listeners directly via `then`, with object expressions or otherwise: 

	```kotlin
	FxaResult.fromValue(42).then(object : OnValueListener<Integer, Void> {
		override fun onValue(value: Integer): FxaResult<Void>? {
			// handle the value
			return FxaResult<Void>()
		}
	}, object : OnExceptionListener<Void> {
		override fun onException(exception: Exception): FxaResult<Void>? {
			// handle the exception
			return FxaResult<Void>()
		}
	})
	```

	Since Java 6 does not support simple lambda syntax, this is one of the main ways to chain `FxaResult`s in Java.

* Passing lambdas via `then`:

	```kotlin
	FxaResult.fromValue(42).then({ value: Int -> // valueListener
		// handle the value
		return FxaResult<Void>()
	}, { exception: Exception ->
		// handle the exception
		return FxaResult<Void>()
	}
	```

* Completing a chain via `whenComplete`:

	```kotlin
	FxaResult.fromValue(42).whenComplete { value: Integer ->
		// handle the value
	}
	```

	Since `whenComplete` implies that the chain of promises has come to an end, there is no need to return another FxaResult at the end.

## Setting up the account

### From a previous session

`FirefoxAccount` is a representation of the authentication state for the current client. It provides two methods for saving and restoring state: `toJSONString` and `fromJSONString`.

> The state provided by `toJSONString` should be stored securely, as the credentials inside could in theory let a user stay authenticated forever.

To restore an account from an existing state in shared preferences:

```kotlin
getSharedPreferences(FXA_STATE_PREFS_KEY, Context.MODE_PRIVATE).getString(FXA_STATE_KEY, "").let {
	FirefoxAccount.fromJSONString(it).whenComplete { this.account = it }
}
```

To persist an account's state in shared preferences:

```kotlin
account?.toJSONString().let {
    getSharedPreferences(FXA_STATE_PREFS_KEY, Context.MODE_PRIVATE).edit().putString(FXA_STATE_KEY, it).apply()
}
```

### From scratch

If no previous auth state was found, we have to create a new one using some default OAuth parameters. Find the hostname, or `CONFIG_URL` for your OAuth provider, then create a `CLIENT_ID` and `REDIRECT_URL` for your application. From there, we can create a `Config` object, and finally our `FirefoxAccount` object:

```kotlin
Config.custom(CONFIG_URL).whenComplete { value: Config ->
	this.account = FirefoxAccount(value, CLIENT_ID, REDIRECT_URL)
}
```

## Viewing the web pages

In order to complete the OAuth flow, the app can spawn a view and capture the code/state parameters in one of three ways:

* Opening a custom tab, then capturing params via intent filters
* Spawning a WebView with a page load hook
* Spawning an EngineView (WebView/GeckoView) [WIP]

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
