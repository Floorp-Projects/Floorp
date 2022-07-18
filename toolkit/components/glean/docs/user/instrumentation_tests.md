# Writing Instrumentation Tests

```{admonition} Old Glean Proverb
If it's important enough to be instrumented, it's important enough to be tested.
```

All metrics and pings in the Glean SDK have [well-documented APIs for testing][glean-metrics-apis].
You'll want to familiarize yourself with `TestGetValue()`
(here's [an example JS (xpcshell) test of some metrics][metrics-xpcshell-test])
for metrics and
[`TestBeforeNextSubmit()`][test-before-next-submit]
(here's [an example C++ (gtest) test of a custom ping][ping-gtest])
for pings.

All test APIs are available in all three of FOG's supported languages:
Rust, C++, and JavaScript.

But how do you get into a position where you can even call these test APIs?
How do they fit in with Firefox Desktop's testing frameworks?

## Manual Testing and Debugging

The Glean SDK has [debugging capabilities][glean-debug]
for manually verifying that instrumentation makes it to Mozilla's Data Pipeline.
Firefox Desktop supports these via environment variables _and_
via the interface on `about:glean`.

This is all well and good for getting a good sense check that things are going well _now_,
but in order to check that everything stays good through the future,
you're going to want to write some automated tests.

## General Things To Bear In Mind

* You may see values from previous tests persist across tests because the profile directory was shared between test cases.
    * You can reset Glean before your test by calling
      `Services.fog.testResetFOG()` (in JS).
    * You shouldn't have to do this in C++ or Rust since there you should use the
      `FOGFixture` test fixture.
* If your metric is based on timing (`timespan`, `timing_distribution`),
  do not expect to be able to assert the correct timing value.
  Glean does a lot of timing for you deep in the SDK, so unless you mock the system's monotonic clock,
  do not expect the values to be predictable.
    * Instead, check that a value is `> 0` or that the number of samples is expected.
    * You might be able to assert that the value is at least as much as a known, timed value,
    but beware of rounding.
* Errors in instrumentation APIs do not panic, throw, or crash.
  But Glean remembers that the errors happened.
    * Test APIs, on the other hand, are permitted
      (some may say "encouraged")
      to panic, throw, or crash on bad behaviour.
    * If you call a test API and it panics, throws, or crashes,
      that means your instrumentation did something wrong.
      Check your test logs for details about what went awry.

## The Usual Test Format

Instrumentation tests tend to follow the same three-part format:
1) Assert no value in the metric
2) Express behaviour
3) Assert correct value in the metric

Your choice of test suite will depend on how the instrumented behaviour can be expressed.


## `xpcshell` Tests

If the instrumented behaviour is on the main or content process and can be called from privileged JS,
`xpcshell` is an excellent choice.

`xpcshell` is so minimal an environment, however, that
(pending [bug 1756055](https://bugzilla.mozilla.org/show_bug.cgi?id=1756055))
you'll need to manually tell it you need two things:
1) A profile directory
2) An initialized FOG

```js
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});
```

From there, just follow The Usual Test Format:

```js
add_task(function test_instrumentation() {
  // 1) Assert no value
  Assert.equal(undefined, Glean.myMetricCategory.myMetricName.testGetValue());

  // 2) Express behaviour
  // ...<left as an exercise to the reader>...

  // 3) Assert correct value
  Assert.equal(kValue, Glean.myMetricCategory.myMetricName.testGetValue());
});
```

If your new instrumentation includes a new custom ping,
there are two small additions to The Usual Test Format:

* 1.1) Call `testBeforeNextSubmit` _before_ your ping is submitted.
  The callback you register in `testBeforeNextSubmit`
  is called synchronously with the call to the ping's `submit()`.
* 3.1) Check that the ping actually was submitted.
  If all your Asserts are inside `testBeforeNextSubmit`'s closure,
  another way this test could pass is by not running any of them.

```js
add_task(function test_custom_ping() {
  // 1) Assert no value
  Assert.equal(undefined, Glean.myMetricCategory.myMetricName.testGetValue());

  // 1.1) Set up Step 3.
  let submitted = false;
  GleanPings.myPing.testBeforeNextSubmit(reason => {
    submitted = true;
    // 3) Assert correct value
    Assert.equal(kExpectedReason, reason, "Reason of submitted ping must match.");
    Assert.equal(kExpectedMetricValue, Glean.myMetricCategory.myMetricName.testGetValue());
  });

  // 2) Express behaviour that sends a ping with expected reason and contents
  // ...<left as an exercise to the reader>...

  // 3.1) Check that the ping actually was submitted.
  Assert.ok(submitted, "Ping was submitted, callback was called.");
});
```

(( We acknowledge that this isn't the most ergonomic form.
Please follow
[bug 1756637](https://bugzilla.mozilla.org/show_bug.cgi?id=1756637)
for updates on a better design and implementation for ping tests. ))

## mochitest

`browser-chrome`-flavoured mochitests can be tested very similarly to `xpcshell`.

Prefer `xpcshell` and only use mochitests if you cannot express the behaviour in `xpcshell`.
This can happen, for example, if the behaviour happens on a non-main process.

### IPC

All test APIs must be called on the main process
(they'll assert otherwise).
But your instrumentation might be on any process, so how do you test it?

In this case there's a slight addition to the Usual Test Format:
1) Assert no value in the metric
2) Express behaviour
3) _Flush all pending FOG IPC operations with `Services.fog.testFlushAllChildren()`_
4) Assert correct value in the metric.

## GTests/Google Tests

Please make use of the `FOGFixture` fixture when writing your tests, like:

```cpp
TEST_F(FOGFixture, MyTestCase) {
  // 1) Assert no value
  ASSERT_EQ(mozilla::Nothing(),
            my_metric_category::my_metric_name.TestGetValue());

  // 2) Express behaviour
  // ...<left as an exercise to the reader>...

  // 3) Assert correct value
  ASSERT_EQ(kValue,
            my_metric_category::my_metric_name.TestGetValue().unwrap().ref());
}
```

The fixture will take care of ensuring storage is reset between tests.

## Rust `rusttests`

The general-purpose
[Testing & Debugging Rust Code in Firefox](/testing-rust-code/index)
is a good thing to review first.

Unfortunately, FOG requires gecko
(to tell it where the profile dir is, and other things),
which means we need to use the
[GTest + FFI approach](/testing-rust-code/index.html#gtests)
where GTest is the runner and Rust is just the language the test is written in.

This means your test will look like a GTest like this:

```cpp
extern "C" void Rust_MyRustTest();
TEST_F(FOGFixture, MyRustTest) { Rust_MyRustTest(); }
```

Plus a Rust test like this:

```rust
#[no_mangle]
pub extern "C" fn Rust_MyRustTest() {
    // 1) Assert no value
    assert_eq!(None,
               fog::metrics::my_metric_category::my_metric_name.test_get_value(None));

    // 2) Express behaviour
    // ...<left as an exercise to the reader>...

    // 3) Assert correct value
    assert_eq!(Some(value),
               fog::metrics::my_metric_category::my_metric_name.test_get_value(None));
}
```

[glean-metrics-apis]: https://mozilla.github.io/glean/book/reference/metrics/index.html
[metrics-xpcshell-test]: https://searchfox.org/mozilla-central/rev/66e59131c1c76fe486424dc37f0a8a399ca874d4/toolkit/mozapps/update/tests/unit_background_update/test_backgroundupdate_glean.js#28
[ping-gtest]: https://searchfox.org/mozilla-central/rev/66e59131c1c76fe486424dc37f0a8a399ca874d4/toolkit/components/glean/tests/gtest/TestFog.cpp#232
[test-before-next-submit]: https://mozilla.github.io/glean/book/reference/pings/index.html#testbeforenextsubmit
[glean-debug]: https://mozilla.github.io/glean/book/reference/debug/index.html
