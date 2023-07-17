/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

module.exports = async function (context, commands) {
  context.log.info(
    "Starting a pageload for which we will first make a speculative connection to the host"
  );

  const url = "https://en.wikipedia.org/wiki/Barack_Obama";

  await commands.navigate("about:blank");
  await commands.wait.byTime(1000);

  context.log.debug("Make privileged call to speculativeConnect");
  const script = `
    var URI = Services.io.newURI("${url}");
    var principal = Services.scriptSecurityManager.createContentPrincipal(URI, {});
    Services.io.QueryInterface(Ci.nsISpeculativeConnect).speculativeConnect(URI, principal, null, false);
  `;

  commands.js.runPrivileged(script);

  // More than enough time for the connection to be made
  await commands.wait.byTime(1000);

  await commands.measure.start();
  await commands.navigate(url);
  await commands.measure.stop();

  let connect_time = await commands.js.run(
    `return (window.performance.timing.connectEnd - window.performance.timing.navigationStart);`
  );
  context.log.info("connect_time: " + connect_time);

  await commands.measure.addObject({
    custom_data: { connect_time },
  });

  context.log.info("Speculative connect test finished.");
  return true;
};
