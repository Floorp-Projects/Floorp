#![allow(dead_code)]

use {Signature, Message, arg::TypeMismatchError};
use std::{fmt, any};
use std::sync::Arc;
use std::rc::Rc;

use super::{Iter, IterAppend, ArgType};

/// Types that can represent a D-Bus message argument implement this trait.
///
/// Types should also implement either Append or Get to be useful. 
pub trait Arg {
    /// The corresponding D-Bus argument type code. 
    const ARG_TYPE: ArgType;
    /// The corresponding D-Bus argument type code; just returns ARG_TYPE. 
    ///
    /// For backwards compatibility.
    #[deprecated(note = "Use associated constant ARG_TYPE instead")]
    fn arg_type() -> ArgType { return Self::ARG_TYPE; }
    /// The corresponding D-Bus type signature for this type. 
    fn signature() -> Signature<'static>;
}

/// Types that can be appended to a message as arguments implement this trait.
pub trait Append: Sized {
    /// Performs the append operation.
    fn append(self, &mut IterAppend);
}

/// Helper trait to append many arguments to a message.
pub trait AppendAll: Sized {
    /// Performs the append operation.
    fn append(self, &mut IterAppend);
}

/// Types that can be retrieved from a message as arguments implement this trait.
pub trait Get<'a>: Sized {
    /// Performs the get operation.
    fn get(i: &mut Iter<'a>) -> Option<Self>;
}

/// Helper trait to read all arguments from a message.
pub trait ReadAll: Sized {
    /// Performs the read operation.
    fn read(i: &mut Iter) -> Result<Self, TypeMismatchError>;
}


/// Object safe version of Arg + Append + Get.
pub trait RefArg: fmt::Debug {
    /// The corresponding D-Bus argument type code.
    fn arg_type(&self) -> ArgType;
    /// The corresponding D-Bus type signature for this type. 
    fn signature(&self) -> Signature<'static>;
    /// Performs the append operation.
    fn append(&self, &mut IterAppend);
    /// Transforms this argument to Any (which can be downcasted to read the current value).
    ///
    /// Note: The internal representation of complex types (Array, Dict, Struct) is unstable
    /// and as_any should not be relied upon for these types. Use as_iter instead.
    fn as_any(&self) -> &any::Any where Self: 'static;
    /// Transforms this argument to Any (which can be downcasted to read the current value).
    ///
    /// Note: The internal representation of complex types (Array, Dict, Struct) is unstable
    /// and as_any should not be relied upon for these types. Use as_iter instead.
    ///
    /// # Panic
    /// Will panic if the interior cannot be made mutable, e g, if encapsulated
    /// inside a Rc with a reference count > 1.
    fn as_any_mut(&mut self) -> &mut any::Any where Self: 'static;
    /// Try to read the argument as an i64.
    ///
    /// Works for: Boolean, Byte, Int16, UInt16, Int32, UInt32, Int64, UnixFd.
    #[inline]
    fn as_i64(&self) -> Option<i64> { None }
    /// Try to read the argument as an u64.
    ///
    /// Works for: Boolean, Byte, Int16, UInt16, Int32, UInt32, UInt64.
    #[inline]
    fn as_u64(&self) -> Option<u64> { None }
    /// Try to read the argument as an f64.
    ///
    /// Works for: Boolean, Byte, Int16, UInt16, Int32, UInt32, Double.
    #[inline]
    fn as_f64(&self) -> Option<f64> { None }
    /// Try to read the argument as a str.
    ///
    /// Works for: String, ObjectPath, Signature.
    #[inline]
    fn as_str(&self) -> Option<&str> { None }
    /// Try to read the argument as an iterator.
    ///
    /// Works for: Array/Dict, Struct, Variant.
    #[inline]
    fn as_iter<'a>(&'a self) -> Option<Box<Iterator<Item=&'a RefArg> + 'a>> { None }
    /// Deep clone of the RefArg, causing the result to be 'static.
    ///
    /// Usable as an escape hatch in case of lifetime problems with RefArg.
    ///
    /// In case of complex types (Array, Dict, Struct), the clone is not guaranteed
    /// to have the same internal representation as the original.
    fn box_clone(&self) -> Box<RefArg + 'static> { unimplemented!() /* Needed for backwards comp */ }
}

impl<'a> Get<'a> for Box<RefArg> {
    fn get(i: &mut Iter<'a>) -> Option<Self> { i.get_refarg() }
}

/// Cast a RefArg as a specific type (shortcut for any + downcast)
#[inline]
pub fn cast<'a, T: 'static>(a: &'a (RefArg + 'static)) -> Option<&'a T> { a.as_any().downcast_ref() }

/// Cast a RefArg as a specific type (shortcut for any_mut + downcast_mut)
///
/// # Panic
/// Will panic if the interior cannot be made mutable, e g, if encapsulated
/// inside a Rc with a reference count > 1.
#[inline]
pub fn cast_mut<'a, T: 'static>(a: &'a mut (RefArg + 'static)) -> Option<&'a mut T> { a.as_any_mut().downcast_mut() }

/// If a type implements this trait, it means the size and alignment is the same
/// as in D-Bus. This means that you can quickly append and get slices of this type.
///
/// Note: Booleans do not implement this trait because D-Bus booleans are 4 bytes and Rust booleans are 1 byte.
pub unsafe trait FixedArray: Arg + 'static + Clone + Copy {}

/// Types that can be used as keys in a dict type implement this trait. 
pub trait DictKey: Arg {}



/// Simple lift over reference to value - this makes some iterators more ergonomic to use
impl<'a, T: Arg> Arg for &'a T {
    const ARG_TYPE: ArgType = T::ARG_TYPE;
    fn signature() -> Signature<'static> { T::signature() }
}
impl<'a, T: Append + Clone> Append for &'a T {
    fn append(self, i: &mut IterAppend) { self.clone().append(i) }
}
impl<'a, T: DictKey> DictKey for &'a T {}

impl<'a, T: RefArg + ?Sized> RefArg for &'a T {
    #[inline]
    fn arg_type(&self) -> ArgType { (&**self).arg_type() }
    #[inline]
    fn signature(&self) -> Signature<'static> { (&**self).signature() }
    #[inline]
    fn append(&self, i: &mut IterAppend) { (&**self).append(i) }
    #[inline]
    fn as_any(&self) -> &any::Any where T: 'static { (&**self).as_any() }
    #[inline]
    fn as_any_mut(&mut self) -> &mut any::Any where T: 'static { unreachable!() }
    #[inline]
    fn as_i64(&self) -> Option<i64> { (&**self).as_i64() }
    #[inline]
    fn as_u64(&self) -> Option<u64> { (&**self).as_u64() }
    #[inline]
    fn as_f64(&self) -> Option<f64> { (&**self).as_f64() }
    #[inline]
    fn as_str(&self) -> Option<&str> { (&**self).as_str() }
    #[inline]
    fn as_iter<'b>(&'b self) -> Option<Box<Iterator<Item=&'b RefArg> + 'b>> { (&**self).as_iter() }
    #[inline]
    fn box_clone(&self) -> Box<RefArg + 'static> { (&**self).box_clone() }
}



macro_rules! deref_impl {
    ($t: ident, $ss: ident, $make_mut: expr) => {

impl<T: RefArg + ?Sized> RefArg for $t<T> {
    #[inline]
    fn arg_type(&self) -> ArgType { (&**self).arg_type() }
    #[inline]
    fn signature(&self) -> Signature<'static> { (&**self).signature() }
    #[inline]
    fn append(&self, i: &mut IterAppend) { (&**self).append(i) }
    #[inline]
    fn as_any(&self) -> &any::Any where T: 'static { (&**self).as_any() }
    #[inline]
    fn as_any_mut<'a>(&'a mut $ss) -> &'a mut any::Any where T: 'static { $make_mut.as_any_mut() }
    #[inline]
    fn as_i64(&self) -> Option<i64> { (&**self).as_i64() }
    #[inline]
    fn as_u64(&self) -> Option<u64> { (&**self).as_u64() }
    #[inline]
    fn as_f64(&self) -> Option<f64> { (&**self).as_f64() }
    #[inline]
    fn as_str(&self) -> Option<&str> { (&**self).as_str() }
    #[inline]
    fn as_iter<'a>(&'a self) -> Option<Box<Iterator<Item=&'a RefArg> + 'a>> { (&**self).as_iter() }
    #[inline]
    fn box_clone(&self) -> Box<RefArg + 'static> { (&**self).box_clone() }
}
impl<T: DictKey> DictKey for $t<T> {}

impl<T: Arg> Arg for $t<T> {
    const ARG_TYPE: ArgType = T::ARG_TYPE;
    fn signature() -> Signature<'static> { T::signature() }
}
impl<'a, T: Get<'a>> Get<'a> for $t<T> {
    fn get(i: &mut Iter<'a>) -> Option<Self> { T::get(i).map(|v| $t::new(v)) }
}

    }
}

impl<T: Append> Append for Box<T> {
    fn append(self, i: &mut IterAppend) { let q: T = *self; q.append(i) }
}

deref_impl!(Box, self, &mut **self );
deref_impl!(Rc, self, Rc::get_mut(self).unwrap());
deref_impl!(Arc, self, Arc::get_mut(self).unwrap());

/// Internal trait to help generics. Implemented for (), (A1), (A1, A2) and so on (where A1: Arg, A2: Arg etc).
///
/// You would probably not use this trait directly, instead use generic functions which
/// take ArgBuilder as an argument. It helps reading and appending multiple arguments
/// to/from a message in one go.
pub trait ArgBuilder: Sized {
    /// A tuple of &static str. Used for introspection.
    type strs;
    /// Low-level introspection helper method.
    fn strs_sig<F: FnMut(&'static str, Signature<'static>)>(a: Self::strs, f: F);
    /// Low-level method to read arguments from a message.
    fn read(msg: &Message) -> Result<Self, TypeMismatchError>;
    /// Low-level method to append arguments to a message.
    fn append(self, msg: &mut Message);
}

impl ArgBuilder for () {
    type strs = ();
    fn strs_sig<F: FnMut(&'static str, Signature<'static>)>(_: Self::strs, _: F) {}
    fn read(_: &Message) -> Result<Self, TypeMismatchError> { Ok(()) }
    fn append(self, _: &mut Message) {}
}

macro_rules! argbuilder_impl {
    ($($n: ident $t: ident $s: ty,)+) => {

impl<$($t: Arg + Append + for<'z> Get<'z>),*> ArgBuilder for ($($t,)*) {
    type strs = ($(&'static $s,)*); 
    fn strs_sig<Q: FnMut(&'static str, Signature<'static>)>(z: Self::strs, mut q: Q) {
        let ( $($n,)*) = z;
        $( q($n, $t::signature()); )*
    }

    fn read(msg: &Message) -> Result<Self, TypeMismatchError> {
        let mut ii = msg.iter_init();
        $( let $n = ii.read()?; )*
        Ok(($( $n, )* ))
    }

    fn append(self, msg: &mut Message) {
        let ( $($n,)*) = self;
        let mut ia = IterAppend::new(msg);
        $( ia.append($n); )*
    }
}

impl<$($t: Append),*> AppendAll for ($($t,)*) {
    fn append(self, ia: &mut IterAppend) {
        let ( $($n,)*) = self;
        $( ia.append($n); )*
    }
}

impl<$($t: Arg + for<'z> Get<'z>),*> ReadAll for ($($t,)*) {
    fn read(ii: &mut Iter) -> Result<Self, TypeMismatchError> {
        $( let $n = ii.read()?; )*
        Ok(($( $n, )* ))
    }
}


    }
}

argbuilder_impl!(a A str,);
argbuilder_impl!(a A str, b B str,);
argbuilder_impl!(a A str, b B str, c C str,);
argbuilder_impl!(a A str, b B str, c C str, d D str,);
argbuilder_impl!(a A str, b B str, c C str, d D str, e E str,);
argbuilder_impl!(a A str, b B str, c C str, d D str, e E str, f F str,);
argbuilder_impl!(a A str, b B str, c C str, d D str, e E str, f F str, g G str,);
argbuilder_impl!(a A str, b B str, c C str, d D str, e E str, f F str, g G str, h H str,);
argbuilder_impl!(a A str, b B str, c C str, d D str, e E str, f F str, g G str, h H str, i I str,);
argbuilder_impl!(a A str, b B str, c C str, d D str, e E str, f F str, g G str, h H str, i I str, j J str,);



#[cfg(test)]
mod test {
    extern crate tempdir;

    use {Connection, ConnectionItem, Message, BusType, Path, Signature};
    use arg::{Array, Variant, Dict, Iter, ArgType, TypeMismatchError, RefArg, cast};

    use std::collections::HashMap;

    #[test]
    fn refarg() {
        let c = Connection::get_private(BusType::Session).unwrap();
        c.register_object_path("/mooh").unwrap();
        let m = Message::new_method_call(&c.unique_name(), "/mooh", "com.example.hello", "Hello").unwrap();

        let mut vv: Vec<Variant<Box<RefArg>>> = vec!();
        vv.push(Variant(Box::new(5i32)));
        vv.push(Variant(Box::new(String::from("Hello world"))));
        let m = m.append_ref(&vv);

        let (f1, f2) = (false, 7u64);
        let mut v: Vec<&RefArg> = vec!();
        v.push(&f1);
        v.push(&f2);
        let m = m.append_ref(&v);
        let vi32 = vec![7i32, 9i32];
        let vstr: Vec<String> = ["This", "is", "dbus", "rs"].iter().map(|&s| s.into()).collect();
        let m = m.append_ref(&[&vi32 as &RefArg, &vstr as &RefArg]);
        let mut map = HashMap::new();
        map.insert(true, String::from("Yes"));
        map.insert(false, String::from("No"));
        let m = m.append_ref(&[&map as &RefArg, &1.5f64 as &RefArg]);

        c.send(m).unwrap();

        for n in c.iter(1000) {
            if let ConnectionItem::MethodCall(m) = n {
                let rv: Vec<Box<RefArg + 'static>> = m.iter_init().collect();
                println!("Receiving {:?}", rv);
                let rv0: &Variant<Box<RefArg>> = cast(&rv[0]).unwrap(); 
                let rv00: &i32 = cast(&rv0.0).unwrap();
                assert_eq!(rv00, &5i32);
                assert_eq!(Some(&false), rv[2].as_any().downcast_ref::<bool>());
                assert_eq!(Some(&vi32), rv[4].as_any().downcast_ref::<Vec<i32>>());
                assert_eq!(Some(&vstr), rv[5].as_any().downcast_ref::<Vec<String>>());
                let mut diter = rv[6].as_iter().unwrap();
                {
                    let mut mmap: HashMap<bool, String> = HashMap::new();
                    while let Some(k) = diter.next() {
                        let x: String = diter.next().unwrap().as_str().unwrap().into();
                        mmap.insert(*cast::<bool>(&k.box_clone()).unwrap(), x);
                    }
                    assert_eq!(mmap[&true], "Yes");
                }
                let mut iter = rv[6].as_iter().unwrap();
                assert!(iter.next().unwrap().as_i64().is_some());
                assert!(iter.next().unwrap().as_str().is_some());
                assert!(iter.next().unwrap().as_str().is_none());
                assert!(iter.next().unwrap().as_i64().is_none());
                assert!(iter.next().is_none());
                assert!(rv[7].as_f64().unwrap() > 1.0);
                assert!(rv[7].as_f64().unwrap() < 2.0);
                break;
            }
        }
    }

    #[test]
    fn message_types() {
        let c = Connection::get_private(BusType::Session).unwrap();
        c.register_object_path("/hello").unwrap();
        let m = Message::new_method_call(&c.unique_name(), "/hello", "com.example.hello", "Hello").unwrap();
        let m = m.append1(2000u16);
        let m = m.append1(Array::new(&vec![129u8, 5, 254]));
        let m = m.append2(Variant(&["Hello", "world"][..]), &[32768u16, 16u16, 12u16][..]);
        let m = m.append3(-1i32, &*format!("Hello world"), -3.14f64);
        let m = m.append1((256i16, Variant(18_446_744_073_709_551_615u64)));
        let m = m.append2(Path::new("/a/valid/path").unwrap(), Signature::new("a{sv}").unwrap());
        let mut z = HashMap::new();
        z.insert(123543u32, true);
        z.insert(0u32, false);
        let m = m.append1(Dict::new(&z));
        let sending = format!("{:?}", m.iter_init());
        println!("Sending {}", sending);
        c.send(m).unwrap();

        for n in c.iter(1000) {
            match n {
                ConnectionItem::MethodCall(m) => {
                    use super::Arg;
                    let receiving = format!("{:?}", m.iter_init());
                    println!("Receiving {}", receiving);
                    assert_eq!(sending, receiving);

                    assert_eq!(2000u16, m.get1().unwrap());
                    assert_eq!(m.get2(), (Some(2000u16), Some(&[129u8, 5, 254][..])));
                    assert_eq!(m.read2::<u16, bool>().unwrap_err(),
                        TypeMismatchError { position: 1, found: ArgType::Array, expected: ArgType::Boolean });

                    let mut g = m.iter_init();
                    let e = g.read::<u32>().unwrap_err();
                    assert_eq!(e.pos(), 0);
                    assert_eq!(e.expected_arg_type(), ArgType::UInt32);
                    assert_eq!(e.found_arg_type(), ArgType::UInt16);

                    assert!(g.next() && g.next());
                    let v: Variant<Iter> = g.get().unwrap();
                    let mut viter = v.0;
                    assert_eq!(viter.arg_type(), Array::<&str,()>::ARG_TYPE);
                    let a: Array<&str, _> = viter.get().unwrap();
                    assert_eq!(a.collect::<Vec<&str>>(), vec!["Hello", "world"]);

                    assert!(g.next());
                    assert_eq!(g.get::<u16>(), None); // It's an array, not a single u16
                    assert!(g.next() && g.next() && g.next() && g.next());

                    assert_eq!(g.get(), Some((256i16, Variant(18_446_744_073_709_551_615u64))));
                    assert!(g.next());
                    assert_eq!(g.get(), Some(Path::new("/a/valid/path").unwrap()));
                    assert!(g.next());
                    assert_eq!(g.get(), Some(Signature::new("a{sv}").unwrap()));
                    assert!(g.next());
                    let d: Dict<u32, bool, _> = g.get().unwrap();
                    let z2: HashMap<_, _> = d.collect();
                    assert_eq!(z, z2);
                    break;
                }
                _ => println!("Got {:?}", n),
            }
        }
    }
}
