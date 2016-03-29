Robocop Mochitest
=================

*Robocop Mochitest* is a Mozilla project which uses Robotium to test
 Firefox on Android devices.

*Robocop Mochitest* tests run on Native Android builds marked with an
'rc' on treeherder.  These are Java based tests which run from the mochitest
harness and generate similar log files.  These are designed for
testing the native UI of Android devices by sending events to the
front end.

See the documentation at
https://wiki.mozilla.org/Auto-tools/Projects/Robocop/WritingTests for
details.

Development cycle
-----------------

To deploy the robocop APK to your device and start the robocop test
suite, use::

    mach robocop

To run a specific test case, such as ``testLoad``::

    mach robocop testLoad

The Java files in ``mobile/android/tests/browser/robocop`` are dependencies of the
robocop APK built by ``build/mobile/robocop``.  If you modify Java files
in ``mobile/android/tests/browser/robocop``, you need to rebuild the robocop APK
with::

    mach build build/mobile/robocop

Changes to ``.html``, ``.css``, ``.sjs``, and ``.js`` files in
``mobile/android/tests/browser/robocop`` do not require rebuilding the robocop
APK -- these changes are always 'live', since they are served by the
mochitest HTTP server and downloaded each test run by your device.

``mach package`` does build and sign a robocop APK, but ``mach
robocop`` does not use it.  (This signed APK is used to test
signed releases on the buildbots).

As always, changes to ``mobile/android/base``, ``mobile/android/chrome``,
``mobile/android/modules``, etc., require::

    mach build mobile/android/base && mach package && mach install

as usual.

Licensing
---------

Robotium is an open source tool licensed under the Apache 2.0 license and the original
source can be found here:
https://github.com/RobotiumTech/robotium

We are including robotium-solo-5.5.4.jar as a binary and are not modifying it in any way
from the original download found at:
https://github.com/RobotiumTech/robotium/wiki/Downloads
