/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Opaque pointers as these types are giant.
pub type PK11SlotInfo = u8;
pub type PK11SymKey = u8;
pub type PK11Context = u8;

#[repr(u32)]
pub enum PK11Origin {
    PK11_OriginNULL = 0,
    PK11_OriginDerive = 1,
    PK11_OriginGenerated = 2,
    PK11_OriginFortezzaHack = 3,
    PK11_OriginUnwrap = 4,
}

#[repr(u32)]
pub enum PK11ObjectType {
    PK11_TypeGeneric = 0,
    PK11_TypePrivKey = 1,
    PK11_TypePubKey = 2,
    PK11_TypeCert = 3,
    PK11_TypeSymKey = 4,
}

// #[repr(C)]
// #[derive(Copy, Clone)]
// pub struct PK11GenericObjectStr {
//     _unused: [u8; 0],
// }
pub type PK11GenericObject = u8;
