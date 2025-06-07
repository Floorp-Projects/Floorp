//? Why autocomplete doesn't work?
//? It is because TS's `experimentalDecorators` support is not in vscode's linter.
//? While some error occurs in vscode, it runs well.
//? So I turned this file to JS and `checkJS` to false.

import { Mixin, Inject } from "../shared/define.js";

@Mixin({
  //? https://searchfox.org/mozilla-central/rev/9993372dd72daea851eba4600d5750067104bc15/browser/components/preferences/preferences.js#200
  path: "browser/chrome/browser/content/browser/preferences/preferences.js",
  type: "function",
  export: false,
  funcName: "init_all",
})
export class MixinWebDriverBiDiConnection {
  @Inject({ at: { value: "INVOKE", target: "register_module" } })
  registerModule() {
    register_module("paneCsk", { init() {} });
  }
}
