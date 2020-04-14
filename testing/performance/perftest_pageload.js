async function setUp(context) {
  context.log.info("setUp example!");
}

async function test(context, commands) {
  let url = context.options.browsertime.url;
  await commands.navigate("https://www.mozilla.org/en-US/");
  await commands.wait.byTime(100);
  await commands.navigate("about:blank");
  await commands.wait.byTime(50);
  return commands.measure.start(url);
}

async function tearDown(context) {
  context.log.info("tearDown example!");
}

module.exports = {   // eslint-disable-line
  setUp,
  tearDown,
  test,
};
