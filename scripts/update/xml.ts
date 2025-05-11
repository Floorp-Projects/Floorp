import { z } from "zod";

const zMeta = z.object({
  version: z.string(),
  version_display: z.string(),
  noraneko_version: z.string(),
  mar_size: z.string(),
  mar_shasum: z.string(),
  buildid: z.string(),
  noraneko_buildid: z.string(),
});

const meta = zMeta.parse(JSON.parse(
  await Deno.readTextFile(Deno.args[0]),
));

const patchUrl =
  "http://github.com/nyanrus/noraneko/releases/download/alpha/noraneko-win-amd64-full.mar";

await Deno.writeTextFile(
  Deno.args[1],
  `<?xml version="1.0" encoding="UTF-8"?>
<updates>
  <update type="minor" displayVersion="${meta.version_display}" appVersion="${meta.version}" platformVersion="${meta.version}" buildID="${meta.buildid}" appVersion2="${meta.noraneko_version}">
    <patch type="complete" URL="${patchUrl}" size="${meta.mar_size} hashFunction="sha512" hashValue="${meta.mar_shasum}"/>
  </update>
</updates>`,
);
