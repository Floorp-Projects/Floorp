# Unit testing glean metrics

In order to support unit testing inside of client applications using glean, a set of testing API 
functions have been included.  The intent is to make glean easier to test 'out of the box' in any
client application it may be used in.  These functions expose a way to inspect and validate recorded
metric values within the client application but are restricted to test code only through visibility
annotations (`@VisibleForTesting(otherwise = VisibleForTesting.NONE)`).

## General test API method semantics

In order to prevent issues with async calls when unit testing glean, it is important to put glean
into testing mode by calling `Glean.enableTestingMode()`.  This will ensure that metrics are done
recording when the other test functions are used.

To check if a value exists (i.e. it has been recorded), there is a `testHasValue()` function on each 
of the metric instances:

```kotlin
assertTrue(GleanMetrics.Foo.UriCount.testHasValue())
```

To check the actual values, there is a `testGetValue()` function on each of the metric instances.  
This function will return a datatype appropriate to the specific type of the metric it is being used 
with:

```kotlin
assertEquals(5, GleanMetrics.Foo.UriCount.testGetValue())
```

Finally there is a function `testClearAllData()` that will help to clear out all collected data, 
such as you would do in an `@Before` block prior to running a new test, or in an `@After` block to 
clean up after a test.  This function is called off of the main `Glean` singleton object:

```kotlin
Glean.testClearAllData()
```

Note that each of these functions has it's visibility limited to the scope to only unit tests by
making use of the `@VisibleForTesting` annotation, so the IDE should complain if you attempt to use
them inside of client code.

## Testing metrics for custom pings

In order to test metrics where the metric is included in more than one ping, the test functions take 
an optional `pingName` argument.  This is the name of the ping that the metric is being sent in, 
such as `events` for the "events" ping, or `metrics` for the "metrics" ping.  This could also be a 
custom ping name that the metric is being sent in.  In most cases you should not have to supply the 
ping name to the test function and can just use the default which is the "default" ping that this 
metric is sent in.  You should only need to provide a `pingName` if the metric is being sent in more
than one ping in order to identify the correct metric store.
 
You can call the `testHasValue()` and `testGetValue()` functions with `pingName` like this:

```kotlin
GleanMetrics.Foo.UriCount.testHasValue("customPing")
GleanMetrics.Foo.UriCount.testGetValue("customPing")
``` 

## Example of using the test API

Here is a longer example to better illustrate the intended use of the test API:
```kotlin
// Enable testing mode
Glean.enableTestingMode()

// Record a metric value with extra to validate against
GleanMetrics.BrowserEngagement.click.record(
    mapOf(
        BrowserEngagement.clickKeys.font to "Courier"
    )   
)

// Record more events without extras attached
BrowserEngagement.click.record()
BrowserEngagement.click.record()

// Check if we collected any events into the 'click' metric
assertTrue(BrowserEngagement.click.testHasValue())

// Retrieve a snapshot of the recorded events
val events = BrowserEngagement.click.testGetValue()
      
// Check if we collected all 3 events in the snapshot
assertEquals(3, events.size)

// Check extra key/value for first event in the list
assertEquals(“Courier”, events.elementAt(0).extra[“font”])

```