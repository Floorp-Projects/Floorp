/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Functions to calculate names for FFI symbols
//!
//! All of these functions input a `namespace` parameter which is:
//!   - The UDL namespace for UDL-based generation
//!   - The "module path" of the item for proc-macro based generation.  Right now this is actually just the
//!     crate name, but we eventually hope to make this the full module path.
//!
//! This could cause collisions in the case where you combine UDL and proc-macro generation and you
//! set the UDL namespace to the name of another crate. This seems so pathological that it's not
//! worth the code complexity to prevent it.

/// FFI symbol name for a top-level function
pub fn fn_symbol_name(namespace: &str, name: &str) -> String {
    let name = name.to_ascii_lowercase();
    format!("uniffi_{namespace}_fn_func_{name}")
}

/// FFI symbol name for an object constructor
pub fn constructor_symbol_name(namespace: &str, object_name: &str, name: &str) -> String {
    let object_name = object_name.to_ascii_lowercase();
    let name = name.to_ascii_lowercase();
    format!("uniffi_{namespace}_fn_constructor_{object_name}_{name}")
}

/// FFI symbol name for an object method
pub fn method_symbol_name(namespace: &str, object_name: &str, name: &str) -> String {
    let object_name = object_name.to_ascii_lowercase();
    let name = name.to_ascii_lowercase();
    format!("uniffi_{namespace}_fn_method_{object_name}_{name}")
}

/// FFI symbol name for the `free` function for an object.
pub fn free_fn_symbol_name(namespace: &str, object_name: &str) -> String {
    let object_name = object_name.to_ascii_lowercase();
    format!("uniffi_{namespace}_fn_free_{object_name}")
}

/// FFI symbol name for the `init_callback` function for a callback interface
pub fn init_callback_fn_symbol_name(namespace: &str, callback_interface_name: &str) -> String {
    let callback_interface_name = callback_interface_name.to_ascii_lowercase();
    format!("uniffi_{namespace}_fn_init_callback_{callback_interface_name}")
}

/// FFI checksum symbol name for a top-level function
pub fn fn_checksum_symbol_name(namespace: &str, name: &str) -> String {
    let name = name.to_ascii_lowercase();
    format!("uniffi_{namespace}_checksum_func_{name}")
}

/// FFI checksum symbol name for an object constructor
pub fn constructor_checksum_symbol_name(namespace: &str, object_name: &str, name: &str) -> String {
    let object_name = object_name.to_ascii_lowercase();
    let name = name.to_ascii_lowercase();
    format!("uniffi_{namespace}_checksum_constructor_{object_name}_{name}")
}

/// FFI checksum symbol name for an object method
pub fn method_checksum_symbol_name(namespace: &str, object_name: &str, name: &str) -> String {
    let object_name = object_name.to_ascii_lowercase();
    let name = name.to_ascii_lowercase();
    format!("uniffi_{namespace}_checksum_method_{object_name}_{name}")
}
