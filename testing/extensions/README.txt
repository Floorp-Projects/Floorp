To run custom Mozilla tests on an extension (Bug 1517083):
1. Drop an extension XPI into testing/profiles/common/extensions (Bug 145119); this extension will be installed with the testing profile.
2. Drop a moz.build file in this directory that registers any relevant manifests for any tests to be run for the extension. This will overwrite the placeholder moz.build file in this directory.
3. Drop those tests into this directory.
