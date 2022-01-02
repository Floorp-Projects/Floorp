/* This example creates a D-Bus server with the following functionality:
   It registers the "com.example.dbustest" name, creates a "/hello" object path,
   which has an "com.example.dbustest" interface.

   The interface has a "Hello" method (which takes no arguments and returns a string),
   and a "HelloHappened" signal (with a string argument) which is sent every time
   someone calls the "Hello" method.
*/


extern crate dbus;

use std::sync::Arc;
use dbus::{Connection, BusType, NameFlag};
use dbus::tree::Factory;

fn main() {
    // Let's start by starting up a connection to the session bus and register a name.
    let c = Connection::get_private(BusType::Session).unwrap();
    c.register_name("com.example.dbustest", NameFlag::ReplaceExisting as u32).unwrap();

    // The choice of factory tells us what type of tree we want,
    // and if we want any extra data inside. We pick the simplest variant.
    let f = Factory::new_fn::<()>();

    // We create the signal first, since we'll need it in both inside the method callback
    // and when creating the tree.
    let signal = Arc::new(f.signal("HelloHappened", ()).sarg::<&str,_>("sender"));
    let signal2 = signal.clone();

    // We create a tree with one object path inside and make that path introspectable.
    let tree = f.tree(()).add(f.object_path("/hello", ()).introspectable().add(

        // We add an interface to the object path...
        f.interface("com.example.dbustest", ()).add_m(

            // ...and a method inside the interface.
            f.method("Hello", (), move |m| {

                // This is the callback that will be called when another peer on the bus calls our method.
                // the callback receives "MethodInfo" struct and can return either an error, or a list of
                // messages to send back.

                let name: &str = m.msg.read1()?;
                let s = format!("Hello {}!", name);
                let mret = m.msg.method_return().append1(s);

                let sig = signal.msg(m.path.get_name(), m.iface.get_name())
                    .append1(&*name);

                // Two messages will be returned - one is the method return (and should always be there),
                // and in our case we also have a signal we want to send at the same time.
                Ok(vec!(mret, sig))

            // Our method has one output argument and one input argument.
            }).outarg::<&str,_>("reply")
            .inarg::<&str,_>("name")

        // We also add the signal to the interface. This is mainly for introspection.
        ).add_s(signal2)
    ));

    // We register all object paths in the tree.
    tree.set_registered(&c, true).unwrap();

    // We add the tree to the connection so that incoming method calls will be handled
    // automatically during calls to "incoming".
    c.add_handler(tree);

    // Serve other peers forever.
    loop { c.incoming(1000).next(); }
}
