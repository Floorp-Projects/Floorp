# Background Task Mode

Gecko supports running privileged JavaScript in a special headless "background task" mode.  Background task mode is intended to be used for periodic maintenance tasks.  The first consumer will be checking for updates, even when Firefox is not running.

Support for background task mode is gated on the build flag `MOZ_BACKGROUNDTASKS`.

## Adding a new background task

Background tasks are invoked with `--backgroundtask TASKNAME`.  Tasks must be packaged at build time; the background task runtime looks for regular JSM modules in the following locations (in order):

1. (App-specific) `resource:///modules/backgroundtasks/BackgroundTask_TASKNAME.jsm`
2. (Toolkit/general) `resource://gre/modules//backgroundtasks/BackgroundTask_TASKNAME.jsm`

To add a new background task, add to your `moz.build` file a stanza like:

```python
EXTRA_JS_MODULES.backgroundtasks += [
    "BackgroundTask_TASKNAME.jsm",
]
```

## Implementing a background task

In `BackgroundTask_TASKNAME.jsm`, define a function `runBackgroundTask` that returns a `Promise`.  `runBackgroundTask` will be awaited and the integer value it resolves to will be used as the exit code of the `--backgroundtask TASKNAME` invocation.  Optionally, `runBackgroundTask` can take an [`nsICommandLine` instance](https://searchfox.org/mozilla-central/source/toolkit/components/commandlines/nsICommandLine.idl) as a parameter.  For example:

```javascript
var EXPORTED_SYMBOLS = ["runBackgroundTask"];
async function runBackgroundTask(commandLine) {
    return Number.parseInt(commandLine.getArgument(0), 10);
}
```

When invoked like `--backgroundtask TASKNAME EXITCODE`, this task will simply complete with the exit code given on the command line.

Task module can optionally export an integer value called `backgroundTaskTimeoutSec`, which will control how long the task can run before it times out. If no value is specified, the timeout value stored in the pref `toolkit.backgroundtasks.defaultTimeoutSec` will be used.

## Special exit codes

The exit codes 2-4 have special meaning:

* Exit code 2 (`EXIT_CODE.NOT_FOUND`) means the background task with the given `TASKNAME` was not found or could not be loaded.
* Exit code 3 (`EXIT_CODE.EXCEPTION`) means the background task invocation rejected with an exception.
* Exit code 4 (`EXIT_CODE.TIMEOUT`) means that the background task timed out before it could complete.

See [`BackgroundTasksManager.EXIT_CODE`](https://searchfox.org/mozilla-central/source/toolkit/components/backgroundtasks/BackgroundTasksManager.jsm) for details.

## Test-only background tasks

There is special support for test-only background tasks.  Add to your `moz.build` file a stanza like:

```python
TESTING_JS_MODULES.backgroundtasks += [
    "BackgroundTask_TESTONLYTASKNAME.jsm",
]
```

For more details, see [`XPCSHELL_TESTING_MODULES_URI`](https://searchfox.org/mozilla-central/search?q=XPCSHELL_TESTING_MODULES_URI).

## Debugging background tasks

Background task mode supports using the JavaScript debugger and the Firefox Devtools and Browser Toolbox.  When invoked with the command line parameters `--jsdebugger` (and optionally `--wait-for-jsdebugger`), the background task framework will launch a Browser Toolbox, connect to the background task, and pause execution at the first line of the task implementation.  The Browser Toolbox is launched with a temporary profile (sibling to the ephemeral temporary profile the background task itself creates.)  The Browser Toolbox profile's preferences are copied from the default browsing profile, allowing to configure devtools preferences.  (The `--start-debugger-server` command line option is also recognized; see the output of `firefox --backgroundtask success --attach-console --help` for details.)

## The background task mode runtime environment

### Most background tasks run in ephemeral temporary profiles

Background tasks are intended for periodic maintenance tasks, especially global/per-installation maintenance tasks.  To allow background tasks to run at the same time as regular, headed Firefox browsing sessions, by default they run with an ephemeral temporary profile.  This ephemeral profile is deleted when the background task main process exits.  Every background task applies the preferences in [`backgroundtasks/defaults/backgroundtasks.js`](https://searchfox.org/mozilla-central/source/toolkit/components/backgroundtasks/defaults/backgroundtasks.js), but any additional preference configuration must be handled by the individual task.  Over time, we anticipate a small library of background task functionality to grow to make it easier to lock and read specific prefs from the default browsing profile, to declare per-installation prefs, etc.

It is possible to run background tasks in non-emphemeral, i.e., persistent, profiles.  See [Bug 1775132](https://bugzilla.mozilla.org/show_bug.cgi?id=1775132) for details.

### Background tasks limit the XPCOM instance graph by default

The technical mechanism that keeps background tasks "lightweight" is very simple.  XPCOM declares a number of observer notifications for loosely coupling components via the observer service.   Some of those observer notifications are declared as category notifications which allow consumers to register themselves in static components.conf registration files (or in now deprecated chrome.manifest files).  In background task mode, category notifications are not registered by default.

For Firefox in particular, this means that [`BrowserContentHandler.jsm`](https://searchfox.org/mozilla-central/source/browser/components/BrowserContentHandler.jsm) is not registered as a command-line-handler.  This means that [`BrowserGlue.jsm`](https://searchfox.org/mozilla-central/source/browser/components/BrowserGlue.jsm) is not loaded, and this short circuits regular headed browsing startup.

See the [documentation for defining static components](https://firefox-source-docs.mozilla.org/build/buildsystem/defining-xpcom-components.html) for how to change this default behaviour, and [Bug 1675848](https://bugzilla.mozilla.org/show_bug.cgi?id=1675848) for details of the implementation.

### Most background tasks do not process updates

To prevent unexpected interactions between background tasks and the Firefox runtime lifecycle, such as those uncovered by [Bug 1736373](https://bugzilla.mozilla.org/show_bug.cgi?id=1736373), most background tasks do not process application updates.  The startup process decides whether to process updates in [`::ShouldProcessUpdates`](https://searchfox.org/mozilla-central/source/toolkit/xre/nsAppRunner.cpp) and the predicate that determines whether a particular background task *does* process updates is [`BackgroundTasks::IsUpdatingTaskName`](https://searchfox.org/mozilla-central/source/toolkit/components/backgroundtasks/BackgroundTasks.h).

Background tasks that are launched at shutdown (and that are not updating) do not prevent Firefox from updating.  However, this can result in Firefox updating out from underneath a running background task: see [this summary of the issue](https://bugzilla.mozilla.org/show_bug.cgi?id=1480452#c8).  Generally, background tasks should be minimal and short-lived and are unlikely to launch additional child subprocesses after startup, so they should not witness this issue, but it is still possible.  See the diagram below visualizing process lifetimes.

```{mermaid}
    gantt
        title Background tasks launched at Firefox shutdown
        dateFormat  YYYY-MM-DD
        axisFormat  
        section Firefox
        firefox (version N)                      :2014-01-03, 3d
        updater                                  :2014-01-06, 1d
        firefox (version N+1)                    :2014-01-07, 3d
        firefox --backgroundtask ... (version N) :2014-01-05, 3d
```
