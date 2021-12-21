extern crate core_foundation;
extern crate coremidi;

use coremidi::{Client, Notification};

use core_foundation::runloop::{kCFRunLoopDefaultMode, CFRunLoopRunInMode};

fn main() {
    println!("Logging MIDI Client Notifications");
    println!("Will Quit Automatically After 10 Seconds");
    println!();

    let _client = Client::new_with_notifications("example-client", print_notification).unwrap();

    // As the MIDIClientCreate docs say (https://developer.apple.com/documentation/coremidi/1495360-midiclientcreate),
    // notifications will be delivered on the run loop that was current when
    // Client was created.
    //
    // In order to actually receive the notifications, a run loop must be
    // running. Since this sample app does not use an app framework like
    // UIApplication or NSApplication, it does not have a run loop running yet.
    // So we start one that lasts for 10 seconds with the following line.
    //
    // You may not have to do this in your app - see https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Multithreading/RunLoopManagement/RunLoopManagement.html#//apple_ref/doc/uid/10000057i-CH16-SW24
    // for information about when run loops are running automatically.
    unsafe { CFRunLoopRunInMode(kCFRunLoopDefaultMode, 10.0, 0) };
}

fn print_notification(notification: &Notification) {
    println!("Received Notification: {:?} \r", notification);
}
