#![allow(unused_variables, unused_parens)]
#![cfg_attr(feature = "cargo-clippy", allow(clippy::float_cmp))]
#![cfg_attr(feature = "cargo-clippy", allow(clippy::unreadable_literal))]
#![cfg_attr(feature = "cargo-clippy", allow(clippy::nonminimal_bool))]
use super::operands::PluralOperands;
use super::PluralCategory;
use unic_langid::subtags;
use unic_langid::LanguageIdentifier;
pub type PluralRule = fn(&PluralOperands) -> PluralCategory;
pub static CLDR_VERSION: usize = 37;
macro_rules! langid {
    ( $ lang : expr , $ script : expr , $ region : expr ) => {{
        unsafe { LanguageIdentifier::from_raw_parts_unchecked($lang, $script, $region, None) }
    }};
}
pub const PRS_CARDINAL: &[(LanguageIdentifier, PluralRule)] = &[
    (
        langid!(subtags::Language::from_raw_unchecked(26209u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27489u64), None, None),
        |po| {
            if ((0..=1).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28001u64), None, None),
        |po| {
            if (po.i == 0) || (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28257u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29281u64), None, None),
        |po| {
            if ((3..=10).contains(&(po.i))) {
                PluralCategory::FEW
            } else if ((11..=99).contains(&(po.i))) {
                PluralCategory::MANY
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else if (po.n == 0.0) {
                PluralCategory::ZERO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7565921u64),
            None,
            None
        ),
        |po| {
            if ((3..=10).contains(&(po.i))) {
                PluralCategory::FEW
            } else if ((11..=99).contains(&(po.i))) {
                PluralCategory::MANY
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else if (po.n == 0.0) {
                PluralCategory::ZERO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29537u64), None, None),
        |po| {
            if (po.i == 0) || (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6386529u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7631713u64),
            None,
            None
        ),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31329u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25954u64), None, None),
        |po| {
            if ((2..=4).contains(&(po.i)) && !(12..=14).contains(&(po.i))) {
                PluralCategory::FEW
            } else if (po.i % 10 == 0)
                || ((5..=9).contains(&(po.i)))
                || ((11..=14).contains(&(po.i)))
            {
                PluralCategory::MANY
            } else if (po.i % 10 == 1 && po.i % 100 != 11) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7169378u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(8021346u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26466u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7301218u64),
            None,
            None
        ),
        |po| {
            if ((0..=1).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28002u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28258u64), None, None),
        |po| {
            if (po.i == 0) || (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28514u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29282u64), None, None),
        |po| {
            if ((po.i % 10 == 9 || (3..=4).contains(&(po.i)))
                && !(10..=19).contains(&(po.i))
                && !(70..=79).contains(&(po.i))
                && !(90..=99).contains(&(po.i)))
            {
                PluralCategory::FEW
            } else if (po.n != 0.0 && po.i % 1000000 == 0) {
                PluralCategory::MANY
            } else if (po.i % 10 == 1 && po.i % 100 != 11 && po.i % 100 != 71 && po.i % 100 != 91) {
                PluralCategory::ONE
            } else if (po.i % 10 == 2 && po.i % 100 != 12 && po.i % 100 != 72 && po.i % 100 != 92) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7893602u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29538u64), None, None),
        |po| {
            if (po.v == 0 && (2..=4).contains(&(po.i % 10)) && !(12..=14).contains(&(po.i % 100)))
                || ((2..=4).contains(&(po.f % 10)) && !(12..=14).contains(&(po.f % 100)))
            {
                PluralCategory::FEW
            } else if (po.v == 0 && po.i % 10 == 1 && po.i % 100 != 11)
                || (po.f % 10 == 1 && po.f % 100 != 11)
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24931u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25955u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6448483u64),
            None,
            None
        ),
        |po| {
            if (po.v == 0 && (po.i == 1 || po.i == 2 || po.i == 3))
                || (po.v == 0 && po.i % 10 != 4 && po.i % 10 != 6 && po.i % 10 != 9)
                || (po.v != 0 && po.f % 10 != 4 && po.f % 10 != 6 && po.f % 10 != 9)
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6776675u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7497827u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6450019u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29539u64), None, None),
        |po| {
            if ((2..=4).contains(&(po.i)) && po.v == 0) {
                PluralCategory::FEW
            } else if (po.v != 0) {
                PluralCategory::MANY
            } else if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31075u64), None, None),
        |po| {
            if (po.n == 3.0) {
                PluralCategory::FEW
            } else if (po.n == 6.0) {
                PluralCategory::MANY
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else if (po.n == 0.0) {
                PluralCategory::ZERO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24932u64), None, None),
        |po| {
            if (po.n == 1.0) || (po.t != 0 && (po.i == 0 || po.i == 1)) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25956u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6452068u64),
            None,
            None
        ),
        |po| {
            if (po.v == 0 && (3..=4).contains(&(po.i % 100))) || ((3..=4).contains(&(po.f % 100))) {
                PluralCategory::FEW
            } else if (po.v == 0 && po.i % 100 == 1) || (po.f % 100 == 1) {
                PluralCategory::ONE
            } else if (po.v == 0 && po.i % 100 == 2) || (po.f % 100 == 2) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30308u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31332u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25957u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27749u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28261u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28517u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29541u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29797u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30053u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24934u64), None, None),
        |po| {
            if (po.i == 0) || (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26214u64), None, None),
        |po| {
            if (po.i == 0 || po.i == 1) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26982u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7104870u64),
            None,
            None
        ),
        |po| {
            if (po.v == 0 && (po.i == 1 || po.i == 2 || po.i == 3))
                || (po.v == 0 && po.i % 10 != 4 && po.i % 10 != 6 && po.i % 10 != 9)
                || (po.v != 0 && po.f % 10 != 4 && po.f % 10 != 6 && po.f % 10 != 9)
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28518u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29286u64), None, None),
        |po| {
            if (po.i == 0 || po.i == 1) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7501158u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31078u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24935u64), None, None),
        |po| {
            if ((3..=6).contains(&(po.i)) && po.f == 0) {
                PluralCategory::FEW
            } else if ((7..=10).contains(&(po.i)) && po.f == 0) {
                PluralCategory::MANY
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25703u64), None, None),
        |po| {
            if ((3..=10).contains(&(po.i)) && po.f == 0 || (13..=19).contains(&(po.i)) && po.f == 0)
            {
                PluralCategory::FEW
            } else if (po.n == 1.0 || po.n == 11.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0 || po.n == 12.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27751u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7828327u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30055u64), None, None),
        |po| {
            if (po.i == 0) || (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7828839u64),
            None,
            None
        ),
        |po| {
            if ((0..=1).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30311u64), None, None),
        |po| {
            if (po.v == 0
                && (po.i % 100 == 0
                    || po.i % 100 == 20
                    || po.i % 100 == 40
                    || po.i % 100 == 60
                    || po.i % 100 == 80))
            {
                PluralCategory::FEW
            } else if (po.v != 0) {
                PluralCategory::MANY
            } else if (po.v == 0 && po.i % 10 == 1) {
                PluralCategory::ONE
            } else if (po.v == 0 && po.i % 10 == 2) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24936u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7823720u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25960u64), None, None),
        |po| {
            if (po.v == 0 && !(0..=10).contains(&(po.i)) && po.f == 0 && po.i % 10 == 0) {
                PluralCategory::MANY
            } else if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else if (po.i == 2 && po.v == 0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26984u64), None, None),
        |po| {
            if (po.i == 0) || (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29288u64), None, None),
        |po| {
            if (po.v == 0 && (2..=4).contains(&(po.i % 10)) && !(12..=14).contains(&(po.i % 100)))
                || ((2..=4).contains(&(po.f % 10)) && !(12..=14).contains(&(po.f % 100)))
            {
                PluralCategory::FEW
            } else if (po.v == 0 && po.i % 10 == 1 && po.i % 100 != 11)
                || (po.f % 10 == 1 && po.f % 100 != 11)
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6452072u64),
            None,
            None
        ),
        |po| {
            if (po.v == 0 && (3..=4).contains(&(po.i % 100))) || ((3..=4).contains(&(po.f % 100))) {
                PluralCategory::FEW
            } else if (po.v == 0 && po.i % 100 == 1) || (po.f % 100 == 1) {
                PluralCategory::ONE
            } else if (po.v == 0 && po.i % 100 == 2) || (po.f % 100 == 2) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30056u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31080u64), None, None),
        |po| {
            if (po.i == 0 || po.i == 1) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24937u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25705u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26473u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26985u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28265u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28521u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29545u64), None, None),
        |po| {
            if (po.t == 0 && po.i % 10 == 1 && po.i % 100 != 11) || (po.t != 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29801u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30057u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30569u64), None, None),
        |po| {
            if (po.v == 0 && !(0..=10).contains(&(po.i)) && po.f == 0 && po.i % 10 == 0) {
                PluralCategory::MANY
            } else if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else if (po.i == 2 && po.v == 0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24938u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7299690u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7300970u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26986u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6516074u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30314u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30570u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24939u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6447467u64),
            None,
            None
        ),
        |po| {
            if (po.i == 0 || po.i == 1) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6971755u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6775659u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6644843u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6382955u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27499u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6974315u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27755u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28011u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28267u64), None, None),
        |po| {
            if (po.i == 0) || (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28523u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29547u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6452075u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6845291u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 0.0) {
                PluralCategory::ZERO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30059u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30571u64), None, None),
        |po| {
            if (po.i % 100 == 3
                || po.i % 100 == 23
                || po.i % 100 == 43
                || po.i % 100 == 63
                || po.i % 100 == 83)
            {
                PluralCategory::FEW
            } else if (po.n != 1.0
                && (po.i % 100 == 1
                    || po.i % 100 == 21
                    || po.i % 100 == 41
                    || po.i % 100 == 61
                    || po.i % 100 == 81))
            {
                PluralCategory::MANY
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.i % 100 == 2
                || po.i % 100 == 22
                || po.i % 100 == 42
                || po.i % 100 == 62
                || po.i % 100 == 82)
                || (po.i % 1000 == 0
                    && (po.i % 100000 == 40000
                        || po.i % 100000 == 60000
                        || po.i % 100000 == 80000
                        || (1000..=20000).contains(&(po.i))))
                || (po.n != 0.0 && po.i % 1000000 == 100000)
            {
                PluralCategory::TWO
            } else if (po.n == 0.0) {
                PluralCategory::ZERO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31083u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6775148u64),
            None,
            None
        ),
        |po| {
            if ((po.i == 0 || po.i == 1) && po.n != 0.0) {
                PluralCategory::ONE
            } else if (po.n == 0.0) {
                PluralCategory::ZERO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25196u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26476u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7629676u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28268u64), None, None),
        |po| {
            if ((0..=1).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28524u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29804u64), None, None),
        |po| {
            if ((2..=9).contains(&(po.i)) && !(11..=19).contains(&(po.i))) {
                PluralCategory::FEW
            } else if (po.f != 0) {
                PluralCategory::MANY
            } else if (po.i % 10 == 1 && !(11..=19).contains(&(po.i))) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30316u64), None, None),
        |po| {
            if (po.i % 10 == 1 && po.i % 100 != 11)
                || (po.v == 2 && po.f % 10 == 1 && po.f % 100 != 11)
                || (po.v != 2 && po.f % 10 == 1)
            {
                PluralCategory::ONE
            } else if (po.i % 10 == 0)
                || ((11..=19).contains(&(po.i)))
                || (po.v == 2 && (11..=19).contains(&(po.f % 100)))
            {
                PluralCategory::ZERO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7561581u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26477u64), None, None),
        |po| {
            if ((0..=1).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7300973u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27501u64), None, None),
        |po| {
            if (po.v == 0 && po.i % 10 == 1 && po.i % 100 != 11)
                || (po.f % 10 == 1 && po.f % 100 != 11)
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27757u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28269u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28525u64), None, None),
        |po| {
            if (po.v != 0) || (po.n == 0.0) || ((2..=19).contains(&(po.i))) {
                PluralCategory::FEW
            } else if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29293u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29549u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29805u64), None, None),
        |po| {
            if (po.n == 0.0) || ((2..=10).contains(&(po.i))) {
                PluralCategory::FEW
            } else if ((11..=19).contains(&(po.i))) {
                PluralCategory::MANY
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31085u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6840686u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7430510u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25198u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25710u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25966u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27758u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28270u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6844014u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28526u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7303534u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29294u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7304046u64),
            None,
            None
        ),
        |po| {
            if ((0..=1).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31086u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7240046u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28015u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29295u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29551u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6386543u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24944u64), None, None),
        |po| {
            if ((0..=1).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7364976u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7168880u64),
            None,
            None
        ),
        |po| {
            if (po.i == 0) || (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27760u64), None, None),
        |po| {
            if (po.v == 0 && (2..=4).contains(&(po.i % 10)) && !(12..=14).contains(&(po.i % 100))) {
                PluralCategory::FEW
            } else if (po.v == 0 && po.i != 1 && (0..=1).contains(&(po.i % 10)))
                || (po.v == 0 && (5..=9).contains(&(po.i % 10)))
                || (po.v == 0 && (12..=14).contains(&(po.i % 100)))
            {
                PluralCategory::MANY
            } else if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6779504u64),
            None,
            None
        ),
        |po| {
            if (po.i % 10 == 1 && po.i % 100 != 11)
                || (po.v == 2 && po.f % 10 == 1 && po.f % 100 != 11)
                || (po.v != 2 && po.f % 10 == 1)
            {
                PluralCategory::ONE
            } else if (po.i % 10 == 0)
                || ((11..=19).contains(&(po.i)))
                || (po.v == 2 && (11..=19).contains(&(po.f % 100)))
            {
                PluralCategory::ZERO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29552u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29808u64), None, None),
        |po| {
            if ((0..=1).contains(&(po.i))) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(29808u64),
            None,
            Some(subtags::Region::from_raw_unchecked(21584u32))
        ),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28018u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28530u64), None, None),
        |po| {
            if (po.v != 0) || (po.n == 0.0) || ((2..=19).contains(&(po.i))) {
                PluralCategory::FEW
            } else if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6713202u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30066u64), None, None),
        |po| {
            if (po.v == 0 && (2..=4).contains(&(po.i % 10)) && !(12..=14).contains(&(po.i % 100))) {
                PluralCategory::FEW
            } else if (po.v == 0 && po.i % 10 == 0)
                || (po.v == 0 && (5..=9).contains(&(po.i % 10)))
                || (po.v == 0 && (11..=14).contains(&(po.i % 100)))
            {
                PluralCategory::MANY
            } else if (po.v == 0 && po.i % 10 == 1 && po.i % 100 != 11) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7042930u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6840691u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7430515u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7627123u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25459u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7234419u64),
            None,
            None
        ),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25715u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6841459u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25971u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6841715u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7562611u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26483u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26739u64), None, None),
        |po| {
            if (po.v == 0 && (2..=4).contains(&(po.i % 10)) && !(12..=14).contains(&(po.i % 100)))
                || ((2..=4).contains(&(po.f % 10)) && !(12..=14).contains(&(po.f % 100)))
            {
                PluralCategory::FEW
            } else if (po.v == 0 && po.i % 10 == 1 && po.i % 100 != 11)
                || (po.f % 10 == 1 && po.f % 100 != 11)
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6908019u64),
            None,
            None
        ),
        |po| {
            if ((2..=10).contains(&(po.i)) && po.f == 0) {
                PluralCategory::FEW
            } else if (po.i == 0) || (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26995u64), None, None),
        |po| {
            if (po.n == 0.0 || po.n == 1.0) || (po.i == 0 && po.f == 1) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27507u64), None, None),
        |po| {
            if ((2..=4).contains(&(po.i)) && po.v == 0) {
                PluralCategory::FEW
            } else if (po.v != 0) {
                PluralCategory::MANY
            } else if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27763u64), None, None),
        |po| {
            if (po.v == 0 && (3..=4).contains(&(po.i % 100))) || (po.v != 0) {
                PluralCategory::FEW
            } else if (po.v == 0 && po.i % 100 == 1) {
                PluralCategory::ONE
            } else if (po.v == 0 && po.i % 100 == 2) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6385011u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6909299u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6974835u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7236979u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7564659u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28275u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28531u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29043u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29299u64), None, None),
        |po| {
            if (po.v == 0 && (2..=4).contains(&(po.i % 10)) && !(12..=14).contains(&(po.i % 100)))
                || ((2..=4).contains(&(po.f % 10)) && !(12..=14).contains(&(po.f % 100)))
            {
                PluralCategory::FEW
            } else if (po.v == 0 && po.i % 10 == 1 && po.i % 100 != 11)
                || (po.f % 10 == 1 && po.f % 100 != 11)
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29555u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7959411u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29811u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30067u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30323u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30579u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7502195u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24948u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25972u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7300468u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26740u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26996u64), None, None),
        |po| {
            if ((0..=1).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6777204u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27508u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27764u64), None, None),
        |po| {
            if (po.v == 0 && (po.i == 1 || po.i == 2 || po.i == 3))
                || (po.v == 0 && po.i % 10 != 4 && po.i % 10 != 6 && po.i % 10 != 9)
                || (po.v != 0 && po.f % 10 != 4 && po.f % 10 != 6 && po.f % 10 != 9)
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28276u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28532u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29300u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29556u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7174772u64),
            None,
            None
        ),
        |po| {
            if ((0..=1).contains(&(po.i)) && po.f == 0)
                || ((11..=99).contains(&(po.i)) && po.f == 0)
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26485u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27509u64), None, None),
        |po| {
            if (po.v == 0 && (2..=4).contains(&(po.i % 10)) && !(12..=14).contains(&(po.i % 100))) {
                PluralCategory::FEW
            } else if (po.v == 0 && po.i % 10 == 0)
                || (po.v == 0 && (5..=9).contains(&(po.i % 10)))
                || (po.v == 0 && (11..=14).contains(&(po.i % 100)))
            {
                PluralCategory::MANY
            } else if (po.v == 0 && po.i % 10 == 1 && po.i % 100 != 11) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29301u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31349u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25974u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26998u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28534u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7239030u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24951u64), None, None),
        |po| {
            if ((0..=1).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6644087u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28535u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26744u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6778744u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27001u64), None, None),
        |po| {
            if (po.i == 1 && po.v == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28537u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6649209u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26746u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30074u64), None, None),
        |po| {
            if (po.i == 0) || (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
];
pub const PRS_ORDINAL: &[(LanguageIdentifier, PluralRule)] = &[
    (
        langid!(subtags::Language::from_raw_unchecked(26209u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28001u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28257u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29281u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29537u64), None, None),
        |po| {
            if (po.n == 4.0) {
                PluralCategory::FEW
            } else if (po.n == 6.0) {
                PluralCategory::MANY
            } else if (po.n == 1.0
                || po.n == 5.0
                || po.n == 7.0
                || po.n == 8.0
                || po.n == 9.0
                || po.n == 10.0)
            {
                PluralCategory::ONE
            } else if (po.n == 2.0 || po.n == 3.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31329u64), None, None),
        |po| {
            if (po.i % 10 == 3 || po.i % 10 == 4)
                || (po.i % 1000 == 100
                    || po.i % 1000 == 200
                    || po.i % 1000 == 300
                    || po.i % 1000 == 400
                    || po.i % 1000 == 500
                    || po.i % 1000 == 600
                    || po.i % 1000 == 700
                    || po.i % 1000 == 800
                    || po.i % 1000 == 900)
            {
                PluralCategory::FEW
            } else if (po.i == 0)
                || (po.i % 10 == 6)
                || (po.i % 100 == 40 || po.i % 100 == 60 || po.i % 100 == 90)
            {
                PluralCategory::MANY
            } else if (po.i % 10 == 1
                || po.i % 10 == 2
                || po.i % 10 == 5
                || po.i % 10 == 7
                || po.i % 10 == 8)
                || (po.i % 100 == 20 || po.i % 100 == 50 || po.i % 100 == 70 || po.i % 100 == 80)
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25954u64), None, None),
        |po| {
            if ((po.i % 10 == 2 || po.i % 10 == 3) && po.i % 100 != 12 && po.i % 100 != 13) {
                PluralCategory::FEW
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26466u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28258u64), None, None),
        |po| {
            if (po.n == 4.0) {
                PluralCategory::FEW
            } else if (po.n == 6.0) {
                PluralCategory::MANY
            } else if (po.n == 1.0
                || po.n == 5.0
                || po.n == 7.0
                || po.n == 8.0
                || po.n == 9.0
                || po.n == 10.0)
            {
                PluralCategory::ONE
            } else if (po.n == 2.0 || po.n == 3.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29538u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24931u64), None, None),
        |po| {
            if (po.n == 4.0) {
                PluralCategory::FEW
            } else if (po.n == 1.0 || po.n == 3.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25955u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29539u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31075u64), None, None),
        |po| {
            if (po.n == 3.0 || po.n == 4.0) {
                PluralCategory::FEW
            } else if (po.n == 5.0 || po.n == 6.0) {
                PluralCategory::MANY
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0) {
                PluralCategory::TWO
            } else if (po.n == 0.0 || po.n == 7.0 || po.n == 8.0 || po.n == 9.0) {
                PluralCategory::ZERO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24932u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25956u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6452068u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27749u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28261u64), None, None),
        |po| {
            if (po.i % 10 == 3 && po.i % 100 != 13) {
                PluralCategory::FEW
            } else if (po.i % 10 == 1 && po.i % 100 != 11) {
                PluralCategory::ONE
            } else if (po.i % 10 == 2 && po.i % 100 != 12) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29541u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29797u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30053u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24934u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26982u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7104870u64),
            None,
            None
        ),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29286u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31078u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24935u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25703u64), None, None),
        |po| {
            if (po.n == 3.0 || po.n == 13.0) {
                PluralCategory::FEW
            } else if (po.n == 1.0 || po.n == 11.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0 || po.n == 12.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27751u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7828327u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30055u64), None, None),
        |po| {
            if (po.n == 4.0) {
                PluralCategory::FEW
            } else if (po.n == 6.0) {
                PluralCategory::MANY
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0 || po.n == 3.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25960u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26984u64), None, None),
        |po| {
            if (po.n == 4.0) {
                PluralCategory::FEW
            } else if (po.n == 6.0) {
                PluralCategory::MANY
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0 || po.n == 3.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29288u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6452072u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30056u64), None, None),
        |po| {
            if (po.n == 1.0 || po.n == 5.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31080u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24937u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25705u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28265u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29545u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29801u64), None, None),
        |po| {
            if (po.n == 11.0 || po.n == 8.0 || po.n == 80.0 || po.n == 800.0) {
                PluralCategory::MANY
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30569u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24938u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24939u64), None, None),
        |po| {
            if (po.i == 0)
                || (po.i % 100 == 40
                    || po.i % 100 == 60
                    || po.i % 100 == 80
                    || (2..=20).contains(&(po.i % 100)))
            {
                PluralCategory::MANY
            } else if (po.i == 1) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27499u64), None, None),
        |po| {
            if (po.i % 10 == 6) || (po.i % 10 == 9) || (po.i % 10 == 0 && po.n != 0.0) {
                PluralCategory::MANY
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28011u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28267u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28523u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30571u64), None, None),
        |po| {
            if (po.n == 5.0) || (po.i % 100 == 5) {
                PluralCategory::MANY
            } else if ((1..=4).contains(&(po.i)) && po.f == 0)
                || ((1..=4).contains(&(po.i))
                    || (21..=24).contains(&(po.i))
                    || (41..=44).contains(&(po.i))
                    || (61..=64).contains(&(po.i))
                    || (81..=84).contains(&(po.i)))
            {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31083u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28524u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29804u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30316u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27501u64), None, None),
        |po| {
            if ((po.i % 10 == 7 || po.i % 10 == 8) && po.i % 100 != 17 && po.i % 100 != 18) {
                PluralCategory::MANY
            } else if (po.i % 10 == 1 && po.i % 100 != 11) {
                PluralCategory::ONE
            } else if (po.i % 10 == 2 && po.i % 100 != 12) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27757u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28269u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28525u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29293u64), None, None),
        |po| {
            if (po.n == 4.0) {
                PluralCategory::FEW
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else if (po.n == 2.0 || po.n == 3.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29549u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31085u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25198u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25966u64), None, None),
        |po| {
            if ((1..=4).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27758u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29295u64), None, None),
        |po| {
            if (po.n == 4.0) {
                PluralCategory::FEW
            } else if (po.n == 6.0) {
                PluralCategory::MANY
            } else if (po.n == 1.0 || po.n == 5.0 || (7..=9).contains(&(po.i)) && po.f == 0) {
                PluralCategory::ONE
            } else if (po.n == 2.0 || po.n == 3.0) {
                PluralCategory::TWO
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24944u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27760u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6779504u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29552u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29808u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(28530u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30066u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25459u64), None, None),
        |po| {
            if (po.n == 11.0 || po.n == 8.0 || po.n == 80.0 || po.n == 800.0) {
                PluralCategory::MANY
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(7234419u64),
            None,
            None
        ),
        |po| {
            if (po.n == 11.0 || po.n == 8.0 || po.n == 80.0 || po.n == 800.0) {
                PluralCategory::MANY
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25715u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26739u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26995u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27507u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27763u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29043u64), None, None),
        |po| {
            if (po.i % 10 == 4 && po.i % 100 != 14) {
                PluralCategory::MANY
            } else if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29299u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30323u64), None, None),
        |po| {
            if ((po.i % 10 == 1 || po.i % 10 == 2) && po.i % 100 != 11 && po.i % 100 != 12) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30579u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(24948u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(25972u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26740u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27508u64), None, None),
        |po| {
            if (po.i % 10 == 6 || po.i % 10 == 9) || (po.n == 10.0) {
                PluralCategory::FEW
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27764u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29300u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(27509u64), None, None),
        |po| {
            if (po.i % 10 == 3 && po.i % 100 != 13) {
                PluralCategory::FEW
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(29301u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(31349u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26998u64), None, None),
        |po| {
            if (po.n == 1.0) {
                PluralCategory::ONE
            } else {
                PluralCategory::OTHER
            }
        },
    ),
    (
        langid!(
            subtags::Language::from_raw_unchecked(6649209u64),
            None,
            None
        ),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(26746u64), None, None),
        |po| PluralCategory::OTHER,
    ),
    (
        langid!(subtags::Language::from_raw_unchecked(30074u64), None, None),
        |po| PluralCategory::OTHER,
    ),
];
