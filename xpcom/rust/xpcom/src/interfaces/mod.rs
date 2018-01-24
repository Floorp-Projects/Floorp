/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module contains the xpcom interfaces exposed to rust code.
//!
//! The items in this module come in a few flavours:
//!
//! 1. `nsI*`: These are the types for XPCOM interfaces. They should always be
//!    passed behind a reference, pointer, or `RefPtr`. They may be coerced to
//!    their base interfaces using the `coerce` method.
//!
//! 2. `nsI*Coerce`: These traits provide the implementation mechanics for the
//!    `coerce` method, and can usually be ignored. *These traits are hidden in
//!    rustdoc*
//!
//! 3. `nsI*VTable`: These structs are the vtable definitions for each type.
//!    They contain the base interface's vtable, followed by pointers for each
//!    of the vtable's methods. If direct access is needed, a `*const nsI*` can
//!    be safely transmuted to a `*const nsI*VTable`. *These structs are hidden
//!    in rustdoc*
//!
//! 4. Typedefs used in idl file definitions.

// Interfaces defined in .idl files
mod idl;
pub use self::idl::*;

// Other interfaces which are needed to compile
mod nonidl;
pub use self::nonidl::*;
