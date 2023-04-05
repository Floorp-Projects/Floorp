## Architecture Overview

### Unidirectional data flow

Firefox Preview's presentation layer architecture is based on the concept of "unidirectional data flow." This is a popular approach in client side development, especially on the web, and is core to Redux, MVI, Elm Architecture, and Flux.  Our architecture is not identical to any of these (and they are not identical to each other), but the base concepts are the same. For a basic understanding of the motivations and approach, see [the official Redux docs](https://redux.js.org/basics/data-flow).  For an article on when unidirectional data flow is and is not a good approach, see [this](https://medium.com/swlh/the-case-for-flux-379b7d1982c6). These are both written from the perspective of React.js developers, but the concepts are largely the same.

Our largest deviation from these architectures is that while they each recommend one large, global store of data, we have a single store per screen. This carries both benefits and drawbacks, both of which will be covered later in this document.

### Important objects

### <a name="store"/>Store
#### Overview
A store of state

See [mozilla.components.lib.state.Store](https://github.com/mozilla-mobile/android-components/blob/main/components/lib/state/src/main/java/mozilla/components/lib/state/Store.kt)

Pushes changes to: [View](#view)

Receives [Actions](#action) from: [Controller](#controller)

#### Description
Maintains a [State](#state) object and a [Reducer](#reducer). Whenever the Store receives a new [Action](#action) via `store.dispatch(action)`, it calls the [Reducer](#reducer) with the previous state and the new action. The result is then stored as the new State, and published to all consumers of the store.

It is recommended that consumers rely as much as possible on `consumeFrom(store)`, rather than querying `store.state`. `consumeBy` is called any time state is updated, ensuring that the most up to date data is always used. This can prevent subtle bugs around call order, as all observers are notified of the same state change before a new change is applied.

Note that there is one Store for any given screen, and only one will be active at any given time. Stores are persisted across configuration changes, but created and destroyed during fragment transactions. This means that data that must be shared across Stores must be passed as arguments to the new fragment.

Stores should be created using [StoreProvider#get](https://github.com/mozilla-mobile/fenix/blob/main/app/src/main/java/org/mozilla/fenix/components/StoreProvider.kt).

-------

### <a name="action"/>Action
#### Overview
Simple description of a state change

See [mozilla.components.lib.state.Action](https://github.com/mozilla-mobile/android-components/blob/main/components/lib/state/src/main/java/mozilla/components/lib/state/Action.kt)

Created by: [Controller](#controller)

Is pushed to: [Store](#store)

#### Description
Simple data object that carries information about a [State](#state) change to a [Store](#store).  An Action describes _something that happened_, and carries any data relevant to that change. For example, `AccountSettingsFragmentAction.SyncFailed(val time: Long)`, which describes a sync that failed at a specific time.

-------

### <a name="state"/>State
#### Overview
Description of the state of a screen

See [mozilla.components.lib.state.State](https://github.com/mozilla-mobile/android-components/blob/main/components/lib/state/src/main/java/mozilla/components/lib/state/State.kt)

Referenced by: [Store](#store)

#### Description
Simple, immutable data object that contains all of the backing data required to display a screen. This does not include style details like colors and view sizes, which are handled by the [View](#view).

As much as possible, the State object should be an accurate, 1:1 representation of what is actually shown on the screen. That is to say, the screen should look exactly the same any time a State with the same values is emitted, regardless of any previous changes. This is not always possible as Android UI elements are very stateful, but it is a good goal to aim for.

One major benefit of rendering a screen based on a State object is its impact on testing. UI tests are notoriously difficult to build and maintain. If we are able to build a simple, reproducible [View](#view) (i.e., if we can trust that the View will render as expected), that allows us to test our UI by verifying the correctness of our State object.

This also gives us a major advantage when debugging. If the UI looks wrong, check the State object. If it's correct, the problem is in the View binding. If not, check that the correct [Action](#action) was sent. If so, the problem is in the reducer. If not, check the [Controller](#controller) that sent the Action. This helps us quickly narrow down problems.

-------

### <a name="reducer"/>Reducer
#### Overview
Pure function used to create new [State](#state) objects

See [mozilla.components.lib.state.Reducer](https://github.com/mozilla-mobile/android-components/blob/main/components/lib/state/src/main/java/mozilla/components/lib/state/Store.kt)

Referenced by: [Store](#store)

#### Description
A function that accepts the previous State and an [Action](#action), then combines them in order to return the new State. It is important that all Reducers remain [pure](https://en.wikipedia.org/wiki/Pure_function). This allows us to test Reducers based only on their inputs, without requiring that we take into account the state of the rest of the app.

Note that the Reducer is always called serially, as state could be lost if it were ever executed in parallel.

-------

### <a name="interactor"/>Interactor
#### Overview
Called in response to a direct user action. Delegates to something else

Called by: [View](#view)

Calls: [Controllers](#controller), other Interactors

#### Description
This is the first object called whenever the user performs an action. Typically this will result in code in the [View](#view) that looks something like `some_button.onClickListener { interactor.onSomeButtonClicked() } `. It is the Interactors job to delegate this button click to whichever object should handle it.

Interactors may hold references to multiple other Interactors and Controllers, in which case they delegate specific methods to their appropriate handlers. This helps prevent bloated Controllers that both perform logic and delegate to other objects.

Sometimes an Interactor will only reference a single Controller. In these cases, the Interactor will simply forward calls to equivalent calls on the Controller. The Interactor does very little in these cases, and exists only to be consistent with the rest of the app.

Note that prior to the introduction of Controllers, Interactors handled the responsibilities of both objects. **You may still find this pattern in some parts of the codebase,** but it is being actively refactored out.

-------

### <a name="controller"/>Controller
#### Overview
Determines how the app should be updated whenever something happens

Called by: [Interactor](#interactor)

Calls: [Store](#store), library code (e.g., forward a back-press to Android, trigger an FxA login, navigate to a new Fragment, use an Android Components UseCase, etc)

#### Description
This is where much of the business logic of the app lives. Whenever called by an Interactor, a Controller will do one of the three following things:
- Create a new [Action](#action) that describes the necessary change, and send it to the Store
- Navigate to a new fragment via the NavController. Optionally include any state necessary to create this new fragment
- Interact with some third party manager. Typically these will update their own internal state and then emit changes to an observer, which will be used to update our Store

Controllers can become very complex, and should be unit tested thoroughly whenever their methods do more than delegate simple calls to other objects.

-------

### <a name="view"/>View
#### Overview
Initializes UI elements, then updates them in response to [State](#state) changes

Observes: [Store](#store)

Calls: [Interactor](#interactor)

#### Description
The view defines the mapping of State to UI. This includes initial setup of Views, and also typically includes an `update(state)` function that is called whenever State is changed.

Views should be as dumb as possible, and should include little or no conditional logic. Ideally, each primitive value in a State object is set on some field of a UI element, with no other logic included.

Views set listeners on to UI elements, which trigger calls to one or more Interactors.

-------

## Important notes
- Unlike other common implementations of unidirectional data flow, which typically have one global Store of data, we maintain a smaller Store for each screen.
- Stores and their State are persisted across configuration changes via an Architecture Components ViewModel.
- Whenever a fragment newly created or finished (by the ViewModel definition), its Store will also be created/destroyed
- Communication between Stores only happens by loading the appropriate data into arguments for the new fragment, which uses them to construct the initial state of the new Store.
  - We currently violate this in a few places, but are actively refactoring out those violations

## Simplified Example
When reading through live code trying to understand an architecture, it can be difficult to find canonical examples, and often hard to locate the most important aspects. This is a simplified example using a hypothetical app that should help clarify the above patterns. These patterns are overkill for the problems being solved, but keep in mind that the example is deliberately simplified.

![example app wireframe](./architectureexample/example-app-wireframe.png?raw=true)

This app currently has three (wonderful) features.
- Clicking on one of the colored circles will update the toolbar color
- Clicking on 'Rename', typing a new name, and selecting return will update the name of the contact
- Clicking anywhere else on a contact will navigate to a text message fragment

These link to the architectural code that accomplishes those features:
- [ContactsView](./architectureexample/ContactsView.kt)
- [ContactsInteractor](./architectureexample/ContactsInteractor.kt)
- [ContactsController](./architectureexample/ContactsController.kt)
- [ThemeController](./architectureexample/ThemeController.kt)
- [ContactsStore](./architectureexample/ContactsStore.kt)
- [ContactsState](./architectureexample/ContactsStore.kt)
- [ContactsReducer](./architectureexample/ContactsStore.kt)
- [ContactsFragment](./architectureexample/ContactsFragment.kt)

## Known Limitations
There are a few known edge cases and potential problems with our architecture, that in certain circumstances can be confusing.

- Since [Stores](#store) live at the fragment level, our architecture does not define any way to set data outside of that scope.
  - For example, if it is determined during application startup that we need to run in private mode, it must eventually be passed to a fragment, but we don't specify how it will be handled until that point.
  - We have no defined way to set values shared by all fragments. They must either be passed as an argument to every individual fragment, or use some system outside of our architecture (e.g., by accessing SharedPreferences).
- There isn't always a clear logical distinction between what should provoke a state change (by dispatching an [Action](#action) to a [Store](#store)), and what should start a new fragment. Passing arguments while creating a new fragment causes changes to the new [State](#state) object, while taking a very different code path than the rest of our app would.
- Many [Interactors](#interactor) have only one dependency, on a single [Controller](#controller). In these cases, they typically just forward each method call on and serve as a largely unnecessary layer. They do, however, 1) maintain consistency with the rest of the architecture, and 2) make it easier to add new Controllers in the future.
