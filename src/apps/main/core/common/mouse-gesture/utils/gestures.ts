/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export type GestureActionFn = () => void;
export interface GestureActionRegistration {
  name: string;
  fn: GestureActionFn;
  description: string;
}
class GestureActionsRegistry {
  private static instance: GestureActionsRegistry;
  private actions: Map<string, GestureActionRegistration> = new Map();

  private constructor() {
    this.registerActions([
      {
        name: "goBack",
        fn: goBack,
        description: "Navigate back to the previous page",
      },
      {
        name: "goForward",
        fn: goForward,
        description: "Navigate forward to the next page",
      },
      {
        name: "reload",
        fn: reload,
        description: "Reload the current page",
      },
      {
        name: "closeTab",
        fn: closeTab,
        description: "Close the current tab",
      },
      {
        name: "newTab",
        fn: newTab,
        description: "Open a new tab",
      },
      {
        name: "duplicateTab",
        fn: duplicateTab,
        description: "Duplicate the current tab",
      },
      {
        name: "reloadAllTabs",
        fn: reloadAllTabs,
        description: "Reload all tabs",
      },
      {
        name: "reopenClosedTab",
        fn: reopenClosedTab,
        description: "Reopen the last closed tab",
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

export function registerGestureAction(
  name: string,
  fn: GestureActionFn,
  description: string,
): void {
  gestureActions.registerAction({ name, fn, description });
}

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
