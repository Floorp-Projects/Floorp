// Methods, signals, properties, and interfaces.
use super::utils::{Argument, Annotations, Introspect, introspect_args};
use super::{MethodType, MethodInfo, MethodResult, MethodErr, DataType, PropInfo, MTFn, MTFnMut, MTSync};
use {Member, Signature, Message, Path, MessageItem};
use Interface as IfaceName;
use arg;
use std::fmt;
use std::cell::RefCell;
use stdintf::org_freedesktop_dbus::PropertiesPropertiesChanged;


// Workaround for https://github.com/rust-lang/rust/issues/31518
struct DebugMethod<M: MethodType<D>, D: DataType>(Box<M::Method>);
impl<M: MethodType<D>, D: DataType> fmt::Debug for DebugMethod<M, D> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result { write!(f, "<Method>") }
}
struct DebugGetProp<M: MethodType<D>, D: DataType>(Box<M::GetProp>);
impl<M: MethodType<D>, D: DataType> fmt::Debug for DebugGetProp<M, D> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result { write!(f, "<GetProp>") }
}
struct DebugSetProp<M: MethodType<D>, D: DataType>(Box<M::SetProp>);
impl<M: MethodType<D>, D: DataType> fmt::Debug for DebugSetProp<M, D> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result { write!(f, "<SetProp>") }
}


#[derive(Debug)]
/// A D-Bus Method.
pub struct Method<M: MethodType<D>, D: DataType> {
    cb: DebugMethod<M, D>,
    data: D::Method,
    name: Member<'static>,
    i_args: Vec<Argument>,
    o_args: Vec<Argument>,
    anns: Annotations,
}

impl<M: MethodType<D>, D: DataType> Method<M, D> {

    /// Builder method that adds an "in" Argument to this Method.
    pub fn in_arg<A: Into<Argument>>(mut self, a: A) -> Self { self.i_args.push(a.into()); self }
    /// Builder method that adds an "in" Argument to this Method.
    pub fn inarg<A: arg::Arg, S: Into<String>>(mut self, s: S) -> Self { self.i_args.push((s.into(), A::signature()).into()); self }
    /// Builder method that adds multiple "in" Arguments to this Method.
    pub fn in_args<Z: Into<Argument>, A: IntoIterator<Item=Z>>(mut self, a: A) -> Self {
        self.i_args.extend(a.into_iter().map(|b| b.into())); self
    }

    /// Builder method that adds an "out" Argument to this Method.
    pub fn out_arg<A: Into<Argument>>(mut self, a: A) -> Self { self.o_args.push(a.into()); self }
    /// Builder method that adds an "out" Argument to this Method.
    pub fn outarg<A: arg::Arg, S: Into<String>>(mut self, s: S) -> Self { self.o_args.push((s.into(), A::signature()).into()); self }
    /// Builder method that adds multiple "out" Arguments to this Method.
    pub fn out_args<Z: Into<Argument>, A: IntoIterator<Item=Z>>(mut self, a: A) -> Self {
        self.o_args.extend(a.into_iter().map(|b| b.into())); self
    }

    /// Builder method that adds an annotation to the method.
    pub fn annotate<N: Into<String>, V: Into<String>>(mut self, name: N, value: V) -> Self {
        self.anns.insert(name, value); self
    }
    /// Builder method that adds an annotation that this entity is deprecated.
    pub fn deprecated(self) -> Self { self.annotate("org.freedesktop.DBus.Deprecated", "true") }

    /// Call the Method
    pub fn call(&self, minfo: &MethodInfo<M, D>) -> MethodResult { M::call_method(&self.cb.0, minfo) }

    /// Get method name
    pub fn get_name(&self) -> &Member<'static> { &self.name }

    /// Get associated data
    pub fn get_data(&self) -> &D::Method { &self.data }

}

impl<M: MethodType<D>, D: DataType> Introspect for Method<M, D> {
    fn xml_name(&self) -> &'static str { "method" }
    fn xml_params(&self) -> String { String::new() }
    fn xml_contents(&self) -> String {
        format!("{}{}{}",
            introspect_args(&self.i_args, "      ", " direction=\"in\""),
            introspect_args(&self.o_args, "      ", " direction=\"out\""),
            self.anns.introspect("      "))
    }
}

pub fn new_method<M: MethodType<D>, D: DataType>(n: Member<'static>, data: D::Method, cb: Box<M::Method>) -> Method<M, D> {
    Method { name: n, i_args: vec!(), o_args: vec!(), anns: Annotations::new(), cb: DebugMethod(cb), data: data }
}



#[derive(Debug)]
/// A D-Bus Signal.
pub struct Signal<D: DataType> {
    name: Member<'static>,
    data: D::Signal,
    arguments: Vec<Argument>,
    anns: Annotations,
}

impl<D: DataType> Signal<D> {

    /// Builder method that adds an Argument to the Signal.
    pub fn arg<A: Into<Argument>>(mut self, a: A) -> Self { self.arguments.push(a.into()); self }

    /// Builder method that adds an Argument to the Signal.
    pub fn sarg<A: arg::Arg, S: Into<String>>(mut self, s: S) -> Self { self.arguments.push((s.into(), A::signature()).into()); self }

    /// Builder method that adds multiple Arguments to the Signal.
    pub fn args<Z: Into<Argument>, A: IntoIterator<Item=Z>>(mut self, a: A) -> Self {
        self.arguments.extend(a.into_iter().map(|b| b.into())); self
    }

    /// Add an annotation to this Signal.
    pub fn annotate<N: Into<String>, V: Into<String>>(mut self, name: N, value: V) -> Self {
        self.anns.insert(name, value); self
    }
    /// Add an annotation that this entity is deprecated.
    pub fn deprecated(self) -> Self { self.annotate("org.freedesktop.DBus.Deprecated", "true") }

    /// Get signal name
    pub fn get_name(&self) -> &Member<'static> { &self.name }

    /// Get associated data
    pub fn get_data(&self) -> &D::Signal { &self.data }

    /// Returns a message which emits the signal when sent.
    ///
    /// Same as "msg" but also takes a "MessageItem" argument.
    pub fn emit(&self, p: &Path<'static>, i: &IfaceName<'static>, items: &[MessageItem]) -> Message {
        let mut m = self.msg(p, i);
        m.append_items(items);
        m
    }

    /// Returns a message which emits the signal when sent.
    ///
    /// Same as "emit" but does not take a "MessageItem" argument.
    pub fn msg(&self, p: &Path<'static>, i: &IfaceName<'static>) -> Message {
        Message::signal(p, i, &self.name)
    }

}

impl<D: DataType> Introspect for Signal<D> {
    fn xml_name(&self) -> &'static str { "signal" }
    fn xml_params(&self) -> String { String::new() }
    fn xml_contents(&self) -> String {
        format!("{}{}",
            introspect_args(&self.arguments, "      ", ""),
            self.anns.introspect("      "))
    }
}

pub fn new_signal<D: DataType>(n: Member<'static>, data: D::Signal) -> Signal<D> {
    Signal { name: n, arguments: vec!(), anns: Annotations::new(), data: data }
}

#[derive(Copy, Clone, PartialEq, Eq, Ord, PartialOrd, Debug)]
/// Enumerates the different signaling behaviors a Property can have
/// to being changed.
pub enum EmitsChangedSignal {
    /// The Property emits a signal that includes the new value.
    True,
    /// The Property emits a signal that does not include the new value.
    Invalidates,
    /// The Property cannot be changed.
    Const,
    /// The Property does not emit a signal when changed.
    False,
}

#[derive(Copy, Clone, PartialEq, Eq, Ord, PartialOrd, Debug)]
/// The possible access characteristics a Property can have.
pub enum Access {
    /// The Property can only be read (Get).
    Read,
    /// The Property can be read or written.
    ReadWrite,
    /// The Property can only be written (Set).
    Write,
}

impl Access {
    fn introspect(&self) -> &'static str {
        match self {
            &Access::Read => "read",
            &Access::ReadWrite => "readwrite",
            &Access::Write => "write",
        }
    }
}


pub fn prop_append_dict<'v, M: MethodType<D> + 'v, D: DataType + 'v, I: Iterator<Item=&'v Property<M, D>>>
    (iter: &mut arg::IterAppend, mut props: I, minfo: &MethodInfo<M, D>) -> Result<(), MethodErr> {

    let mut result = Ok(());
    iter.append_dict(&Signature::make::<&str>(), &Signature::make::<arg::Variant<bool>>(), |subiter| loop {
        let p = if let Some(p) = props.next() { p } else { return };
        if p.can_get().is_err() { continue; }
        let pinfo = minfo.to_prop_info(minfo.iface, p);
        subiter.append_dict_entry(|mut entryiter| {
            entryiter.append(&*p.get_name());
            result = p.get_as_variant(&mut entryiter, &pinfo);
        });
        if result.is_err() { return };
    });
    result
}


#[derive(Debug)]
/// A D-Bus Property.
pub struct Property<M: MethodType<D>, D: DataType> {
    name: String,
    data: D::Property,
    sig: Signature<'static>,
    emits: EmitsChangedSignal,
    auto_emit: bool,
    rw: Access,
    get_cb: Option<DebugGetProp<M, D>>,
    set_cb: Option<DebugSetProp<M, D>>,
    anns: Annotations,
}

impl<M: MethodType<D>, D: DataType> Property<M, D> {

    /// Builder method that allows setting the Property's signal
    /// behavior when changed.
    ///
    /// Note: if e is set to const, the property will be read only. 
    pub fn emits_changed(mut self, e: EmitsChangedSignal) -> Self {
        self.emits = e;
        if self.emits == EmitsChangedSignal::Const { self.rw = Access::Read };
        self
    }

    /// Builder method that determines whether or not setting this property
    /// will result in an PropertiesChanged signal. Defaults to true.
    ///
    /// When set to true (the default), the behaviour is determined by "emits_changed".
    /// When set to false, no PropertiesChanged signal will be emitted (but the signal
    /// still shows up in introspection data).
    /// You can still emit the signal manually by, e g, calling `add_propertieschanged`
    /// and send the resulting message(s).
    pub fn auto_emit_on_set(mut self, b: bool) -> Self {
        self.auto_emit = b;
        self
    }

    /// Builder method that allows setting the Property as readable,
    /// writable, or both.
    ///
    /// Note: might modify emits_changed as well, if property is changed to non-readonly and emit is set to "Const".
    pub fn access(mut self, e: Access) -> Self {
        self.rw = e;
        if self.rw != Access::Read && self.emits == EmitsChangedSignal::Const {
            self.emits = EmitsChangedSignal::False
        };
        self
    }

    /// Builder method that adds an annotation to the method.
    pub fn annotate<N: Into<String>, V: Into<String>>(mut self, name: N, value: V) -> Self {
        self.anns.insert(name, value); self
    }

    /// Builder method that adds an annotation that this entity is deprecated.
    pub fn deprecated(self) -> Self { self.annotate("org.freedesktop.DBus.Deprecated", "true") }

    /// Get property name
    pub fn get_name(&self) -> &str { &self.name }

    /// Get associated data
    pub fn get_data(&self) -> &D::Property { &self.data }

    /// Returns Ok if the property is gettable
    pub fn can_get(&self) -> Result<(), MethodErr> {
        if self.rw == Access::Write || self.get_cb.is_none() { 
            Err(MethodErr::failed(&format!("Property {} is write only", &self.name)))
        } else { Ok(()) }
    }

    /// Calls the on_get function and appends the result as a variant.
    ///
    /// Note: Will panic if get_cb is not set.
    pub fn get_as_variant(&self, i: &mut arg::IterAppend, pinfo: &PropInfo<M, D>) -> Result<(), MethodErr> {
        let mut r = Ok(());
        i.append_variant(&self.sig, |subi| {
            r = M::call_getprop(&*self.get_cb.as_ref().unwrap().0, subi, pinfo); 
        });
        r
    }

    /// Returns Ok if the property is settable.
    ///
    /// Will verify signature in case iter is not None; iter is supposed to point at the Variant with the item inside.
    pub fn can_set(&self, i: Option<arg::Iter>) -> Result<(), MethodErr> {
        use arg::Arg;
        if self.rw == Access::Read || self.set_cb.is_none() || self.emits == EmitsChangedSignal::Const {
            return Err(MethodErr::ro_property(&self.name))
        }
        if let Some(mut i) = i {
            let mut subiter = try!(i.recurse(arg::Variant::<bool>::ARG_TYPE).ok_or_else(|| MethodErr::invalid_arg(&2)));
            if &*subiter.signature() != &*self.sig {
               return Err(MethodErr::failed(&format!("Property {} cannot change type", &self.name)))
            }
        }
        Ok(())
    }

    /// Calls the on_set function, which reads from i.
    ///
    /// The return value might contain an extra message containing the EmitsChanged signal.
    /// Note: Will panic if set_cb is not set.
    pub fn set_as_variant(&self, i: &mut arg::Iter, pinfo: &PropInfo<M, D>) -> Result<Option<Message>, MethodErr> {
        use arg::Arg;
        let mut subiter = try!(i.recurse(arg::Variant::<bool>::ARG_TYPE).ok_or_else(|| MethodErr::invalid_arg(&2)));
        try!(M::call_setprop(&*self.set_cb.as_ref().unwrap().0, &mut subiter, pinfo));
        self.get_emits_changed_signal(pinfo)
    }

    /// Gets the signal (if any) associated with the Property.
    fn get_signal(&self, p: &PropInfo<M, D>) -> Message {
        Message::signal(p.path.get_name(), &"org.freedesktop.DBus.Properties".into(), &"PropertiesChanged".into())
            .append1(&**p.iface.get_name())
    }

    /// Adds this property to a list of PropertiesChanged signals.
    ///
    /// "v" is updated with the signal for this property. "new_value" is only called if self.emits is "true",
    /// it should return the value of the property.
    /// If no PropertiesChanged signal should be emitted for this property, "v" is left unchanged.
    pub fn add_propertieschanged<F: FnOnce() -> Box<arg::RefArg>>(&self, v: &mut Vec<PropertiesPropertiesChanged>, iface: &IfaceName, new_value: F) {

        // Impl note: It is a bit silly that this function cannot be used from e g get_emits_changed_signal below,
        // but it is due to the fact that we cannot create a RefArg out of an IterAppend; which is what the 'on_get'
        // handler currently receives.

        if self.emits == EmitsChangedSignal::Const || self.emits == EmitsChangedSignal::False { return; }
        let vpos = v.iter().position(|vv| &*vv.interface_name == &**iface);
        let vpos = vpos.unwrap_or_else(|| {
            let mut z: PropertiesPropertiesChanged = Default::default();
            z.interface_name = (&**iface).into();
            v.push(z);
            v.len()-1
        });

        let vv = &mut v[vpos];
        if self.emits == EmitsChangedSignal::Invalidates {
            vv.invalidated_properties.push(self.name.clone());
        } else {
            vv.changed_properties.insert(self.name.clone(), arg::Variant(new_value()));
        }
    }

    fn get_emits_changed_signal(&self, m: &PropInfo<M, D>) -> Result<Option<Message>, MethodErr> {
        if !self.auto_emit { return Ok(None) }
        match self.emits {
            EmitsChangedSignal::False => Ok(None),
            EmitsChangedSignal::Const => Err(MethodErr::ro_property(&self.name)),
            EmitsChangedSignal::True => Ok(Some({
                let mut s = self.get_signal(m);
                {
                    let mut iter = arg::IterAppend::new(&mut s);
                    try!(prop_append_dict(&mut iter, Some(self).into_iter(), &m.to_method_info()));
                    iter.append(arg::Array::<&str, _>::new(vec!()));
                }
                s
            })),
            EmitsChangedSignal::Invalidates => Ok(Some(self.get_signal(m).append2(
                arg::Dict::<&str, arg::Variant<bool>, _>::new(vec!()),
                arg::Array::new(Some(&*self.name).into_iter())
            ))),
        }
    }
}

impl<'a, D: DataType> Property<MTFn<D>, D> {
    /// Sets the callback for getting a property.
    ///
    /// For single-thread use.
    pub fn on_get<H>(mut self, handler: H) -> Property<MTFn<D>, D>
        where H: 'static + Fn(&mut arg::IterAppend, &PropInfo<MTFn<D>, D>) -> Result<(), MethodErr> {
        self.get_cb = Some(DebugGetProp(Box::new(handler) as Box<_>));
        self
    }

    /// Sets the callback for setting a property.
    ///
    /// For single-thread use.
    pub fn on_set<H>(mut self, handler: H) -> Property<MTFn<D>, D>
        where H: 'static + Fn(&mut arg::Iter, &PropInfo<MTFn<D>, D>) -> Result<(), MethodErr> {
        self.set_cb = Some(DebugSetProp(Box::new(handler) as Box<_>));
        self
    }
}


impl<'a, D: DataType> Property<MTFnMut<D>, D> {
    /// Sets the callback for getting a property.
    ///
    /// For single-thread use.
    pub fn on_get<H>(mut self, handler: H) -> Property<MTFnMut<D>, D>
        where H: 'static + Fn(&mut arg::IterAppend, &PropInfo<MTFnMut<D>, D>) -> Result<(), MethodErr> {
        self.get_cb = Some(DebugGetProp(Box::new(RefCell::new(handler)) as Box<_>));
        self
    }

    /// Sets the callback for setting a property.
    ///
    /// For single-thread use.
    pub fn on_set<H>(mut self, handler: H) -> Property<MTFnMut<D>, D>
        where H: 'static + Fn(&mut arg::Iter, &PropInfo<MTFnMut<D>, D>) -> Result<(), MethodErr> {
        self.set_cb = Some(DebugSetProp(Box::new(RefCell::new(handler)) as Box<_>));
        self
    }
}

impl<D: DataType> Property<MTSync<D>, D> {
    /// Sets the callback for getting a property.
    ///
    /// For multi-thread use.
    pub fn on_get<H>(mut self, handler: H) -> Property<MTSync<D>, D>
        where H: Fn(&mut arg::IterAppend, &PropInfo<MTSync<D>, D>) -> Result<(), MethodErr> + Send + Sync + 'static {
        self.get_cb = Some(DebugGetProp(Box::new(handler) as Box<_>));
        self
    }

    /// Sets the callback for setting a property.
    ///
    /// For single-thread use.
    pub fn on_set<H>(mut self, handler: H) -> Property<MTSync<D>, D>
        where H: Fn(&mut arg::Iter, &PropInfo<MTSync<D>, D>) -> Result<(), MethodErr> + Send + Sync + 'static {
        self.set_cb = Some(DebugSetProp(Box::new(handler) as Box<_>));
        self
    }
}


impl<M: MethodType<D>, D: DataType> Property<M, D> where D::Property: arg::Append + Clone {
    /// Adds a "standard" get handler.
    pub fn default_get(mut self) -> Self {
        let g = |i: &mut arg::IterAppend, p: &PropInfo<M, D>| { i.append(p.prop.get_data()); Ok(()) };
        self.get_cb = Some(DebugGetProp(M::make_getprop(g)));
        self
    }
}


impl<M: MethodType<D>, D: DataType> Property<M, D> where D::Property: arg::RefArg {
    /// Adds a "standard" get handler (for RefArgs).
    pub fn default_get_refarg(mut self) -> Self {
        let g = |i: &mut arg::IterAppend, p: &PropInfo<M, D>| { (p.prop.get_data() as &arg::RefArg).append(i); Ok(()) };
        self.get_cb = Some(DebugGetProp(M::make_getprop(g)));
        self
    }
}

impl<M: MethodType<D>, D: DataType> Introspect for Property<M, D> {
    fn xml_name(&self) -> &'static str { "property" }
    fn xml_params(&self) -> String { format!(" type=\"{}\" access=\"{}\"", self.sig, self.rw.introspect()) }
    fn xml_contents(&self) -> String {
        let s = match self.emits {
             EmitsChangedSignal::True => return self.anns.introspect("      "),
             EmitsChangedSignal::False => "false",
             EmitsChangedSignal::Const => "const",
             EmitsChangedSignal::Invalidates => "invalidates",
        };
        let mut tempanns = self.anns.clone();
        tempanns.insert("org.freedesktop.DBus.Property.EmitsChangedSignal", s);
        tempanns.introspect("      ")
    }
}

pub fn new_property<M: MethodType<D>, D: DataType>
    (n: String, sig: Signature<'static>, data: D::Property) -> Property<M, D> {
    Property {
        name: n, emits: EmitsChangedSignal::True, auto_emit: true, rw: Access::Read,
        sig: sig, anns: Annotations::new(), set_cb: None, get_cb: None, data: data
    }
}

#[test]
fn test_prop_handlers() {
    use tree::Factory;
    use std::collections::BTreeMap;
    use arg::{Dict, Variant};

    #[derive(Default, Debug)]
    struct Custom;
    impl DataType for Custom {
        type Tree = ();
        type ObjectPath = ();
        type Interface = ();
        type Property = i32;
        type Method = ();
        type Signal = ();
    }

    let f = Factory::new_fn::<Custom>();
    let tree = f.tree(()).add(f.object_path("/test", ()).introspectable().object_manager()
        .add(f.interface("com.example.test", ())
            .add_p(f.property::<i32,_>("Value1", 5i32).default_get())
            .add_p(f.property::<i32,_>("Value2", 9i32).default_get())
        )
    );

    let mut msg = Message::new_method_call("com.example.test", "/test", "org.freedesktop.DBus.Properties", "Get").unwrap()
        .append2("com.example.test", "Value1");
    ::message::message_set_serial(&mut msg, 4);
    let res = tree.handle(&msg).unwrap();
    assert_eq!(res[0].get1(), Some(arg::Variant(5i32)));

    let mut msg = Message::new_method_call("com.example.test", "/test", "org.freedesktop.DBus.Properties", "Set").unwrap()
        .append3("com.example.test", "Value1", arg::Variant(3i32));
    ::message::message_set_serial(&mut msg, 4);
    let mut res = tree.handle(&msg).unwrap();
    assert!(res[0].as_result().is_err());

    let mut msg = Message::new_method_call("com.example.test", "/test", "org.freedesktop.DBus.Properties", "GetAll").unwrap()
        .append1("com.example.test");
    ::message::message_set_serial(&mut msg, 4);
    let res = tree.handle(&msg).unwrap();
    let d: Dict<&str, Variant<i32>, _> = res[0].get1().unwrap();
    let z2: BTreeMap<_, _> = d.collect();
    assert_eq!(z2.get("Value1"), Some(&arg::Variant(5i32)));
    assert_eq!(z2.get("Value2"), Some(&arg::Variant(9i32)));
    assert_eq!(z2.get("Mooh"), None);

    let mut msg = Message::new_method_call("com.example.test", "/test", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects").unwrap();
    ::message::message_set_serial(&mut msg, 4);
    let res = tree.handle(&msg).unwrap();
    let pdict: arg::Dict<Path, Dict<&str, Dict<&str, Variant<i32>, _>, _>, _> = res[0].get1().unwrap();
    let pmap: BTreeMap<_, _> = pdict.collect();
    let idict = pmap.get(&Path::from("/test")).unwrap();
    let imap: BTreeMap<_, _> = idict.collect();
    let propdict = imap.get("com.example.test").unwrap();
    let propmap: BTreeMap<_, _> = propdict.collect();
    assert_eq!(propmap.get("Value1"), Some(&arg::Variant(5i32)));
    assert_eq!(propmap.get("Value2"), Some(&arg::Variant(9i32)));
    assert_eq!(propmap.get("Mooh"), None);
}

#[test]
fn test_set_prop() {
    use tree::{Factory, Access};
    use std::cell::{Cell, RefCell};
    use std::collections::BTreeMap;
    use std::rc::Rc;
 
    let changes = Rc::new(Cell::new(0i32));
    let (changes1, changes2) = (changes.clone(), changes.clone());
    let setme = Rc::new(RefCell::new("I have not been set yet!".to_owned()));
    let (setme1, setme2) = (setme.clone(), setme.clone());

    let f = Factory::new_fn::<()>();
    let tree = f.tree(()).add(f.object_path("/example", ()).introspectable()
        .add(f.interface("com.example.dbus.rs", ())
            .add_p(f.property::<i32,_>("changes", ())
                .on_get(move |i, _| { i.append(changes1.get()); Ok(()) })) 
            .add_p(f.property::<String,_>("setme", ())
                .access(Access::ReadWrite)
                .on_get(move |i, _| { i.append(&*setme1.borrow()); Ok(()) })
                .on_set(move |i, _| {
                    *setme2.borrow_mut() = i.get().unwrap();
                    changes2.set(changes2.get() + 1);
                    Ok(())
                }))
        )
    );
    
    // Read-only
    let mut msg = Message::new_method_call("com.example.dbus.rs", "/example", "org.freedesktop.DBus.Properties", "Set").unwrap()
        .append3("com.example.dbus.rs", "changes", arg::Variant(5i32));
    ::message::message_set_serial(&mut msg, 20);
    let mut r = tree.handle(&msg).unwrap();
    assert!(r.get_mut(0).unwrap().as_result().is_err());

    // Wrong type
    let mut msg = Message::new_method_call("com.example.dbus.rs", "/example", "org.freedesktop.DBus.Properties", "Set").unwrap()
        .append3("com.example.dbus.rs", "setme", arg::Variant(8i32));
    ::message::message_set_serial(&mut msg, 30);
    let mut r = tree.handle(&msg).unwrap();
    assert!(r.get_mut(0).unwrap().as_result().is_err());

    // Correct!
    let mut msg = Message::new_method_call("com.example.dbus.rs", "/example", "org.freedesktop.DBus.Properties", "Set").unwrap()
        .append3("com.example.dbus.rs", "setme", arg::Variant("Correct"));
    ::message::message_set_serial(&mut msg, 30);
    let r = tree.handle(&msg).unwrap();

    assert_eq!(changes.get(), 1);
    assert_eq!(&**setme.borrow(), "Correct");

    println!("{:?}", r);
    assert_eq!(r.len(), 2);
    assert_eq!(&*r[0].member().unwrap(), "PropertiesChanged");
    let (s, d): (Option<&str>, Option<arg::Dict<&str, arg::Variant<_>, _>>) = r[0].get2();
    assert_eq!(s, Some("com.example.dbus.rs"));
    let z2: BTreeMap<_, _> = d.unwrap().collect();
    assert_eq!(z2.get("setme"), Some(&arg::Variant("Correct")));

}


#[test]
fn test_sync_prop() {
    use std::sync::atomic::{AtomicUsize, Ordering};
    use std::sync::Arc;
    use tree::{Factory, Access, EmitsChangedSignal};

    let f = Factory::new_sync::<()>();

    let count = Arc::new(AtomicUsize::new(3));
    let (cget, cset) = (count.clone(), count.clone());

    let tree1 = Arc::new(f.tree(()).add(f.object_path("/syncprop", ()).introspectable()
        .add(f.interface("com.example.syncprop", ())
            .add_p(f.property::<u32,_>("syncprop", ())
                .access(Access::ReadWrite)
                .emits_changed(EmitsChangedSignal::False)
                .on_get(move |i,_| { i.append(cget.load(Ordering::SeqCst) as u32); Ok(()) }) 
                .on_set(move |i,_| { cset.store(i.get::<u32>().unwrap() as usize, Ordering::SeqCst); Ok(()) })
            )
        )
    ));

    let tree2 = tree1.clone();
    println!("{:#?}", tree2);

    ::std::thread::spawn(move || {
        let mut msg = Message::new_method_call("com.example.syncprop", "/syncprop", "org.freedesktop.DBus.Properties", "Set").unwrap()
            .append3("com.example.syncprop", "syncprop", arg::Variant(5u32));
         ::message::message_set_serial(&mut msg, 30);
         let mut r = tree2.handle(&msg).unwrap();
         assert!(r[0].as_result().is_ok());
    });

    loop {
        let mut msg = Message::new_method_call("com.example.echoserver", "/syncprop", "org.freedesktop.DBus.Properties", "Get").unwrap()
            .append("com.example.syncprop").append1("syncprop");
        ::message::message_set_serial(&mut msg, 4);
        let mut r = tree1.handle(&msg).unwrap();
        let r = r[0].as_result().unwrap();
        let z: arg::Variant<u32> = r.get1().unwrap();
        if z.0 == 5 { break; }
        assert_eq!(z.0, 3);
   }
   assert_eq!(count.load(Ordering::SeqCst), 5);
}
