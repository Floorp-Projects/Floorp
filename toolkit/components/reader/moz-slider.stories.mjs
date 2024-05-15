/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../../content/widgets/vendor/lit.all.mjs";
import "chrome://global/content/reader/moz-slider.mjs";

export default {
  title: "Domain-specific UI Widgets/Reader View/Slider",
  component: "moz-slider",
  argTypes: {},
  parameters: {
    status: "stable",
    fluent: `moz-slider-label =
      .label = Slider test
      `,
  },
};

const Template = ({ min, max, value, ticks, labelL10nId, sliderIcon }) => html`
  <moz-slider
    min=${min}
    max=${max}
    value=${value}
    ticks=${ticks}
    tick-labels='["Narrow", "Wide"]'
    data-l10n-id=${labelL10nId}
    data-l10n-attrs="label"
    slider-icon=${sliderIcon}
  ></moz-slider>
`;

export const Default = Template.bind({});
Default.args = {
  min: 0,
  max: 4,
  value: 2,
  ticks: 9,
  labelL10nId: "moz-slider-label",
  sliderIcon: "chrome://global/skin/icons/defaultFavicon.svg",
};
