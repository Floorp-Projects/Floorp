/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import "./moz-radio-group.mjs";

let greetings = ["hello", "howdy", "hola"];
let icons = [
  "chrome://global/skin/icons/highlights.svg",
  "chrome://global/skin/icons/delete.svg",
  "chrome://global/skin/icons/defaultFavicon.svg",
];

export default {
  title: "UI Widgets/Radio Group",
  component: "moz-radio-group",
  argTypes: {
    disabledButtons: {
      options: greetings,
      control: { type: "check" },
    },
  },
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

const Template = ({
  groupL10nId = "moz-radio-group",
  groupName,
  unchecked,
  showIcons,
  disabled,
  disabledButtons,
}) => html`
  <moz-radio-group
    name=${groupName}
    data-l10n-id=${groupL10nId}
    ?disabled=${disabled}
  >
    ${greetings.map(
      (greeting, i) => html`
        <moz-radio
          ?checked=${i == 0 && !unchecked}
          ?disabled=${disabledButtons.includes(greeting)}
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
  label: "",
  groupName: "greeting",
  unchecked: false,
  showIcons: false,
  disabled: false,
  disabledButtons: [],
};

export const AllUnchecked = Template.bind({});
AllUnchecked.args = {
  ...Default.args,
  unchecked: true,
};

export const WithIcon = Template.bind({});
WithIcon.args = {
  ...Default.args,
  showIcons: true,
};

export const DisabledRadioGroup = Template.bind({});
DisabledRadioGroup.args = {
  ...Default.args,
  disabled: true,
};

export const DisabledRadioButton = Template.bind({});
DisabledRadioButton.args = {
  ...Default.args,
  disabledButtons: ["hello"],
};
