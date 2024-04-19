//! Testing macros

macro_rules! test_if {
    ($cfg_tt:tt: $it:item) => {
        #[cfg(any(
                                                            // Test everything if:
                                                            //
                                                            // * tests are enabled,
                                                            // * no features about exclusively testing
                                                            //   specific vector classes are enabled
                                                            all(test, not(any(
                                                                test_v16,
                                                                test_v32,
                                                                test_v64,
                                                                test_v128,
                                                                test_v256,
                                                                test_v512,
                                                                test_none,  // disables all tests
                                                            ))),
                                                            // Test if:
                                                            //
                                                            // * tests are enabled
                                                            // * a particular cfg token tree returns true
                                                            all(test, $cfg_tt),
                                                        ))]
        $it
    };
}

#[cfg(test)]
#[allow(unused)]
macro_rules! ref_ {
    ($anything:tt) => {
        &$anything
    };
}

#[cfg(test)]
#[allow(unused)]
macro_rules! ref_mut_ {
    ($anything:tt) => {
        &mut $anything
    };
}
