use super::*;
use {message, Signature};
use std::any;

#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq, Ord, PartialOrd)]
/// A simple wrapper to specify a D-Bus variant.
///
/// See the argument guide and module level documentation for details and examples.
pub struct Variant<T>(pub T);

impl Variant<Box<RefArg>> {
    /// Creates a new refarg from an Iter. Mainly for internal use.
    pub fn new_refarg<'a>(i: &mut Iter<'a>) -> Option<Self> {
        i.recurse(ArgType::Variant).and_then(|mut si| si.get_refarg()).map(|v| Variant(v))
    }
}

impl Default for Variant<Box<RefArg>> {
    // This is a bit silly, because there is no such thing as a default argument.
    // Unfortunately due to a design mistake while making the SignalArgs trait, we'll
    // have to work around that by adding a default implementation here.
    // https://github.com/diwic/dbus-rs/issues/136
    fn default() -> Self { Variant(Box::new(0u8) as Box<RefArg>) }
}

impl<T:Default> Default for Variant<T> {
    fn default() -> Self { Variant(T::default()) }
}


impl<T> Arg for Variant<T> {
    const ARG_TYPE: ArgType = ArgType::Variant;
    fn signature() -> Signature<'static> { unsafe { Signature::from_slice_unchecked(b"v\0") } }
}

impl<T: Arg + Append> Append for Variant<T> {
    fn append(self, i: &mut IterAppend) {
        let z = self.0;
        i.append_container(ArgType::Variant, Some(T::signature().as_cstr()), |s| z.append(s));
    }
}

impl Append for Variant<message::MessageItem> {
    fn append(self, i: &mut IterAppend) {
        let z = self.0;
        let asig = z.signature();
        let sig = asig.as_cstr();
        i.append_container(ArgType::Variant, Some(&sig), |s| z.append(s));
    }
}

impl Append for Variant<Box<RefArg>> {
    fn append(self, i: &mut IterAppend) {
        let z = self.0;
        i.append_container(ArgType::Variant, Some(z.signature().as_cstr()), |s| z.append(s));
    }
}

impl<'a, T: Get<'a>> Get<'a> for Variant<T> {
    fn get(i: &mut Iter<'a>) -> Option<Variant<T>> {
        i.recurse(ArgType::Variant).and_then(|mut si| si.get().map(|v| Variant(v)))
    }
}

impl<'a> Get<'a> for Variant<Iter<'a>> {
    fn get(i: &mut Iter<'a>) -> Option<Variant<Iter<'a>>> {
        i.recurse(ArgType::Variant).map(|v| Variant(v))
    }
}
/*
impl<'a> Get<'a> for Variant<Box<RefArg>> {
    fn get(i: &mut Iter<'a>) -> Option<Variant<Box<RefArg>>> {
        i.recurse(ArgType::Variant).and_then(|mut si| si.get_refarg().map(|v| Variant(v)))
    }
}
*/
impl<T: RefArg> RefArg for Variant<T> {
    fn arg_type(&self) -> ArgType { ArgType::Variant } 
    fn signature(&self) -> Signature<'static> { unsafe { Signature::from_slice_unchecked(b"v\0") } }
    fn append(&self, i: &mut IterAppend) {
        let z = &self.0;
        i.append_container(ArgType::Variant, Some(z.signature().as_cstr()), |s| z.append(s));
    }
    #[inline]
    fn as_any(&self) -> &any::Any where T: 'static { self }
    #[inline]
    fn as_any_mut(&mut self) -> &mut any::Any where T: 'static { self }
    #[inline]
    fn as_i64(&self) -> Option<i64> { self.0.as_i64() }
    #[inline]
    fn as_u64(&self) -> Option<u64> { self.0.as_u64() }
    #[inline]
    fn as_f64(&self) -> Option<f64> { self.0.as_f64() }
    #[inline]
    fn as_str(&self) -> Option<&str> { self.0.as_str() }
    #[inline]
    fn as_iter<'a>(&'a self) -> Option<Box<Iterator<Item=&'a RefArg> + 'a>> {
        use std::iter;
        let z: &RefArg = &self.0;
        Some(Box::new(iter::once(z)))
    }
    #[inline]
    fn box_clone(&self) -> Box<RefArg + 'static> { Box::new(Variant(self.0.box_clone())) }
}

macro_rules! struct_impl {
    ( $($n: ident $t: ident,)+ ) => {

/// Tuples are represented as D-Bus structs. 
impl<$($t: Arg),*> Arg for ($($t,)*) {
    const ARG_TYPE: ArgType = ArgType::Struct;
    fn signature() -> Signature<'static> {
        let mut s = String::from("(");
        $( s.push_str(&$t::signature()); )*
        s.push_str(")");
        Signature::from(s)
    }
}

impl<$($t: Append),*> Append for ($($t,)*) {
    fn append(self, i: &mut IterAppend) {
        let ( $($n,)*) = self;
        i.append_container(ArgType::Struct, None, |s| { $( $n.append(s); )* });
    }
}

impl<'a, $($t: Get<'a>),*> Get<'a> for ($($t,)*) {
    fn get(i: &mut Iter<'a>) -> Option<Self> {
        let si = i.recurse(ArgType::Struct);
        if si.is_none() { return None; }
        let mut si = si.unwrap();
        let mut _valid_item = true;
        $(
            if !_valid_item { return None; }
            let $n: Option<$t> = si.get();
            if $n.is_none() { return None; }
            _valid_item = si.next();
        )*
        Some(($( $n.unwrap(), )* ))
    }
}

impl<$($t: RefArg),*> RefArg for ($($t,)*) {
    fn arg_type(&self) -> ArgType { ArgType::Struct }
    fn signature(&self) -> Signature<'static> {
        let &( $(ref $n,)*) = self;
        let mut s = String::from("(");
        $( s.push_str(&$n.signature()); )*
        s.push_str(")");
        Signature::from(s)
    }
    fn append(&self, i: &mut IterAppend) {
        let &( $(ref $n,)*) = self;
        i.append_container(ArgType::Struct, None, |s| { $( $n.append(s); )* });
    }
    fn as_any(&self) -> &any::Any where Self: 'static { self }
    fn as_any_mut(&mut self) -> &mut any::Any where Self: 'static { self }
    fn as_iter<'a>(&'a self) -> Option<Box<Iterator<Item=&'a RefArg> + 'a>> {
        let &( $(ref $n,)*) = self;
        let v = vec!(
        $( $n as &RefArg, )*
        );
        Some(Box::new(v.into_iter()))
    }
    #[inline]
    fn box_clone(&self) -> Box<RefArg + 'static> {
        let &( $(ref $n,)*) = self;
        let mut z = vec!();
        $( z.push($n.box_clone()); )*
        Box::new(z)
    }
}


}} // macro_rules end

struct_impl!(a A,);
struct_impl!(a A, b B,);
struct_impl!(a A, b B, c C,);
struct_impl!(a A, b B, c C, d D,);
struct_impl!(a A, b B, c C, d D, e E,);
struct_impl!(a A, b B, c C, d D, e E, f F,);
struct_impl!(a A, b B, c C, d D, e E, f F, g G,);
struct_impl!(a A, b B, c C, d D, e E, f F, g G, h H,);
struct_impl!(a A, b B, c C, d D, e E, f F, g G, h H, i I,);
struct_impl!(a A, b B, c C, d D, e E, f F, g G, h H, i I, j J,);
struct_impl!(a A, b B, c C, d D, e E, f F, g G, h H, i I, j J, k K,);
struct_impl!(a A, b B, c C, d D, e E, f F, g G, h H, i I, j J, k K, l L,);

impl RefArg for Vec<Box<RefArg>> {
    fn arg_type(&self) -> ArgType { ArgType::Struct }
    fn signature(&self) -> Signature<'static> {
        let mut s = String::from("(");
        for z in self {
            s.push_str(&z.signature());
        }
        s.push_str(")");
        Signature::from(s)
    }
    fn append(&self, i: &mut IterAppend) {
        i.append_container(ArgType::Struct, None, |s| {
            for z in self { z.append(s); }
        });
    }
    #[inline]
    fn as_any(&self) -> &any::Any where Self: 'static { self }
    #[inline]
    fn as_any_mut(&mut self) -> &mut any::Any where Self: 'static { self }
    fn as_iter<'a>(&'a self) -> Option<Box<Iterator<Item=&'a RefArg> + 'a>> {
        Some(Box::new(self.iter().map(|b| &**b)))
    }
    #[inline]
    fn box_clone(&self) -> Box<RefArg + 'static> {
        let t: Vec<Box<RefArg + 'static>> = self.iter().map(|x| x.box_clone()).collect();
        Box::new(t)
    }
}

impl Append for message::MessageItem {
    fn append(self, i: &mut IterAppend) {
        message::append_messageitem(&mut i.0, &self)
    }
}

impl<'a> Get<'a> for message::MessageItem {
    fn get(i: &mut Iter<'a>) -> Option<Self> {
        message::get_messageitem(&mut i.0)
    }
}

impl RefArg for message::MessageItem {
    fn arg_type(&self) -> ArgType { ArgType::from_i32(self.array_type()).unwrap() }
    fn signature(&self) -> Signature<'static> { message::MessageItem::signature(&self) }
    fn append(&self, i: &mut IterAppend) { message::append_messageitem(&mut i.0, self) }
    #[inline]
    fn as_any(&self) -> &any::Any where Self: 'static { self }
    #[inline]
    fn as_any_mut(&mut self) -> &mut any::Any where Self: 'static { self }
    #[inline]
    fn box_clone(&self) -> Box<RefArg + 'static> { Box::new(self.clone()) }
}

