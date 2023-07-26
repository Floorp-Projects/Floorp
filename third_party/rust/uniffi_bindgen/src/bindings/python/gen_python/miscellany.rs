/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeType, Literal};
use paste::paste;

macro_rules! impl_code_type_for_miscellany {
    ($T:ty, $canonical_name:literal) => {
        paste! {
            #[derive(Debug)]
            pub struct $T;

            impl CodeType for $T  {
                fn type_label(&self) -> String {
                    format!("{}", $canonical_name)
                }

                fn canonical_name(&self) -> String {
                    format!("{}", $canonical_name)
                }

                fn literal(&self, _literal: &Literal) -> String {
                    unreachable!()
                }
            }
        }
    };
}

impl_code_type_for_miscellany!(TimestampCodeType, "Timestamp");

impl_code_type_for_miscellany!(DurationCodeType, "Duration");
