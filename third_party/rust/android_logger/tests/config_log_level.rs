extern crate android_logger;
extern crate log;

#[test]
fn config_log_level() {
    android_logger::init_once(android_logger::Config::default().with_min_level(log::Level::Trace));

    assert_eq!(log::max_level(), log::LevelFilter::Trace);
}