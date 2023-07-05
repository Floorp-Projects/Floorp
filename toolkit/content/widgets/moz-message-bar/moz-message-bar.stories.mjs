/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./moz-message-bar.mjs";

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
    `,
  },
};

const Template = ({ type, message, l10nId, dismissable }) => html`
  <moz-message-bar
    type=${type}
    message=${ifDefined(message)}
    data-l10n-id=${ifDefined(l10nId)}
    data-l10n-attrs="message"
    ?dismissable=${dismissable}
  >
  </moz-message-bar>
`;

export const Info = Template.bind({});
Info.args = {
  type: "info",
  l10nId: "moz-message-bar-message",
  dismissable: true,
};
