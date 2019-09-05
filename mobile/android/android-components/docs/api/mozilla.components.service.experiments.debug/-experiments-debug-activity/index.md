[android-components](../../index.md) / [mozilla.components.service.experiments.debug](../index.md) / [ExperimentsDebugActivity](./index.md)

# ExperimentsDebugActivity

`class ExperimentsDebugActivity` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/debug/ExperimentsDebugActivity.kt#L33)

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

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ExperimentsDebugActivity()`<br>Debugging activity exported by service-experiments to allow easier debugging. This accepts commands that can force the library to do the following: |

### Functions

| Name | Summary |
|---|---|
| [onCreate](on-create.md) | `fun onCreate(savedInstanceState: <ERROR CLASS>?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>On creation of the debug activity, process the command switches |

### Companion Object Properties

| Name | Summary |
|---|---|
| [OVERRIDE_BRANCH_EXTRA_KEY](-o-v-e-r-r-i-d-e_-b-r-a-n-c-h_-e-x-t-r-a_-k-e-y.md) | `const val OVERRIDE_BRANCH_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Used only with [OVERRIDE_EXPERIMENT_EXTRA_KEY](-o-v-e-r-r-i-d-e_-e-x-p-e-r-i-m-e-n-t_-e-x-t-r-a_-k-e-y.md). |
| [OVERRIDE_CLEAR_ALL_EXTRA_KEY](-o-v-e-r-r-i-d-e_-c-l-e-a-r_-a-l-l_-e-x-t-r-a_-k-e-y.md) | `const val OVERRIDE_CLEAR_ALL_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Clears any existing overrides. |
| [OVERRIDE_EXPERIMENT_EXTRA_KEY](-o-v-e-r-r-i-d-e_-e-x-p-e-r-i-m-e-n-t_-e-x-t-r-a_-k-e-y.md) | `const val OVERRIDE_EXPERIMENT_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Overrides the current experiment and set the active experiment to the given `branch`. This command requires two parameters to be passed, `overrideExperiment` and `branch` in order for it to work. |
| [SET_KINTO_INSTANCE_EXTRA_KEY](-s-e-t_-k-i-n-t-o_-i-n-s-t-a-n-c-e_-e-x-t-r-a_-k-e-y.md) | `const val SET_KINTO_INSTANCE_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Sets the Kinto endpoint to the supplied endpoint. Must be one of: `dev`, `staging`, or `prod`. |
| [UPDATE_EXPERIMENTS_EXTRA_KEY](-u-p-d-a-t-e_-e-x-p-e-r-i-m-e-n-t-s_-e-x-t-r-a_-k-e-y.md) | `const val UPDATE_EXPERIMENTS_EXTRA_KEY: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Fetch experiments from the server and update the active experiment if necessary. |
