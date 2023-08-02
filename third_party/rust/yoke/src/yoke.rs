// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::either::EitherCart;
#[cfg(feature = "alloc")]
use crate::erased::{ErasedArcCart, ErasedBoxCart, ErasedRcCart};
use crate::trait_hack::YokeTraitHack;
use crate::Yokeable;
use core::marker::PhantomData;
use core::ops::Deref;
use stable_deref_trait::StableDeref;

#[cfg(feature = "alloc")]
use alloc::boxed::Box;
#[cfg(feature = "alloc")]
use alloc::rc::Rc;
#[cfg(feature = "alloc")]
use alloc::sync::Arc;

/// A Cow-like borrowed object "yoked" to its backing data.
///
/// This allows things like zero copy deserialized data to carry around
/// shared references to their backing buffer, by "erasing" their static lifetime
/// and turning it into a dynamically managed one.
///
/// `Y` (the [`Yokeable`]) is the object containing the references,
/// and will typically be of the form `Foo<'static>`. The `'static` is
/// not the actual lifetime of the data, rather it is a convenient way to mark the
/// erased lifetime and make it dynamic.
///
/// `C` is the "cart", which `Y` may contain references to. After the yoke is constructed,
/// the cart serves little purpose except to guarantee that `Y`'s references remain valid
/// for as long as the yoke remains in memory (by calling the destructor at the appropriate moment).
///
/// The primary constructor for [`Yoke`] is [`Yoke::attach_to_cart()`]. Several variants of that
/// constructor are provided to serve numerous types of call sites and `Yoke` signatures.
///
/// The key behind this type is [`Yoke::get()`], where calling [`.get()`][Yoke::get] on a type like
/// `Yoke<Cow<'static, str>, _>` will get you a short-lived `&'a Cow<'a, str>`, restricted to the
/// lifetime of the borrow used during `.get()`. This is entirely safe since the `Cow` borrows from
/// the cart type `C`, which cannot be interfered with as long as the `Yoke` is borrowed by `.get
/// ()`. `.get()` protects access by essentially reifying the erased lifetime to a safe local one
/// when necessary.
///
/// Furthermore, there are various [`.map_project()`][Yoke::map_project] methods that allow turning a `Yoke`
/// into another `Yoke` containing a different type that may contain elements of the original yoked
/// value. See the [`Yoke::map_project()`] docs for more details.
///
/// In general, `C` is a concrete type, but it is also possible for it to be a trait object.
///
/// # Example
///
/// For example, we can use this to store zero-copy deserialized data in a cache:
///
/// ```rust
/// # use yoke::{Yoke, Yokeable};
/// # use std::rc::Rc;
/// # use std::borrow::Cow;
/// # fn load_from_cache(_filename: &str) -> Rc<[u8]> {
/// #     // dummy implementation
/// #     Rc::new([0x5, 0, 0, 0, 0, 0, 0, 0, 0x68, 0x65, 0x6c, 0x6c, 0x6f])
/// # }
///
/// fn load_object(filename: &str) -> Yoke<Cow<'static, str>, Rc<[u8]>> {
///     let rc: Rc<[u8]> = load_from_cache(filename);
///     Yoke::<Cow<'static, str>, Rc<[u8]>>::attach_to_cart(rc, |data: &[u8]| {
///         // essentially forcing a #[serde(borrow)]
///         Cow::Borrowed(bincode::deserialize(data).unwrap())
///     })
/// }
///
/// let yoke = load_object("filename.bincode");
/// assert_eq!(&**yoke.get(), "hello");
/// assert!(matches!(yoke.get(), &Cow::Borrowed(_)));
/// ```
#[derive(Debug)]
pub struct Yoke<Y: for<'a> Yokeable<'a>, C> {
    // must be the first field for drop order
    // this will have a 'static lifetime parameter, that parameter is a lie
    yokeable: Y,
    cart: C,
}

impl<Y: for<'a> Yokeable<'a>, C: StableDeref> Yoke<Y, C>
where
    <C as Deref>::Target: 'static,
{
    /// Construct a [`Yoke`] by yokeing an object to a cart in a closure.
    ///
    /// See also [`Yoke::try_attach_to_cart()`] to return a `Result` from the closure.
    ///
    /// Call sites for this function may not compile pre-1.61; if this still happens, use
    /// [`Yoke::attach_to_cart_badly()`] and file a bug.
    ///
    /// # Examples
    ///
    /// ```
    /// # use yoke::{Yoke, Yokeable};
    /// # use std::rc::Rc;
    /// # use std::borrow::Cow;
    /// # fn load_from_cache(_filename: &str) -> Rc<[u8]> {
    /// #     // dummy implementation
    /// #     Rc::new([0x5, 0, 0, 0, 0, 0, 0, 0, 0x68, 0x65, 0x6c, 0x6c, 0x6f])
    /// # }
    ///
    /// fn load_object(filename: &str) -> Yoke<Cow<'static, str>, Rc<[u8]>> {
    ///     let rc: Rc<[u8]> = load_from_cache(filename);
    ///     Yoke::<Cow<'static, str>, Rc<[u8]>>::attach_to_cart(rc, |data: &[u8]| {
    ///         // essentially forcing a #[serde(borrow)]
    ///         Cow::Borrowed(bincode::deserialize(data).unwrap())
    ///     })
    /// }
    ///
    /// let yoke: Yoke<Cow<str>, _> = load_object("filename.bincode");
    /// assert_eq!(&**yoke.get(), "hello");
    /// assert!(matches!(yoke.get(), &Cow::Borrowed(_)));
    /// ```
    pub fn attach_to_cart<F>(cart: C, f: F) -> Self
    where
        // safety note: This works by enforcing that the *only* place the return value of F
        // can borrow from is the cart, since `F` must be valid for all lifetimes `'de`
        //
        // The <C as Deref>::Target: 'static on the impl is crucial for safety as well
        //
        // See safety docs at the bottom of this file for more information
        F: for<'de> FnOnce(&'de <C as Deref>::Target) -> <Y as Yokeable<'de>>::Output,
        <C as Deref>::Target: 'static,
    {
        let deserialized = f(cart.deref());
        Self {
            yokeable: unsafe { Y::make(deserialized) },
            cart,
        }
    }

    /// Construct a [`Yoke`] by yokeing an object to a cart. If an error occurs in the
    /// deserializer function, the error is passed up to the caller.
    ///
    /// Call sites for this function may not compile pre-1.61; if this still happens, use
    /// [`Yoke::try_attach_to_cart_badly()`] and file a bug.
    pub fn try_attach_to_cart<E, F>(cart: C, f: F) -> Result<Self, E>
    where
        F: for<'de> FnOnce(&'de <C as Deref>::Target) -> Result<<Y as Yokeable<'de>>::Output, E>,
    {
        let deserialized = f(cart.deref())?;
        Ok(Self {
            yokeable: unsafe { Y::make(deserialized) },
            cart,
        })
    }

    /// Use [`Yoke::attach_to_cart()`].
    ///
    /// This was needed because the pre-1.61 compiler couldn't always handle the FnOnce trait bound.
    #[deprecated]
    pub fn attach_to_cart_badly(
        cart: C,
        f: for<'de> fn(&'de <C as Deref>::Target) -> <Y as Yokeable<'de>>::Output,
    ) -> Self {
        Self::attach_to_cart(cart, f)
    }

    /// Use [`Yoke::try_attach_to_cart()`].
    ///
    /// This was needed because the pre-1.61 compiler couldn't always handle the FnOnce trait bound.
    #[deprecated]
    pub fn try_attach_to_cart_badly<E>(
        cart: C,
        f: for<'de> fn(&'de <C as Deref>::Target) -> Result<<Y as Yokeable<'de>>::Output, E>,
    ) -> Result<Self, E> {
        Self::try_attach_to_cart(cart, f)
    }
}

impl<Y: for<'a> Yokeable<'a>, C> Yoke<Y, C> {
    /// Obtain a valid reference to the yokeable data
    ///
    /// This essentially transforms the lifetime of the internal yokeable data to
    /// be valid.
    /// For example, if you're working with a `Yoke<Cow<'static, T>, C>`, this
    /// will return an `&'a Cow<'a, T>`
    ///
    /// # Example
    ///
    /// ```rust
    /// # use yoke::{Yoke, Yokeable};
    /// # use std::rc::Rc;
    /// # use std::borrow::Cow;
    /// # fn load_from_cache(_filename: &str) -> Rc<[u8]> {
    /// #     // dummy implementation
    /// #     Rc::new([0x5, 0, 0, 0, 0, 0, 0, 0, 0x68, 0x65, 0x6c, 0x6c, 0x6f])
    /// # }
    /// #
    /// # fn load_object(filename: &str) -> Yoke<Cow<'static, str>, Rc<[u8]>> {
    /// #     let rc: Rc<[u8]> = load_from_cache(filename);
    /// #     Yoke::<Cow<'static, str>, Rc<[u8]>>::attach_to_cart(rc, |data: &[u8]| {
    /// #         Cow::Borrowed(bincode::deserialize(data).unwrap())
    /// #     })
    /// # }
    ///
    /// // load_object() defined in the example at the top of this page
    /// let yoke: Yoke<Cow<str>, _> = load_object("filename.bincode");
    /// assert_eq!(yoke.get(), "hello");
    /// ```
    #[inline]
    pub fn get<'a>(&'a self) -> &'a <Y as Yokeable<'a>>::Output {
        self.yokeable.transform()
    }

    /// Get a reference to the backing cart.
    ///
    /// This can be useful when building caches, etc. However, if you plan to store the cart
    /// separately from the yoke, read the note of caution below in [`Yoke::into_backing_cart`].
    pub fn backing_cart(&self) -> &C {
        &self.cart
    }

    /// Get the backing cart by value, dropping the yokeable object.
    ///
    /// **Caution:** Calling this method could cause information saved in the yokeable object but
    /// not the cart to be lost. Use this method only if the yokeable object cannot contain its
    /// own information.
    ///
    /// # Example
    ///
    /// Good example: the yokeable object is only a reference, so no information can be lost.
    ///
    /// ```
    /// use yoke::Yoke;
    ///
    /// let local_data = "foo".to_owned();
    /// let yoke = Yoke::<&'static str, Box<String>>::attach_to_zero_copy_cart(
    ///     Box::new(local_data),
    /// );
    /// assert_eq!(*yoke.get(), "foo");
    ///
    /// // Get back the cart
    /// let cart = yoke.into_backing_cart();
    /// assert_eq!(&*cart, "foo");
    /// ```
    ///
    /// Bad example: information specified in `.with_mut()` is lost.
    ///
    /// ```
    /// use std::borrow::Cow;
    /// use yoke::Yoke;
    ///
    /// let local_data = "foo".to_owned();
    /// let mut yoke =
    ///     Yoke::<Cow<'static, str>, Box<String>>::attach_to_zero_copy_cart(
    ///         Box::new(local_data),
    ///     );
    /// assert_eq!(yoke.get(), "foo");
    ///
    /// // Override data in the cart
    /// yoke.with_mut(|cow| {
    ///     let mut_str = cow.to_mut();
    ///     mut_str.clear();
    ///     mut_str.push_str("bar");
    /// });
    /// assert_eq!(yoke.get(), "bar");
    ///
    /// // Get back the cart
    /// let cart = yoke.into_backing_cart();
    /// assert_eq!(&*cart, "foo"); // WHOOPS!
    /// ```
    pub fn into_backing_cart(self) -> C {
        self.cart
    }

    /// Unsafe function for replacing the cart with another
    ///
    /// This can be used for type-erasing the cart, for example.
    ///
    /// # Safety
    ///
    /// - `f()` must not panic
    /// - References from the yokeable `Y` should still be valid for the lifetime of the
    ///   returned cart type `C`.
    /// - Lifetimes inside C must not be lengthened, even if they are themselves contravariant.
    ///   I.e., if C contains an `fn(&'a u8)`, it cannot be replaced with `fn(&'static u8),
    ///   even though that is typically safe.
    ///
    /// Typically, this means implementing `f` as something which _wraps_ the inner cart type `C`.
    /// `Yoke` only really cares about destructors for its carts so it's fine to erase other
    /// information about the cart, as long as the backing data will still be destroyed at the
    /// same time.
    #[inline]
    pub unsafe fn replace_cart<C2>(self, f: impl FnOnce(C) -> C2) -> Yoke<Y, C2> {
        Yoke {
            yokeable: self.yokeable,
            cart: f(self.cart),
        }
    }

    /// Mutate the stored [`Yokeable`] data.
    ///
    /// See [`Yokeable::transform_mut()`] for why this operation is safe.
    ///
    /// # Example
    ///
    /// This can be used to partially mutate the stored data, provided
    /// no _new_ borrowed data is introduced.
    ///
    /// ```rust
    /// # use yoke::{Yoke, Yokeable};
    /// # use std::rc::Rc;
    /// # use std::borrow::Cow;
    /// # use std::mem;
    /// # fn load_from_cache(_filename: &str) -> Rc<[u8]> {
    /// #     // dummy implementation
    /// #     Rc::new([0x5, 0, 0, 0, 0, 0, 0, 0, 0x68, 0x65, 0x6c, 0x6c, 0x6f])
    /// # }
    /// #
    /// # fn load_object(filename: &str) -> Yoke<Bar<'static>, Rc<[u8]>> {
    /// #     let rc: Rc<[u8]> = load_from_cache(filename);
    /// #     Yoke::<Bar<'static>, Rc<[u8]>>::attach_to_cart(rc, |data: &[u8]| {
    /// #         // A real implementation would properly deserialize `Bar` as a whole
    /// #         Bar {
    /// #             numbers: Cow::Borrowed(bincode::deserialize(data).unwrap()),
    /// #             string: Cow::Borrowed(bincode::deserialize(data).unwrap()),
    /// #             owned: Vec::new(),
    /// #         }
    /// #     })
    /// # }
    ///
    /// // also implements Yokeable
    /// struct Bar<'a> {
    ///     numbers: Cow<'a, [u8]>,
    ///     string: Cow<'a, str>,
    ///     owned: Vec<u8>,
    /// }
    ///
    /// // `load_object()` deserializes an object from a file
    /// let mut bar: Yoke<Bar, _> = load_object("filename.bincode");
    /// assert_eq!(bar.get().string, "hello");
    /// assert!(matches!(bar.get().string, Cow::Borrowed(_)));
    /// assert_eq!(&*bar.get().numbers, &[0x68, 0x65, 0x6c, 0x6c, 0x6f]);
    /// assert!(matches!(bar.get().numbers, Cow::Borrowed(_)));
    /// assert_eq!(&*bar.get().owned, &[]);
    ///
    /// bar.with_mut(|bar| {
    ///     bar.string.to_mut().push_str(" world");
    ///     bar.owned.extend_from_slice(&[1, 4, 1, 5, 9]);
    /// });
    ///
    /// assert_eq!(bar.get().string, "hello world");
    /// assert!(matches!(bar.get().string, Cow::Owned(_)));
    /// assert_eq!(&*bar.get().owned, &[1, 4, 1, 5, 9]);
    /// // Unchanged and still Cow::Borrowed
    /// assert_eq!(&*bar.get().numbers, &[0x68, 0x65, 0x6c, 0x6c, 0x6f]);
    /// assert!(matches!(bar.get().numbers, Cow::Borrowed(_)));
    ///
    /// # unsafe impl<'a> Yokeable<'a> for Bar<'static> {
    /// #     type Output = Bar<'a>;
    /// #     fn transform(&'a self) -> &'a Bar<'a> {
    /// #         self
    /// #     }
    /// #
    /// #     fn transform_owned(self) -> Bar<'a> {
    /// #         // covariant lifetime cast, can be done safely
    /// #         self
    /// #     }
    /// #
    /// #     unsafe fn make(from: Bar<'a>) -> Self {
    /// #         let ret = mem::transmute_copy(&from);
    /// #         mem::forget(from);
    /// #         ret
    /// #     }
    /// #
    /// #     fn transform_mut<F>(&'a mut self, f: F)
    /// #     where
    /// #         F: 'static + FnOnce(&'a mut Self::Output),
    /// #     {
    /// #         unsafe { f(mem::transmute(self)) }
    /// #     }
    /// # }
    /// ```
    pub fn with_mut<'a, F>(&'a mut self, f: F)
    where
        F: 'static + for<'b> FnOnce(&'b mut <Y as Yokeable<'a>>::Output),
    {
        self.yokeable.transform_mut(f)
    }

    /// Helper function allowing one to wrap the cart type `C` in an `Option<T>`.
    #[inline]
    pub fn wrap_cart_in_option(self) -> Yoke<Y, Option<C>> {
        unsafe {
            // safe because the cart is preserved, just wrapped
            self.replace_cart(Some)
        }
    }
}

impl<Y: for<'a> Yokeable<'a>> Yoke<Y, ()> {
    /// Construct a new [`Yoke`] from static data. There will be no
    /// references to `cart` here since [`Yokeable`]s are `'static`,
    /// this is good for e.g. constructing fully owned
    /// [`Yoke`]s with no internal borrowing.
    ///
    /// This is similar to [`Yoke::new_owned()`] but it does not allow you to
    /// mix the [`Yoke`] with borrowed data. This is primarily useful
    /// for using [`Yoke`] in generic scenarios.
    ///
    /// # Example
    ///
    /// ```rust
    /// # use yoke::Yoke;
    /// # use std::borrow::Cow;
    /// # use std::rc::Rc;
    ///
    /// let owned: Cow<str> = "hello".to_owned().into();
    /// // this yoke can be intermingled with actually-borrowed Yokes
    /// let yoke: Yoke<Cow<str>, ()> = Yoke::new_always_owned(owned);
    ///
    /// assert_eq!(yoke.get(), "hello");
    /// ```
    pub fn new_always_owned(yokeable: Y) -> Self {
        Self { yokeable, cart: () }
    }

    /// Obtain the yokeable out of a `Yoke<Y, ()>`
    ///
    /// For most `Yoke` types this would be unsafe but it's
    /// fine for `Yoke<Y, ()>` since there are no actual internal
    /// references
    pub fn into_yokeable(self) -> Y {
        self.yokeable
    }
}

impl<Y: for<'a> Yokeable<'a>, C: StableDeref> Yoke<Y, Option<C>> {
    /// Construct a new [`Yoke`] from static data. There will be no
    /// references to `cart` here since [`Yokeable`]s are `'static`,
    /// this is good for e.g. constructing fully owned
    /// [`Yoke`]s with no internal borrowing.
    ///
    /// This can be paired with [`Yoke:: wrap_cart_in_option()`] to mix owned
    /// and borrowed data.
    ///
    /// If you do not wish to pair this with borrowed data, [`Yoke::new_always_owned()`] can
    /// be used to get a [`Yoke`] API on always-owned data.
    ///
    /// # Example
    ///
    /// ```rust
    /// # use yoke::Yoke;
    /// # use std::borrow::Cow;
    /// # use std::rc::Rc;
    ///
    /// let owned: Cow<str> = "hello".to_owned().into();
    /// // this yoke can be intermingled with actually-borrowed Yokes
    /// let yoke: Yoke<Cow<str>, Option<Rc<[u8]>>> = Yoke::new_owned(owned);
    ///
    /// assert_eq!(yoke.get(), "hello");
    /// ```
    pub fn new_owned(yokeable: Y) -> Self {
        Self {
            yokeable,
            cart: None,
        }
    }

    /// Obtain the yokeable out of a `Yoke<Y, Option<C>>` if possible.
    ///
    /// If the cart is `None`, this returns `Some`, but if the cart is `Some`,
    /// this returns `self` as an error.
    pub fn try_into_yokeable(self) -> Result<Y, Self> {
        match self.cart {
            Some(_) => Err(self),
            None => Ok(self.yokeable),
        }
    }
}

/// This trait marks cart types that do not change source on cloning
///
/// This is conceptually similar to [`stable_deref_trait::CloneStableDeref`],
/// however [`stable_deref_trait::CloneStableDeref`] is not (and should not) be
/// implemented on [`Option`] (since it's not [`Deref`]). [`CloneableCart`] essentially is
/// "if there _is_ data to borrow from here, cloning the cart gives you an additional
/// handle to the same data".
///
/// # Safety
/// This trait is safe to implement `StableDeref` types which, once `Clone`d, point to the same underlying data.
///
/// (This trait is also implemented on `Option<T>` and `()`, which are the two non-`StableDeref` cart types that
/// Yokes can be constructed for)
pub unsafe trait CloneableCart: Clone {}

#[cfg(feature = "alloc")]
unsafe impl<T: ?Sized> CloneableCart for Rc<T> {}
#[cfg(feature = "alloc")]
unsafe impl<T: ?Sized> CloneableCart for Arc<T> {}
unsafe impl<T: CloneableCart> CloneableCart for Option<T> {}
unsafe impl<'a, T: ?Sized> CloneableCart for &'a T {}
unsafe impl CloneableCart for () {}

/// Clone requires that the cart type `C` derefs to the same address after it is cloned. This works for
/// Rc, Arc, and &'a T.
///
/// For other cart types, clone `.backing_cart()` and re-use `.attach_to_cart()`; however, doing
/// so may lose mutations performed via `.with_mut()`.
///
/// Cloning a `Yoke` is often a cheap operation requiring no heap allocations, in much the same
/// way that cloning an `Rc` is a cheap operation. However, if the `yokeable` contains owned data
/// (e.g., from `.with_mut()`), that data will need to be cloned.
impl<Y: for<'a> Yokeable<'a>, C: CloneableCart> Clone for Yoke<Y, C>
where
    for<'a> YokeTraitHack<<Y as Yokeable<'a>>::Output>: Clone,
{
    fn clone(&self) -> Self {
        let this: &Y::Output = self.get();
        // We have an &T not a T, and we can clone YokeTraitHack<T>
        let this_hack = YokeTraitHack(this).into_ref();
        Yoke {
            yokeable: unsafe { Y::make(this_hack.clone().0) },
            cart: self.cart.clone(),
        }
    }
}

#[test]
fn test_clone() {
    let local_data = "foo".to_owned();
    let y1 = Yoke::<alloc::borrow::Cow<'static, str>, Rc<String>>::attach_to_zero_copy_cart(
        Rc::new(local_data),
    );

    // Test basic clone
    let y2 = y1.clone();
    assert_eq!(y1.get(), "foo");
    assert_eq!(y2.get(), "foo");

    // Test clone with mutation on target
    let mut y3 = y1.clone();
    y3.with_mut(|y| {
        y.to_mut().push_str("bar");
    });
    assert_eq!(y1.get(), "foo");
    assert_eq!(y2.get(), "foo");
    assert_eq!(y3.get(), "foobar");

    // Test that mutations on source do not affect target
    let y4 = y3.clone();
    y3.with_mut(|y| {
        y.to_mut().push_str("baz");
    });
    assert_eq!(y1.get(), "foo");
    assert_eq!(y2.get(), "foo");
    assert_eq!(y3.get(), "foobarbaz");
    assert_eq!(y4.get(), "foobar");
}

impl<Y: for<'a> Yokeable<'a>, C> Yoke<Y, C> {
    /// Allows one to "project" a yoke to perform a transformation on the data, potentially
    /// looking at a subfield, and producing a new yoke. This will move cart, and the provided
    /// transformation is only allowed to use data known to be borrowed from the cart.
    ///
    /// The callback takes an additional `PhantomData<&()>` parameter to anchor lifetimes
    /// (see [#86702](https://github.com/rust-lang/rust/issues/86702)) This parameter
    /// should just be ignored in the callback.
    ///
    /// This can be used, for example, to transform data from one format to another:
    ///
    /// ```
    /// # use std::rc::Rc;
    /// # use yoke::Yoke;
    /// #
    /// fn slice(y: Yoke<&'static str, Rc<[u8]>>) -> Yoke<&'static [u8], Rc<[u8]>> {
    ///     y.map_project(move |yk, _| yk.as_bytes())
    /// }
    /// ```
    ///
    /// This can also be used to create a yoke for a subfield
    ///
    /// ```
    /// # use std::borrow::Cow;
    /// # use yoke::{Yoke, Yokeable};
    /// # use std::mem;
    /// # use std::rc::Rc;
    /// #
    /// // also safely implements Yokeable<'a>
    /// struct Bar<'a> {
    ///     string_1: &'a str,
    ///     string_2: &'a str,
    /// }
    ///
    /// fn map_project_string_1(
    ///     bar: Yoke<Bar<'static>, Rc<[u8]>>,
    /// ) -> Yoke<&'static str, Rc<[u8]>> {
    ///     bar.map_project(|bar, _| bar.string_1)
    /// }
    ///
    /// #
    /// # unsafe impl<'a> Yokeable<'a> for Bar<'static> {
    /// #     type Output = Bar<'a>;
    /// #     fn transform(&'a self) -> &'a Bar<'a> {
    /// #         self
    /// #     }
    /// #
    /// #     fn transform_owned(self) -> Bar<'a> {
    /// #         // covariant lifetime cast, can be done safely
    /// #         self
    /// #     }
    /// #
    /// #     unsafe fn make(from: Bar<'a>) -> Self {
    /// #         let ret = mem::transmute_copy(&from);
    /// #         mem::forget(from);
    /// #         ret
    /// #     }
    /// #
    /// #     fn transform_mut<F>(&'a mut self, f: F)
    /// #     where
    /// #         F: 'static + FnOnce(&'a mut Self::Output),
    /// #     {
    /// #         unsafe { f(mem::transmute(self)) }
    /// #     }
    /// # }
    /// ```
    //
    // Safety docs can be found below on `__project_safety_docs()`
    pub fn map_project<P, F>(self, f: F) -> Yoke<P, C>
    where
        P: for<'a> Yokeable<'a>,
        F: for<'a> FnOnce(
            <Y as Yokeable<'a>>::Output,
            PhantomData<&'a ()>,
        ) -> <P as Yokeable<'a>>::Output,
    {
        let p = f(self.yokeable.transform_owned(), PhantomData);
        Yoke {
            yokeable: unsafe { P::make(p) },
            cart: self.cart,
        }
    }

    /// This is similar to [`Yoke::map_project`], however it does not move
    /// [`Self`] and instead clones the cart (only if the cart is a [`CloneableCart`])
    ///
    /// This is a bit more efficient than cloning the [`Yoke`] and then calling [`Yoke::map_project`]
    /// because then it will not clone fields that are going to be discarded.
    pub fn map_project_cloned<'this, P, F>(&'this self, f: F) -> Yoke<P, C>
    where
        P: for<'a> Yokeable<'a>,
        C: CloneableCart,
        F: for<'a> FnOnce(
            &'this <Y as Yokeable<'a>>::Output,
            PhantomData<&'a ()>,
        ) -> <P as Yokeable<'a>>::Output,
    {
        let p = f(self.get(), PhantomData);
        Yoke {
            yokeable: unsafe { P::make(p) },
            cart: self.cart.clone(),
        }
    }

    /// This is similar to [`Yoke::map_project`], however it can also bubble up an error
    /// from the callback.
    ///
    /// ```
    /// # use std::rc::Rc;
    /// # use yoke::Yoke;
    /// # use std::str::{self, Utf8Error};
    /// #
    /// fn slice(
    ///     y: Yoke<&'static [u8], Rc<[u8]>>,
    /// ) -> Result<Yoke<&'static str, Rc<[u8]>>, Utf8Error> {
    ///     y.try_map_project(move |bytes, _| str::from_utf8(bytes))
    /// }
    /// ```
    ///
    /// This can also be used to create a yoke for a subfield
    ///
    /// ```
    /// # use std::borrow::Cow;
    /// # use yoke::{Yoke, Yokeable};
    /// # use std::mem;
    /// # use std::rc::Rc;
    /// # use std::str::{self, Utf8Error};
    /// #
    /// // also safely implements Yokeable<'a>
    /// struct Bar<'a> {
    ///     bytes_1: &'a [u8],
    ///     string_2: &'a str,
    /// }
    ///
    /// fn map_project_string_1(
    ///     bar: Yoke<Bar<'static>, Rc<[u8]>>,
    /// ) -> Result<Yoke<&'static str, Rc<[u8]>>, Utf8Error> {
    ///     bar.try_map_project(|bar, _| str::from_utf8(bar.bytes_1))
    /// }
    ///
    /// #
    /// # unsafe impl<'a> Yokeable<'a> for Bar<'static> {
    /// #     type Output = Bar<'a>;
    /// #     fn transform(&'a self) -> &'a Bar<'a> {
    /// #         self
    /// #     }
    /// #
    /// #     fn transform_owned(self) -> Bar<'a> {
    /// #         // covariant lifetime cast, can be done safely
    /// #         self
    /// #     }
    /// #
    /// #     unsafe fn make(from: Bar<'a>) -> Self {
    /// #         let ret = mem::transmute_copy(&from);
    /// #         mem::forget(from);
    /// #         ret
    /// #     }
    /// #
    /// #     fn transform_mut<F>(&'a mut self, f: F)
    /// #     where
    /// #         F: 'static + FnOnce(&'a mut Self::Output),
    /// #     {
    /// #         unsafe { f(mem::transmute(self)) }
    /// #     }
    /// # }
    /// ```
    pub fn try_map_project<P, F, E>(self, f: F) -> Result<Yoke<P, C>, E>
    where
        P: for<'a> Yokeable<'a>,
        F: for<'a> FnOnce(
            <Y as Yokeable<'a>>::Output,
            PhantomData<&'a ()>,
        ) -> Result<<P as Yokeable<'a>>::Output, E>,
    {
        let p = f(self.yokeable.transform_owned(), PhantomData)?;
        Ok(Yoke {
            yokeable: unsafe { P::make(p) },
            cart: self.cart,
        })
    }

    /// This is similar to [`Yoke::try_map_project`], however it does not move
    /// [`Self`] and instead clones the cart (only if the cart is a [`CloneableCart`])
    ///
    /// This is a bit more efficient than cloning the [`Yoke`] and then calling [`Yoke::map_project`]
    /// because then it will not clone fields that are going to be discarded.
    pub fn try_map_project_cloned<'this, P, F, E>(&'this self, f: F) -> Result<Yoke<P, C>, E>
    where
        P: for<'a> Yokeable<'a>,
        C: CloneableCart,
        F: for<'a> FnOnce(
            &'this <Y as Yokeable<'a>>::Output,
            PhantomData<&'a ()>,
        ) -> Result<<P as Yokeable<'a>>::Output, E>,
    {
        let p = f(self.get(), PhantomData)?;
        Ok(Yoke {
            yokeable: unsafe { P::make(p) },
            cart: self.cart.clone(),
        })
    }
    /// This is similar to [`Yoke::map_project`], but it works around older versions
    /// of Rust not being able to use `FnOnce` by using an explicit capture input.
    /// See [#1061](https://github.com/unicode-org/icu4x/issues/1061).
    ///
    /// See the docs of [`Yoke::map_project`] for how this works.
    pub fn map_project_with_explicit_capture<P, T>(
        self,
        capture: T,
        f: for<'a> fn(
            <Y as Yokeable<'a>>::Output,
            capture: T,
            PhantomData<&'a ()>,
        ) -> <P as Yokeable<'a>>::Output,
    ) -> Yoke<P, C>
    where
        P: for<'a> Yokeable<'a>,
    {
        let p = f(self.yokeable.transform_owned(), capture, PhantomData);
        Yoke {
            yokeable: unsafe { P::make(p) },
            cart: self.cart,
        }
    }

    /// This is similar to [`Yoke::map_project_cloned`], but it works around older versions
    /// of Rust not being able to use `FnOnce` by using an explicit capture input.
    /// See [#1061](https://github.com/unicode-org/icu4x/issues/1061).
    ///
    /// See the docs of [`Yoke::map_project_cloned`] for how this works.
    pub fn map_project_cloned_with_explicit_capture<'this, P, T>(
        &'this self,
        capture: T,
        f: for<'a> fn(
            &'this <Y as Yokeable<'a>>::Output,
            capture: T,
            PhantomData<&'a ()>,
        ) -> <P as Yokeable<'a>>::Output,
    ) -> Yoke<P, C>
    where
        P: for<'a> Yokeable<'a>,
        C: CloneableCart,
    {
        let p = f(self.get(), capture, PhantomData);
        Yoke {
            yokeable: unsafe { P::make(p) },
            cart: self.cart.clone(),
        }
    }

    /// This is similar to [`Yoke::try_map_project`], but it works around older versions
    /// of Rust not being able to use `FnOnce` by using an explicit capture input.
    /// See [#1061](https://github.com/unicode-org/icu4x/issues/1061).
    ///
    /// See the docs of [`Yoke::try_map_project`] for how this works.
    #[allow(clippy::type_complexity)]
    pub fn try_map_project_with_explicit_capture<P, T, E>(
        self,
        capture: T,
        f: for<'a> fn(
            <Y as Yokeable<'a>>::Output,
            capture: T,
            PhantomData<&'a ()>,
        ) -> Result<<P as Yokeable<'a>>::Output, E>,
    ) -> Result<Yoke<P, C>, E>
    where
        P: for<'a> Yokeable<'a>,
    {
        let p = f(self.yokeable.transform_owned(), capture, PhantomData)?;
        Ok(Yoke {
            yokeable: unsafe { P::make(p) },
            cart: self.cart,
        })
    }

    /// This is similar to [`Yoke::try_map_project_cloned`], but it works around older versions
    /// of Rust not being able to use `FnOnce` by using an explicit capture input.
    /// See [#1061](https://github.com/unicode-org/icu4x/issues/1061).
    ///
    /// See the docs of [`Yoke::try_map_project_cloned`] for how this works.
    #[allow(clippy::type_complexity)]
    pub fn try_map_project_cloned_with_explicit_capture<'this, P, T, E>(
        &'this self,
        capture: T,
        f: for<'a> fn(
            &'this <Y as Yokeable<'a>>::Output,
            capture: T,
            PhantomData<&'a ()>,
        ) -> Result<<P as Yokeable<'a>>::Output, E>,
    ) -> Result<Yoke<P, C>, E>
    where
        P: for<'a> Yokeable<'a>,
        C: CloneableCart,
    {
        let p = f(self.get(), capture, PhantomData)?;
        Ok(Yoke {
            yokeable: unsafe { P::make(p) },
            cart: self.cart.clone(),
        })
    }
}

#[cfg(feature = "alloc")]
impl<Y: for<'a> Yokeable<'a>, C: 'static + Sized> Yoke<Y, Rc<C>> {
    /// Allows type-erasing the cart in a `Yoke<Y, Rc<C>>`.
    ///
    /// The yoke only carries around a cart type `C` for its destructor,
    /// since it needs to be able to guarantee that its internal references
    /// are valid for the lifetime of the Yoke. As such, the actual type of the
    /// Cart is not very useful unless you wish to extract data out of it
    /// via [`Yoke::backing_cart()`]. Erasing the cart allows for one to mix
    /// [`Yoke`]s obtained from different sources.
    ///
    /// In case the cart type `C` is not already an `Rc<T>`, you can use
    /// [`Yoke::wrap_cart_in_rc()`] to wrap it.
    ///
    /// # Example
    ///
    /// ```rust
    /// use std::rc::Rc;
    /// use yoke::erased::ErasedRcCart;
    /// use yoke::Yoke;
    ///
    /// let buffer1: Rc<String> = Rc::new("   foo bar baz  ".into());
    /// let buffer2: Box<String> = Box::new("  baz quux  ".into());
    ///
    /// let yoke1 =
    ///     Yoke::<&'static str, _>::attach_to_cart(buffer1, |rc| rc.trim());
    /// let yoke2 = Yoke::<&'static str, _>::attach_to_cart(buffer2, |b| b.trim());
    ///
    /// let erased1: Yoke<_, ErasedRcCart> = yoke1.erase_rc_cart();
    /// // Wrap the Box in an Rc to make it compatible
    /// let erased2: Yoke<_, ErasedRcCart> =
    ///     yoke2.wrap_cart_in_rc().erase_rc_cart();
    ///
    /// // Now erased1 and erased2 have the same type!
    /// ```
    ///
    /// Available with the `"alloc"` Cargo feature enabled.
    pub fn erase_rc_cart(self) -> Yoke<Y, ErasedRcCart> {
        unsafe {
            // safe because the cart is preserved, just
            // type-erased
            self.replace_cart(|c| c as ErasedRcCart)
        }
    }
}

#[cfg(feature = "alloc")]
impl<Y: for<'a> Yokeable<'a>, C: 'static + Sized + Send + Sync> Yoke<Y, Arc<C>> {
    /// Allows type-erasing the cart in a `Yoke<Y, Arc<C>>`.
    ///
    /// The yoke only carries around a cart type `C` for its destructor,
    /// since it needs to be able to guarantee that its internal references
    /// are valid for the lifetime of the Yoke. As such, the actual type of the
    /// Cart is not very useful unless you wish to extract data out of it
    /// via [`Yoke::backing_cart()`]. Erasing the cart allows for one to mix
    /// [`Yoke`]s obtained from different sources.
    ///
    /// In case the cart type `C` is not already an `Arc<T>`, you can use
    /// [`Yoke::wrap_cart_in_arc()`] to wrap it.
    ///
    /// # Example
    ///
    /// ```rust
    /// use std::sync::Arc;
    /// use yoke::erased::ErasedArcCart;
    /// use yoke::Yoke;
    ///
    /// let buffer1: Arc<String> = Arc::new("   foo bar baz  ".into());
    /// let buffer2: Box<String> = Box::new("  baz quux  ".into());
    ///
    /// let yoke1 =
    ///     Yoke::<&'static str, _>::attach_to_cart(buffer1, |arc| arc.trim());
    /// let yoke2 = Yoke::<&'static str, _>::attach_to_cart(buffer2, |b| b.trim());
    ///
    /// let erased1: Yoke<_, ErasedArcCart> = yoke1.erase_arc_cart();
    /// // Wrap the Box in an Rc to make it compatible
    /// let erased2: Yoke<_, ErasedArcCart> =
    ///     yoke2.wrap_cart_in_arc().erase_arc_cart();
    ///
    /// // Now erased1 and erased2 have the same type!
    /// ```
    ///
    /// Available with the `"alloc"` Cargo feature enabled.
    pub fn erase_arc_cart(self) -> Yoke<Y, ErasedArcCart> {
        unsafe {
            // safe because the cart is preserved, just
            // type-erased
            self.replace_cart(|c| c as ErasedArcCart)
        }
    }
}

#[cfg(feature = "alloc")]
impl<Y: for<'a> Yokeable<'a>, C: 'static + Sized> Yoke<Y, Box<C>> {
    /// Allows type-erasing the cart in a `Yoke<Y, Box<C>>`.
    ///
    /// The yoke only carries around a cart type `C` for its destructor,
    /// since it needs to be able to guarantee that its internal references
    /// are valid for the lifetime of the Yoke. As such, the actual type of the
    /// Cart is not very useful unless you wish to extract data out of it
    /// via [`Yoke::backing_cart()`]. Erasing the cart allows for one to mix
    /// [`Yoke`]s obtained from different sources.
    ///
    /// In case the cart type `C` is not already `Box<T>`, you can use
    /// [`Yoke::wrap_cart_in_box()`] to wrap it.
    ///
    /// # Example
    ///
    /// ```rust
    /// use std::rc::Rc;
    /// use yoke::erased::ErasedBoxCart;
    /// use yoke::Yoke;
    ///
    /// let buffer1: Rc<String> = Rc::new("   foo bar baz  ".into());
    /// let buffer2: Box<String> = Box::new("  baz quux  ".into());
    ///
    /// let yoke1 =
    ///     Yoke::<&'static str, _>::attach_to_cart(buffer1, |rc| rc.trim());
    /// let yoke2 = Yoke::<&'static str, _>::attach_to_cart(buffer2, |b| b.trim());
    ///
    /// // Wrap the Rc in an Box to make it compatible
    /// let erased1: Yoke<_, ErasedBoxCart> =
    ///     yoke1.wrap_cart_in_box().erase_box_cart();
    /// let erased2: Yoke<_, ErasedBoxCart> = yoke2.erase_box_cart();
    ///
    /// // Now erased1 and erased2 have the same type!
    /// ```
    ///
    /// Available with the `"alloc"` Cargo feature enabled.
    pub fn erase_box_cart(self) -> Yoke<Y, ErasedBoxCart> {
        unsafe {
            // safe because the cart is preserved, just
            // type-erased
            self.replace_cart(|c| c as ErasedBoxCart)
        }
    }
}

#[cfg(feature = "alloc")]
impl<Y: for<'a> Yokeable<'a>, C> Yoke<Y, C> {
    /// Helper function allowing one to wrap the cart type `C` in a `Box<T>`.
    /// Can be paired with [`Yoke::erase_box_cart()`]
    ///
    /// Available with the `"alloc"` Cargo feature enabled.
    #[inline]
    pub fn wrap_cart_in_box(self) -> Yoke<Y, Box<C>> {
        unsafe {
            // safe because the cart is preserved, just wrapped
            self.replace_cart(Box::new)
        }
    }
    /// Helper function allowing one to wrap the cart type `C` in an `Rc<T>`.
    /// Can be paired with [`Yoke::erase_rc_cart()`], or generally used
    /// to make the [`Yoke`] cloneable.
    ///
    /// Available with the `"alloc"` Cargo feature enabled.
    #[inline]
    pub fn wrap_cart_in_rc(self) -> Yoke<Y, Rc<C>> {
        unsafe {
            // safe because the cart is preserved, just wrapped
            self.replace_cart(Rc::new)
        }
    }
    /// Helper function allowing one to wrap the cart type `C` in an `Rc<T>`.
    /// Can be paired with [`Yoke::erase_arc_cart()`], or generally used
    /// to make the [`Yoke`] cloneable.
    ///
    /// Available with the `"alloc"` Cargo feature enabled.
    #[inline]
    pub fn wrap_cart_in_arc(self) -> Yoke<Y, Arc<C>> {
        unsafe {
            // safe because the cart is preserved, just wrapped
            self.replace_cart(Arc::new)
        }
    }
}

impl<Y: for<'a> Yokeable<'a>, C> Yoke<Y, C> {
    /// Helper function allowing one to wrap the cart type `C` in an [`EitherCart`].
    ///
    /// This function wraps the cart into the `A` variant. To wrap it into the
    /// `B` variant, use [`Self::wrap_cart_in_either_b()`].
    ///
    /// For an example, see [`EitherCart`].
    #[inline]
    pub fn wrap_cart_in_either_a<B>(self) -> Yoke<Y, EitherCart<C, B>> {
        unsafe {
            // safe because the cart is preserved, just wrapped
            self.replace_cart(EitherCart::A)
        }
    }
    /// Helper function allowing one to wrap the cart type `C` in an [`EitherCart`].
    ///
    /// This function wraps the cart into the `B` variant. To wrap it into the
    /// `A` variant, use [`Self::wrap_cart_in_either_a()`].
    ///
    /// For an example, see [`EitherCart`].
    #[inline]
    pub fn wrap_cart_in_either_b<A>(self) -> Yoke<Y, EitherCart<A, C>> {
        unsafe {
            // safe because the cart is preserved, just wrapped
            self.replace_cart(EitherCart::B)
        }
    }
}

/// # Safety docs for project()
///
/// (Docs are on a private const to allow the use of compile_fail doctests)
///
/// This is safe to perform because of the choice of lifetimes on `f`, that is,
/// `for<a> fn(<Y as Yokeable<'a>>::Output, &'a ()) -> <P as Yokeable<'a>>::Output`.
///
/// What we want this function to do is take a Yokeable (`Y`) that is borrowing from the cart, and
/// produce another Yokeable (`P`) that also borrows from the same cart. There are a couple potential
/// hazards here:
///
/// - `P` ends up borrowing data from `Y` (or elsewhere) that did _not_ come from the cart,
///   for example `P` could borrow owned data from a `Cow`. This would make the `Yoke<P>` dependent
///   on data owned only by the `Yoke<Y>`.
/// - Borrowed data from `Y` escapes with the wrong lifetime
///
/// Let's walk through these and see how they're prevented.
///
/// ```rust, compile_fail
/// # use std::rc::Rc;
/// # use yoke::Yoke;
/// # use std::borrow::Cow;
/// fn borrow_potentially_owned(y: &Yoke<Cow<'static, str>, Rc<[u8]>>) -> Yoke<&'static str, Rc<[u8]>> {
///    y.map_project_cloned(|cow, _| &*cow)   
/// }
/// ```
///
/// In this case, the lifetime of `&*cow` is `&'this str`, however the function needs to be able to return
/// `&'a str` _for all `'a`_, which isn't possible.
///
///
/// ```rust, compile_fail
/// # use std::rc::Rc;
/// # use yoke::Yoke;
/// # use std::borrow::Cow;
/// fn borrow_potentially_owned(y: Yoke<Cow<'static, str>, Rc<[u8]>>) -> Yoke<&'static str, Rc<[u8]>> {
///    y.map_project(|cow, _| &*cow)   
/// }
/// ```
///
/// This has the same issue, `&*cow` is borrowing for a local lifetime.
///
/// Similarly, trying to project an owned field of a struct will produce similar errors:
///
/// ```rust,compile_fail
/// # use std::borrow::Cow;
/// # use yoke::{Yoke, Yokeable};
/// # use std::mem;
/// # use std::rc::Rc;
/// #
/// // also safely implements Yokeable<'a>
/// struct Bar<'a> {
///     owned: String,
///     string_2: &'a str,
/// }
///
/// fn map_project_owned(bar: &Yoke<Bar<'static>, Rc<[u8]>>) -> Yoke<&'static str, Rc<[u8]>> {
///     // ERROR (but works if you replace owned with string_2)
///     bar.map_project_cloned(|bar, _| &*bar.owned)   
/// }
///
/// #
/// # unsafe impl<'a> Yokeable<'a> for Bar<'static> {
/// #     type Output = Bar<'a>;
/// #     fn transform(&'a self) -> &'a Bar<'a> {
/// #         self
/// #     }
/// #
/// #     fn transform_owned(self) -> Bar<'a> {
/// #         // covariant lifetime cast, can be done safely
/// #         self
/// #     }
/// #
/// #     unsafe fn make(from: Bar<'a>) -> Self {
/// #         let ret = mem::transmute_copy(&from);
/// #         mem::forget(from);
/// #         ret
/// #     }
/// #
/// #     fn transform_mut<F>(&'a mut self, f: F)
/// #     where
/// #         F: 'static + FnOnce(&'a mut Self::Output),
/// #     {
/// #         unsafe { f(mem::transmute(self)) }
/// #     }
/// # }
/// ```
///
/// Borrowed data from `Y` similarly cannot escape with the wrong lifetime because of the `for<'a>`, since
/// it will never be valid for the borrowed data to escape for all lifetimes of 'a. Internally, `.project()`
/// uses `.get()`, however the signature forces the callers to be able to handle every lifetime.
///
///  `'a` is the only lifetime that matters here; `Yokeable`s must be `'static` and since
/// `Output` is an associated type it can only have one lifetime, `'a` (there's nowhere for it to get another from).
/// `Yoke`s can get additional lifetimes via the cart, and indeed, `project()` can operate on `Yoke<_, &'b [u8]>`,
/// however this lifetime is inaccessible to the closure, and even if it were accessible the `for<'a>` would force
/// it out of the output. All external lifetimes (from other found outside the yoke/closures
/// are similarly constrained here.
///
/// Essentially, safety is achieved by using `for<'a> fn(...)` with `'a` used in both `Yokeable`s to ensure that
/// the output yokeable can _only_ have borrowed data flow in to it from the input. All paths of unsoundness require the
/// unification of an existential and universal lifetime, which isn't possible.
const _: () = ();

/// # Safety docs for attach_to_cart()'s signature
///
/// The `attach_to_cart()` family of methods get by by using the following bound:
///
/// ```rust,ignore
/// F: for<'de> FnOnce(&'de <C as Deref>::Target) -> <Y as Yokeable<'de>>::Output,
/// C::Target: 'static
/// ```
///
/// to enforce that the yoking closure produces a yokeable that is *only* allowed to borrow from the cart.
/// A way to be sure of this is as follows: imagine if `F` *did* borrow data of lifetime `'a` and stuff it in
/// its output. Then that lifetime `'a` would have to live at least as long as `'de` *for all `'de`*.
/// The only lifetime that satisfies that is `'static` (since at least one of the potential `'de`s is `'static`),
/// and we're fine with that.
///
/// ## Implied bounds and variance
///
/// The `C::Target: 'static` bound is tricky, however. Let's imagine a situation where we *didn't* have that bound.
///
/// One thing to remember is that we are okay with the cart itself borrowing from places,
/// e.g. `&[u8]` is a valid cart, as is `Box<&[u8]>`. `C` is not `'static`.
///
/// (I'm going to use `CT` in prose to refer to `C::Target` here, since almost everything here has to do
/// with C::Target and not C itself.)
///
/// Unfortunately, there's a sneaky additional bound inside `F`. The signature of `F` is *actually*
///
/// ```rust,ignore
/// F: for<'de> where<C::Target: 'de> FnOnce(&'de C::Target) -> <Y as Yokeable<'de>>::Output
/// ```
///
/// using made-up "where clause inside HRTB" syntax to represent a type that can be represented inside the compiler
/// and type system but not in Rust code. The `CT: 'de` bond comes from the `&'de C::Target`: any time you
/// write `&'a T`, an implied bound of `T: 'a` materializes and is stored alongside it, since references cannot refer
/// to data that itself refers to data of shorter lifetimes. If a reference is valid, its referent must be valid for
/// the duration of the reference's lifetime, so every reference *inside* its referent must also be valid, giving us `T: 'a`.
/// This kind of constraint is often called a "well formedness" constraint: `&'a T` is not "well formed" without that
/// bound, and rustc is being helpful by giving it to us for free.
///
/// Unfortunately, this messes with our universal quantification. The `for<'de>` is no longer "For all lifetimes `'de`",
/// it is "for all lifetimes `'de` *where `CT: 'de`*". And if `CT` borrows from somewhere (with lifetime `'ct`), then we get a
/// `'ct: 'de` bound, and `'de` candidates that live longer than `'ct` won't actually be considered.
/// The neat little logic at the beginning stops working.
///
/// `attach_to_cart()` will instead enforce that the produced yokeable *either* borrows from the cart (fine), or from
/// data that has a lifetime that is at least `'ct`. Which means that `attach_to_cart()` will allow us to borrow locals
/// provided they live at least as long as `'ct`.
///
/// Is this a problem?
///
/// This is totally fine if CT's lifetime is covariant: if C is something like `Box<&'ct [u8]>`, even if our
/// yoked object borrows from locals outliving `'ct`, our Yoke can't outlive that
/// lifetime `'ct` anyway (since it's a part of the cart type), so we're fine.
///
/// However it's completely broken for contravariant carts (e.g. `Box<fn(&'ct u8)>`). In that case
/// we still get `'ct: 'de`, and we still end up being able to
/// borrow from locals that outlive `'ct`. However, our Yoke _can_ outlive
/// that lifetime, because Yoke shares its variance over `'ct`
/// with the cart type, and the cart type is contravariant over `'ct`.
/// So the Yoke can be upcast to having a longer lifetime than `'ct`, and *that* Yoke
/// can outlive `'ct`.
///
/// We fix this by forcing `C::Target: 'static` in `attach_to_cart()`, which would make it work
/// for fewer types, but would also allow Yoke to continue to be covariant over cart lifetimes if necessary.
///
/// An alternate fix would be to not allowing yoke to ever be upcast over lifetimes contained in the cart
/// by forcing them to be invariant. This is a bit more restrictive and affects *all* `Yoke` users, not just
/// those using `attach_to_cart()`.
///
/// See https://github.com/unicode-org/icu4x/issues/2926
/// See also https://github.com/rust-lang/rust/issues/106431 for potentially fixing this upstream by
/// changing how the bound works.
///
/// # Tests
///
/// Here's a broken `attach_to_cart()` that attempts to borrow from a local:
///
/// ```rust,compile_fail
/// use yoke::{Yoke, Yokeable};
///
/// let cart = vec![1, 2, 3, 4].into_boxed_slice();
/// let local = vec![4, 5, 6, 7];
/// let yoke: Yoke<&[u8], Box<[u8]>> = Yoke::attach_to_cart(cart, |_| &*local);
/// ```
///
/// Fails as expected.
///
/// And here's a working one with a local borrowed cart that does not do any sneaky borrows whilst attaching.
///
/// ```rust
/// use yoke::{Yoke, Yokeable};
///
/// let cart = vec![1, 2, 3, 4].into_boxed_slice();
/// let local = vec![4, 5, 6, 7];
/// let yoke: Yoke<&[u8], &[u8]> = Yoke::attach_to_cart(&cart, |c| &*c);
/// ```
///
/// Here's an `attach_to_cart()` that attempts to borrow from a longer-lived local due to
/// the cart being covariant. It fails, but would not if the alternate fix of forcing Yoke to be invariant
/// were implemented. It is technically a safe operation:
///
/// ```rust,compile_fail
/// use yoke::{Yoke, Yokeable};
/// // longer lived
/// let local = vec![4, 5, 6, 7];
///
/// let backing = vec![1, 2, 3, 4];
/// let cart = Box::new(&*backing);
///
/// let yoke: Yoke<&[u8], Box<&[u8]>> = Yoke::attach_to_cart(cart, |_| &*local);
/// println!("{:?}", yoke.get());
/// ```
///
/// Finally, here's an `attach_to_cart()` that attempts to borrow from a longer lived local
/// in the case of a contravariant lifetime. It does not compile, but in and of itself is not dangerous:
///
/// ```rust,compile_fail
/// use yoke::Yoke;
///
/// type Contra<'a> = fn(&'a ());
///
/// let local = String::from("Hello World!");
/// let yoke: Yoke<&'static str, Box<Contra<'_>>> = Yoke::attach_to_cart(Box::new((|_| {}) as _), |_| &local[..]);
/// println!("{:?}", yoke.get());
/// ```
///
/// It is dangerous if allowed to transform (testcase from #2926)
///
/// ```rust,compile_fail
/// use yoke::Yoke;
///
/// type Contra<'a> = fn(&'a ());
///
///
/// let local = String::from("Hello World!");
/// let yoke: Yoke<&'static str, Box<Contra<'_>>> = Yoke::attach_to_cart(Box::new((|_| {}) as _), |_| &local[..]);
/// println!("{:?}", yoke.get());
/// let yoke_longer: Yoke<&'static str, Box<Contra<'static>>> = yoke;
/// let leaked: &'static Yoke<&'static str, Box<Contra<'static>>> = Box::leak(Box::new(yoke_longer));
/// let reference: &'static str = leaked.get();
///
/// println!("pre-drop: {reference}");
/// drop(local);
/// println!("post-drop: {reference}");
/// ```
const _: () = ();
