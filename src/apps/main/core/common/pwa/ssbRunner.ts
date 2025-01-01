/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DataManager } from "./dataStore";
import type { Manifest } from "./type";
import { SiteSpecificBrowserManager } from "./ssbManager";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

type TQueryInterface = <T extends nsIID>(aIID: T) => nsQIResult<T>;

export class SsbRunner {
  private static instance: SsbRunner;
  public static getInstance(): SsbRunner {
    if (!SsbRunner.instance) {
      SsbRunner.instance = new SsbRunner();
    }
    return SsbRunner.instance;
  }

  public async runSsbById(id: string) {
    const dataManager = DataManager.getInstance();
    const ssbData = await dataManager.getCurrentSsbData();

    this.openSsbWindow(ssbData[id]);
  }

  public async runSsbByUrl(url: string) {
    const dataManager = DataManager.getInstance();
    const ssbData = await dataManager.getCurrentSsbData();
    this.openSsbWindow(ssbData[url]);
  }

  public openSsbWindow(ssb: Manifest) {
    const browserWindowFeatures =
      "chrome,location=yes,centerscreen,dialog=no,resizable=yes,scrollbars=yes";
    //"chrome,location=yes,centerscreen,dialog=no,resizable=yes,scrollbars=yes";

    const args = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString,
    );

    // URL
    args.data = `${ssb.start_url},${ssb.id},?FloorpEnableSSBWindow=true`;

    Services.ww.openWindow(
      null as unknown as mozIDOMWindowProxy,
      AppConstants.BROWSER_CHROME_URL,
      "_blank",
      browserWindowFeatures,
      args,
    );
  }
}

async function startSSBFromCmdLine(id: string) {
  // Loading the SSB is async. Until that completes and launches we will
  // be without an open window and the platform will not continue startup
  // in that case. Flag that a window is coming.
  Services.startup.enterLastWindowClosingSurvivalArea();

  // Whatever happens we must exitLastWindowClosingSurvivalArea when done.
  try {
    const ssb = await SiteSpecificBrowserManager.getInstance().getSsbObj(id);
    if (ssb) {
      SsbRunner.getInstance().openSsbWindow(ssb);
    } else {
      dump(`No SSB installed as ID ${id}\n`);
    }
  } finally {
    Services.startup.exitLastWindowClosingSurvivalArea();
  }
}
class CSSB implements nsIFactory, nsICommandLineHandler {
  classDescription = "Floorp SSB Implementation";
  contractID = "@floorp-app/site-specific-browser;1";
  //@ts-expect-error
  classID = Components.ID("{2bab4429-4cc6-494c-9ee7-02bd389d945c}");
  QueryInterface = ChromeUtils.generateQI([
    "nsICommandLineHandler",
  ]) as TQueryInterface;
  createInstance<T extends nsIID>(iid: T) {
    return this.QueryInterface<T>(iid);
  }

  handle(cmdLine: nsICommandLine) {
    console.log("handle nsICommandLine for start-ssb");
    const id = cmdLine.handleFlagWithParam("start-ssb", false);
    if (id) {
      cmdLine.preventDefault = true;
      startSSBFromCmdLine(id);
    }
  }
  helpInfo = "  --start-ssb <id>  Start the SSB with the given id.\n";
}

const SSB = new CSSB();

try {
  // biome-ignore lint/style/noNonNullAssertion: <explanation>
  const registrar = Components.manager.QueryInterface!(
    Ci.nsIComponentRegistrar,
  );
  registrar.registerFactory(
    SSB.classID,
    SSB.classDescription,
    SSB.contractID,
    SSB,
  );

  Services.catMan.addCategoryEntry(
    "command-line-handler",
    "e-ssb",
    SSB.contractID,
    false,
    true,
  );
} catch (e) {
  console.error(e);
}
