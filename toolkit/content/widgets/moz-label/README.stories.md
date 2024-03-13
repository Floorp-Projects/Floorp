# MozLabel

`moz-label` is an extension of the built-in `HTMLLabelElement` that provides accesskey styling and formatting as well as some click handling logic.

```html story
<label is="moz-label" accesskey="c" for="check" style={{ display: "inline-block" }}>
    This is a label with an accesskey:
</label>
<input id="check" type="checkbox" defaultChecked style={{ display: "inline-block" }} />
```

Accesskey underlining is enabled by default on Windows and Linux. It is also enabled in Storybook on Mac for demonstrative purposes, but is usually controlled by the `ui.key.menuAccessKey` preference.

## Component status

At this time `moz-label` may not be suitable for general use in Firefox.

`moz-label` is currently only used in the `moz-toggle` custom element. There are no instances in Firefox where we set an accesskey on a toggle, so it is still largely untested in the wild.

Additionally there is at least [one outstanding bug](https://bugzilla.mozilla.org/show_bug.cgi?id=1819469) related to accesskey handling in the shadow DOM.
