/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../../content/widgets/vendor/lit.all.mjs";
import "chrome://global/content/reader/color-input.mjs";

export default {
  title: "Domain-specific UI Widgets/Reader View/Color Input",
  component: "color-input",
  argTypes: {},
  parameters: {
    status: "stable",
    fluent: `moz-color-input-label = Background`,
  },
};

const Template = ({ color, propName, labelL10nId }) => html`
  <color-input
    color=${color}
    data-l10n-id=${labelL10nId}
    prop-name=${propName}
  ></color-input>
`;

export const Default = Template.bind({});
Default.args = {
  propName: "background",
  color: "#7293C9",
  labelL10nId: "moz-color-input-label",
};
