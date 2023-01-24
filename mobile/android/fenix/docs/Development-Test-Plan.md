It's important to know where we require tests and where they don't add significant value. This document intends to lay out that distinction.

In general, it's not necessary to test that the Android framework does what it says it does. If a code path sets the value of a TextView, one does not need to write an Espresso or Robolectric test to check that the TextView was set. This is why the Android framework team at Google has tests. Espresso tests are expensive in terms of runtime and should be used to ensure that all units of our software are correctly integrated together as a whole. Even Robolectric imposes a noticeable cost on test runtime, but it has proper uses.

### Components

Our app architecture is tied together by components that render UI. Generally, it's smart to write a set of unit tests for every UI component, but to mock the view. This can be accomplished by creating a Spy for the component. One may observe inputs and outputs of the component under test using RxKotlin TestObservers.

When testing Components, it's important to test any significant logic. If a reducer has more logic than mere Kotlin copy() operators, it should be tested.

### UIViews

When testing UIViews, we don't need to verify TextView set calls are made, but we should verify any branching logic that exists. If the ViewState is not basically ready to display, those transformations probably belong in the component and should be tested there. If the UIView class is simply a straight conduit from the ViewState directly to the UI, then there is no need for unit tests.

### Fragments

Fragments can get complicated. In theory, there should not be much View logic if the architecture is correctly integrated. One can mock out any components and the extension methods which perform layout and then use Robolectric to verify significant logic.

Generally, there shouldn't be large, branching blocks directly inside Android lifecycle callbacks. These should be broken out into methods, which can be tested on their own.

### Activities

Activities should exist only as entry points to the app in order to display fragments. The logic should be limited and should be testable as Robolectric tests with TestNavigators for testing navigation.