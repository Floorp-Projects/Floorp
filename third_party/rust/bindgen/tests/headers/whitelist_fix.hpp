// bindgen-flags: --whitelist-function 'Servo_.*' --blacklist-type Test --raw-line "pub enum Test {}"

struct Test {};
extern "C" void Servo_Test(Test* a);
