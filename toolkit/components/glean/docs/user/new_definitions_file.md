# New Metrics and Pings

**Note:** FOG is not ready to be used without assistance,
so you probably should not be adding metrics and pings using this guide yet.
Instead, please use Firefox Telemetry unless you have explicitly been given permission by a
[Telemetry Module Peer](https://wiki.mozilla.org/Modules/All#Telemetry).

To add a new metric or ping to Firefox Desktop you should follow the
[Glean SDK documentation on the subject](https://mozilla.github.io/glean/book/user/adding-new-metrics.html),
with some few twists.

## IPC

Firefox Desktop is made of multiple processes.
You can record data from any process in Firefox Desktop
[subject to certain conditions](../dev/ipc.md).

If you will be recording data to this metric in multiple processes,
you should make yourself aware of those conditions.

## Where do I Define new Metrics and Pings?

Metrics and pings are defined in their definitions files
(`metrics.yaml` or `pings.yaml`, respectively).
But where can you find `metrics.yaml` or `pings.yaml`?

If you're not the first person in your component to ask that question,
the answer is likely "in the root of your component".
Look for the definitions files near to where you are instrumenting your code.
Or you can look in
`toolkit/components/glean/metrics_index.py`
to see the list of all currently-known definitions files.

If you _are_ the first person in your component to ask that question,
you get to choose where to start them!
We recommend adding them in the root of your component, next to a `moz.build`.

When you do so, be sure to edit `toolkit/components/glean/metrics_index.py`,
adding your definitions files to the Python lists therein.
If you don't, no API will be generated for your metrics and your build will fail.

In addition, do not forget to file a bug in `Data Platform and Tools :: General`
asking for your definitions files to be added to the others for `firefox_desktop`.
If you don't, your metrics will not show up in datasets and tools
because the pipeline won't know that they exist.

If you have any questions, be sure to ask on
[the #glean channel](https://chat.mozilla.org/#/room/#glean:mozilla.org).

**Note:** Do _not_ use `toolkit/components/glean/metrics.yaml`
or `toolkit/components/glean/pings.yaml`.
These are for metrics instrumenting the code under `toolkit/components/glean`
and are not general-purpose locations for adding metrics and pings.

## How does Expiry Work?

In FOG,
unlike in other Glean-SDK-using projects,
metrics expire based on Firefox application version.
This is to allow metrics to be valid over the entire life of an application version,
whether that is the 4-6 weeks of usual releases or the 13 months of ESR releases.

There are three values accepted in the `expires` field of `metrics.yaml`s for FOG:
* `"X"` (where `X` is the major portion of a Firefox Desktop version) -
  The metric will be expired when the `MOZ_APP_VERSION` reaches or exceeds `X`.
  (For example, when the Firefox Version is `88.0a1`,
  all metrics marked with `expires: "88"` or lower will be expired.)
  This is the recommended form for all new metrics to ensure they stop recording when they stop being relevant.
* `expired` - For marking a metric as manually expired.
  Not usually used, but sometimes helpful for internal tests.
* `never` - For marking a metric as part of a permanent data collection.
  Metrics marked with `never` must have [instrumentation tests](testing.md).

For more information on what expiry means and the
`metrics.yaml` format, see
[the Glean SDK docs](https://mozilla.github.io/glean/book/user/metric-parameters.html)
on this subject. Some quick facts:

* Data collected to expired metrics is not recorded or sent.
* Recording to expired metrics is not an error at runtime.
* Expired metrics being in a `metrics.yaml` is a linting error in `glean_parser`.
* Expired (and non-expired) metrics that are no longer useful should be promptly removed from your `metrics.yaml`.
  This reduces the size and improves the performance of Firefox
  (and speeds up the Firefox build process)
  by decreasing the amount of code that needs to be generated.
