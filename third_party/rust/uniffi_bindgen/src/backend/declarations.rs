/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::CodeOracle;

/// A trait that is able to render a declaration about a particular member declared in
/// the `ComponentInterface`.
/// Like `CodeType`, it can render declaration code and imports. It also is able to render
/// code at start-up of the FFI.
/// All methods are optional, and there is no requirement that the trait be used for a particular
/// `interface::` member. Thus, it can also be useful for conditionally rendering code.
pub trait CodeDeclaration {
    /// A list of imports that are needed if this type is in use.
    /// Classes are imported exactly once.
    fn imports(&self, _oracle: &dyn CodeOracle) -> Option<Vec<String>> {
        None
    }

    /// Code (one or more statements) that is run on start-up of the library,
    /// but before the client code has access to it.
    fn initialization_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        None
    }

    /// Code which represents this member. e.g. the foreign language class definition for
    /// a given Object type.
    fn definition_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
        None
    }
}
