/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NRSearchEngineChild extends JSWindowActorChild {
  actorCreated() {
    console.debug("NRSearchEngineChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5187" ||
      window?.location.port === "5186" ||
      window?.location.href.startsWith("chrome://")
    ) {
      console.debug("NRSearchEngine 5187 ! or Chrome Page!");

      Cu.exportFunction(this.getSearchEngines.bind(this), window, {
        defineAs: "NRGetSearchEngines",
      });

      Cu.exportFunction(this.getDefaultEngine.bind(this), window, {
        defineAs: "NRGetDefaultEngine",
      });

      Cu.exportFunction(this.setDefaultEngine.bind(this), window, {
        defineAs: "NRSetDefaultEngine",
      });

      Cu.exportFunction(this.getDefaultPrivateEngine.bind(this), window, {
        defineAs: "NRGetDefaultPrivateEngine",
      });

      Cu.exportFunction(this.setDefaultPrivateEngine.bind(this), window, {
        defineAs: "NRSetDefaultPrivateEngine",
      });

      Cu.exportFunction(this.getSuggestions.bind(this), window, {
        defineAs: "NRGetSuggestions",
      });
    }
  }

  getSearchEngines(callback: (data: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetSearchEngines = resolve;
    });
    this.sendAsyncMessage("SearchEngine:getSearchEngines");
    promise.then((data) => callback(data));
  }

  resolveGetSearchEngines: ((data: string) => void) | null = null;

  getDefaultEngine(callback: (data: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetDefaultEngine = resolve;
    });
    this.sendAsyncMessage("SearchEngine:getDefaultEngine");
    promise.then((data) => callback(data));
  }

  resolveGetDefaultEngine: ((data: string) => void) | null = null;

  setDefaultEngine(
    engineId: string,
    callback: (response: string) => void = () => {},
  ) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveSetDefaultEngine = resolve;
    });
    this.sendAsyncMessage("SearchEngine:setDefaultEngine", { engineId });
    promise.then((response) => callback(response));
  }

  resolveSetDefaultEngine: ((response: string) => void) | null = null;

  getDefaultPrivateEngine(callback: (data: string) => void = () => {}) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetDefaultPrivateEngine = resolve;
    });
    this.sendAsyncMessage("SearchEngine:getDefaultPrivateEngine");
    promise.then((data) => callback(data));
  }

  resolveGetDefaultPrivateEngine: ((data: string) => void) | null = null;

  setDefaultPrivateEngine(
    engineId: string,
    callback: (response: string) => void = () => {},
  ) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveSetDefaultPrivateEngine = resolve;
    });
    this.sendAsyncMessage("SearchEngine:setDefaultPrivateEngine", { engineId });
    promise.then((response) => callback(response));
  }

  resolveSetDefaultPrivateEngine: ((response: string) => void) | null = null;

  getSuggestions(
    query: string,
    engineId: string,
    callback: (data: string) => void = () => {},
  ) {
    const promise = new Promise<string>((resolve, _reject) => {
      this.resolveGetSuggestions = resolve;
    });
    this.sendAsyncMessage("SearchEngine:getSuggestions", { query, engineId });
    promise.then((data) => callback(data));
  }

  resolveGetSuggestions: ((data: string) => void) | null = null;

  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "SearchEngine:searchEnginesResponse": {
        this.resolveGetSearchEngines?.(message.data);
        this.resolveGetSearchEngines = null;
        break;
      }

      case "SearchEngine:defaultEngineResponse": {
        this.resolveGetDefaultEngine?.(message.data);
        this.resolveGetDefaultEngine = null;
        break;
      }

      case "SearchEngine:setDefaultEngineResponse": {
        this.resolveSetDefaultEngine?.(message.data);
        this.resolveSetDefaultEngine = null;
        break;
      }

      case "SearchEngine:defaultPrivateEngineResponse": {
        this.resolveGetDefaultPrivateEngine?.(message.data);
        this.resolveGetDefaultPrivateEngine = null;
        break;
      }

      case "SearchEngine:setDefaultPrivateEngineResponse": {
        this.resolveSetDefaultPrivateEngine?.(message.data);
        this.resolveSetDefaultPrivateEngine = null;
        break;
      }

      case "SearchEngine:getSuggestionsResponse": {
        this.resolveGetSuggestions?.(message.data);
        this.resolveGetSuggestions = null;
        break;
      }
    }
  }
}
