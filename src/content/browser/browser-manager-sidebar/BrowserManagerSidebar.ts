/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Sidebar3Data, Sidebar3Panel } from "./SidebarData";

interface StaticSidebarDatum {
  url: string;
  l10n: string;
  defaultWidth: number;
  disabled: boolean;
}

const STATIC_SIDEBAR_DATA: Record<string, StaticSidebarDatum> = {
  "floorp//bms": {
    url: "chrome://browser/content/places/places.xhtml",
    l10n: "browser-manager-sidebar",
    defaultWidth: 600,
    disabled: false,
  },
  "floorp//bookmarks": {
    url: "chrome://browser/content/places/bookmarksSidebar.xhtml",
    l10n: "bookmark-sidebar",
    defaultWidth: 415,
    disabled: false,
  },
  "floorp//history": {
    url: "chrome://browser/content/places/historySidebar.xhtml",
    l10n: "history-sidebar",
    defaultWidth: 415,
    disabled: false,
  },
  "floorp//downloads": {
    url: "about:downloads",
    l10n: "download-sidebar",
    defaultWidth: 415,
    disabled: false,
  },
  //notes is available in floorp for v11.0.0.
  "floorp//notes": {
    url: "chrome://browser/content/notes/notes-bms.html",
    l10n: "notes-sidebar",
    defaultWidth: 550,
    disabled: false,
  },
};

export class BMSUtils {
  STATIC_SIDEBAR_DATA = STATIC_SIDEBAR_DATA;
  DEFAULT_WEBPANEL = [
    "https://translate.google.com",
    "https://support.ablaze.one",
    "https://docs.floorp.app",
  ];
  addPanel(url: string, uc: string) {
    let parentWindow = Services.wm.getMostRecentWindow("navigator:browser");
    const updateId = `w${new Date().toISOString()}`;
    const args = new (class implements nsISupports {
      QueryInterface = undefined;
      new = true;
      id = updateId;
      url = url !== "" ? url : undefined;
      userContext = uc !== "" ? uc : undefined;
    })();

    if (
      //@ts-expect-error
      parentWindow?.document.documentURI ===
      "chrome://browser/content/hiddenWindowMac.xhtml"
    ) {
      //@ts-expect-error
      parentWindow = null;
    }
    //@ts-expect-error
    if (parentWindow?.gDialogBox) {
      //@ts-expect-error
      parentWindow.gDialogBox.open(
        "chrome://browser/content/preferences/dialogs/customURLs.xhtml",
        args,
      );
    } else {
      Services.ww.openWindow(
        parentWindow,
        "chrome://browser/content/preferences/dialogs/customURLs.xhtml",
        "AddWebpanel",
        "chrome,titlebar,dialog,centerscreen,modal",
        args,
      );
    }
  }
  /**
   * @deprecated
   */
  prefsUpdate() {
    this.updatePrefs();
  }
  updatePrefs() {
    const defaultPref = Sidebar3Data.parse({
      panels: {},
    });

    let elem: keyof typeof this.STATIC_SIDEBAR_DATA;
    for (elem in this.STATIC_SIDEBAR_DATA) {
      if (this.STATIC_SIDEBAR_DATA[elem].disabled) {
        continue;
      }
      defaultPref.panels[elem.replace("//", "__")] = Sidebar3Panel.parse({
        url: elem,
        width: this.STATIC_SIDEBAR_DATA[elem].defaultWidth,
      });
    }

    for (const elem in this.DEFAULT_WEBPANEL) {
      defaultPref.panels[`w${elem}`] = Sidebar3Panel.parse({
        url: this.DEFAULT_WEBPANEL[elem],
        width: -1,
      });
    }
    Services.prefs
      .getDefaultBranch("")
      .setStringPref(
        "floorp.browser.sidebar2.data",
        JSON.stringify(defaultPref),
      );

    if (Services.prefs.prefHasUserValue("floorp.browser.sidebar2.data")) {
      const prefTemp = JSON.parse(
        Services.prefs.getStringPref("floorp.browser.sidebar2.data"),
      );
      const setPref = Sidebar3Data.parse(undefined);
      for (const elem of prefTemp.index) {
        setPref.panels[elem] = prefTemp.data[elem];
        //setPref.index.push(elem);
      }
      Services.prefs.setStringPref(
        "floorp.browser.sidebar2.data",
        JSON.stringify(setPref),
      );
    }
  }
}

// export const BrowserManagerSidebar = {
//   STATIC_SIDEBAR_DATA: STATIC_SIDEBAR_DATA,

//   DEFAULT_WEBPANEL: [
//     "https://translate.google.com",
//     "https://support.ablaze.one",
//     "https://docs.floorp.app",
//   ],
// prefsUpdate() {
//   const defaultPref = { data: {}, index: [] };
//   for (const elem in this.STATIC_SIDEBAR_DATA) {
//     if (this.STATIC_SIDEBAR_DATA[elem].enabled === false) {
//       delete this.STATIC_SIDEBAR_DATA[elem];
//       continue;
//     }
//     defaultPref.data[elem.replace("//", "__")] = {
//       url: elem,
//       width: this.STATIC_SIDEBAR_DATA[elem].defaultWidth,
//     };
//     defaultPref.index.push(elem.replace("//", "__"));
//   }
//   for (const elem in this.DEFAULT_WEBPANEL) {
//     defaultPref.data[`w${elem}`] = { url: this.DEFAULT_WEBPANEL[elem] };
//     defaultPref.index.push(`w${elem}`);
//   }
//   Services.prefs
//     .getDefaultBranch(null)
//     .setStringPref(
//       "floorp.browser.sidebar2.data",
//       JSON.stringify(defaultPref),
//     );

//   if (Services.prefs.prefHasUserValue("floorp.browser.sidebar2.data")) {
//     const prefTemp = JSON.parse(
//       Services.prefs.getStringPref("floorp.browser.sidebar2.data"),
//     );
//     const setPref = { data: {}, index: [] };
//     for (const elem of prefTemp.index) {
//       setPref.data[elem] = prefTemp.data[elem];
//       setPref.index.push(elem);
//     }
//     Services.prefs.setStringPref(
//       "floorp.browser.sidebar2.data",
//       JSON.stringify(setPref),
//     );
//   }
// },
// getFavicon(sbar_url: string, elem: Element) {
//   try {
//     new URL(sbar_url);
//   } catch (e) {
//     elem.style.removeProperty("--BMSIcon");
//   }

//   if (sbar_url.startsWith("http://") || sbar_url.startsWith("https://")) {
//     const iconProvider = Services.prefs.getStringPref(
//       "floorp.browser.sidebar.useIconProvider",
//       null,
//     );
//     let icon_url;
//     switch (iconProvider) {
//       case "google":
//         icon_url = `https://www.google.com/s2/favicons?domain_url=${encodeURIComponent(
//           sbar_url,
//         )}`;
//         break;
//       case "duckduckgo":
//         icon_url = `https://external-content.duckduckgo.com/ip3/${
//           new URL(sbar_url).hostname
//         }.ico`;
//         break;
//       case "yandex":
//         icon_url = `https://favicon.yandex.net/favicon/v2/${
//           new URL(sbar_url).origin
//         }`;
//         break;
//       case "hatena":
//         icon_url = `https://cdn-ak.favicon.st-hatena.com/?url=${encodeURIComponent(
//           sbar_url,
//         )}`; // or `https://favicon.hatena.ne.jp/?url=${encodeURIComponent(sbar_url)}`
//         break;
//       default:
//         icon_url = `https://www.google.com/s2/favicons?domain_url=${encodeURIComponent(
//           sbar_url,
//         )}`;
//         break;
//     }

//     fetch(icon_url)
//       .then(async (response) => {
//         if (response.status !== 200) {
//           throw new Error(`${response.status} ${response.statusText}`);
//         }

//         const reader = new FileReader();

//         const blob_data = await response.blob();

//         const icon_data_url = await new Promise((resolve) => {
//           reader.addEventListener("load", function () {
//             resolve(this.result);
//           });
//           reader.readAsDataURL(blob_data);
//         });

//         if (
//           icon_data_url ===
//           "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4AWIAAYAAAwAABQABggWTzwAAAABJRU5ErkJggg=="
//         ) {
//           // Yandex will return a 1px size icon with status code 200 if the icon is not available. If it matches a specific Data URL, it will be considered as a failure to acquire, and this process will be aborted.
//           throw new Error("Yandex returns 404. (1px size icon)");
//         }
//         if (elem.style.getPropertyValue("--BMSIcon") !== icon_data_url) {
//           elem.style.setProperty("--BMSIcon", `url(${icon_data_url})`);
//         }
//       })
//       .catch((reject) => {
//         const origin = new URL(sbar_url).origin;
//         const iconExtensions = ["ico", "png", "jpg", "jpeg"];

//         const fetchIcon = async (extension) => {
//           const iconUrl = `${origin}/favicon.${extension}`;
//           const response = await fetch(iconUrl);
//           if (
//             response.ok &&
//             elem.style.getPropertyValue("--BMSIcon") !== iconUrl
//           ) {
//             elem.style.setProperty("--BMSIcon", `url(${iconUrl})`);
//             return true;
//           }
//           return false;
//         };

//         const fetchIcons = iconExtensions.map(fetchIcon);

//         Promise.allSettled(fetchIcons).then((results) => {
//           const iconFound = results.some(
//             (result) => result.status === "fulfilled" && result.value,
//           );
//           if (!iconFound) {
//             elem.style.removeProperty("--BMSIcon");
//           }
//         });
//       });
//   } else if (sbar_url.startsWith("moz-extension://")) {
//     const addon_id = new URL(sbar_url).hostname;
//     const addon_base_url = `moz-extension://${addon_id}`;
//     fetch(`${addon_base_url}/manifest.json`)
//       .then(async (response) => {
//         if (response.status !== 200) {
//           throw new Error(`${response.status} ${response.statusText}`);
//         }

//         const addon_manifest = await response.json();

//         const addon_icon_path =
//           addon_manifest.icons[
//             Math.max(...Object.keys(addon_manifest.icons))
//           ];
//         if (addon_icon_path === undefined) {
//           throw new Error(`Icon not found.${addon_manifest.icons}`);
//         }

//         const addon_icon_url = addon_icon_path.startsWith("/")
//           ? `${addon_base_url}${addon_icon_path}`
//           : `${addon_base_url}/${addon_icon_path}`;

//         if (BROWSER_SIDEBAR_DATA.data[sbar_id.slice(7)].url === sbar_url) {
//           // Check that the URL has not changed after the icon is retrieved.
//           elem.style.setProperty("--BMSIcon", `url(${addon_icon_url})`);
//         }
//       })
//       .catch((reject) => {
//         elem.style.setProperty(
//           "--BMSIcon",
//           "url(chrome://mozapps/skin/extensions/extensionGeneric.svg)",
//         );
//       });
//   } else if (sbar_url.startsWith("file://")) {
//     elem.style.setProperty("--BMSIcon", `url(moz-icon:${sbar_url}?size=128)`);
//   } else if (sbar_url.startsWith("extension")) {
//     const iconURL = sbar_url.split(",")[4];
//     elem.style.setProperty("--BMSIcon", `url(${iconURL})`);
//     elem.className += " extension-icon";
//     const listTexts =
//       "chrome://browser/content/BMS-extension-needs-white-bg.txt";
//     fetch(listTexts)
//       .then((response) => {
//         return response.text();
//       })
//       .then((text) => {
//         const lines = text.split(/\r?\n/);
//         for (const line of lines) {
//           if (line === sbar_url.split(",")[2]) {
//             elem.className += " extension-icon-add-white";
//             break;
//           }
//           elem.classList.remove("extension-icon-add-white");
//         }
//       });
//   }

//   if (!sbar_url.startsWith("extension")) {
//     elem.classList.remove("extension-icon");
//     elem.classList.remove("extension-icon-add-white");
//   }
// },
// async getAddonSidebarPage(addonId: string) {
//   const addonUUID = JSON.parse(
//     Services.prefs.getStringPref("extensions.webextensions.uuids"),
//   );
//   const manifestJSON = await (
//     await fetch(`moz-extension://${addonUUID[addonId]}/manifest.json`)
//   ).json();
//   let toURL = manifestJSON.sidebar_action.default_panel;
//   if (!toURL.startsWith("./")) {
//     toURL = `./${toURL}`;
//   }
//   return new URL(toURL, `moz-extension://${addonUUID[addonId]}/`).href;
// },
//};
