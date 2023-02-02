# New Metrics and Pings

To add a new metric or ping to Firefox Desktop you should follow the
[Glean SDK documentation on the subject](https://mozilla.github.io/glean/book/user/adding-new-metrics.html),
with some few twists we detail herein:

## Testing

Instrumentation, being code, should be tested.
Firefox on Glean [supports a wide variety of Firefox Desktop test suites][instrumentation-tests]
in addition to [Glean's own debugging mechanisms][glean-debug].

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
Be sure to link to this document at the top of the file!
It contains many useful tidbits of information that anyone adding new metrics should know.
Preferably, use this blank template to get started,
substituting your component's `product :: component` tag from
[the list](https://searchfox.org/mozilla-central/source/toolkit/components/glean/tags.yaml):

```yaml
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Adding a new metric? We have docs for that!
# https://firefox-source-docs.mozilla.org/toolkit/components/glean/user/new_definitions_file.html

---
$schema: moz://mozilla.org/schemas/glean/metrics/2-0-0
$tags:
  - 'Your Product :: Your Component'

```

If you add a new definitions file, be sure to edit
`toolkit/components/glean/metrics_index.py`,
adding your definitions files to the Python lists therein.
If you don't, no API will be generated for your metrics and your build will fail.
You will have to decide which products your metrics will be used in.
For code that's also used in other Gecko-based products (Firefox Desktop, Firefox for Android, Focus for Android), use `gecko_metrics`.
For Desktop-only instrumentation use `firefox_desktop_metrics`.
For other products use their respective lists.

Changes to `metrics_index.py` are automatically reflected in the data pipeline once a day
using the [fog-updater automation in probe-scraper](https://github.com/mozilla/probe-scraper/tree/main/fog-updater).
Data will not show up in datasets and tools until this happens.
If something is unclear or data is not showing up in time you will need to file a bug in
`Data Platform and Tools :: General`.

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
  Metrics marked with `never` must have
  [instrumentation tests](instrumentation_tests).

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

[instrumentation-tests]: ./instrumentation_tests
[glean-debug]: https://mozilla.github.io/glean/book/reference/debug/index.html
