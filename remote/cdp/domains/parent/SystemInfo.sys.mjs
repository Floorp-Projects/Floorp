/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Domain } from "chrome://remote/content/cdp/domains/Domain.sys.mjs";

export class SystemInfo extends Domain {
  async getProcessInfo() {
    const procInfo = await ChromeUtils.requestProcInfo();

    // Add child processes
    const processInfo = procInfo.children.map(proc => ({
      type: this.#getProcessType(proc.type),
      id: proc.pid,
      cpuTime: this.#getCpuTime(proc.cpuTime),
    }));

    // Add parent process
    processInfo.unshift({
      type: "browser",
      id: procInfo.pid,
      cpuTime: this.#getCpuTime(procInfo.cpuTime),
    });

    return processInfo;
  }

  #getProcessType(type) {
    // Map internal types to CDP types if applicable
    switch (type) {
      case "gpu":
        return "GPU";

      case "web":
      case "webIsolated":
      case "privilegedabout":
        return "renderer";

      default:
        return type;
    }
  }

  #getCpuTime(cpuTime) {
    // cpuTime is tracked internally as nanoseconds, CDP is in seconds
    return cpuTime / 1000 / 1000 / 1000;
  }
}
