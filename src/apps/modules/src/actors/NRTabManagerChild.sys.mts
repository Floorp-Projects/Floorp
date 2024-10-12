/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

type NRAddTabOptions =
  | {
      allowInheritPrincipal: boolean;
      allowThirdPartyFixup: boolean;
      bulkOrderedOpen: boolean;
      charset: string;
      createLazyBrowser: boolean;
      disableTRR: boolean;
      eventDetail: string;
      focusUrlBar: boolean;
      forceNotRemote: boolean;
      forceAllowDataURI: boolean;
      fromExternal: boolean;
      inBackground: boolean;
      index: number;
      lazyTabTitle: string;
      name: string;
      noInitialLabel: boolean;
      openWindowInfo: object;
      openerBrowser: object;
      originPrincipal: object;
      originStoragePrincipal: object;
      ownerTab: object;
      pinned: boolean;
      postData: object;
      preferredRemoteType: string;
      referrerInfo: object;
      relatedToCurrent: boolean;
      initialBrowsingContextGroupId: number;
      skipAnimation: boolean;
      skipBackgroundNotify: boolean;
      triggeringPrincipal: object;
      userContextId: number;
      csp: string;
      skipLoad: boolean;
      insertTab: boolean;
      globalHistoryOptions: object;
      triggeringRemoteType: string;
      wasSchemelessInput: boolean;
      hasValidUserGestureActivation: boolean;
      textDirectiveUserActivation: boolean;
    }
  | object;

export class NRTabManagerChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRTabManagerChild created!");
    const window = this.contentWindow;
    if (window?.location.port === "5183") {
      console.debug("NRTabManager 5183!");
      Cu.exportFunction(this.NRAddTab.bind(this), window, {
        defineAs: "NRAddTab",
      });
    }
  }
  NRAddTab(
    url: string,
    options: NRAddTabOptions = {},
    callback: () => void = () => {},
  ) {
    const promise = new Promise<void>((resolve, _reject) => {
      this.resolveAddTab = resolve;
    });
    this.sendAsyncMessage("Tabs:AddTab", { url, options });
    promise.then((_v) => callback());
  }

  resolveAddTab: (() => void) | null = null;
  resolveLoadTrustedUrl: (() => void) | null = null;
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "Tabs:AddTab": {
        this.resolveAddTab?.();
        this.resolveLoadTrustedUrl = null;
        break;
      }
    }
  }
}
