/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable import/no-unassigned-import */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import "./moz-toggle.mjs";
import "../moz-support-link/moz-support-link.mjs";

export default {
  title: "UI Widgets/Toggle",
  component: "moz-toggle",
  parameters: {
    status: "in-development",
    actions: {
      handles: ["toggle"],
    },
    fluent: `
moz-toggle-aria-label =
  .aria-label = This is the aria-label
moz-toggle-label =
  .label = This is the label
moz-toggle-description =
  .label = This is the label
  .description = This is the description.
    `,
  },
};

const Template = ({
  pressed,
  disabled,
  label,
  description,
  ariaLabel,
  l10nId,
  hasSupportLink,
  accessKey,
}) => html`
  <div style="max-width: 400px">
    <moz-toggle
      ?pressed=${pressed}
      ?disabled=${disabled}
      label=${ifDefined(label)}
      description=${ifDefined(description)}
      aria-label=${ifDefined(ariaLabel)}
      data-l10n-id=${ifDefined(l10nId)}
      data-l10n-attrs="aria-label, description, label"
      accesskey=${ifDefined(accessKey)}
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
  l10nId: "moz-toggle-aria-label",
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
  l10nId: "moz-toggle-label",
  hasSupportLink: false,
  accessKey: "h",
};

export const WithDescription = Template.bind({});
WithDescription.args = {
  ...WithLabel.args,
  l10nId: "moz-toggle-description",
};

export const WithSupportLink = Template.bind({});
WithSupportLink.args = {
  ...WithDescription.args,
  hasSupportLink: true,
};
