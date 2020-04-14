module.exports = async function(context, commands) {
  let rootUrl = "https://2019.jsconf.eu/";

  await commands.navigate(rootUrl);

  // Wait for browser to settle
  await commands.wait.byTime(10000);

  // Start the measurement
  await commands.measure.start("pageload");

  // Click on the link and wait for page complete check to finish.
  await commands.click.byLinkTextAndWait("Artists");

  // Stop and collect the measurement
  await commands.measure.stop();
};
