/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./moz-support-link.mjs";

MozXULElement.insertFTLIfNeeded(
  "locales-preview/moz-support-link-storybook.ftl"
);
MozXULElement.insertFTLIfNeeded("browser/components/mozSupportLink.ftl");

const fluentStrings = [
  "storybook-amo-test",
  "storybook-fluent-test",
  "moz-support-link-text",
];

export default {
  title: "Design System/Experiments/MozSupportLink",
  argTypes: {
    dataL10nId: {
      type: "string",
      options: [fluentStrings[0], fluentStrings[1], fluentStrings[2]],
      control: { type: "select" },
      description: "Fluent ID used to generate the text content.",
    },
    supportPage: {
      type: "string",
      description:
        "Short-hand string from SUMO to the specific support page. **Note:** changing this will not create a valid URL since we don't have access to the generated support link used in Firefox",
    },
    utmContent: {
      type: "string",
      description:
        "UTM parameter for a URL, if it is an AMO URL. **Note:** changing this will not create a valid URL since we don't have access to the generated support link used in Firefox",
    },
    onClick: { action: "clicked" },
  },
};

const Template = ({ dataL10nId, supportPage, utmContent }) => html`
  <a
    is="moz-support-link"
    data-l10n-id=${ifDefined(dataL10nId)}
    support-page=${ifDefined(supportPage)}
    utm-content=${ifDefined(utmContent)}
  >
  </a>
`;

export const withAMOUrl = Template.bind({});
withAMOUrl.args = {
  dataL10nId: fluentStrings[0],
  supportPage: "addons",
  utmContent: "promoted-addon-badge",
};

export const Primary = Template.bind({});
Primary.args = {
  supportPage: "preferences",
};

export const withFluentId = Template.bind({});
withFluentId.args = {
  dataL10nId: fluentStrings[1],
  supportPage: "preferences",
};
