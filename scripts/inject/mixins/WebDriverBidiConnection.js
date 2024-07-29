//? Why autocomplete doesn't work?
//? It is because TS's `experimentalDecorators` support is not in vscode's linter.
//? While some error occurs in vscode, it runs well.
//? So I turned this file to JS and `checkJS` to false.

import { Mixin, Inject } from "../shared/define";

@Mixin({
  //? https://searchfox.org/mozilla-central/rev/dd421ae14997e3bebb9c08634633a4a3e3edeffc/remote/webdriver-bidi/WebDriverBiDiConnection.sys.mjs#217
  path: "chrome/remote/content/webdriver-bidi/WebDriverBidiConnection.sys.mjs",
  type: "class",
  export: true,
  className: "WebDriverBiDiConnection",
  extends: "WebSocketConnection",
})
export class MixinWebDriverBiDiConnection {
  @Inject({
    method: "onPacket",
    at: { value: "INVOKE", target: "sendResult" },
  })
  onPacket() {
    if (module === "session" && command === "new") {
      result = JSON.parse(JSON.stringify(result));
      result.capabilities.browserName = "firefox";
    }
  }
}
