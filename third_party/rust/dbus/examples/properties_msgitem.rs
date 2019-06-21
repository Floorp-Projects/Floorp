extern crate dbus;

use dbus::{Connection, BusType, Props};

fn main() {
    let c = Connection::get_private(BusType::System).unwrap();
    let p = Props::new(&c, "org.freedesktop.PolicyKit1", "/org/freedesktop/PolicyKit1/Authority",
        "org.freedesktop.PolicyKit1.Authority", 10000);
    println!("BackendVersion: {:?}", p.get("BackendVersion").unwrap())
}
