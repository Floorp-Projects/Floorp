extern crate android_logger;
extern crate log;

#[test]
fn default_init() {
    android_logger::init_once(Default::default());

    // android_logger has default log level "off"
    assert_eq!(log::max_level(), log::LevelFilter::Off);
}