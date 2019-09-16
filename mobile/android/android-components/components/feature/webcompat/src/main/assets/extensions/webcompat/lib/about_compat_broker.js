/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global browser, module */

class AboutCompatBroker {
  constructor(bindings) {
    this.portsToAboutCompatTabs = this.buildPorts();

    this._injections = bindings.injections;
    this._injections.bindAboutCompatBroker(this);

    this._uaOverrides = bindings.uaOverrides;
    this._uaOverrides.bindAboutCompatBroker(this);
  }

  buildPorts() {
    const ports = new Set();

    browser.runtime.onConnect.addListener(port => {
      ports.add(port);
      port.onDisconnect.addListener(function() {
        ports.delete(port);
      });
    });

    async function broadcast(message) {
      for (const port of ports) {
        port.postMessage(message);
      }
    }

    return { broadcast };
  }

  filterOverrides(overrides) {
    return overrides
      .filter(override => override.availableOnPlatform)
      .map(override => {
        const { id, active, bug, domain, hidden } = override;
        return { id, active, bug, domain, hidden };
      });
  }

  getOverrideOrInterventionById(id) {
    for (const [type, things] of Object.entries({
      overrides: this._uaOverrides.getAvailableOverrides(),
      interventions: this._injections.getAvailableInjections(),
    })) {
      for (const what of things) {
        if (what.id === id) {
          return { type, what };
        }
      }
    }
    return {};
  }

  bootup() {
    browser.runtime.onMessage.addListener(msg => {
      switch (msg.command || msg) {
        case "toggle": {
          const id = msg.id;
          const { type, what } = this.getOverrideOrInterventionById(id);
          if (!what) {
            return Promise.reject(
              `No such override or intervention to toggle: ${id}`
            );
          }
          this.portsToAboutCompatTabs
            .broadcast({ toggling: id, active: what.active })
            .then(async () => {
              switch (type) {
                case "interventions": {
                  if (what.active) {
                    await this._injections.disableInjection(what);
                  } else {
                    await this._injections.enableInjection(what);
                  }
                  break;
                }
                case "overrides": {
                  if (what.active) {
                    await this._uaOverrides.disableOverride(what);
                  } else {
                    await this._uaOverrides.enableOverride(what);
                  }
                  break;
                }
              }
              this.portsToAboutCompatTabs.broadcast({
                toggled: id,
                active: what.active,
              });
            });
          break;
        }
        case "getOverridesAndInterventions": {
          return Promise.resolve({
            overrides:
              (this._uaOverrides.isEnabled() &&
                this.filterOverrides(
                  this._uaOverrides.getAvailableOverrides()
                )) ||
              false,
            interventions:
              (this._injections.isEnabled() &&
                this.filterOverrides(
                  this._injections.getAvailableInjections()
                )) ||
              false,
          });
        }
      }
      return undefined;
    });
  }
}

module.exports = AboutCompatBroker;
