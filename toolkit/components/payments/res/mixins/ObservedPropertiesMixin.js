/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Define getters and setters for observedAttributes converted to camelCase and
 * trigger a batched aynchronous call to `render` upon observed
 * attribute/property changes.
 */

/* exported ObservedPropertiesMixin */

function ObservedPropertiesMixin(superClass) {
  return class ObservedProperties extends superClass {
    constructor() {
      super();

      this._observedPropertiesMixin = {
        pendingRender: false,
      };

      // Reflect property changes for `observedAttributes` to attributes.
      for (let name of (this.constructor.observedAttributes || [])) {
        if (name in this) {
          // Don't overwrite existing properties.
          continue;
        }
        // Convert attribute names from kebab-case to camelCase properties
        Object.defineProperty(this, name.replace(/-([a-z])/g, ($0, $1) => $1.toUpperCase()), {
          configurable: true,
          get() {
            return this.getAttribute(name);
          },
          set(value) {
            if (value === null || value === undefined) {
              this.removeAttribute(name);
            } else {
              this.setAttribute(name, value);
            }
          },
        });
      }
    }

    async _invalidateFromObservedPropertiesMixin() {
      if (this._observedPropertiesMixin.pendingRender) {
        return;
      }

      this._observedPropertiesMixin.pendingRender = true;
      await Promise.resolve();
      try {
        this.render();
      } finally {
        this._observedPropertiesMixin.pendingRender = false;
      }
    }

    attributeChangedCallback(attr, oldValue, newValue) {
      if (super.attributeChangedCallback) {
        super.attributeChangedCallback(attr, oldValue, newValue);
      }
      if (oldValue === newValue) {
        return;
      }
      this._invalidateFromObservedPropertiesMixin();
    }
  };
}
