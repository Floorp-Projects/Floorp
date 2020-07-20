/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["SpecialMessageActionSchemas"];

const SpecialMessageActionSchemas = {
  DISABLE_STP_DOORHANGERS: {
    description: "Disables all STP doorhangers.",
    properties: {
      type: {
        enum: ["DISABLE_STP_DOORHANGERS"],
        type: "string",
      },
    },
    type: "object",
  },
  HIGHLIGHT_FEATURE: {
    description: "Highlights an element, such as a menu item.",
    properties: {
      data: {
        properties: {
          args: {
            description: "The element to highlight",
            type: "string",
          },
        },
        type: "object",
      },
      type: {
        enum: ["HIGHLIGHT_FEATURE"],
        type: "string",
      },
    },
    type: "object",
  },
  INSTALL_ADDON_FROM_URL: {
    description: "Install an add-on from AMO.",
    properties: {
      data: {
        properties: {
          telemetrySource: {
            type: "string",
          },
          url: {
            type: "string",
          },
        },
        type: "object",
      },
      type: {
        enum: ["INSTALL_ADDON_FROM_URL"],
        type: "string",
      },
    },
    type: "object",
  },
  OPEN_ABOUT_PAGE: {
    description: "Opens an about: page in Firefox",
    properties: {
      data: {
        properties: {
          args: {
            description: 'The about page. E.g. "welcome" for about:welcome',
            type: "string",
          },
          where: {
            default: "tab",
            description: "Where the URL is opened.",
            enum: ["current", "save", "tab", "tabshifted", "window"],
            type: "string",
          },
          entrypoint: {
            description:
              'Any optional entrypoint value that will be added to the search. E.g. "foo=bar" would result in about:welcome?foo=bar',
            type: "string",
          },
        },
        type: "object",
      },
      type: {
        enum: ["OPEN_ABOUT_PAGE"],
        type: "string",
      },
    },
    type: "object",
  },
  OPEN_APPLICATIONS_MENU: {
    description: "Opens an application menu",
    properties: {
      data: {
        properties: {
          args: {
            description: 'The menu name, e.g. "appMenu"',
            type: "string",
          },
        },
        type: "object",
      },
      type: {
        enum: ["OPEN_APPLICATIONS_MENU"],
        type: "string",
      },
    },
    type: "object",
  },
  OPEN_AWESOME_BAR: {
    description: "Focuses and expands the awesome bar.",
    properties: {
      type: {
        enum: ["OPEN_AWESOME_BAR"],
        type: "string",
      },
    },
    type: "object",
  },
  OPEN_PREFERENCES_PAGE: {
    description: "Opens a preference page",
    properties: {
      data: {
        properties: {
          category: {
            description: 'Section of about:preferences, e.g. "privacy-reports"',
            type: "string",
          },
          entrypoint: {
            description: "Add a queryparam for metrics",
            type: "string",
          },
        },
        type: "object",
      },
      type: {
        enum: ["OPEN_PREFERENCES_PAGE"],
        type: "string",
      },
    },
    type: "object",
  },
  OPEN_PRIVATE_BROWSER_WINDOW: {
    description: "Opens a private browsing window.",
    properties: {
      type: {
        enum: ["OPEN_PRIVATE_BROWSER_WINDOW"],
        type: "string",
      },
    },
    type: "object",
  },
  OPEN_PROTECTION_PANEL: {
    description: "Opens the protecions panel",
    properties: {
      type: {
        enum: ["OPEN_PROTECTION_PANEL"],
        type: "string",
      },
    },
    type: "object",
  },
  OPEN_PROTECTION_REPORT: {
    description: "Opens the protecions report",
    properties: {
      type: {
        enum: ["OPEN_PROTECTION_REPORT"],
        type: "string",
      },
    },
    type: "object",
  },
  OPEN_URL: {
    description: "Opens a URL.",
    properties: {
      data: {
        properties: {
          args: {
            type: "string",
          },
          where: {
            default: "currrent",
            description: "Where the URL is opened.",
            enum: ["current", "save", "tab", "tabshifted", "window"],
            type: "string",
          },
        },
        type: "object",
      },
      type: {
        enum: ["OPEN_URL"],
        type: "string",
      },
    },
    type: "object",
  },
  PIN_CURRENT_TAB: {
    description: "Pin the current tab.",
    properties: {
      type: {
        enum: ["PIN_CURRENT_TAB"],
        type: "string",
      },
    },
    type: "object",
  },
  SHOW_FIREFOX_ACCOUNTS: {
    description: "Show Firefox Accounts",
    properties: {
      type: {
        enum: ["SHOW_FIREFOX_ACCOUNTS"],
        type: "string",
      },
      data: {
        properties: {
          entrypoint: {
            type: "string",
            description: "Adds entrypoint={your value} to the FXA URL",
          },
        },
        type: "object",
      },
    },
    type: "object",
  },
  SHOW_MIGRATION_WIZARD: {
    description:
      "Shows the Migration Wizard to import data from another Browser. See https://support.mozilla.org/en-US/kb/import-data-another-browser",
    properties: {
      type: {
        enum: ["SHOW_MIGRATION_WIZARD"],
        type: "string",
      },
    },
    type: "object",
  },
  CANCEL: {
    description: "Cancel (dismiss) the CFR doorhanger.",
    properties: {
      type: {
        enum: ["CANCEL"],
        type: "string",
      },
    },
    type: "object",
  },
  DISABLE_DOH: {
    description: "Disable the DoH feature.",
    properties: {
      type: {
        enum: ["DISABLE_DOH"],
        type: "string",
      },
    },
    type: "object",
  },
  ACCEPT_DOH: {
    description: "Confirm to continue using the DoH feature.",
    properties: {
      type: {
        enum: ["ACCEPT_DOH"],
        type: "string",
      },
    },
    type: "object",
  },
};
