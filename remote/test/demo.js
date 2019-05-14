"use strict";

const CDP = require("chrome-remote-interface");

async function demo() {
  let client;
  try {
    client = await CDP();
    const {Log, Network, Page, Runtime} = client;
    let { result } = await Runtime.evaluate({expression: "this.obj = {foo:true}; this.obj"});
    console.log("1", result);
    ({ result } = await Runtime.evaluate({expression: "this.obj"}));
    console.log("2", result);
    ({ result } = await Runtime.evaluate({expression: "this.obj.foo"}));
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

    await Page.navigate({url: "data:text/html,test-page<script>console.log('foo');</script><script>'</script>"});
    await Page.loadEventFired();
  } catch (e) {
    console.error(e);
  } finally {
    if (client) {
      await client.close();
    }
  }
}

demo();
