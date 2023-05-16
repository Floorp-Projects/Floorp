/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./moz-label.mjs";

MozXULElement.insertFTLIfNeeded("locales-preview/moz-label.storybook.ftl");

export default {
  title: "UI Widgets/Label",
  component: "moz-label",
  argTypes: {
    inputType: {
      options: ["checkbox", "radio"],
      control: { type: "select" },
    },
  },
  parameters: {
    status: {
      type: "unstable",
      links: [
        {
          title: "Learn more",
          href: "?path=/docs/ui-widgets-label-readme--page#component-status",
        },
      ],
    },
  },
};

const Template = ({
  accesskey,
  inputType,
  disabled,
  "data-l10n-id": dataL10nId,
}) => html`
  <style>
    div {
      display: flex;
      align-items: center;
    }

    label {
      margin-inline-end: 8px;
    }
  </style>
  <div>
    <label
      is="moz-label"
      accesskey=${ifDefined(accesskey)}
      data-l10n-id=${ifDefined(dataL10nId)}
      for="cheese"
    >
    </label>
    <input
      type=${inputType}
      name="cheese"
      id="cheese"
      ?disabled=${disabled}
      checked
    />
  </div>
`;

export const AccessKey = Template.bind({});
AccessKey.args = {
  accesskey: "c",
  inputType: "checkbox",
  disabled: false,
  "data-l10n-id": "default-label",
};

export const AccessKeyNotInLabel = Template.bind({});
AccessKeyNotInLabel.args = {
  ...AccessKey.args,
  accesskey: "x",
  "data-l10n-id": "label-with-colon",
};

export const DisabledCheckbox = Template.bind({});
DisabledCheckbox.args = {
  ...AccessKey.args,
  disabled: true,
};
