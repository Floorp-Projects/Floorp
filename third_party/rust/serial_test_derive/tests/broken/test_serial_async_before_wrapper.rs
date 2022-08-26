use serial_test_derive::serial;

#[serial]
#[actix_rt::test]
async fn test_async_serial_no_arg_actix() {}

fn main() {}