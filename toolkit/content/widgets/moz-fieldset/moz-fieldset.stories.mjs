/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./moz-fieldset.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "../moz-toggle/moz-toggle.mjs";

export default {
  title: "UI Widgets/Fieldset",
  component: "moz-fieldset",
  parameters: {
    status: "in-development",
    fluent: `
moz-fieldset-label =
  .label = Related Settings
moz-fieldset-description =
  .label = Some Settings
  .description = Perhaps you want to have a longer description of what these settings do. Width is set explicitly for emphasis.
  `,
  },
};

const Template = ({ label, description, l10nId }) => html`
  <moz-fieldset
    data-l10n-id=${l10nId}
    .label=${label}
    .description=${description}
    style="width: 400px;"
  >
    <moz-toggle
      pressed
      label="First setting"
      description="These could have descriptions too."
    ></moz-toggle>
    <label><input type="checkbox" /> Second setting</label>
    <label><input type="checkbox" /> Third setting</label>
  </moz-fieldset>
`;

export const Default = Template.bind({});
Default.args = {
  label: "",
  description: "",
  l10nId: "moz-fieldset-label",
};
export const WithDescription = Template.bind({});
WithDescription.args = {
  ...Default.args,
  l10nId: "moz-fieldset-description",
};
