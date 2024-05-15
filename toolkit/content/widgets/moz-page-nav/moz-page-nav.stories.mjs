/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, when } from "../vendor/lit.all.mjs";
import "./moz-page-nav.mjs";

export default {
  title: "UI Widgets/Page Nav",
  component: "moz-page-nav",
  parameters: {
    status: "in-development",
    actions: {
      handles: ["change-view"],
    },
    fluent: `
moz-page-nav-button-one = View 1
  .title = View 1
moz-page-nav-button-two = View 2
  .title = View 2
moz-page-nav-button-three = View 3
  .title = View 3
moz-page-nav-button-four = Support Link
  .title = Support Link
moz-page-nav-button-five = External Link
  .title = External Link
moz-page-nav-heading =
  .heading = Heading
     `,
  },
};

const Template = ({ hasFooterLinks }) => html`
  <style>
    #page {
      height: 100%;
      display: flex;

      @media (max-width: 52rem) {
        grid-template-columns: 82px 1fr;
      }
    }
    moz-page-nav {
      margin-inline-start: 10px;
      --page-nav-margin-top: 10px;

      @media (max-width: 52rem) {
        margin-inline-start: 0;
      }
    }
  </style>
  <div id="page">
    <moz-page-nav data-l10n-id="moz-page-nav-heading" data-l10n-attrs="heading">
      <moz-page-nav-button
        view="view-one"
        data-l10n-id="moz-page-nav-button-one"
        iconSrc="chrome://browser/skin/preferences/category-general.svg"
      >
      </moz-page-nav-button>
      <moz-page-nav-button
        view="view-two"
        data-l10n-id="moz-page-nav-button-two"
        iconSrc="chrome://browser/skin/preferences/category-general.svg"
      >
      </moz-page-nav-button>
      <moz-page-nav-button
        view="view-three"
        data-l10n-id="moz-page-nav-button-three"
        iconSrc="chrome://browser/skin/preferences/category-general.svg"
      >
      </moz-page-nav-button>
      ${when(
        hasFooterLinks,
        () => html` <moz-page-nav-button
            support-page="test"
            data-l10n-id="moz-page-nav-button-four"
            iconSrc="chrome://browser/skin/preferences/category-general.svg"
            slot="secondary-nav"
          >
          </moz-page-nav-button>
          <moz-page-nav-button
            href="https://www.example.com"
            data-l10n-id="moz-page-nav-button-five"
            iconSrc="chrome://browser/skin/preferences/category-general.svg"
            slot="secondary-nav"
          >
          </moz-page-nav-button>`
      )}
    </moz-page-nav>
    <main></main>
  </div>
`;

export const Default = Template.bind({});
Default.args = { hasFooterLinks: false };

export const WithFooterLinks = Template.bind({});
WithFooterLinks.args = { hasFooterLinks: true };
