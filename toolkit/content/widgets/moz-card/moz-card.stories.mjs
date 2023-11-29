/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html, ifDefined } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./moz-card.mjs";

export default {
  title: "UI Widgets/Card",
  component: "moz-card",
  parameters: {
    status: "in-development",
    fluent: `
moz-card-heading =
  .heading = This is the label
    `,
  },
  argTypes: {
    type: {
      options: ["default", "accordion"],
      control: { type: "select" },
    },
  },
};

const Template = ({ l10nId, content, type }) => html`
  <main style="max-width: 400px">
    <moz-card
      type=${ifDefined(type)}
      data-l10n-id=${ifDefined(l10nId)}
      data-l10n-attrs="heading"
    >
      <div>${content}</div>
    </moz-card>
  </main>
`;

export const DefaultCard = Template.bind({});
DefaultCard.args = {
  l10nId: "moz-card-heading",
  content: "This is the content",
};

export const CardOnlyContent = Template.bind({});
CardOnlyContent.args = {
  content: "This card only contains content",
};

export const CardTypeAccordion = Template.bind({});
CardTypeAccordion.args = {
  ...DefaultCard.args,
  content: `Lorem ipsum dolor sit amet, consectetur adipiscing elit.
  Nunc velit turpis, mollis a ultricies vitae, accumsan ut augue.
  In a eros ac dolor hendrerit varius et at mauris.`,
  type: "accordion",
};
CardTypeAccordion.parameters = {
  a11y: {
    config: {
      rules: [
        /* 
        The accordion card can be expanded either by the chevron icon
        button or by activating the details element. Mouse users can
        click on the chevron button or the details element, while
        keyboard users can tab to the details element and have a
        focus ring around the details element in the card.
        Additionally, the details element is announced as a button
        so I don't believe we are providing a degraded experience
        to non-mouse users.

        Bug 1854008: We should probably make the accordion button a
        clickable div or something that isn't announced to screen
        readers.
        */
        {
          id: "button-name",
          reviewOnFail: true,
        },
        {
          id: "nested-interactive",
          reviewOnFail: true,
        },
      ],
    },
  },
};
