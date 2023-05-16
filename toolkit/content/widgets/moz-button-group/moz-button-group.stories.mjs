/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import {
  PLATFORM_LINUX,
  PLATFORM_MACOS,
  PLATFORM_WINDOWS,
} from "./moz-button-group.mjs";

export default {
  title: "UI Widgets/Button Group",
  component: "moz-button-group",
  argTypes: {
    platform: {
      options: [PLATFORM_LINUX, PLATFORM_MACOS, PLATFORM_WINDOWS],
      control: { type: "select" },
    },
  },
  parameters: {
    status: "stable",
    fluent: `
moz-button-group-p = The button group is below. Card for emphasis.
moz-button-group-ok = OK
moz-button-group-cancel = Cancel
    `,
  },
};

const Template = ({ platform }) => html`
  <div class="card card-no-hover" style="max-width: 400px">
    <p data-l10n-id="moz-button-group-p"></p>
    <moz-button-group .platform=${platform}>
      <button class="primary" data-l10n-id="moz-button-group-ok"></button>
      <button data-l10n-id="moz-button-group-cancel"></button>
    </moz-button-group>
  </div>
`;

export const Default = Template.bind({});
Default.args = {
  // Platform will auto-detected.
};

export const Windows = Template.bind({});
Windows.args = {
  platform: PLATFORM_WINDOWS,
};
export const Mac = Template.bind({});
Mac.args = {
  platform: PLATFORM_MACOS,
};
export const Linux = Template.bind({});
Linux.args = {
  platform: PLATFORM_LINUX,
};
