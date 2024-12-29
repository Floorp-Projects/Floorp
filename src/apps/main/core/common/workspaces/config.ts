/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Accessor, createEffect, createSignal, onCleanup, Setter } from "solid-js";
import { TWorkspacesServicesConfigs, zWorkspacesServicesConfigs } from "./utils/type.js";
import { getOldConfigs } from "./old-config.js";
import { createRootHMR } from "@nora/solid-xul";

function createEnabled(): [Accessor<boolean>,Setter<boolean>] {
  const [enabled, setEnabled] = createSignal(
    Services.prefs.getBoolPref("floorp.browser.workspaces.enabled", true),
  );
  createEffect(() => {
    Services.prefs.setBoolPref("floorp.browser.workspaces.enabled", enabled());
  });

  const observer = () =>
    setEnabled(Services.prefs.getBoolPref("floorp.browser.workspaces.enabled"));
  Services.prefs.addObserver("floorp.browser.workspaces.enabled", observer);
  onCleanup(()=>{
    Services.prefs.removeObserver("floorp.browser.workspaces.enabled", observer);
  })
  return [enabled,setEnabled]
}

 /** enable/disable workspaces */
export const [enabled,setEnabled] = createRootHMR(createEnabled,import.meta.hot);

function createConfig(): [Accessor<TWorkspacesServicesConfigs>,Setter<TWorkspacesServicesConfigs>] {
  const [config, setConfig] = createSignal(
    zWorkspacesServicesConfigs.parse(
      JSON.parse(
        Services.prefs.getStringPref("floorp.workspaces.configs", getOldConfigs),
      ),
    ),
  );
  createEffect(() => {
    Services.prefs.setStringPref(
      "floorp.workspaces.configs",
      JSON.stringify(config()),
    );
  });

  const observer = () => setConfig(
    zWorkspacesServicesConfigs.parse(
      JSON.parse(Services.prefs.getStringPref("floorp.workspaces.configs")),
    ),
  );
  Services.prefs.addObserver("floorp.workspaces.configs", observer);
  onCleanup(()=>{
    Services.prefs.removeObserver("floorp.workspaces.configs", observer);
  });
  return [config,setConfig]
}

/** Configs */
export const [config,setConfig] = createRootHMR(createConfig,import.meta.hot);
