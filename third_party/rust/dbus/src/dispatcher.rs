use crate::{Message, MessageType, Error, to_c_str, c_str_to_slice};
use std::ptr;

use std::collections::HashMap;

/// [Unstable and Experimental]
pub trait MessageDispatcherConfig: Sized {
    /// The type of method reply stored inside the dispatcher
    type Reply;

    /// Called when a method call has received a reply.
    fn on_reply(reply: Self::Reply, msg: Message, dispatcher: &mut MessageDispatcher<Self>);

    /// Called when a signal is received.
    ///
    /// Defaults to doing nothing.
    #[allow(unused_variables)]
    fn on_signal(msg: Message, dispatcher: &mut MessageDispatcher<Self>) {}

    /// Called when a method call is received.
    ///
    /// Defaults to calling default_dispatch.
    fn on_method_call(msg: Message, dispatcher: &mut MessageDispatcher<Self>) {
        if let Some(reply) = MessageDispatcher::<Self>::default_dispatch(&msg) {
            Self::on_send(reply, dispatcher);
        }
    }

    /// Called in the other direction, i e, when a message should be sent over the connection.
    fn on_send(msg: Message, dispatcher: &mut MessageDispatcher<Self>);
}

/// Dummy implementation
impl MessageDispatcherConfig for () {
    type Reply = ();
    fn on_reply(_: Self::Reply, _: Message, _: &mut MessageDispatcher<Self>) { unreachable!() }
    fn on_send(_: Message, _: &mut MessageDispatcher<Self>) { unreachable!() }
}

/// [Unstable and Experimental] Meant for usage with RxTx.
pub struct MessageDispatcher<C: MessageDispatcherConfig> {
    waiting_replies: HashMap<u32, C::Reply>,
    inner: C,
}

impl<C: MessageDispatcherConfig> MessageDispatcher<C> {

    /// Creates a new message dispatcher.
    pub fn new(inner: C) -> Self { MessageDispatcher {
        waiting_replies: HashMap::new(),
        inner: inner,
    } }

    /// "Inner" accessor
    pub fn inner(&self) -> &C { &self.inner }

    /// "Inner" mutable accessor
    pub fn inner_mut(&mut self) -> &mut C { &mut self.inner }

    /// Adds a waiting reply to a method call. func will be called when a method reply is dispatched.
    pub fn add_reply(&mut self, serial: u32, func: C::Reply) {
        if let Some(_) = self.waiting_replies.insert(serial, func) {
            // panic because we're overwriting something else, or just ignore?
        }
    }

    /// Cancels a waiting reply.
    pub fn cancel_reply(&mut self, serial: u32) -> Option<C::Reply> {
        self.waiting_replies.remove(&serial)
    }


    /// Dispatch an incoming message.
    pub fn dispatch(&mut self, msg: Message) {
        if let Some(serial) = msg.get_reply_serial() {
            if let Some(sender) = self.waiting_replies.remove(&serial) {
                C::on_reply(sender, msg, self);
                return;
            }
        }
        match msg.msg_type() {
            MessageType::Signal => C::on_signal(msg, self),
            MessageType::MethodCall => C::on_method_call(msg, self),
            MessageType::Error | MessageType::MethodReturn => {},
            MessageType::Invalid => unreachable!(),
        }
    }

    /// Handles what we need to be a good D-Bus citizen.
    ///
    /// Call this if you have not handled the message yourself:
    /// * It handles calls to org.freedesktop.DBus.Peer.
    /// * For other method calls, it sends an error reply back that the method was unknown.
    pub fn default_dispatch(m: &Message) -> Option<Message> {
        Self::peer(&m)
            .or_else(|| Self::unknown_method(&m))
    }

    /// Replies if this is a call to org.freedesktop.DBus.Peer, otherwise returns None.
    pub fn peer(m: &Message) -> Option<Message> {
        if let Some(intf) = m.interface() {
            if &*intf != "org.freedesktop.DBus.Peer" { return None; }
            if let Some(method) = m.member() {
                if &*method == "Ping" { return Some(m.method_return()) }
                if &*method == "GetMachineId" {
                    let mut r = m.method_return();
                    let mut e = Error::empty();
                    unsafe {
                        let id = ffi::dbus_try_get_local_machine_id(e.get_mut());
                        if id != ptr::null_mut() {
                            r = r.append1(c_str_to_slice(&(id as *const _)).unwrap());
                            ffi::dbus_free(id as *mut _);
                            return Some(r)
                        }
                    }
                }
            }
            Some(m.error(&"org.freedesktop.DBus.Error.UnknownMethod".into(), &to_c_str("Method does not exist")))
        } else { None }
    }

    /// For method calls, it replies that the method was unknown, otherwise returns None.
    pub fn unknown_method(m: &Message) -> Option<Message> {
        if m.msg_type() != MessageType::MethodCall { return None; }
        // if m.get_no_reply() { return None; } // The reference implementation does not do this?
        Some(m.error(&"org.freedesktop.DBus.Error.UnknownMethod".into(), &to_c_str("Path, Interface, or Method does not exist")))
    }
}

