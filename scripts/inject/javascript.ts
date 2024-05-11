import * as fs from "node:fs/promises";

const NORA_COMMENT_START = "//@nora|INJECT|START\n";
const NORA_COMMENT_END = "//@nora|INJECT|END\n";

export async function injectScript(
  scriptPath: string,
  findStr: string,
  inStr: string,
  position: "pre" | "post",
) {
  const script = (await fs.readFile(scriptPath)).toString();

  const index = script.indexOf(findStr);

  switch (position) {
    case "pre": {
      await fs.writeFile(
        scriptPath,
        // biome-ignore lint/style/useTemplate: <explanation>
        script.slice(0, index) +
          NORA_COMMENT_START +
          inStr +
          "\n" +
          NORA_COMMENT_END +
          script.slice(index),
      );
      break;
    }
    case "post": {
      await fs.writeFile(
        scriptPath,
        // biome-ignore lint/style/useTemplate: <explanation>
        script.slice(0, index + findStr.length) +
          NORA_COMMENT_START +
          inStr +
          "\n" +
          NORA_COMMENT_END +
          script.slice(index + findStr.length),
      );
      break;
    }
  }
}
