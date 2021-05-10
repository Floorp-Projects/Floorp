#[macro_use]
extern crate ioctl_sys;

ioctl!(bad kiocsound with 0x4B2F);
ioctl!(none drm_ioctl_set_master with b'd', 0x1e);
ioctl!(read ev_get_version with b'E', 0x01; u32);
ioctl!(write ev_set_repeat with b'E', 0x03; [u32; 2]);

ioctl!(try none drm_ioctl_set_master2 with b'd', 0x1e);
ioctl!(try read ev_get_version2 with b'E', 0x01; u32);
ioctl!(try read0 ev_get_version3 with b'E', 0x01; u32);
ioctl!(try write ev_set_repeat2 with b'E', 0x03; [u32; 2]);

fn main() {
    let mut x = 0;
    let ret = unsafe { ev_get_version(0, &mut x) };
    println!("returned {}, x = {}", ret, x);
    let mut x2 = 0;
    let ret2 = unsafe { ev_get_version2(0, &mut x2) };
    println!("returned {:?}, x = {}", ret2, x2);
    let ret3 = unsafe { ev_get_version3(0) };
    println!("returned {:?}", ret3);
}
