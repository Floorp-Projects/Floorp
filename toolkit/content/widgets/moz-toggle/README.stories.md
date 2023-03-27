# MozToggle

`moz-toggle` is a toggle element that can be used to switch between two states.
It may be helpful to think of it as a button that can be pressed or unpressed,
corresponding with "on" and "off" states.

```html story
<moz-toggle pressed
            label="Toggle label"
            description="This is a demo toggle for the docs.">
</moz-toggle>
```

## When to use

* Use a toggle for binary controls like on/off or enabled/disabled.
* Use when the action is performed immediately and doesn't require confirmation
  or form submission.
* A toggle is like a switch. If it would be appropriate to use a switch in the
  physical world for this action, it is likely appropriate to use a toggle in
  software.

## When not to use

* If another action is required to execute the choice, use a checkbox (i.e. a
  toggle should not generally be used as part of a form).

## Code

The source for `moz-toggle` can be found under
[toolkit/content/widgets/moz-toggle](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/moz-toggle/moz-toggle.mjs).
You can find an examples of `moz-toggle` in use in the Firefox codebase in both
[about:preferences](https://searchfox.org/mozilla-central/source/browser/components/preferences/privacy.inc.xhtml#696)
and [about:addons](https://searchfox.org/mozilla-central/source/toolkit/mozapps/extensions/content/aboutaddons.html#182).

`moz-toggle` can be imported into `.html`/`.xhtml` files:

```html
<script type="module" src="chrome://global/content/elements/moz-toggle.mjs"></script>
```

And used as follows:

```html
<moz-toggle pressed
            label="Label for the toggle"
            description="Longer explanation of what the toggle is for"
            aria-label="Toggle label if label text isn't visible"></moz-toggle>
```

### Fluent usage

Generally the `label`, `description`, and `aria-label` properties of
`moz-toggle` will be provided via [Fluent attributes](https://mozilla-l10n.github.io/localizer-documentation/tools/fluent/basic_syntax.html#attributes).
To get this working you will need to specify a `data-l10n-id` as well as
`data-l10n-attrs` if you're providing a label and a description:

```html
<moz-toggle data-l10n-id="with-label-and-description"
            data-l10n-attrs="label, description"></moz-toggle>
```

In which case your Fluent messages will look something like this:

```
with-label-and-description =
  .label = Label text goes here
  .description = Description text goes here
```

You do not have to specify `data-l10n-attrs` if you're only using an `aria-label`:

```html
<moz-toggle data-l10n-id="with-aria-label-only"></moz-toggle>
```

```
with-aria-label-only =
  .aria-label = aria-label text goes here
```
