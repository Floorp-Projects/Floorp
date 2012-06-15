// Root logger
pref("services.aitc.log.dump", false);
pref("services.aitc.log.level", "All");

pref("services.aitc.browserid.url", "https://browserid.org/sign_in");
pref("services.aitc.browserid.log.level", "Debug");

pref("services.aitc.client.log.level", "Debug");
pref("services.aitc.client.timeout", 120); // 120 seconds

pref("services.aitc.dashboard.url", "https://myapps.mozillalabs.com");

pref("services.aitc.main.idleTime", 120000); // 2 minutes

pref("services.aitc.manager.putFreq", 10000); // 10 seconds
pref("services.aitc.manager.getActiveFreq", 120000); // 2 minutes
pref("services.aitc.manager.getPassiveFreq", 7200000); // 2 hours
pref("services.aitc.manager.log.level", "Debug");

pref("services.aitc.marketplace.url", "https://marketplace.mozilla.org");

pref("services.aitc.service.log.level", "Debug");

// TODO: Temporary value. Change to the production server (bug 760903)
pref("services.aitc.tokenServer.url", "https://stage-token.services.mozilla.com");

pref("services.aitc.storage.log.level", "Debug");
