# MozPageNav

`moz-page-nav` is a grouping of navigation buttons that is displayed at the page level,
intended to change the selected view, provide a heading, and have links to external resources.

```html story
<moz-page-nav heading="This is a nav" style={{ '--page-nav-margin-top': 0, '--page-nav-margin-bottom': 0, height: '275px' }}>
  <moz-page-nav-button
    view="view-one"
    iconSrc="chrome://browser/skin/preferences/category-general.svg"
  >
    <p style={{ margin: 0 }}>Test 1</p>
  </moz-page-nav-button>
  <moz-page-nav-button
    view="view-two"
    iconSrc="chrome://browser/skin/preferences/category-general.svg"
  >
    <p style={{ margin: 0 }}>Test 2</p>
  </moz-page-nav-button>
  <moz-page-nav-button
    view="view-three"
    iconSrc="chrome://browser/skin/preferences/category-general.svg"
  >
    <p style={{ margin: 0 }}>Test 3</p>
  </moz-page-nav-button>
  <moz-page-nav-button
    support-page="test"
    iconSrc="chrome://browser/skin/preferences/category-general.svg"
    slot="secondary-nav"
  >
    <p style={{ margin: 0 }}>Support Link</p>
  </moz-page-nav-button>
  <moz-page-nav-button
    href="https://www.example.com"
    iconSrc="chrome://browser/skin/preferences/category-general.svg"
    slot="secondary-nav"
  >
   <p style={{ margin: 0 }}>External Link</p>
  </moz-page-nav-button>
</moz-page-nav>
```

## When to use

* Use moz-page-nav for single-page navigation to switch between different views.
* moz-page-nav also supports footer buttons for external and support links
* This component will be used in about: pages such as about:firefoxview, about:preferences, about:addons, about:debugging, etc.

## When not to use

* If you need a navigation menu that does not switch between views within a single page

## Code

The source for `moz-page-nav` and `moz-page-nav-button` can be found under
[toolkit/content/widgets/moz-page-nav](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-page-nav).
You can find an examples of `moz-page-nav` in use in the Firefox codebase in
[about:firefoxview](https://searchfox.org/mozilla-central/source/browser/components/firefoxview/firefoxview.html#52-87).

`moz-page-nav` can be imported into `.html`/`.xhtml` files:

```html
<script type="module" src="chrome://global/content/elements/moz-page-nav.mjs"></script>
```

And used as follows:

```html
<moz-page-nav>
  <moz-page-nav-button
    view="A name for the first view"
    iconSrc="A url for the icon for the first navigation button">
  </moz-page-nav-button>
  <moz-page-nav-button
    view="A name for the second view"
    iconSrc="A url for the icon for the second navigation button">
  </moz-page-nav-button>
  <moz-page-nav-button
    view="A name for the third view"
    iconSrc="A url for the icon for the third navigation button">
  </moz-page-nav-button>

  <!-- Footer Links -->

  <!-- Support Link -->
  <moz-page-nav-button
    support-page="A name for a support link"
    iconSrc="A url for the icon for the third navigation button"
    slot="secondary-nav">
  </moz-page-nav-button>

  <!-- External Link -->
  <moz-page-nav-button
    href="A url for an external link"
    iconSrc="A url for the icon for the third navigation button"
    slot="secondary-nav">
  </moz-page-nav-button>
</moz-page-nav>
```

### Fluent usage

Generally the `heading` property of
`moz-page-nav` will be provided via [Fluent attributes](https://mozilla-l10n.github.io/localizer-documentation/tools/fluent/basic_syntax.html#attributes).
To get this working you will need to specify a `data-l10n-id` as well as
`data-l10n-attrs` if you're providing a heading:

```html
<moz-page-nav data-l10n-id="with-heading"
            data-l10n-attrs="heading"></moz-page-nav>
```

In which case your Fluent messages will look something like this:

```
with-heading =
  .heading = Heading text goes here
```

You also need to specify a `data-l10n-id` for each `moz-page-nav-button`:

```html
<moz-page-nav-button data-l10n-id="with-button-text"></moz-page-nav-button>
```

```
with-button-text = button text goes here
```
