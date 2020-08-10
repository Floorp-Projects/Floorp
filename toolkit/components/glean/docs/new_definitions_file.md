# New Metrics and Pings

**Note:** FOG is not ready to be used,
so you probably should not be adding metrics and pings using this guide yet.
Instead, please use Firefox Telemetry unless you have explicitly been given permission by a
[Telemetry Module Peer](https://wiki.mozilla.org/Modules/All#Telemetry).

To add a new metric or ping to Firefox Desktop you should follow the
[Glean SDK documentation on the subject](https://mozilla.github.io/glean/book/user/adding-new-metrics.html),
with some few twists.

## IPC

Firefox Desktop is made of multiple processes.
You can record data from any process in Firefox Desktop
[subject to certain conditions](ipc.md).

If you will be recording data to this metric in multiple processes,
you should make yourself aware of those conditions.

## Where do I Define new Metrics and Pings?

Metrics and pings are defined in their definitions files
(`metrics.yaml` or `pings.yaml`, respectively).
But where can you find `metrics.yaml` or `pings.yaml`?

If you're not the first person in your component to ask that question,
the answer is likely "in the root of your component".
Look for the definitions files near to where you are instrumenting your code.

If you _are_ the first person in your component to ask that question,
you get to choose where to start them!
We recommend adding them in the root of your component, next to a `moz.build`.

When you do so, be sure to edit `toolkit/components/glean/moz.build`,
adding your definitions files to the `GeneratedFile` directives therein.
If you don't, no API will be generated for your metrics and your build will fail.

In addition, do not forget to file a bug in `Data Platform and Tools :: General`
asking for your definitions files to be added to the others for `firefox.desktop`.
If you don't, your metrics will not show up in datasets and tools
because the pipeline won't know that they exist.

If you have any questions, be sure to ask on
[the #glean channel](https://chat.mozilla.org/#/room/#glean:mozilla.org).

**Note:** Do _not_ use `toolkit/components/glean/metrics.yaml`
or `toolkit/components/glean/pings.yaml`.
These are for metrics instrumenting the code under `toolkit/components/glean`
and are not general-purpose locations for adding metrics and pings.
