/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { classMap, html, ifDefined } from "lit.all.mjs";
import "./moz-card.mjs";

export default {
  title: "UI Widgets/Card",
  component: "moz-card",
  parameters: {
    status: "in-development",
    fluent: `
moz-card-heading =
  .heading = This is the label
moz-card-heading-with-icon =
  .heading = This is a card with a heading icon
    `,
  },
  argTypes: {
    type: {
      options: ["default", "accordion"],
      control: { type: "select" },
    },
    hasHeadingIcon: {
      options: [true, false],
      control: { type: "select" },
    },
  },
};

const Template = ({ l10nId, content, type, hasHeadingIcon }) => html`
  <style>
    main {
      max-width: 400px;
    }
    moz-card.headingWithIcon::part(icon) {
      background-image: url("chrome://browser/skin/preferences/category-general.svg");
    }
  </style>
  <main>
    <moz-card
      type=${ifDefined(type)}
      ?icon=${hasHeadingIcon}
      class=${classMap({ headingWithIcon: hasHeadingIcon })}
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

export const CardWithHeadingIcon = Template.bind({});
CardWithHeadingIcon.args = {
  l10nId: "moz-card-heading-with-icon",
  content: `Lorem ipsum dolor sit amet, consectetur adipiscing elit.
  Nunc velit turpis, mollis a ultricies vitae, accumsan ut augue.
  In a eros ac dolor hendrerit varius et at mauris.`,
  type: "default",
  hasHeadingIcon: true,
};
