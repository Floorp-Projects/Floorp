extern crate devd_rs;

use devd_rs::*;

fn main() {
    let mut ctx = Context::new().unwrap();
    loop {
        if let Ok(ev) = ctx.wait_for_event(1000) {
            println!("{:?}", ev);
        }
    }
}
