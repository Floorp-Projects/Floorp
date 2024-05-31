/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import "./moz-radio.mjs";

export default {
  title: "UI Widgets/Radio",
  component: "moz-radio",
  parameters: {
    actions: {
      handles: ["click", "input", "change"],
    },
    status: "in-development",
    fluent: `
moz-radio-0 =
  .label = Hello
moz-radio-1 =
  .label = Howdy
moz-radio-2=
  .label = Hola
moz-radio-group =
  .label = This is the group label
    `,
  },
};

let greetings = ["hello", "howdy", "hola"];
let icons = [
  "chrome://global/skin/icons/highlights.svg",
  "chrome://global/skin/icons/delete.svg",
  "chrome://global/skin/icons/defaultFavicon.svg",
];

const Template = ({
  groupL10nId = "moz-radio-group",
  groupName,
  unchecked,
  showIcons,
}) => html`
  <moz-radio-group name=${groupName} data-l10n-id=${groupL10nId}>
    ${greetings.map(
      (greeting, i) => html`
        <moz-radio
          ?checked=${i == 0 && !unchecked}
          value=${greeting}
          data-l10n-id=${`moz-radio-${i}`}
          iconSrc=${ifDefined(showIcons ? icons[i] : "")}
        ></moz-radio>
      `
    )}
  </moz-radio-group>
`;

export const Default = Template.bind({});
Default.args = {
  groupName: "greeting",
  unchecked: false,
  showIcons: false,
};

export const AllUnchecked = Template.bind({});
AllUnchecked.args = {
  groupName: "greeting",
  unchecked: true,
};

export const WithIcon = Template.bind({});
WithIcon.args = {
  ...Default.args,
  showIcons: true,
};
