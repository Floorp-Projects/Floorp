#![allow(dead_code)]

#[macro_use]
extern crate derive_more;

use std::convert::{TryFrom, TryInto};

// Ensure that the TryFrom macro is hygenic and doesn't break when `Result` has
// been redefined.
type Result = ();

#[derive(Clone, Copy, TryInto)]
#[try_into(owned, ref, ref_mut)]
enum MixedInts {
    SmallInt(i32),
    NamedBigInt {
        int: i64,
    },
    UnsignedWithIgnoredField(#[try_into(ignore)] bool, i64),
    NamedUnsignedWithIgnnoredField {
        #[try_into(ignore)]
        useless: bool,
        x: i64,
    },
    TwoSmallInts(i32, i32),
    NamedBigInts {
        x: i64,
        y: i64,
    },
    Unsigned(u32),
    NamedUnsigned {
        x: u32,
    },
    Unit,
    #[try_into(ignore)]
    Unit2,
}

#[test]
fn test_try_into() {
    let mut i = MixedInts::SmallInt(42);
    assert_eq!(Ok(42i32), i.try_into());
    assert_eq!(Ok(&42i32), (&i).try_into());
    assert_eq!(Ok(&mut 42i32), (&mut i).try_into());
    assert_eq!(
        i64::try_from(i),
        Err("Only NamedBigInt, UnsignedWithIgnoredField, NamedUnsignedWithIgnnoredField can be converted to i64")
    );
    assert_eq!(
        <(i32, i32)>::try_from(i),
        Err("Only TwoSmallInts can be converted to (i32, i32)")
    );
    assert_eq!(
        <(i64, i64)>::try_from(i),
        Err("Only NamedBigInts can be converted to (i64, i64)")
    );
    assert_eq!(
        u32::try_from(i),
        Err("Only Unsigned, NamedUnsigned can be converted to u32")
    );
    assert_eq!(<()>::try_from(i), Err("Only Unit can be converted to ()"));

    let mut i = MixedInts::NamedBigInt { int: 42 };
    assert_eq!(
        i32::try_from(i),
        Err("Only SmallInt can be converted to i32")
    );
    assert_eq!(Ok(42i64), i.try_into());
    assert_eq!(Ok(&42i64), (&i).try_into());
    assert_eq!(Ok(&mut 42i64), (&mut i).try_into());
    assert_eq!(
        <(i32, i32)>::try_from(i),
        Err("Only TwoSmallInts can be converted to (i32, i32)")
    );
    assert_eq!(
        <(i64, i64)>::try_from(i),
        Err("Only NamedBigInts can be converted to (i64, i64)")
    );
    assert_eq!(
        u32::try_from(i),
        Err("Only Unsigned, NamedUnsigned can be converted to u32")
    );
    assert_eq!(<()>::try_from(i), Err("Only Unit can be converted to ()"));

    let mut i = MixedInts::TwoSmallInts(42, 64);
    assert_eq!(
        i32::try_from(i),
        Err("Only SmallInt can be converted to i32")
    );
    assert_eq!(
        i64::try_from(i),
        Err("Only NamedBigInt, UnsignedWithIgnoredField, NamedUnsignedWithIgnnoredField can be converted to i64")
    );
    assert_eq!(Ok((42i32, 64i32)), i.try_into());
    assert_eq!(Ok((&42i32, &64i32)), (&i).try_into());
    assert_eq!(Ok((&mut 42i32, &mut 64i32)), (&mut i).try_into());
    assert_eq!(
        <(i64, i64)>::try_from(i),
        Err("Only NamedBigInts can be converted to (i64, i64)")
    );
    assert_eq!(
        u32::try_from(i),
        Err("Only Unsigned, NamedUnsigned can be converted to u32")
    );
    assert_eq!(<()>::try_from(i), Err("Only Unit can be converted to ()"));

    let mut i = MixedInts::NamedBigInts { x: 42, y: 64 };
    assert_eq!(
        i32::try_from(i),
        Err("Only SmallInt can be converted to i32")
    );
    assert_eq!(
        i64::try_from(i),
        Err("Only NamedBigInt, UnsignedWithIgnoredField, NamedUnsignedWithIgnnoredField can be converted to i64")
    );
    assert_eq!(
        <(i32, i32)>::try_from(i),
        Err("Only TwoSmallInts can be converted to (i32, i32)")
    );
    assert_eq!(Ok((42i64, 64i64)), i.try_into());
    assert_eq!(Ok((&42i64, &64i64)), (&i).try_into());
    assert_eq!(Ok((&mut 42i64, &mut 64i64)), (&mut i).try_into());
    assert_eq!(
        u32::try_from(i),
        Err("Only Unsigned, NamedUnsigned can be converted to u32")
    );
    assert_eq!(<()>::try_from(i), Err("Only Unit can be converted to ()"));

    let mut i = MixedInts::Unsigned(42);
    assert_eq!(
        i32::try_from(i),
        Err("Only SmallInt can be converted to i32")
    );
    assert_eq!(
        i64::try_from(i),
        Err("Only NamedBigInt, UnsignedWithIgnoredField, NamedUnsignedWithIgnnoredField can be converted to i64")
    );
    assert_eq!(
        <(i32, i32)>::try_from(i),
        Err("Only TwoSmallInts can be converted to (i32, i32)")
    );
    assert_eq!(
        <(i64, i64)>::try_from(i),
        Err("Only NamedBigInts can be converted to (i64, i64)")
    );
    assert_eq!(Ok(42u32), i.try_into());
    assert_eq!(Ok(&42u32), (&i).try_into());
    assert_eq!(Ok(&mut 42u32), (&mut i).try_into());
    assert_eq!(<()>::try_from(i), Err("Only Unit can be converted to ()"));

    let mut i = MixedInts::NamedUnsigned { x: 42 };
    assert_eq!(
        i32::try_from(i),
        Err("Only SmallInt can be converted to i32")
    );
    assert_eq!(
        i64::try_from(i),
        Err("Only NamedBigInt, UnsignedWithIgnoredField, NamedUnsignedWithIgnnoredField can be converted to i64")
    );
    assert_eq!(
        i64::try_from(i),
        Err("Only NamedBigInt, UnsignedWithIgnoredField, NamedUnsignedWithIgnnoredField can be converted to i64")
    );
    assert_eq!(
        <(i32, i32)>::try_from(i),
        Err("Only TwoSmallInts can be converted to (i32, i32)")
    );
    assert_eq!(
        <(i64, i64)>::try_from(i),
        Err("Only NamedBigInts can be converted to (i64, i64)")
    );
    assert_eq!(Ok(42u32), i.try_into());
    assert_eq!(Ok(&42u32), (&i).try_into());
    assert_eq!(Ok(&mut 42u32), (&mut i).try_into());
    assert_eq!(<()>::try_from(i), Err("Only Unit can be converted to ()"));

    let i = MixedInts::Unit;
    assert_eq!(
        i32::try_from(i),
        Err("Only SmallInt can be converted to i32")
    );
    assert_eq!(
        i64::try_from(i),
        Err("Only NamedBigInt, UnsignedWithIgnoredField, NamedUnsignedWithIgnnoredField can be converted to i64")
    );
    assert_eq!(
        <(i32, i32)>::try_from(i),
        Err("Only TwoSmallInts can be converted to (i32, i32)")
    );
    assert_eq!(
        <(i64, i64)>::try_from(i),
        Err("Only NamedBigInts can be converted to (i64, i64)")
    );
    assert_eq!(
        u32::try_from(i),
        Err("Only Unsigned, NamedUnsigned can be converted to u32")
    );
    assert_eq!(Ok(()), i.try_into());
}
