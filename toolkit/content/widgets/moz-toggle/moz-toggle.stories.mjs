/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable import/no-unassigned-import */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import "./moz-toggle.mjs";
import "../moz-support-link/moz-support-link.mjs";

export default {
  title: "Design System/Experiments/MozToggle",
  component: "moz-toggle",
  parameters: {
    actions: {
      handles: ["toggle"],
    },
  },
};

const Template = ({
  pressed,
  disabled,
  label,
  description,
  ariaLabel,
  hasSupportLink,
}) => html`
  <div style="max-width: 400px">
    <moz-toggle
      ?pressed=${pressed}
      ?disabled=${disabled}
      label=${ifDefined(label)}
      description=${ifDefined(description)}
      aria-label=${ifDefined(ariaLabel)}
    >
      ${hasSupportLink
        ? html`
            <a
              is="moz-support-link"
              support-page="addons"
              slot="support-link"
            ></a>
          `
        : ""}
    </moz-toggle>
  </div>
`;

export const Toggle = Template.bind({});
Toggle.args = {
  pressed: true,
  disabled: false,
  ariaLabel: "This is the aria-label",
};

export const ToggleDisabled = Template.bind({});
ToggleDisabled.args = {
  ...Toggle.args,
  disabled: true,
};

export const WithLabel = Template.bind({});
WithLabel.args = {
  pressed: true,
  disabled: false,
  label: "This is the label",
  hasSupportLink: false,
};

export const WithDescription = Template.bind({});
WithDescription.args = {
  ...WithLabel.args,
  description: "This is the description.",
};

export const WithSupportLink = Template.bind({});
WithSupportLink.args = {
  ...WithDescription.args,
  hasSupportLink: true,
};
