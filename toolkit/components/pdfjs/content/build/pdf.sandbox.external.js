/* Copyright 2021 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

class SandboxSupportBase {
 constructor(win) {
  this.win = win;
  this.timeoutIds = new Map();
  this.commFun = null;
 }
 destroy() {
  this.commFunc = null;
  this.timeoutIds.forEach(([_, id]) => this.win.clearTimeout(id));
  this.timeoutIds = null;
 }
 exportValueToSandbox(val) {
  throw new Error("Not implemented");
 }
 importValueFromSandbox(val) {
  throw new Error("Not implemented");
 }
 createErrorForSandbox(errorMessage) {
  throw new Error("Not implemented");
 }
 callSandboxFunction(name, args) {
  try {
   args = this.exportValueToSandbox(args);
   this.commFun(name, args);
  } catch (e) {
   this.win.console.error(e);
  }
 }
 createSandboxExternals() {
  const externals = {
   setTimeout: (callbackId, nMilliseconds) => {
    if (typeof callbackId !== "number" || typeof nMilliseconds !== "number") {
     return;
    }
    const id = this.win.setTimeout(() => {
     this.timeoutIds.delete(callbackId);
     this.callSandboxFunction("timeoutCb", {
      callbackId,
      interval: false
     });
    }, nMilliseconds);
    this.timeoutIds.set(callbackId, id);
   },
   clearTimeout: id => {
    this.win.clearTimeout(this.timeoutIds.get(id));
    this.timeoutIds.delete(id);
   },
   setInterval: (callbackId, nMilliseconds) => {
    if (typeof callbackId !== "number" || typeof nMilliseconds !== "number") {
     return;
    }
    const id = this.win.setInterval(() => {
     this.callSandboxFunction("timeoutCb", {
      callbackId,
      interval: true
     });
    }, nMilliseconds);
    this.timeoutIds.set(callbackId, id);
   },
   clearInterval: id => {
    this.win.clearInterval(this.timeoutIds.get(id));
    this.timeoutIds.delete(id);
   },
   alert: cMsg => {
    if (typeof cMsg !== "string") {
     return;
    }
    this.win.alert(cMsg);
   },
   confirm: cMsg => {
    if (typeof cMsg !== "string") {
     return false;
    }
    return this.win.confirm(cMsg);
   },
   prompt: (cQuestion, cDefault) => {
    if (typeof cQuestion !== "string" || typeof cDefault !== "string") {
     return null;
    }
    return this.win.prompt(cQuestion, cDefault);
   },
   parseURL: cUrl => {
    const url = new this.win.URL(cUrl);
    const props = [
     "hash",
     "host",
     "hostname",
     "href",
     "origin",
     "password",
     "pathname",
     "port",
     "protocol",
     "search",
     "searchParams",
     "username"
    ];
    return Object.fromEntries(props.map(name => [
     name,
     url[name].toString()
    ]));
   },
   send: data => {
    if (!data) {
     return;
    }
    const event = new this.win.CustomEvent("updatefromsandbox", { detail: this.importValueFromSandbox(data) });
    this.win.dispatchEvent(event);
   }
  };
  Object.setPrototypeOf(externals, null);
  return (name, args) => {
   try {
    const result = externals[name](...args);
    return this.exportValueToSandbox(result);
   } catch (error) {
    throw this.createErrorForSandbox(error?.toString() ?? "");
   }
  };
 }
}
{
 var EXPORTED_SYMBOLS = ["SandboxSupportBase"];
}