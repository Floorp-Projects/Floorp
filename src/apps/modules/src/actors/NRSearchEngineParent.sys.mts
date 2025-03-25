/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// nsISearchEngine IDLに基づいた型定義
interface nsISearchEngine {
  name: string;
  iconURI?: { spec: string };
  identifier: string;
  isAppProvided: boolean;
  id: string;
  telemetryId: string;
  description: string;
  hidden: boolean;
  alias: string;
  aliases: string[];
  searchUrlDomain: string;
  getIconURL(preferredWidth?: number): Promise<string>;
}

export class NRSearchEngineParent extends JSWindowActorParent {
  async receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "SearchEngine:getSearchEngines": {
        try {
          await Services.search.promiseInitialized;

          const engines = await Services.search.getVisibleEngines();
          const defaultEngine = await Services.search.getDefault();
          const defaultPrivateEngine = await Services.search
            .getDefaultPrivate();

          const engineList = await Promise.all(
            engines.map(async (engine: nsISearchEngine) => {
              let iconURL = null;
              try {
                iconURL = await engine.getIconURL();

                if (
                  iconURL &&
                  (iconURL.startsWith("blob:") ||
                    iconURL.startsWith("moz-extension:"))
                ) {
                  const { ExtensionUtils } = ChromeUtils.importESModule(
                    "resource://gre/modules/ExtensionUtils.sys.mjs",
                  );
                  iconURL = await ExtensionUtils.makeDataURI(iconURL);
                }
              } catch (error) {
                console.error("Failed to get icon URL:", error);
              }

              return {
                name: engine.name,
                iconURL: iconURL,
                searchUrl: engine.searchURLWithNoTerms.spec,
                identifier: engine.identifier,
                id: engine.id || engine.identifier,
                telemetryId: engine.telemetryId || "",
                description: engine.description || "",
                hidden: !!engine.hidden,
                alias: engine.alias || "",
                aliases: Array.isArray(engine.aliases) ? engine.aliases : [],
                domain: engine.searchUrlDomain || "",
                isAppProvided: !!engine.isAppProvided,
                isDefault: defaultEngine && defaultEngine.name === engine.name,
                isPrivateDefault: defaultPrivateEngine &&
                  defaultPrivateEngine.name === engine.name,
              };
            }),
          );

          this.sendAsyncMessage(
            "SearchEngine:searchEnginesResponse",
            JSON.stringify(engineList),
          );
        } catch (error) {
          this.sendAsyncMessage(
            "SearchEngine:searchEnginesResponse",
            JSON.stringify({
              success: false,
              error: String(error),
            }),
          );
        }
        break;
      }

      case "SearchEngine:getDefaultEngine": {
        try {
          await Services.search.promiseInitialized;

          const defaultEngine = await Services.search.getDefault();

          let iconURL = null;
          try {
            iconURL = await defaultEngine.getIconURL();

            if (
              iconURL &&
              (iconURL.startsWith("blob:") ||
                iconURL.startsWith("moz-extension:"))
            ) {
              const { ExtensionUtils } = ChromeUtils.importESModule(
                "resource://gre/modules/ExtensionUtils.sys.mjs",
              );
              iconURL = await ExtensionUtils.makeDataURI(iconURL);
            }
          } catch (error) {
            console.error("Failed to get icon URL:", error);
          }

          const engineData = {
            name: defaultEngine.name,
            iconURL: iconURL,
            identifier: defaultEngine.identifier,
            id: defaultEngine.id || defaultEngine.identifier,
            telemetryId: defaultEngine.telemetryId || "",
            description: defaultEngine.description || "",
            hidden: !!defaultEngine.hidden,
            alias: defaultEngine.alias || "",
            aliases: Array.isArray(defaultEngine.aliases)
              ? defaultEngine.aliases
              : [],
            domain: defaultEngine.searchUrlDomain || "",
            isAppProvided: !!defaultEngine.isAppProvided,
          };

          this.sendAsyncMessage(
            "SearchEngine:defaultEngineResponse",
            JSON.stringify(engineData),
          );
        } catch (error) {
          this.sendAsyncMessage(
            "SearchEngine:defaultEngineResponse",
            JSON.stringify({
              success: false,
              error: String(error),
            }),
          );
        }
        break;
      }

      case "SearchEngine:setDefaultEngine": {
        try {
          await Services.search.promiseInitialized;

          const { engineId } = message.data;

          const engines = await Services.search.getVisibleEngines();
          const targetEngine = engines.find((engine: nsISearchEngine) =>
            engine.identifier === engineId || engine.name === engineId
          );

          if (!targetEngine) {
            throw new Error(`Engine with ID ${engineId} not found`);
          }

          await Services.search.setDefault(
            targetEngine,
            Ci.nsISearchService.CHANGE_REASON_UNKNOWN,
          );

          this.sendAsyncMessage(
            "SearchEngine:setDefaultEngineResponse",
            JSON.stringify({
              success: true,
              engineId,
            }),
          );
        } catch (error) {
          this.sendAsyncMessage(
            "SearchEngine:setDefaultEngineResponse",
            JSON.stringify({
              success: false,
              error: String(error),
            }),
          );
        }
        break;
      }

      case "SearchEngine:getDefaultPrivateEngine": {
        try {
          await Services.search.promiseInitialized;

          const defaultPrivateEngine = await Services.search
            .getDefaultPrivate();

          let iconURL = null;
          try {
            iconURL = await defaultPrivateEngine.getIconURL();

            if (
              iconURL &&
              (iconURL.startsWith("blob:") ||
                iconURL.startsWith("moz-extension:"))
            ) {
              const { ExtensionUtils } = ChromeUtils.importESModule(
                "resource://gre/modules/ExtensionUtils.sys.mjs",
              );
              iconURL = await ExtensionUtils.makeDataURI(iconURL);
            }
          } catch (error) {
            console.error("Failed to get icon URL:", error);
          }

          const engineData = {
            name: defaultPrivateEngine.name,
            iconURL: iconURL,
            identifier: defaultPrivateEngine.identifier,
            id: defaultPrivateEngine.id || defaultPrivateEngine.identifier,
            telemetryId: defaultPrivateEngine.telemetryId || "",
            description: defaultPrivateEngine.description || "",
            hidden: !!defaultPrivateEngine.hidden,
            alias: defaultPrivateEngine.alias || "",
            aliases: Array.isArray(defaultPrivateEngine.aliases)
              ? defaultPrivateEngine.aliases
              : [],
            domain: defaultPrivateEngine.searchUrlDomain || "",
            isAppProvided: !!defaultPrivateEngine.isAppProvided,
          };

          this.sendAsyncMessage(
            "SearchEngine:defaultPrivateEngineResponse",
            JSON.stringify(engineData),
          );
        } catch (error) {
          this.sendAsyncMessage(
            "SearchEngine:defaultPrivateEngineResponse",
            JSON.stringify({
              success: false,
              error: String(error),
            }),
          );
        }
        break;
      }

      case "SearchEngine:setDefaultPrivateEngine": {
        try {
          await Services.search.promiseInitialized;

          const { engineId } = message.data;

          const engines = await Services.search.getVisibleEngines();
          const targetEngine = engines.find((engine: nsISearchEngine) =>
            engine.identifier === engineId || engine.name === engineId
          );

          if (!targetEngine) {
            throw new Error(`Engine with ID ${engineId} not found`);
          }

          await Services.search.setDefaultPrivate(
            targetEngine,
            Ci.nsISearchService.CHANGE_REASON_UNKNOWN,
          );

          this.sendAsyncMessage(
            "SearchEngine:setDefaultPrivateEngineResponse",
            JSON.stringify({
              success: true,
              engineId,
            }),
          );
        } catch (error) {
          this.sendAsyncMessage(
            "SearchEngine:setDefaultPrivateEngineResponse",
            JSON.stringify({
              success: false,
              error: String(error),
            }),
          );
        }
        break;
      }
    }
  }
}
