use serial_test::local_serial_core;

#[test]
fn test_empty_serial_call() {
    local_serial_core("beta", || {
        println!("Bar");
    });
}
