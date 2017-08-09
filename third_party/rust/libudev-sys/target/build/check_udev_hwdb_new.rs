extern "C" {
    fn udev_hwdb_new();
}

fn main() {
    unsafe {
        udev_hwdb_new();
    }
}
