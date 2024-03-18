# [Android Components](../../../README.md) > Concept > Awesomebar

An abstract definition of an awesome bar component.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md)):

```Groovy
implementation "org.mozilla.components:concept-awesomebar:{latest-version}"
```

### Implementing an Awesome Bar

An Awesome Bar can be any [Android View](https://developer.android.com/reference/android/view/View.html) that implements the `AwesomeBar` interface.

An `AwesomeBar` implementation needs to react to the following events:

* `onInputStarted()`: The user starts interacting with the awesome bar by entering text in the [toolbar](../toolbar/README.md). This callback is a good place to initialize code that will be required once the user starts typing.
* `onInputChanged(text: String)`: The user changed the text in the [toolbar](../toolbar/README.md). The awesome bar implementation should update its suggestions based on the text entered now.
* `onInputCancelled()`: The user has cancelled their interaction with the awesome bar. This callback is a good place to free resources that are no longer needed.

The suggestions an awesome bar displays are provided by an `SuggestionProvider`. Those providers are passed by the app (or another component) to the awesome bar by calling `addProviders()`. Once the text changes the awesome bar queries the `SuggestionProvider` instances and receives a list of `Suggestion` objects.

Once the user selects a suggestion and the awesome bar wants to stop the interaction it can invoke the callback provided via the `setOnStopListener()` method. This is required as the awesome bar implementation is unaware of how it gets displayed and how interaction with it should be stopped (e.g. leaving the [toolbar's](../toolbar/README.md) editing mode).

### Suggestions

A `Suggestion` object contains the data required to be displayed and callbacks for when a suggestion was selected by the user.

It is up to the suggestion or its provider to define the behavior that should happen in that situation (e.g. loading a URL, performing a search, switching tabs..).

All data in the `Suggestion` object is optional. It is up to the awesome bar implementation to handle missing data (e.g. show the `description` instead of a missing `title`).

Every `Suggestion` has an `id`. By default the `Suggestion` will generate a random ID. This ID can be used by the awesome bar to determine whether two suggestions are the same even though they are containing different/updated data. For example a `Suggestion` showing search suggestions from a search engine might use a constant ID when it is showing new search suggestions - to avoid the awesome bar implementation animating the previous suggestion leaving and a new suggestion appearing.

### Implementing a Suggestion Provider

For implementing a Suggestion Provider the `SuggestionProvider` interface needs to be implemented. The awesome bar forwards the events it receives to every provider: `onInputStarted()`, `onInputCancelled()`, `onInputChanged(text: String)`. A provider is required to return a list of `Suggestion` objects from `onInputChanged()`. This implementation can be synchronous. The awesome bar implementation takes care of performing the requests from worker threads.

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
