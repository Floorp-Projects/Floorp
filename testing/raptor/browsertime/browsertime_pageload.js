/* eslint-env node */

module.exports = async function(context, commands) {
  let url = context.options.browsertime.url;
  let page_cycles = context.options.browsertime.page_cycles;
  let page_cycle_delay = context.options.browsertime.page_cycle_delay;

  await commands.wait.byTime(context.options.browsertime.foreground_delay);
  await commands.navigate("about:blank");

  for (let count = 0; count < page_cycles; count++) {
    await commands.wait.byTime(page_cycle_delay);
    await commands.measure.start(url);
  }
  return true;
};
