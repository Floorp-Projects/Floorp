use std::{thread, time::Duration};

fn main() {
    let start = zeitstempel::now();
    println!("Now: {}", start);

    thread::sleep(Duration::from_secs(2));

    let diff = zeitstempel::now() - start;
    println!("Diff: {} ms", Duration::from_nanos(diff).as_millis());
}
