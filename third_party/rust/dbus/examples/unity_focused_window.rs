extern crate dbus;

// Tracks currently focused window under the Unity desktop by listening to the
// FocusedWindowChanged signal. The signal contains "window_id", "app_id" and "stage",
// we print only "app_id". 

use dbus::{Connection, BusType, ConnectionItem};

fn focus_msg(ci: &ConnectionItem) -> Option<&str> {
    let m = if let &ConnectionItem::Signal(ref s) = ci { s } else { return None };
    if &*m.interface().unwrap() != "com.canonical.Unity.WindowStack" { return None };
    if &*m.member().unwrap() != "FocusedWindowChanged" { return None };
    let (_, app) = m.get2::<u32, &str>();
    app
}

fn main() {
    let c = Connection::get_private(BusType::Session).unwrap();
    c.add_match("interface='com.canonical.Unity.WindowStack',member='FocusedWindowChanged'").unwrap();

    for i in c.iter(1000) {
        if let Some(app) = focus_msg(&i) { println!("{} has now focus.", app) };
    }
}

