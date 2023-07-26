# Panel Menu

The `panel-list` and `panel-item` components work together to create a menu for
in-content contexts. The basic structure is a `panel-list` with `panel-item`
children and optional `hr` elements as separators. The `panel-list` will anchor
itself to the target of the initiating event when opened with
`panelList.toggle(event)`.

Note: Nested menus are not currently supported. XUL is currently required to
support accesskey underlining (although using `moz-label` could change that).
Shortcuts are not displayed automatically in the `panel-item`.

```html story
<panel-list stay-open open>
    <panel-item action="new" accesskey="N">New</panel-item>
    <panel-item accesskey="O">Open</panel-item>
    <hr />
    <panel-item action="save" accesskey="S">Save</panel-item>
    <hr />
    <panel-item accesskey="Q">Quit</panel-item>
</panel-list>
```

## Status

Current status is listed as in-development since this is only intended for use
within in-content contexts. XUL is still required for accesskey underlining, but
could be migrated to use the `moz-label` component. This is a useful but
historical element that could likely use some attention at the API level and to
be brought up to our design systems standards.

## When to use

* When there are multiple options for something that would take too
  much space with individual buttons.
* When the actions are not frequently needed.
* When you are within an in-content context.

## When not to use

* When there is only one action.
* When the actions are frequently needed.
* In the browser chrome, you probably want to use
  [menupopup](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/menupopup.js)
  or
  [panel](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/panel.js)
  instead.

## Basic usage

The source for `panel-list` can be found under
[toolkit/content/widgets/panel-list.js](https://searchfox.org/mozilla-central/source/toolkit/content/widgets/panel-list.js).
You can find an examples of `panel-list` in use in the Firefox codebase in both
[about:addons](https://searchfox.org/mozilla-central/source/toolkit/mozapps/extensions/content/aboutaddons.html#87,102,114)
and the
[migration-wizard](https://searchfox.org/mozilla-central/source/browser/components/migration/content/migration-dialog-window.html#18).

`panel-list` will automatically be imported in chrome documents, both through
markup and through JS with `document.createElement("panel-list")` or by cloning
a template.

```html
<!-- This will import `panel-list` if needed in most cases. -->
<panel-list></panel-list>
```

In non-chrome documents it can be imported into `.html`/`.xhtml` files:

```html
<script src="chrome://global/content/elements/panel-list.js"></script>
```

And used as follows:

```html
<panel-list>
  <panel-item accesskey="N">New</panel-item>
  <panel-item accesskey="O">Open</panel-item>
  <hr />
  <panel-item accesskey="S">Save</panel-item>
  <hr />
  <panel-item accesskey="Q">Quit</panel-item>
</panel-list>
```

The `toggle` method takes the event you received on your anchor button and opens
the menu attached to that element.

```js
anchorButton.addEventListener("mousedown", e => panelList.toggle(e));
```

Accesskeys are activated with the bare accesskey letter when the menu is opened.
So for this example after opening the menu pressing `s` will fire a click event
on the Save `panel-item`.

Note: XUL is currently required for accesskey underlining, but can be [replaced
with `moz-label`](https://bugzilla.mozilla.org/show_bug.cgi?id=1828741) later.

### Fluent usage

The `panel-item` expects to have text content set by fluent.

```html
<panel-list>
  <panel-item data-l10n-id="menu-new"></panel-item>
  <panel-item data-l10n-id="menu-save"></panel-item>
</panel-list>
```

In which case your Fluent messages will look something like this:

```
menu-new = New
    .accesskey = N
menu-save = Save
    .accesskey = S
```

## Advanced usage

### Showing the menu

By default the menu will be hidden. It is shown when the `open` attribute is
set, but that won't position the menu by default.

To trigger the auto-positioning of the menu, it should be opened or closed using
the `toggle(event)` method.

```js
function onMenuButton(event) {
  document.querySelector("panel-list").toggle(event);
}
```

The `toggle(event)` method will use `event.target` as the anchor for the menu.

To achieve the expected behaviour, the menu should open on `mousedown` for mouse
events, and `click` for keyboard events. This can be accomplished by checking
the `event.inputSource` property in chrome contexts or `event.detail` in
non-chrome contexts (`event.detail` will be the click count which is `0` when a
click is from the keyboard).

```js
function openMenu(event) {
  if (
    event.type == "mousedown" ||
    event.inputSource == MouseEvent.MOZ_SOURCE_KEYBOARD ||
    !event.detail
  ) {
    document.querySelector("panel-list").toggle(event);
  }
}

let menuButton = document.getElementById("open-menu-button");
menuButton.addEventListener("mousedown", openMenu);
menuButton.addEventListener("click", openMenu);
```

### Icons

Icons can be added to the `panel-item`s by setting a `background-image` on
`panel-item::part(button)`.

```css
panel-item[action="new"]::part(button) {
  background-image: url("./new.svg");
}

panel-item[action="save"]::part(button) {
  background-image: url("./save.svg");
}
```

### Badging

Icons may be badged by setting the `badged` attribute. This adds a dot next to
the icon.

```html
<panel-list>
  <panel-item action="new">New</panel-item>
  <panel-item action="save" badged>Save</panel-item>
</panel-list>
```

```html story
<panel-list stay-open open>
  <panel-item action="new">New</panel-item>
  <panel-item action="save" badged>Save</panel-item>
</panel-list>
```

### Matching anchor width

When using the `panel-list` like a `select` dropdown, it's nice to have it match
the size of the anchor button. You can see this in practice in the
[Wide variant](?path=/story/ui-widgets-panel-list--wide) and the
`migration-wizard`. Setting the `min-width-from-anchor` attribute will cause the
menu to match its anchor's width when it is opened.

```html
<button class="current-selection">Apples</button>
<panel-list min-width-from-anchor>
  <panel-item>Apples</panel-list>
  <panel-item>Bananas</panel-list>
</panel-list>
```

### Usage in a XUL `panel`

The "new" (as of early 2023) migration wizard uses the `panel-list` inside of a
XUL `panel` element to let its contents escape its container dialog by creating
an OS-level window. This can be useful if the menu could be larger than its
container, however in chrome contexts you are likely better off using
`menupopup`.

By placing a `panel-list` inside of a XUL `panel` it will automatically defer
its positioning responsibilities to the XUL `panel` and it will then be able to
grow larger than its containing window if needed.

```html
<!-- Assuming we're in a XUL document. -->
<panel>
  <html:panel-list>
    <html:panel-item>Apples</html:panel-item>
    <html:panel-item>Apples</html:panel-item>
    <html:panel-item>Apples</html:panel-item>
  </html:panel-list>
</panel>
```
