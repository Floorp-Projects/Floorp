Testing with one-click loaners
==============================

[Taskcluster] is the task execution framework that supports Mozilla's
continuous integration and release processes.

Build and test jobs (like Marionette) are executed across all supported
platforms, and job results are pushed to [Treeherder] for observation.

The best way to debug issues for intermittent test failures of
Marionette tests for Firefox and Fennec (Android) is to use a
one-click loaner as provided by Taskcluster. Such a loaner creates
an interactive task you can interact with via a shell and VNC.

To create an interactive task for a Marionette job which is shown
as failed on Treeherder, follow the Taskcluster documentation for
[Debugging a task].

Please note that you need special permissions to actually request
such a loaner.

When the task has been created the shell needs to be opened.
Once that has been done a wizard will automatically launch and
provide some options. Best here is to choose the second option,
which will run all the setup steps, installs the Firefox or Fennec
binary, and then exits.

[Taskcluster]: https://docs.taskcluster.net/
[Treeherder]: https://treeherder.mozilla.org
[Debugging a task]: https://docs.taskcluster.net/tutorial/debug-task#content


Setting up the Marionette environment
-------------------------------------

Best here is to use a virtual environment, which has all the
necessary packages installed. If no modifications to any Python
package will be done, the already created environment by the
wizard can be used:

	% cd /builds/worker/workspace/build
	% source venv/bin/activate

Otherwise a new virtual environment needs to be created and
populated with the mozbase and marionette packages installed:

	% cd /builds/worker/workspace/build && rm -r venv
	% virtualenv venv && source venv/bin/activate
	% cd tests/mozbase && ./setup_development.py
	% cd ../marionette/client && python setup.py develop
	% cd ../harness && python setup.py develop
	% cd ../../../


Running Marionette tests
------------------------

### Firefox

To run the Marionette tests execute the `runtests.py` script. For all
the required options as best search in the log file of the failing job
the interactive task has been created from.  Then copy the complete
command and run it inside the already sourced virtual environment:

	% /builds/worker/workspace/build/venv/bin/python -u /builds/worker/workspace/build/tests/marionette/harness/marionette_harness/runtests.py --gecko-log=- -vv --binary=/builds/worker/workspace/build/application/firefox/firefox --address=127.0.0.1:2828 --symbols-path=https://queue.taskcluster.net/v1/task/GSuwee61Qyibujtxq4UV3A/artifacts/public/build/target.crashreporter-symbols.zip /builds/worker/workspace/build/tests/marionette/tests/testing/marionette/harness/marionette_harness/tests/unit-tests.ini


#### Fennec

The Marionette tests for Fennec are executed by using an Android
emulator which runs on the host platform. As such some extra setup
steps compared to Firefox on desktop are required.

The following lines set necessary environment variables before
starting the emulator in the background, and to let Marionette
know of various Android SDK tools.

	% export ADB_PATH=/builds/worker/workspace/build/android-sdk-linux/platform-tools/adb
	% export ANDROID_AVD_HOME=/builds/worker/workspace/build/.android/avd/
	% /builds/worker/workspace/build/android-sdk-linux/tools/emulator -avd test-1 -show-kernel -debug init,console,gles,memcheck,adbserver,adbclient,adb,avd_config,socket &

The actual call to `runtests.py` is different per test job because
those are using chunks on Android. As best search for the command
and its options in the log file of the failing job the interactive
task has been created from. Then copy the complete command and run
it inside the already sourced virtual environment.

Here an example for chunk 1 which runs all the tests in the current
chunk with some options for logs removed:

	% /builds/worker/workspace/build/venv/bin/python -u /builds/worker/workspace/build/tests/marionette/harness/marionette_harness/runtests.py --emulator --app=fennec --package=org.mozilla.fennec_aurora --address=127.0.0.1:2828 /builds/worker/workspace/build/tests/marionette/tests/testing/marionette/harness/marionette_harness/tests/unit-tests.ini --disable-e10s --gecko-log=- --symbols-path=/builds/worker/workspace/build/symbols --startup-timeout=300 --this-chunk 1 --total-chunks 10

To execute a specific test only simply replace `unit-tests.ini`
with its name.
