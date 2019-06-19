"use strict";

/**
 * nodejs script to test basic CDP behaviors against Firefox and Chromium.
 *
 * Install chrome-remote-interface, the npm package for a CDP client in node:
 * $ npm install chrome-remote-interface
 *
 * Run Firefox or Chromium with server turned on:
 * $ ./mach run --setpref "remote.enabled=true" --remote-debugging-port 9222
 * $ firefox --remote-debugging-port 9222
 * $ chromium-browser --remote-debugging-port=9222
 *
 * Run this script:
 * $ node demo.js
 */
const CDP = require("chrome-remote-interface");

async function demo() {
  let client;
  try {
    client = await CDP();
    const {Log, Network, Page, Runtime} = client;

    // Bug 1553756, Firefox requires `contextId` argument to be passed to
    // Runtime.evaluate, so fetch the current context id it first.
    Runtime.enable();
    const { context } = await Runtime.executionContextCreated();
    const contextId = context.id;

    let { result } = await Runtime.evaluate({
      expression: "this.obj = {foo:true}; this.obj",
      contextId,
    });
    console.log("1", result);
    ({ result } = await Runtime.evaluate({
      expression: "this.obj",
      contextId,
    }));
    console.log("2", result);
    ({ result } = await Runtime.evaluate({
      expression: "this.obj.foo",
      contextId,
    }));
    console.log("3", result);

    // receive console.log messages and print them
    Log.enable();
    Log.entryAdded(({entry}) => {
      const {timestamp, level, text, args} = entry;
      const msg = text || args.join(" ");
      console.log(`${new Date(timestamp)}\t${level.toUpperCase()}\t${msg}`);
    });

    // turn on navigation related events, such as DOMContentLoaded et al.
    await Page.enable();

    const onLoad = Page.loadEventFired();
    await Page.navigate({url: "data:text/html,test-page<script>console.log('foo');</script><script>'</script>"});
    await onLoad;
  } catch (e) {
    console.error(e);
  } finally {
    if (client) {
      await client.close();
    }
  }
}

demo();
