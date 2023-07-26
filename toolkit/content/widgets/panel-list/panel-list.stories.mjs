/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unassigned-import
import "./panel-list.js";
import { html, ifDefined } from "../vendor/lit.all.mjs";

export default {
  title: "UI Widgets/Panel List",
  component: "panel-list",
  parameters: {
    status: "in-development",
    actions: {
      handles: ["showing", "shown", "hidden", "click"],
    },
    fluent: `
panel-list-item-one = Item One
panel-list-item-two = Item Two (accesskey w)
panel-list-item-three = Item Three
panel-list-checked = Checked
panel-list-badged = Badged, look at me
panel-list-passwords = Passwords
panel-list-settings = Settings
    `,
  },
};

function openMenu(event) {
  if (
    event.type == "mousedown" ||
    event.inputSource == MouseEvent.MOZ_SOURCE_KEYBOARD ||
    !event.detail
  ) {
    event.target.getRootNode().querySelector("panel-list").toggle(event);
  }
}

const Template = ({ isOpen, items, wideAnchor }) =>
  html`
    <style>
      panel-item[icon="passwords"]::part(button) {
        background-image: url("chrome://browser/skin/login.svg");
      }
      panel-item[icon="settings"]::part(button) {
        background-image: url("chrome://global/skin/icons/settings.svg");
      }
      button {
        position: absolute;
        background-image: url("chrome://global/skin/icons/more.svg");
      }
      button[wide] {
        width: 400px !important;
      }
      .end {
        inset-inline-end: 30px;
      }

      .bottom {
        inset-block-end: 30px;
      }
    </style>
    ${isOpen
      ? ""
      : html`
          <button
            class="ghost-button icon-button"
            @click=${openMenu}
            @mousedown=${openMenu}
            ?wide="${wideAnchor}"
          ></button>
          <button
            class="ghost-button icon-button end"
            @click=${openMenu}
            @mousedown=${openMenu}
            ?wide="${wideAnchor}"
          ></button>
          <button
            class="ghost-button icon-button bottom"
            @click=${openMenu}
            @mousedown=${openMenu}
            ?wide="${wideAnchor}"
          ></button>
          <button
            class="ghost-button icon-button bottom end"
            @click=${openMenu}
            @mousedown=${openMenu}
            ?wide="${wideAnchor}"
          ></button>
        `}
    <panel-list
      ?stay-open=${isOpen}
      ?open=${isOpen}
      ?min-width-from-anchor=${wideAnchor}
    >
      ${items.map(i =>
        i == "<hr>"
          ? html` <hr /> `
          : html`
              <panel-item
                icon=${i.icon ?? ""}
                ?checked=${i.checked}
                ?badged=${i.badged}
                accesskey=${ifDefined(i.accesskey)}
                data-l10n-id=${i.l10nId ?? i}
              ></panel-item>
            `
      )}
    </panel-list>
  `;

export const Simple = Template.bind({});
Simple.args = {
  isOpen: false,
  wideAnchor: false,
  items: [
    "panel-list-item-one",
    { l10nId: "panel-list-item-two", accesskey: "w" },
    "panel-list-item-three",
    "<hr>",
    { l10nId: "panel-list-checked", checked: true },
    { l10nId: "panel-list-badged", badged: true, icon: "settings" },
  ],
};

export const Icons = Template.bind({});
Icons.args = {
  isOpen: false,
  wideAnchor: false,
  items: [
    { l10nId: "panel-list-passwords", icon: "passwords" },
    { l10nId: "panel-list-settings", icon: "settings" },
  ],
};

export const Open = Template.bind({});
Open.args = {
  ...Simple.args,
  wideAnchor: false,
  isOpen: true,
};

export const Wide = Template.bind({});
Wide.args = {
  ...Simple.args,
  wideAnchor: true,
};
