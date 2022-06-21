# Runtime Metric Definition Subsystem: JOG

```{admonition} I'm Sorry
Why is it caled JOG? Because it's concerned with... run... time.
```

The normal mechanism for registering metrics,
for reasons as varied from ease-of-impl to performance,
happens at compile time.
However, this doesn't support use cases like
* [Artifact Builds][artifact-build]
  (Where only the JavaScript of Firefox Desktop is repackaged at build time,
  so there is no compile environment)
* Dynamic Telemetry
  (A theorized system for instrumenting Firefox Desktop without shipping code)
* Web Extensions
  (Or at least the kind that can't or won't use
  [the Glean JS SDK][glean-js])

Thus we need a subsystem that supports the runtime registration of metrics.
We call it JOG and it was implemented in [bug 1698184][impl-bug].

## JavaScript Only

Metrics in C++ and Rust are identified by identifiers which we can't swap out at runtime.
Thus, in order for changes to metrics to be visible to instrumented systems in C++ or Rust, you must compile.

JavaScript, on the other hand, we supply instances to on-demand.
It not only supports the specific use cases driving this project,
it's the only environment that can benefit from runtime metric definition in Firefox Desktop.

## Design

The original design was done as part of
[bug 1662863][design-bug].
Things have mostly just been refined from there.

## Architecture

We silo as much of the subsystem as we can into the
`jog` module located in `toolkit/components/glean/bindings/jog/`.
This includes the metrics construction factory and storage for metrics instances and their names and ids.

Unfortunately, so that the metrics instances can be accessed by FFI,
the Rust metrics instances created by the `jog` crate are stored within the `fog` crate.

Speaking of FFI, the `jog` crate is using cbindgen to be accessible to C++.

If necessary or pleasant, it is probably possible to do away with the C++ storage,
moving the category set and metrics id map to Rust and moving information over FFI as needed.

Test methods are run from `nsIFOG` (so we can use them in JS in xpcshell)
to static `JOG::` functions.

[artifact-build]: https://firefox-source-docs.mozilla.org/contributing/build/artifact_builds.html
[glean-js]: https://mozilla.github.io/glean/book/user/adding-glean-to-your-project/javascript.html
[impl-bug]: https://bugzilla.mozilla.org/show_bug.cgi?id=1698184
[design-bug]: https://bugzilla.mozilla.org/show_bug.cgi?id=1662863
