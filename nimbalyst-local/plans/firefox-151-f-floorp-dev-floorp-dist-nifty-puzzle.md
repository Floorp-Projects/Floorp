# Fix about-preferences.ts for Firefox 151 Settings Page Redesign

## Context

Firefox 151 completely redesigned the `about:preferences` navigation structure. The old `richlistbox`-based sidebar was replaced with Lit-based custom elements (`moz-page-nav` / `moz-page-nav-button`). The warning banner insertion point (`.sticky-container`) no longer exists. The Floorp customizations in `bridge/startup/src/about-preferences.ts` are broken as a result.

**File to modify:** `bridge/startup/src/about-preferences.ts`

## Key Structural Changes in Firefox 151

| Element | Old (Firefox ≤150) | New (Firefox 151) |
|---|---|---|
| Nav container | `<richlistbox id="categories">` | `<html:moz-page-nav id="categories">` |
| Nav items | `<richlistitem class="category">` | `<html:moz-page-nav-button view="...">` |
| Icon attribute | `<image class="category-icon" src="...">` (child element) | `iconsrc="..."` (attribute on button) |
| Label | `<label class="category-name">` (child element) | Slot text content inside the button |
| Warning insertion | `.sticky-container` (no longer exists) | `.pane-container` → insert before `#mainPrefPane` |

Source: `_dist/bin/floorp/browser/chrome/browser/content/browser/preferences/preferences.xhtml`

## Changes

### 1. Fix `addFloorpHubCategory()` — Replace richlistitem with moz-page-nav-button

**Lines 216-249**

The function currently uses `MozXULElement.parseXULToFragment()` to create a `<richlistitem>` element. The new `moz-page-nav` container expects `<moz-page-nav-button>` children.

**New approach:**
- Create element via `doc.createElement("moz-page-nav-button")` (HTML custom element)
- Set `id="category-nora-link"`, `iconsrc` attribute with the existing base64 SVG
- Set text content to `"Floorp Hub"` (the button uses a default `<slot>` for label text)
- Override the `activate()` method on the instance to call `openFloorpHub()` instead of dispatching `change-view` (which would try to route to a non-existent pane)
- Remove `parseXULToFragment` call and the old fragment insertion

Reference: `_dist/bin/floorp/chrome/toolkit/content/global/elements/moz-page-nav.mjs` — `MozPageNavButton` uses `iconSrc` property (reflected as attribute), slot for label, and `activate()` dispatches `change-view`.

### 2. Fix `createFloorpHubWarning()` — Replace `.sticky-container` with `.pane-container`

**Lines 270-363**

The function queries `.sticky-container` which no longer exists. The warning banner should be inserted inside `.pane-container`, before `#mainPrefPane`.

**New approach:**
- Replace `doc.querySelector(".sticky-container")` with `doc.querySelector(".pane-container")`
- Insert the warning container before `#mainPrefPane` within `.pane-container`
  ```ts
  const paneContainer = doc.querySelector(".pane-container");
  const mainPrefPane = doc.querySelector("#mainPrefPane");
  if (paneContainer && mainPrefPane) {
    paneContainer.insertBefore(container, mainPrefPane);
  }
  ```

### 3. Fix `createFloorpStartWarning()` — Same `.sticky-container` replacement

**Lines 723-787**

Same issue as #2. Replace `.sticky-container` with `.pane-container` → before `#mainPrefPane`.

**New approach:**
- Same as #2: use `.pane-container` and insert before `#mainPrefPane`

## Verification

1. Build the project (`pnpm build` or equivalent for the startup bridge)
2. Launch Floorp and navigate to `about:preferences`
3. Verify:
   - "Floorp Hub" category button appears in the left navigation sidebar, positioned before the (hidden) Sync category
   - Clicking "Floorp Hub" opens `about:hub` in a new tab
   - Yellow warning banner ("Floorp-specific settings...") appears between the search bar and the preferences content
   - Blue "Floorp Start" warning banner appears below the yellow banner
   - No console errors related to missing `.sticky-container` or failed element creation
