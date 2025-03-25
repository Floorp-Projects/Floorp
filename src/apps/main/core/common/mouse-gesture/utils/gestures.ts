/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import i18next from "i18next";

export type GestureActionFn = () => void;
export interface GestureActionRegistration {
  name: string;
  fn: GestureActionFn;
}
class GestureActionsRegistry {
  private static instance: GestureActionsRegistry;
  private actions: Map<string, GestureActionRegistration> = new Map();

  private constructor() {
    this.registerActions([
      {
        name: "goBack",
        fn: goBack,
      },
      {
        name: "goForward",
        fn: goForward,
      },
      {
        name: "reload",
        fn: reload,
      },
      {
        name: "closeTab",
        fn: closeTab,
      },
      {
        name: "newTab",
        fn: newTab,
      },
      {
        name: "duplicateTab",
        fn: duplicateTab,
      },
      {
        name: "reloadAllTabs",
        fn: reloadAllTabs,
      },
      {
        name: "reopenClosedTab",
        fn: reopenClosedTab,
      },
    ]);
  }

  public static getInstance(): GestureActionsRegistry {
    if (!GestureActionsRegistry.instance) {
      GestureActionsRegistry.instance = new GestureActionsRegistry();
    }
    return GestureActionsRegistry.instance;
  }

  public registerAction(action: GestureActionRegistration): void {
    this.actions.set(action.name, action);
  }

  public registerActions(actions: GestureActionRegistration[]): void {
    for (const action of actions) {
      this.registerAction(action);
    }
  }

  public getAction(name: string): GestureActionFn | undefined {
    return this.actions.get(name)?.fn;
  }

  public getAllActions(): Map<string, GestureActionRegistration> {
    return this.actions;
  }

  public getActionsList(): GestureActionRegistration[] {
    return Array.from(this.actions.values());
  }
}

export const gestureActions = GestureActionsRegistry.getInstance();

export function getAllGestureActions(): GestureActionRegistration[] {
  return gestureActions.getActionsList();
}

export function executeGestureAction(name: string): boolean {
  const action = gestureActions.getAction(name);
  if (action) {
    action();
    return true;
  }
  return false;
}

export function getActionDisplayName(actionId: string): string {
  return i18next.t(`mouseGesture.actions.${actionId}`, {
    defaultValue: actionId,
  });
}

export function getActionDescription(actionId: string): string {
  return i18next.t(`mouseGesture.descriptions.${actionId}`, {
    defaultValue: "",
  });
}

export function goBack(): void {
  globalThis.gBrowser.goBack();
}

export function goForward(): void {
  globalThis.gBrowser.goForward();
}

export function reload(): void {
  globalThis.gBrowser.reload();
}

export function closeTab(): void {
  globalThis.gBrowser.removeCurrentTab();
}

export function newTab(): void {
  globalThis.gBrowser.addTab("about:newtab", {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
}

export function duplicateTab(): void {
  globalThis.gBrowser.duplicateTab(globalThis.gBrowser.selectedTab);
}

export function reloadAllTabs(): void {
  for (const tab of globalThis.gBrowser.tabs) {
    globalThis.gBrowser.reloadTab(tab);
  }
}

export function reopenClosedTab(): void {
  Services.obs.notifyObservers({}, "Browser:RestoreLastSession");
}
