/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Registered custom properties.

use crate::Atom;
use crate::selector_map::PrecomputedHashMap;
use crate::stylesheets::UrlExtraData;
use super::syntax::Descriptor;
use super::rule::InitialValue;

/// A computed, already-validated property registration.
/// <https://drafts.css-houdini.org/css-properties-values-api-1/#custom-property-registration>
#[derive(Debug, Clone, MallocSizeOf)]
pub struct PropertyRegistration {
    /// The syntax of the property.
    pub syntax: Descriptor,
    /// Whether the property inherits.
    pub inherits: bool,
    /// The initial value. Only missing for universal syntax.
    #[ignore_malloc_size_of = "Arc"]
    pub initial_value: Option<InitialValue>,
    /// The url data is used to parse the property at computed value-time.
    pub url_data: UrlExtraData,
}

/// The script registry of custom properties.
/// <https://drafts.css-houdini.org/css-properties-values-api-1/#dom-window-registeredpropertyset-slot>
#[derive(Default)]
pub struct ScriptRegistry {
    properties: PrecomputedHashMap<Atom, PropertyRegistration>,
}

impl ScriptRegistry {
    /// Gets an already-registered custom property via script.
    #[inline]
    pub fn get(&self, name: &Atom) -> Option<&PropertyRegistration> {
        self.properties.get(name)
    }

    /// Gets already-registered custom properties via script.
    #[inline]
    pub fn properties(&self) -> &PrecomputedHashMap<Atom, PropertyRegistration> {
        &self.properties
    }

    /// Register a given property. As per
    /// <https://drafts.css-houdini.org/css-properties-values-api-1/#the-registerproperty-function>
    /// we don't allow overriding the registration.
    #[inline]
    pub fn register(&mut self, name: Atom, registration: PropertyRegistration) {
        let old = self.properties.insert(name, registration);
        debug_assert!(old.is_none(), "Already registered? Should be an error");
    }

    /// Returns the properties hashmap.
    #[inline]
    pub fn get_all(&self) -> &PrecomputedHashMap<Atom, PropertyRegistration> {
        &self.properties
    }
}
