// Even if allows private_in_public, these are errors.

#![allow(private_in_public)]

pub enum PublicEnum {
    Variant(PrivateEnum), //~ ERROR E0446
}

enum PrivateEnum {
    Variant(u8),
}

mod foo {
    pub(crate) enum CrateEnum {
        Variant(PrivateEnum), //~ ERROR E0446
    }

    enum PrivateEnum {
        Variant(u8),
    }
}

fn main() {}
