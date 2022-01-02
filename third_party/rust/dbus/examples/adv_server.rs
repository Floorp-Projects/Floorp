// More advanced server example.

// This is supposed to look like a D-Bus service that allows the user to manipulate storage devices.

// Note: in the dbus-codegen/example directory, there is a version of this example where dbus-codegen
// was used to create some boilerplate code - feel free to compare the two examples.

extern crate dbus;

use std::sync::Arc;
use std::sync::mpsc;
use std::cell::Cell;
use std::thread;

use dbus::{Connection, BusType, tree, Path};
use dbus::tree::{Interface, Signal, MTFn, Access, MethodErr, EmitsChangedSignal};

// Our storage device
#[derive(Debug)]
struct Device {
    description: String,
    path: Path<'static>,
    index: i32,
    online: Cell<bool>,
    checking: Cell<bool>,
}

// Every storage device has its own object path.
// We therefore create a link from the object path to the Device.
#[derive(Copy, Clone, Default, Debug)]
struct TData;
impl tree::DataType for TData {
    type Tree = ();
    type ObjectPath = Arc<Device>;
    type Property = ();
    type Interface = ();
    type Method = ();
    type Signal = ();
}


impl Device {
    // Creates a "test" device (not a real one, since this is an example).
    fn new_bogus(index: i32) -> Device {
        Device {
            description: format!("This is device {}, which is {}.", index,
                ["totally awesome", "really fancy", "still going strong"][(index as usize) % 3]),
            path: format!("/Device{}", index).into(),
            index: index,
            online: Cell::new(index % 2 == 0),
            checking: Cell::new(false),
        }
    } 
}

// Here's where we implement the code for our interface.
fn create_iface(check_complete_s: mpsc::Sender<i32>) -> (Interface<MTFn<TData>, TData>, Arc<Signal<TData>>) {
    let f = tree::Factory::new_fn();

    let check_complete = Arc::new(f.signal("CheckComplete", ()));

    (f.interface("com.example.dbus.rs.device", ())
        // The online property can be both set and get
        .add_p(f.property::<bool,_>("online", ())
            .access(Access::ReadWrite)
            .on_get(|i, m| {
                let dev: &Arc<Device> = m.path.get_data();
                i.append(dev.online.get());
                Ok(())
            })
            .on_set(|i, m| {
                let dev: &Arc<Device> = m.path.get_data();
                let b: bool = try!(i.read());
                if b && dev.checking.get() {
                    return Err(MethodErr::failed(&"Device currently under check, cannot bring online"))
                }
                dev.online.set(b);
                Ok(())
            })
        )
        // The "checking" property is read only
        .add_p(f.property::<bool,_>("checking", ())
            .emits_changed(EmitsChangedSignal::False)
            .on_get(|i, m| {
                let dev: &Arc<Device> = m.path.get_data();
                i.append(dev.checking.get());
                Ok(())
            })
        )
        // ...and so is the "description" property
        .add_p(f.property::<&str,_>("description", ())
            .emits_changed(EmitsChangedSignal::Const)
            .on_get(|i, m| {
                let dev: &Arc<Device> = m.path.get_data();
                i.append(&dev.description);
                Ok(())
            })
        )
        // ...add a method for starting a device check... 
        .add_m(f.method("check", (), move |m| {
            let dev: &Arc<Device> = m.path.get_data();
            if dev.checking.get() {
                return Err(MethodErr::failed(&"Device currently under check, cannot start another check"))
            }
            if dev.online.get() {
                return Err(MethodErr::failed(&"Device is currently online, cannot start check"))
            }
            dev.checking.set(true);

            // Start some lengthy processing in a separate thread...
            let devindex = dev.index;
            let ch = check_complete_s.clone();
            thread::spawn(move || {

                // Bogus check of device
                use std::time::Duration;
                thread::sleep(Duration::from_secs(15));

                // Tell main thread that we finished
                ch.send(devindex).unwrap();
            });
            Ok(vec!(m.msg.method_return()))
        }))
        // Indicate that we send a special signal once checking has completed.
        .add_s(check_complete.clone())
    , check_complete)
}

fn create_tree(devices: &[Arc<Device>], iface: &Arc<Interface<MTFn<TData>, TData>>)
    -> tree::Tree<MTFn<TData>, TData> {

    let f = tree::Factory::new_fn();
    let mut tree = f.tree(());
    for dev in devices {
        tree = tree.add(f.object_path(dev.path.clone(), dev.clone())
            .introspectable()
            .add(iface.clone())
        );
    }
    tree 
}

fn run() -> Result<(), Box<std::error::Error>> {
    // Create our bogus devices
    let devices: Vec<Arc<Device>> = (0..10).map(|i| Arc::new(Device::new_bogus(i))).collect();

    // Create tree
    let (check_complete_s, check_complete_r) = mpsc::channel::<i32>(); 
    let (iface, sig) = create_iface(check_complete_s);
    let tree = create_tree(&devices, &Arc::new(iface));

    // Setup DBus connection
    let c = try!(Connection::get_private(BusType::Session));
    try!(c.register_name("com.example.dbus.rs.advancedserverexample", 0));
    try!(tree.set_registered(&c, true));

    // ...and serve incoming requests.
    c.add_handler(tree);
    loop {
        // Wait for incoming messages. This will block up to one second.
        // Discard the result - relevant messages have already been handled.
        c.incoming(1000).next();

        // Do all other things we need to do in our main loop.
        if let Ok(idx) = check_complete_r.try_recv() {
            let dev = &devices[idx as usize];
            dev.checking.set(false);
            try!(c.send(sig.msg(&dev.path, &"com.example.dbus.rs.device".into())).map_err(|_| "Sending DBus signal failed"));
        }
    }
}

fn main() {
    if let Err(e) = run() {
        println!("{}", e);
    }
}
