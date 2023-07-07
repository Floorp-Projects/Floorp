/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable import/no-unassigned-import */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import "./moz-message-bar.mjs";
import "../moz-support-link/moz-support-link.mjs";

export default {
  title: "UI Widgets/Moz Message Bar",
  component: "moz-message-bar",
  argTypes: {
    type: {
      options: ["info", "warning", "success", "error"],
      control: { type: "select" },
    },
    message: {
      table: {
        disable: true,
      },
    },
  },
  parameters: {
    status: "unstable",
    fluent: `
moz-message-bar-message =
  .message = For your information message
moz-message-bar-button = Click me!
    `,
  },
};

const Template = ({
  type,
  message,
  l10nId,
  dismissable,
  hasSupportLink,
  hasActionButton,
}) => html`
  <moz-message-bar
    type=${type}
    message=${ifDefined(message)}
    data-l10n-id=${ifDefined(l10nId)}
    data-l10n-attrs="message"
    ?dismissable=${dismissable}
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
    ${hasActionButton
      ? html`
          <button data-l10n-id="moz-message-bar-button" slot="actions"></button>
        `
      : ""}
  </moz-message-bar>
`;

export const Default = Template.bind({});
Default.args = {
  type: "info",
  l10nId: "moz-message-bar-message",
  dismissable: false,
  hasSupportLink: false,
  hasActionButton: false,
};

export const Dismissable = Template.bind({});
Dismissable.args = {
  type: "info",
  l10nId: "moz-message-bar-message",
  dismissable: true,
  hasSupportLink: false,
  hasActionButton: false,
};

export const WithActionButton = Template.bind({});
WithActionButton.args = {
  type: "info",
  l10nId: "moz-message-bar-message",
  dismissable: false,
  hasSupportLink: false,
  hasActionButton: true,
};

export const WithSupportLink = Template.bind({});
WithSupportLink.args = {
  type: "info",
  l10nId: "moz-message-bar-message",
  dismissable: false,
  hasSupportLink: true,
  hasActionButton: false,
};
