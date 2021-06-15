#![warn(rust_2018_idioms, single_use_lifetimes)]
#![allow(dead_code)]

// Refs: https://doc.rust-lang.org/nightly/reference/attributes.html

use std::{marker::PhantomPinned, pin::Pin};

use pin_project::pin_project;

fn is_unpin<T: Unpin>() {}

#[cfg(target_os = "linux")]
struct Linux;
#[cfg(not(target_os = "linux"))]
struct Other;

// Use this type to check that `cfg(any())` is working properly.
// If `cfg(any())` is not working properly, `is_unpin` will fail.
struct Any(PhantomPinned);

#[test]
fn cfg() {
    // structs

    #[pin_project(project_replace)]
    struct SameName {
        #[cfg(target_os = "linux")]
        #[pin]
        inner: Linux,
        #[cfg(not(target_os = "linux"))]
        #[pin]
        inner: Other,
        #[cfg(any())]
        #[pin]
        any: Any,
    }

    is_unpin::<SameName>();

    #[cfg(target_os = "linux")]
    let _x = SameName { inner: Linux };
    #[cfg(not(target_os = "linux"))]
    let _x = SameName { inner: Other };

    #[pin_project(project_replace)]
    struct DifferentName {
        #[cfg(target_os = "linux")]
        #[pin]
        l: Linux,
        #[cfg(not(target_os = "linux"))]
        #[pin]
        o: Other,
        #[cfg(any())]
        #[pin]
        a: Any,
    }

    is_unpin::<DifferentName>();

    #[cfg(target_os = "linux")]
    let _x = DifferentName { l: Linux };
    #[cfg(not(target_os = "linux"))]
    let _x = DifferentName { o: Other };

    #[pin_project(project_replace)]
    struct TupleStruct(
        #[cfg(target_os = "linux")]
        #[pin]
        Linux,
        #[cfg(not(target_os = "linux"))]
        #[pin]
        Other,
        #[cfg(any())]
        #[pin]
        Any,
    );

    is_unpin::<TupleStruct>();

    #[cfg(target_os = "linux")]
    let _x = TupleStruct(Linux);
    #[cfg(not(target_os = "linux"))]
    let _x = TupleStruct(Other);

    // enums

    #[pin_project(project_replace)]
    enum Variant {
        #[cfg(target_os = "linux")]
        Inner(#[pin] Linux),
        #[cfg(not(target_os = "linux"))]
        Inner(#[pin] Other),

        #[cfg(target_os = "linux")]
        Linux(#[pin] Linux),
        #[cfg(not(target_os = "linux"))]
        Other(#[pin] Other),
        #[cfg(any())]
        Any(#[pin] Any),
    }

    is_unpin::<Variant>();

    #[cfg(target_os = "linux")]
    let _x = Variant::Inner(Linux);
    #[cfg(not(target_os = "linux"))]
    let _x = Variant::Inner(Other);

    #[cfg(target_os = "linux")]
    let _x = Variant::Linux(Linux);
    #[cfg(not(target_os = "linux"))]
    let _x = Variant::Other(Other);

    #[pin_project(project_replace)]
    enum Field {
        SameName {
            #[cfg(target_os = "linux")]
            #[pin]
            inner: Linux,
            #[cfg(not(target_os = "linux"))]
            #[pin]
            inner: Other,
            #[cfg(any())]
            #[pin]
            any: Any,
        },
        DifferentName {
            #[cfg(target_os = "linux")]
            #[pin]
            l: Linux,
            #[cfg(not(target_os = "linux"))]
            #[pin]
            w: Other,
            #[cfg(any())]
            #[pin]
            any: Any,
        },
        TupleVariant(
            #[cfg(target_os = "linux")]
            #[pin]
            Linux,
            #[cfg(not(target_os = "linux"))]
            #[pin]
            Other,
            #[cfg(any())]
            #[pin]
            Any,
        ),
    }

    is_unpin::<Field>();

    #[cfg(target_os = "linux")]
    let _x = Field::SameName { inner: Linux };
    #[cfg(not(target_os = "linux"))]
    let _x = Field::SameName { inner: Other };

    #[cfg(target_os = "linux")]
    let _x = Field::DifferentName { l: Linux };
    #[cfg(not(target_os = "linux"))]
    let _x = Field::DifferentName { w: Other };

    #[cfg(target_os = "linux")]
    let _x = Field::TupleVariant(Linux);
    #[cfg(not(target_os = "linux"))]
    let _x = Field::TupleVariant(Other);
}

#[test]
fn cfg_attr() {
    #[pin_project(project_replace)]
    struct SameCfg {
        #[cfg(target_os = "linux")]
        #[cfg_attr(target_os = "linux", pin)]
        inner: Linux,
        #[cfg(not(target_os = "linux"))]
        #[cfg_attr(not(target_os = "linux"), pin)]
        inner: Other,
        #[cfg(any())]
        #[cfg_attr(any(), pin)]
        any: Any,
    }

    is_unpin::<SameCfg>();

    #[cfg(target_os = "linux")]
    let mut x = SameCfg { inner: Linux };
    #[cfg(not(target_os = "linux"))]
    let mut x = SameCfg { inner: Other };

    let x = Pin::new(&mut x).project();
    #[cfg(target_os = "linux")]
    let _: Pin<&mut Linux> = x.inner;
    #[cfg(not(target_os = "linux"))]
    let _: Pin<&mut Other> = x.inner;

    #[pin_project(project_replace)]
    struct DifferentCfg {
        #[cfg(target_os = "linux")]
        #[cfg_attr(target_os = "linux", pin)]
        inner: Linux,
        #[cfg(not(target_os = "linux"))]
        #[cfg_attr(target_os = "linux", pin)]
        inner: Other,
        #[cfg(any())]
        #[cfg_attr(any(), pin)]
        any: Any,
    }

    is_unpin::<DifferentCfg>();

    #[cfg(target_os = "linux")]
    let mut x = DifferentCfg { inner: Linux };
    #[cfg(not(target_os = "linux"))]
    let mut x = DifferentCfg { inner: Other };

    let x = Pin::new(&mut x).project();
    #[cfg(target_os = "linux")]
    let _: Pin<&mut Linux> = x.inner;
    #[cfg(not(target_os = "linux"))]
    let _: &mut Other = x.inner;

    #[cfg_attr(not(any()), pin_project)]
    struct Foo<T> {
        #[cfg_attr(not(any()), pin)]
        inner: T,
    }

    let mut x = Foo { inner: 0_u8 };
    let x = Pin::new(&mut x).project();
    let _: Pin<&mut u8> = x.inner;
}

#[test]
fn cfg_attr_any_packed() {
    // Since `cfg(any())` can never be true, it is okay for this to pass.
    #[pin_project(project_replace)]
    #[cfg_attr(any(), repr(packed))]
    struct Struct {
        #[pin]
        field: u32,
    }
}
