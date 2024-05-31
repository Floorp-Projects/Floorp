/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
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
moz-radio-1 =
  .label = Hello
moz-radio-2 =
  .label = Howdy
moz-radio-3=
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
}) => html`
  <moz-radio-group name=${groupName} data-l10n-id=${groupL10nId}>
    <moz-radio
      ?checked=${!unchecked}
      value="hello"
      data-l10n-id="moz-radio-1"
    ></moz-radio>
    <moz-radio value="howdy" data-l10n-id="moz-radio-2"></moz-radio>
    <moz-radio value="hola" data-l10n-id="moz-radio-3"></moz-radio>
  </moz-radio-group>
`;

export const Default = Template.bind({});
Default.args = {
  groupName: "greeting",
  unchecked: false,
};

export const AllUnchecked = Template.bind({});
AllUnchecked.args = {
  groupName: "greeting",
  unchecked: true,
};
