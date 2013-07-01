Robocop Mochitest
=================

*Robocop Mochitest* tests run on Native Android builds marked with an
'rc' in TBPL.  These are Java based tests which run from the mochitest
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

    make -C $OBJDIR mochitest-robocop

To run a specific test case, such as ``testLoad``::

    make -C $OBJDIR mochitest-robocop TEST_PATH=testLoad

The Java files in ``mobile/android/base/tests`` are dependencies of the
robocop APK built by ``build/mobile/robocop``.  If you modify Java files
in ``mobile/android/base/tests``, you need to rebuild the robocop APK
with::

    mach build build/mobile/robocop

Changes to ``.html``, ``.css``, ``.sjs``, and ``.js`` files in
``mobile/android/base/tests`` do not require rebuilding the robocop
APK -- these changes are always 'live', since they are served by the
mochitest HTTP server and downloaded each test run by your device.

``mach package`` does build and sign a robocop APK, but ``make
mochitest-robocop`` does not use it.  (This signed APK is used to test
signed releases on the buildbots).

As always, changes to ``mobile/android/base``, ``mobile/android/chrome``,
``mobile/android/modules``, etc., require::

    mach build mobile/android/base && mach package && mach install

as usual.
