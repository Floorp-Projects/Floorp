extern crate android_logger;
extern crate log;

#[test]
fn multiple_init() {
    android_logger::init_once(android_logger::Config::default().with_min_level(log::Level::Trace));

    // Second initialization should be silently ignored
    android_logger::init_once(android_logger::Config::default().with_min_level(log::Level::Error));

    assert_eq!(log::max_level(), log::LevelFilter::Trace);
}