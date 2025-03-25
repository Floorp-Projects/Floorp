/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { type Accessor, createSignal } from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import i18next from "i18next";
import { addI18nObserver } from "../../../../i18n/config.ts";
import { workspaceIconTranslationKeys } from "./icon-translations.ts";

type IconTranslationsMap = Record<string, string>;

const getDefaultTranslations = (): IconTranslationsMap =>
  Object.keys(workspaceIconTranslationKeys).reduce(
    (acc, iconName) => {
      acc[iconName] = iconName;
      return acc;
    },
    {} as IconTranslationsMap,
  );

export class IconTranslationsHandler {
  private static instance: IconTranslationsHandler | null = null;

  private iconTranslations: Accessor<IconTranslationsMap> = () =>
    getDefaultTranslations();
  private setIconTranslations: (value: IconTranslationsMap) => void = () => {};

  public static getInstance(): IconTranslationsHandler {
    if (!IconTranslationsHandler.instance) {
      IconTranslationsHandler.instance = new IconTranslationsHandler();
    }
    return IconTranslationsHandler.instance;
  }

  private constructor() {
    createRootHMR(() => {
      const [translations, setTranslations] = createSignal<IconTranslationsMap>(
        getDefaultTranslations(),
      );
      this.iconTranslations = translations;
      this.setIconTranslations = setTranslations;
      this.updateTranslations();

      addI18nObserver(() => {
        this.updateTranslations();
      });
    }, import.meta.hot);
  }

  private updateTranslations(): void {
    const translations: IconTranslationsMap = {};

    Object.entries(workspaceIconTranslationKeys).forEach(
      ([iconName, translationKey]) => {
        translations[iconName] = i18next.t(translationKey);
      },
    );

    this.setIconTranslations(translations);
  }

  public getTranslatedIconName(iconName: string): string {
    const translations = this.iconTranslations();
    return translations[iconName] || iconName;
  }

  public getAllTranslations(): IconTranslationsMap {
    return this.iconTranslations();
  }
}
