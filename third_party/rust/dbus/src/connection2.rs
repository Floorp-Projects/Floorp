use crate::{BusType, Error, Message, to_c_str, Watch};
use std::{ptr, str};
use std::ffi::CStr;
use std::os::raw::{c_void};

#[derive(Debug)]
pub struct ConnHandle(*mut ffi::DBusConnection);

unsafe impl Send for ConnHandle {}
unsafe impl Sync for ConnHandle {}

impl Drop for ConnHandle {
    fn drop(&mut self) {
        unsafe {
            ffi::dbus_connection_close(self.0);
            ffi::dbus_connection_unref(self.0);
        }
    }
}

/// Experimental rewrite of Connection [unstable / experimental]
///
/// Slightly lower level, with better support for async operations.
/// Also, this struct is Send + Sync.
///
/// Blocking operations should be clearly marked as such, although if you
/// try to access the connection from several threads at the same time,
/// blocking might occur due to an internal mutex inside the dbus library.
///
/// This version avoids dbus_connection_dispatch, and thus avoids
/// callbacks from that function. Instead the same functionality needs to be
/// implemented by these bindings somehow - this is not done yet.
#[derive(Debug)]
pub struct TxRx {
    handle: ConnHandle,
}

impl TxRx {
    #[inline(always)]
    pub (crate) fn conn(&self) -> *mut ffi::DBusConnection {
        self.handle.0
    }

    fn conn_from_ptr(ptr: *mut ffi::DBusConnection) -> Result<TxRx, Error> {
        let handle = ConnHandle(ptr);

        /* No, we don't want our app to suddenly quit if dbus goes down */
        unsafe { ffi::dbus_connection_set_exit_on_disconnect(ptr, 0) };

        let c = TxRx { handle };

        Ok(c)
    }


    /// Creates a new D-Bus connection.
    ///
    /// Blocking: until the connection is up and running. 
    pub fn get_private(bus: BusType) -> Result<TxRx, Error> {
        let mut e = Error::empty();
        let conn = unsafe { ffi::dbus_bus_get_private(bus, e.get_mut()) };
        if conn == ptr::null_mut() {
            return Err(e)
        }
        Self::conn_from_ptr(conn)
    }

    /// Creates a new D-Bus connection to a remote address.
    ///
    /// Note: for all common cases (System / Session bus) you probably want "get_private" instead.
    ///
    /// Blocking: until the connection is established.
    pub fn open_private(address: &str) -> Result<TxRx, Error> {
        let mut e = Error::empty();
        let conn = unsafe { ffi::dbus_connection_open_private(to_c_str(address).as_ptr(), e.get_mut()) };
        if conn == ptr::null_mut() {
            return Err(e)
        }
        Self::conn_from_ptr(conn)
    }

    /// Registers a new D-Bus connection with the bus.
    ///
    /// Note: `get_private` does this automatically, useful with `open_private`
    ///
    /// Blocking: until a "Hello" response is received from the server.
    pub fn register(&mut self) -> Result<(), Error> {
        // This function needs to take &mut self, because it changes unique_name and unique_name takes a &self
        let mut e = Error::empty();
        if unsafe { ffi::dbus_bus_register(self.conn(), e.get_mut()) == 0 } {
            Err(e)
        } else {
            Ok(())
        }
    }

    /// Gets whether the connection is currently open.
    pub fn is_connected(&self) -> bool {
        unsafe { ffi::dbus_connection_get_is_connected(self.conn()) != 0 }
    }

    /// Get the connection's unique name.
    ///
    /// It's usually something like ":1.54"
    pub fn unique_name(&self) -> Option<&str> {
        let c = unsafe { ffi::dbus_bus_get_unique_name(self.conn()) };
        if c == ptr::null_mut() { return None; }
        let s = unsafe { CStr::from_ptr(c) };
        str::from_utf8(s.to_bytes()).ok()
    }


    /// Puts a message into libdbus out queue. Use "flush" or "read_write" to make sure it is sent over the wire.
    ///
    /// Returns a serial number than can be used to match against a reply.
    pub fn send(&self, msg: Message) -> Result<u32, ()> {
        let mut serial = 0u32;
        let r = unsafe { ffi::dbus_connection_send(self.conn(), msg.ptr(), &mut serial) };
        if r == 0 { return Err(()); }
        Ok(serial)
    }

    /// Flush the queue of outgoing messages.
    /// 
    /// Blocking: until the outgoing queue is empty.
    pub fn flush(&self) { unsafe { ffi::dbus_connection_flush(self.conn()) } }

    /// Read and write to the connection.
    ///
    /// Incoming messages are put in the internal queue, outgoing messages are written.
    ///
    /// Blocking: If there are no messages, for up to timeout_ms milliseconds, or forever if timeout_ms is None.
    /// For non-blocking behaviour, set timeout_ms to Some(0).
    pub fn read_write(&self, timeout_ms: Option<i32>) -> Result<(), ()> {
        let t = timeout_ms.unwrap_or(-1);
        if unsafe { ffi::dbus_connection_read_write(self.conn(), t) == 0 } {
            Err(())
        } else {
            Ok(())
        }
    }

    /// Removes a message from the incoming queue, or returns None if the queue is empty.
    ///
    /// Use "read_write" first, so that messages are put into the incoming queue.
    /// For unhandled messages, please call MessageDispatcher::default_dispatch to return
    /// default replies for method calls.
    pub fn pop_message(&self) -> Option<Message> {
        let mptr = unsafe { ffi::dbus_connection_pop_message(self.conn()) };
        if mptr == ptr::null_mut() {
            None
        } else {
            Some(Message::from_ptr(mptr, false))
        }
    }

    /// Get an up-to-date list of file descriptors to watch.
    ///
    /// Might be changed into something that allows for callbacks when the watch list is changed.
    pub fn watch_fds(&mut self) -> Result<Vec<Watch>, ()> {
        extern "C" fn add_watch_cb(watch: *mut ffi::DBusWatch, data: *mut c_void) -> u32 {
            unsafe {
                let wlist: &mut Vec<Watch> = &mut *(data as *mut _);
                wlist.push(Watch::from_raw(watch));
            }
            1
        }
        let mut r = vec!();
        if unsafe { ffi::dbus_connection_set_watch_functions(self.conn(),
            Some(add_watch_cb), None, None, &mut r as *mut _ as *mut _, None) } == 0 { return Err(()) }
        assert!(unsafe { ffi::dbus_connection_set_watch_functions(self.conn(),
            None, None, None, ptr::null_mut(), None) } != 0);
        Ok(r)
    }
}

#[test]
fn test_txrx_send_sync() {
    fn is_send<T: Send>(_: &T) {}
    fn is_sync<T: Sync>(_: &T) {}
    let c = TxRx::get_private(BusType::Session).unwrap();
    is_send(&c);
    is_sync(&c);
}

#[test]
fn txrx_simple_test() {
    let mut c = TxRx::get_private(BusType::Session).unwrap();
    assert!(c.is_connected());
    let fds = c.watch_fds().unwrap();
    println!("{:?}", fds);
    assert!(fds.len() > 0);
    let m = Message::new_method_call("org.freedesktop.DBus", "/", "org.freedesktop.DBus", "ListNames").unwrap();
    let reply = c.send(m).unwrap();
    let my_name = c.unique_name().unwrap();
    loop {
        while let Some(mut msg) = c.pop_message() {
            println!("{:?}", msg);
            if msg.get_reply_serial() == Some(reply) {
                let r = msg.as_result().unwrap();
                let z: ::arg::Array<&str, _>  = r.get1().unwrap();
                for n in z {
                    println!("{}", n);
                    if n == my_name { return; } // Hooray, we found ourselves!
                }
                assert!(false);
            } else if let Some(r) = crate::MessageDispatcher::<()>::default_dispatch(&msg) {
                c.send(r).unwrap();
            }
        }
        c.read_write(Some(100)).unwrap();
    }
}

