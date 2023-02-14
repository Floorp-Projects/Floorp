Follow instructions in https://experimenter.info/mobile-feature-api. Example implementation [here](https://github.com/mozilla-mobile/fenix/pull/23996)

Nimbus FML https://experimenter.info/fml-spec/


There are some clarification on how to test your Nimbus implementation:
1. Add `nimbus.remote-settings.url=https://settings-cdn.stage.mozaws.net` to local.properties.
2. After building Fenix, make sure you turn on `Secret Settings` -> `Use Nimbus Preview Collections`.
3. The experiment in https://stage.experimenter.nonprod.dataops.mozgcp.net/nimbus/ does not have to be live for the test.  In preview is sufficient.
4. Example of a test is [here](https://stage.experimenter.nonprod.dataops.mozgcp.net/nimbus/unified-search-test)
5. Make sure you archive the test after you're done with it.
6. In your PR, make sure to submit the change for .experimenter.yaml as well.