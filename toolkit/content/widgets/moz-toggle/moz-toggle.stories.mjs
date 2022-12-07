/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./moz-toggle.mjs";

const fxToggleConfig = {
  title: "Design System/Experiments/FxToggle",
};

const Template = ({ checked, disabled, label, description, ariaLabel }) => html`
  <div style="max-width: 400px">
    <moz-toggle
      ?checked=${checked}
      ?disabled=${disabled}
      label=${ifDefined(label)}
      description=${ifDefined(description)}
      aria-label=${ifDefined(ariaLabel)}
    ></moz-toggle>
  </div>
`;

export const Default = Template.bind({});
Default.args = {
  checked: true,
  disabled: false,
  ariaLabel: "This is the aria-label",
};

export const Disabled = Template.bind({});
Disabled.args = {
  ...Default.args,
  disabled: true,
};

export const WithLabel = Template.bind({});
WithLabel.args = {
  checked: true,
  disabled: false,
  label: "This is the label",
};

export const WithDescription = Template.bind({});
WithDescription.args = {
  ...WithLabel.args,
  description: "This is the description",
};

export default fxToggleConfig;
