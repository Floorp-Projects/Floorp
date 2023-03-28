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

### Build Integration

If JOG detects we're an artifact build (by checking `MOZ_ARTIFACT_BUILDS`),
it generates `jogfile.json` and ensures it is placed in `GreD`
(next to the `firefox` binary).

`jogfile.json` includes only the metric and ping information necessary to register them at runtime.
(It doesn't know about tags or descriptions, just the shapes and names of things)

This file is read the first time JS tries to get a metric category from the
`Glean` global or a ping from the `GleanPings` global.

Yes, this is on the main thread. Yes, this is synchronous. Yes, this is file I/O.

Since this is a developer build, we think this is worth it to support Artifact Builds.

If we're wrong about this and there are additional conditions we should place JOG under,
please [contact us][glean-channel].

#### If things get weird, delete `objdir/dist/bin/jogfile.json`

Sometimes, metrics or pings you've added may not appear when you run Firefox.
For these and other odd cases, the solution is the same:
delete `jogfile.json` from the `dist/bin` directory of your objdir, then try again.

This shouldn't happen if you keep your artifact and non-artifact objdirs segregated
(as is good practice).

If, despite doing things properly you still see this or something else odd, then that's a bug.
Please [file it in Toolkit :: Telemetry][file-bug]

[artifact-build]: https://firefox-source-docs.mozilla.org/contributing/build/artifact_builds.html
[glean-js]: https://mozilla.github.io/glean/book/user/adding-glean-to-your-project/javascript.html
[impl-bug]: https://bugzilla.mozilla.org/show_bug.cgi?id=1698184
[design-bug]: https://bugzilla.mozilla.org/show_bug.cgi?id=1662863
[glean-channel]: https://chat.mozilla.org/#/room/#glean:mozilla.org
[file-bug]: https://bugzilla.mozilla.org/enter_bug.cgi?assigned_to=nobody%40mozilla.org&bug_ignored=0&bug_severity=--&bug_status=NEW&bug_type=defect&cf_a11y_review_project_flag=---&cf_fx_iteration=---&cf_fx_points=---&cf_performance_impact=---&cf_status_firefox106=---&cf_status_firefox107=---&cf_status_firefox108=---&cf_status_firefox_esr102=---&cf_status_thunderbird_esr102=---&cf_status_thunderbird_esr91=---&cf_tracking_firefox106=---&cf_tracking_firefox107=---&cf_tracking_firefox108=---&cf_tracking_firefox_esr102=---&cf_tracking_firefox_relnote=---&cf_tracking_thunderbird_esr102=---&cf_tracking_thunderbird_esr91=---&cf_webcompat_priority=---&component=Telemetry&contenttypemethod=list&contenttypeselection=text%2Fplain&defined_groups=1&filed_via=standard_form&flag_type-203=X&flag_type-37=X&flag_type-41=X&flag_type-607=X&flag_type-721=X&flag_type-737=X&flag_type-787=X&flag_type-799=X&flag_type-800=X&flag_type-803=X&flag_type-846=X&flag_type-855=X&flag_type-864=X&flag_type-930=X&flag_type-936=X&flag_type-937=X&flag_type-952=X&form_name=enter_bug&maketemplate=Remember%20values%20as%20bookmarkable%20template&op_sys=Unspecified&priority=--&product=Toolkit&rep_platform=Unspecified&short_desc=Problem%20with%20JOG%3A%20&target_milestone=---&version=unspecified
