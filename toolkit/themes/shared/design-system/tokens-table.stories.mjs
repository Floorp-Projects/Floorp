/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  LitElement,
  classMap,
} from "chrome://global/content/vendor/lit.all.mjs";
import { storybookTables, variableLookupTable } from "./tokens-storybook.mjs";
import styles from "./tokens-table.css";

export default {
  title: "Docs/Tokens Table",
  parameters: {
    options: {
      showPanel: false,
    },
  },
};

const HCM_MAP = {
  ActiveText: "#8080FF",
  ButtonBorder: "#000000",
  ButtonFace: "#000000",
  ButtonText: "#FFEE32",
  Canvas: "#000000",
  CanvasText: "#ffffff",
  Field: "#000000",
  FieldText: "#ffffff",
  GrayText: "#A6A6A6",
  Highlight: "#D6B4FD",
  HighlightText: "#2B2B2B",
  LinkText: "#8080FF",
  Mark: "#000000",
  MarkText: "#000000",
  SelectedItem: "#D6B4FD",
  SelectedItemText: "#2B2B2B",
  AccentColor: "8080FF",
  AccentColorText: "#2B2B2B",
  VisitedText: "#8080FF",
};

const THEMED_TABLES = [
  "attention-dot",
  "background-color",
  "border",
  "border-color",
  "opacity",
  "text-color",
  "color",
  "outline",
  "icon-color",
  "link",
];

/**
 *
 */
class TablesPage extends LitElement {
  #query = "";

  static properties = {
    surface: { type: String, state: true },
    tokensData: { type: Object, state: true },
    filteredTokens: { type: Object, state: true },
  };

  constructor() {
    super();
    this.surface = "brand";
    this.tokensData = storybookTables;
  }

  handleSurfaceChange(e) {
    this.surface = e.target.value;
  }

  handleInput(e) {
    this.#query = e.originalTarget.value.trim().toLowerCase();
    e.preventDefault();
    this.handleSearch();
  }

  debounce(fn, delayMs = 300) {
    let timeout;
    return function () {
      clearTimeout(timeout);
      timeout = setTimeout(() => {
        fn.apply(this, arguments);
      }, delayMs);
    };
  }

  handleSearch() {
    // Clear filteredTokens to show all data.
    if (!this.#query) {
      this.filteredTokens = null;
    }

    let filteredTokens = Object.entries(this.tokensData).reduce(
      (acc, [key, tokens]) => {
        if (key.includes(this.#query)) {
          return { ...acc, [key]: tokens };
        }
        let filteredItems = tokens.filter(({ name: tokenName, value }) =>
          this.filterTokens(this.#query, tokenName, value)
        );
        if (filteredItems.length) {
          return { ...acc, [key]: filteredItems };
        }
        return acc;
      },
      {}
    );
    this.filteredTokens = filteredTokens;
  }

  filterTokens(searchTerm, tokenName, tokenVal) {
    if (
      tokenName.includes(searchTerm) ||
      (typeof tokenVal == "string" && tokenVal.includes(searchTerm))
    ) {
      return true;
    }
    if (typeof tokenVal == "object") {
      return Object.entries(tokenVal).some(([key, val]) =>
        this.filterTokens(searchTerm, key, val)
      );
    }
    return false;
  }

  handleClearSearch(e) {
    this.#query = "";
    e.preventDefault();
    this.handleSearch();
  }

  render() {
    if (!this.tokensData) {
      return "";
    }

    return html`
      <link rel="stylesheet" href=${styles} />
      <div class="page-wrapper">
        <div class="filters-wrapper">
          <div class="search-wrapper">
            <div class="search-icon"></div>
            <input
              type="search"
              placeholder="Filter tokens"
              @input=${this.debounce(this.handleInput)}
              .value=${this.#query}
            />
            <div
              class="clear-icon"
              role="button"
              title="Clear"
              ?hidden=${!this.#query}
              @click=${this.handleClearSearch}
            ></div>
          </div>
          <fieldset id="surface" @change=${this.handleSurfaceChange}>
            <label>
              <input
                type="radio"
                name="surface"
                value="brand"
                ?checked=${this.surface == "brand"}
              />
              In-content
            </label>
            <label>
              <input
                type="radio"
                name="surface"
                value="platform"
                ?checked=${this.surface == "platform"}
              />
              Chrome
            </label>
          </fieldset>
        </div>
        <div class="tables-wrapper">
          ${Object.entries(this.filteredTokens ?? this.tokensData).map(
            ([tableName, tableEntries]) => {
              return html`
                <tokens-table
                  name=${tableName}
                  surface=${this.surface}
                  .tokens=${tableEntries}
                >
                </tokens-table>
              `;
            }
          )}
        </div>
      </div>
    `;
  }
}
customElements.define("tables-page", TablesPage);

/**
 *
 */
class TokensTable extends LitElement {
  TEMPLATES = {
    "font-size": this.fontTemplate,
    "font-weight": this.fontTemplate,
    "icon-color": this.iconTemplate,
    "icon-size": this.iconTemplate,
    link: this.linkTemplate,
    margin: this.spaceAndSizeTemplate,
    "min-height": this.spaceAndSizeTemplate,
    outline: this.outlineTemplate,
    padding: this.paddingTemplate,
    size: this.spaceAndSizeTemplate,
    space: this.spaceAndSizeTemplate,
    "text-color": this.fontTemplate,
  };

  static properties = {
    name: { type: String },
    tokens: { type: Array },
    surface: { type: String },
  };

  createRenderRoot() {
    return this;
  }

  getDisplayValues(theme, token) {
    let value = this.getResolvedValue(theme, token);
    let raw = this.getRawValue(theme, value);
    return { value, ...(raw !== value && { raw }) };
  }

  // Return the value with variable references.
  // e.g. var(--color-white)
  getResolvedValue(theme, token) {
    if (typeof token == "string" || typeof token == "number") {
      return token;
    }

    if (theme == "hcm") {
      return (
        token.forcedColors ||
        token.prefersContrast ||
        token[this.surface]?.default ||
        token.default
      );
    }

    if (token[this.surface]) {
      return this.getResolvedValue(theme, token[this.surface]);
    }

    if (token[theme]) {
      return token[theme];
    }

    return token.default;
  }

  // Return the value with any variables replaced by what they represent.
  // e.g. #ffffff
  getRawValue(theme, token) {
    let cssRegex = /(?<var>var\(--(?<lookupKey>[a-z-0-9,\s]+)\))/;
    let matches = cssRegex.exec(token);
    if (matches) {
      let variableValue = variableLookupTable[matches.groups?.lookupKey];
      if (typeof variableValue == "undefined") {
        return token;
      }
      if (typeof variableValue == "object") {
        variableValue = this.getResolvedValue(theme, variableValue);
      }
      let rawValue = token.replace(matches.groups?.var, variableValue);
      if (rawValue.match(cssRegex)) {
        return this.getRawValue(theme, rawValue);
      }
      return rawValue;
    }
    return token;
  }

  getTemplate(value, tokenName, category = this.name) {
    // 0 is a valid value
    if (value == undefined) {
      return "N/A";
    }

    let templateFn = this.TEMPLATES[category]?.bind(this);
    if (templateFn) {
      return templateFn(category, value, tokenName);
    }

    return html`
      <div
        class="default-preview"
        style="${this.getDisplayProperty(category)}: ${HCM_MAP[value] ?? value}"
      ></div>
    `;
  }

  outlineTemplate(_, value, tokenName) {
    let property = tokenName.replaceAll(/--|focus-|-error/g, "");
    if (property == "outline-inset") {
      property = "outline-offset";
    }
    return html`
      <div
        class="outline-preview"
        style="${property}: ${HCM_MAP[value] ?? value};"
      ></div>
    `;
  }

  fontTemplate(category, value) {
    return html`
      <div class="text-wrapper">
        <p
          style="${this.getDisplayProperty(category)}: ${HCM_MAP[value] ??
          value};"
        >
          The quick brown fox jumps over the lazy dog
        </p>
      </div>
    `;
  }

  iconTemplate(_, value, tokenName) {
    let property = tokenName.includes("color") ? "background-color" : "height";
    return html`
      <div
        class="icon-preview"
        style="${property}: ${HCM_MAP[value] ?? value};"
      ></div>
    `;
  }

  linkTemplate(_, value, tokenName) {
    let hasOutline = tokenName.includes("outline");
    return html`
      <a
        class=${classMap({ "link-preview": true, outline: hasOutline })}
        style="color: ${HCM_MAP[value] ?? value};"
      >
        Click me!
      </a>
    `;
  }

  spaceAndSizeTemplate(_, value, tokenName) {
    let isSize = tokenName.includes("size") || tokenName.includes("height");
    let items = isSize
      ? html`
          <div class="item" style="height: ${value};width: ${value};"></div>
        `
      : html`
          <div class="item"></div>
          <div class="item"></div>
        `;

    return html`
      <div
        class="space-size-preview space-size-background"
        style="gap: ${value};"
      >
        ${items}
      </div>
    `;
  }

  paddingTemplate(_, value) {
    return html`
      <div class="space-size-background">
        <div class="padding-item outer" style="padding: ${value}">
          <div class="padding-item inner"></div>
        </div>
      </div>
    `;
  }

  getDisplayProperty(category) {
    switch (category) {
      case "attention-dot":
      case "color":
        return "background-color";
      case "text-color":
        return "color";
      default:
        return category.replace("button", "");
    }
  }

  cellTemplate(type, tokenValue, tokenName) {
    let { value, raw } = this.getDisplayValues("default", tokenValue);
    return html`
      <td>
        <div class="preview-wrapper">
          ${type == "preview"
            ? html`${this.getTemplate(raw ?? value, tokenName)}`
            : html`<p class="value">${value}</p>
                ${raw ? html`<p class="value">${raw}</p>` : ""}`}
        </div>
      </td>
    `;
  }

  themeCellTemplate(theme, tokenValue, tokenName) {
    let { value, raw } = this.getDisplayValues(theme, tokenValue);
    return html`
      <td class="${theme}-theme">
        <div class="preview-wrapper">
          ${this.getTemplate(raw ?? value, tokenName)}
          <p class="value">${value}</p>
          ${raw ? html`<p class="value">${raw}</p>` : ""}
        </div>
      </td>
    `;
  }

  render() {
    let themedTable = THEMED_TABLES.includes(this.name);
    return html`
      <details class="table-wrapper" open="">
        <summary class="table-heading">
          <h3>${this.name}</h3>
        </summary>
        <table>
          <thead>
            <tr>
              <th>Name</th>
              ${themedTable
                ? html`
                    <th>Light</th>
                    <th>Dark</th>
                    <th>High contrast</th>
                  `
                : html`
                    <th>Value</th>
                    <th>Preview</th>
                  `}
            </tr>
          </thead>
          <tbody>
            ${this.tokens.map(({ name: tokenName, value }) => {
              return html`<tr id=${tokenName}>
                <td>${tokenName}</td>
                ${themedTable
                  ? html`
                      ${this.themeCellTemplate("light", value, tokenName)}
                      ${this.themeCellTemplate("dark", value, tokenName)}
                      ${this.themeCellTemplate("hcm", value, tokenName)}
                    `
                  : html`
                      ${this.cellTemplate("value", value, tokenName)}
                      ${this.cellTemplate("preview", value, tokenName)}
                    `}
              </tr>`;
            })}
          </tbody>
        </table>
      </details>
    `;
  }
}
customElements.define("tokens-table", TokensTable);

export const Default = () => {
  return html`<tables-page></tables-page>`;
};
