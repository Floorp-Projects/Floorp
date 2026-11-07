// SPDX-License-Identifier: MPL-2.0
/** Execute a QA script without collecting its return value or raw browser errors. */
import { parseArgs } from "@std/cli";
import { dirname, join } from "@std/path";
import { MarionetteClient } from "../src/browser_connector.ts";

const options = parseArgs(Deno.args, {
  string: ["setup", "client", "script", "context"],
  boolean: ["stdin"],
  default: { context: "chrome" },
});
if (
  !options.setup || !["A", "B"].includes(options.client ?? "") ||
  !["chrome", "content"].includes(options.context) ||
  Boolean(options.script) === Boolean(options.stdin)
) {
  throw new Error(
    "Use --setup <QA setup.json> --client A|B --script <file>|--stdin [--context chrome|content]",
  );
}
type QAClient = { name: string; profile: string; marionettePort: number };
const setupFile = await Deno.realPath(options.setup);
const setup = JSON.parse(await Deno.readTextFile(setupFile)) as {
  clients: QAClient[];
};
const target = setup.clients.find((item) => item.name === options.client);
if (
  !target || target.profile !==
    join(dirname(setupFile), `profile-${target.name.toLowerCase()}`)
) throw new Error("Expected a prepared dedicated QA profile");
const source = options.stdin
  ? await new Response(Deno.stdin.readable).text()
  : await Deno.readTextFile(options.script!);
let client: MarionetteClient | undefined;
let succeeded = false;
try {
  client = await MarionetteClient.connect(target.marionettePort);
  await client.setContext("chrome");
  const checked = await client.executeScript(
    `return Services.dirsvc.get("ProfD",Ci.nsIFile).path===arguments[0]&&Services.prefs.getStringPref("floorp.workspaces.sync.acceptance-profile","")===arguments[1];`,
    [target.profile, target.name],
  );
  if (checked !== true) throw new Error("Dedicated QA identity check failed");
  await client.setContext(options.context as "chrome" | "content");
  await client.send("WebDriver:SetTimeouts", { script: 120_000 });
  const response = await client.sendWithTimeout(
    "WebDriver:ExecuteAsyncScript",
    {
      script:
        `const done=arguments[arguments.length-1];(async()=>{${source}\n})().then(()=>done({qaCommandSucceeded:true}),()=>done({qaCommandSucceeded:false}));`,
      args: [],
    },
    125_000,
  );
  succeeded = response?.value?.qaCommandSucceeded === true;
} catch {
  // Browser exception messages and evaluated values may contain account data.
  // Deliberately report only success/failure, including transport failures.
} finally {
  await client?.close();
}
console.log(JSON.stringify({ client: target.name, succeeded }));
Deno.exit(succeeded ? 0 : 1);
