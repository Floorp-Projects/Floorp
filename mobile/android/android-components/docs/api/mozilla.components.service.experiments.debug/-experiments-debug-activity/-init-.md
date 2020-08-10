[android-components](../../index.md) / [mozilla.components.service.experiments.debug](../index.md) / [ExperimentsDebugActivity](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ExperimentsDebugActivity()`

Debugging activity exported by service-experiments to allow easier debugging. This accepts
commands that can force the library to do the following:

* Fetch or update experiments
* Change the Kinto endpoint to the `dev`, `staging`, or `prod` endpoint
* Override the active experiment to a branch specified by the `branch` command
* Clear any overridden experiment

See here for more information on using the ExperimentsDebugActivity:
https://github.com/mozilla-mobile/android-components/tree/master/components/service/experiments#experimentsdebugactivity-usage

See the adb developer docs for more info:
https://developer.android.com/studio/command-line/adb#am

