use super::{MethodType, DataType, MTFn, MTFnMut, MTSync, MethodResult, MethodInfo};
use super::{Tree, ObjectPath, Interface, Property, Signal, Method};
use super::objectpath::IfaceCache;
use std::sync::Arc;
use Interface as IfaceName;
use {Member, Path, arg};
use std::cell::RefCell;

/// The factory is used to create object paths, interfaces, methods etc.
///
/// There are three factories:
///
///  **MTFn** - all methods are `Fn()`.
///
///  **MTFnMut** - all methods are `FnMut()`. This means they can mutate their environment,
///  which has the side effect that if you call it recursively, it will RefCell panic.
///
///  **MTSync** - all methods are `Fn() + Send + Sync + 'static`. This means that the methods
///  can be called from different threads in parallel.
///
#[derive(Debug, Clone)]
pub struct Factory<M: MethodType<D>, D: DataType=()>(Arc<IfaceCache<M, D>>);

impl<M: MethodType<D>, D: DataType> From<Arc<IfaceCache<M, D>>> for Factory<M, D> {
    fn from(f: Arc<IfaceCache<M, D>>) -> Self { Factory(f) }
}

impl Factory<MTFn<()>, ()> {
    /// Creates a new factory for single-thread use.
    pub fn new_fn<D: DataType>() -> Factory<MTFn<D>, D> { Factory(IfaceCache::new()) }

    /// Creates a new factory for single-thread use, where callbacks can mutate their environment.
    pub fn new_fnmut<D: DataType>() -> Factory<MTFnMut<D>, D> { Factory(IfaceCache::new()) }

    /// Creates a new factory for multi-thread use.
    pub fn new_sync<D: DataType>() -> Factory<MTSync<D>, D> { Factory(IfaceCache::new()) }
   
}

impl<D: DataType> Factory<MTFn<D>, D> {
    /// Creates a new method for single-thread use.
    pub fn method<H, T>(&self, t: T, data: D::Method, handler: H) -> Method<MTFn<D>, D>
        where H: 'static + Fn(&MethodInfo<MTFn<D>, D>) -> MethodResult, T: Into<Member<'static>> {
        super::leaves::new_method(t.into(), data, Box::new(handler) as Box<_>)
    }
}

impl<D: DataType> Factory<MTFnMut<D>, D> {
    /// Creates a new method for single-thread use.
    pub fn method<H, T>(&self, t: T, data: D::Method, handler: H) -> Method<MTFnMut<D>, D>
        where H: 'static + FnMut(&MethodInfo<MTFnMut<D>, D>) -> MethodResult, T: Into<Member<'static>> {
        super::leaves::new_method(t.into(), data, Box::new(RefCell::new(handler)) as Box<_>)
    }
}

impl<D: DataType> Factory<MTSync<D>, D> {
    /// Creates a new method for multi-thread use.
    pub fn method<H, T>(&self, t: T, data: D::Method, handler: H) -> Method<MTSync<D>, D>
        where H: Fn(&MethodInfo<MTSync<D>, D>) -> MethodResult + Send + Sync + 'static, T: Into<Member<'static>> {
        super::leaves::new_method(t.into(), data, Box::new(handler) as Box<_>)
    }
}


impl<M: MethodType<D>, D: DataType> Factory<M, D> {

    /// Creates a new property.
    ///
    /// `A` is used to calculate the type signature of the property.
    pub fn property<A: arg::Arg, T: Into<String>>(&self, name: T, data: D::Property) -> Property<M, D> {
        let sig = A::signature();
        super::leaves::new_property(name.into(), sig, data)
    }

    /// Creates a new signal.
    pub fn signal<T: Into<Member<'static>>>(&self, name: T, data: D::Signal) -> Signal<D> {
        super::leaves::new_signal(name.into(), data)
    }

    /// Creates a new interface.
    pub fn interface<T: Into<IfaceName<'static>>>(&self, name: T, data: D::Interface) -> Interface<M, D> {
        super::objectpath::new_interface(name.into(), data)
    }

    /// Creates a new object path.
    pub fn object_path<T: Into<Path<'static>>>(&self, name: T, data: D::ObjectPath) -> ObjectPath<M, D> {
        super::objectpath::new_objectpath(name.into(), data, self.0.clone())
    }

    /// Creates a new tree.
    pub fn tree(&self, data: D::Tree) -> Tree<M, D> {
        super::objectpath::new_tree(data)
    }

    /// Creates a new method - usually you'll use "method" instead.
    ///
    /// This is useful for being able to create methods in code which is generic over methodtype.
    pub fn method_sync<H, T>(&self, t: T, data: D::Method, handler: H) -> Method<M, D>
    where H: Fn(&MethodInfo<M, D>) -> MethodResult + Send + Sync + 'static, T: Into<Member<'static>> {
        super::leaves::new_method(t.into(), data, M::make_method(handler))
    }
}


#[test]
fn create_fnmut() {
    let f = Factory::new_fnmut::<()>();
    let mut move_me = 5u32;
    let m = f.method("test", (), move |m| {
        move_me += 1; 
        Ok(vec!(m.msg.method_return().append1(&move_me)))
    });
    assert_eq!(&**m.get_name(), "test");
}


#[test]
fn fn_customdata() {
    #[derive(Default)]
    struct Custom;
    impl DataType for Custom {
        type Tree = ();
        type ObjectPath = Arc<u8>;
        type Interface = ();
        type Property = ();
        type Method = i32;
        type Signal = ();
    }

    let f = Factory::new_fn::<Custom>();
  
    let m = f.method("test", 789, |_| unimplemented!());
    assert_eq!(*m.get_data(), 789);
   
    let o = f.object_path("/test/test", Arc::new(7));
    assert_eq!(**o.get_data(), 7);
}
