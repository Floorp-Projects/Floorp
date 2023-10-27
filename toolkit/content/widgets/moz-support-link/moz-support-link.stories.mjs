/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./moz-support-link.mjs";

MozXULElement.insertFTLIfNeeded(
  "locales-preview/moz-support-link-storybook.ftl"
);
MozXULElement.insertFTLIfNeeded("toolkit/global/mozSupportLink.ftl");

const fluentStrings = [
  "storybook-amo-test",
  "storybook-fluent-test",
  "moz-support-link-text",
];

export default {
  title: "UI Widgets/Support Link",
  component: "moz-support-link",
  argTypes: {
    "data-l10n-id": {
      options: [fluentStrings[0], fluentStrings[1], fluentStrings[2]],
      control: { type: "select" },
    },
    onClick: { action: "clicked" },
  },
  parameters: {
    status: "stable",
  },
};

const Template = ({
  "data-l10n-id": dataL10nId,
  "support-page": supportPage,
  "utm-content": utmContent,
}) => html`
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
  "data-l10n-id": fluentStrings[0],
  "support-page": "addons",
  "utm-content": "promoted-addon-badge",
};

export const Primary = Template.bind({});
Primary.args = {
  "support-page": "preferences",
  "utm-content": "",
};

export const withFluentId = Template.bind({});
withFluentId.args = {
  "data-l10n-id": fluentStrings[1],
  "support-page": "preferences",
  "utm-content": "",
};
