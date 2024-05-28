/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import "./moz-checkbox.mjs";

export default {
  title: "UI Widgets/Checkbox",
  component: "moz-checkbox",
  parameters: {
    status: "in-development",
    handles: ["click", "input", "change"],
    fluent: `
moz-checkbox-label =
  .label = The label of the checkbox
    `,
  },
};

const Template = ({ l10nId, checked, label }) => html`
  <moz-checkbox
    ?checked=${checked}
    .label=${label}
    data-l10n-id=${ifDefined(l10nId)}
    data-l10n-attrs="label"
  ></moz-checkbox>
`;

export const Default = Template.bind({});
Default.args = {
  l10nId: "moz-checkbox-label",
  checked: false,
  label: "",
};

export const CheckedByDefault = Template.bind({});
CheckedByDefault.args = {
  ...Default.args,
  checked: true,
};
