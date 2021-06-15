use super::*;

#[test]
fn unit() {
    assert!(SimpleErr.backtrace().is_none());
}

#[test]
fn named_implicit_no_backtrace() {
    derive_display!(TestErr);
    #[derive(Default, Debug, Error)]
    struct TestErr {
        field: i32,
    }

    assert!(TestErr::default().backtrace().is_none());
}

#[test]
fn named_implicit_backtrace_by_field_name() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr {
        backtrace: MyBacktrace,
        field: i32,
    }

    type MyBacktrace = Backtrace;

    let err = TestErr {
        backtrace: Backtrace::force_capture(),
        field: 0,
    };
    assert!(err.backtrace().is_some());
    assert_bt!(==, err);
}

#[test]
fn named_implicit_backtrace_by_field_type() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr {
        implicit_backtrace: Backtrace,
        field: i32,
    }

    let err = TestErr {
        implicit_backtrace: Backtrace::force_capture(),
        field: 0,
    };
    assert!(err.backtrace().is_some());
    assert_bt!(==, err, implicit_backtrace);
}

#[test]
fn named_explicit_no_backtrace_by_field_name() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr {
        #[error(not(backtrace))]
        backtrace: MyBacktrace,
        field: i32,
    }

    type MyBacktrace = Backtrace;

    assert!(TestErr {
        backtrace: Backtrace::force_capture(),
        field: 0
    }
    .backtrace()
    .is_none());
}

#[test]
fn named_explicit_no_backtrace_by_field_type() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr {
        #[error(not(backtrace))]
        implicit_backtrace: Backtrace,
        field: i32,
    }

    assert!(TestErr {
        implicit_backtrace: Backtrace::force_capture(),
        field: 0
    }
    .backtrace()
    .is_none());
}

#[test]
fn named_explicit_backtrace() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr {
        #[error(backtrace)]
        explicit_backtrace: MyBacktrace,
        field: i32,
    }

    type MyBacktrace = Backtrace;

    let err = TestErr {
        explicit_backtrace: Backtrace::force_capture(),
        field: 0,
    };
    assert!(err.backtrace().is_some());
    assert_bt!(==, err, explicit_backtrace);
}

#[test]
fn named_explicit_no_backtrace_redundant() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr {
        #[error(not(backtrace))]
        not_backtrace: MyBacktrace,
        #[error(not(backtrace))]
        field: i32,
    }

    type MyBacktrace = Backtrace;

    assert!(TestErr {
        not_backtrace: Backtrace::force_capture(),
        field: 0
    }
    .backtrace()
    .is_none());
}

#[test]
fn named_explicit_backtrace_by_field_name_redundant() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr {
        #[error(backtrace)]
        backtrace: MyBacktrace,
        field: i32,
    }

    type MyBacktrace = Backtrace;

    let err = TestErr {
        backtrace: Backtrace::force_capture(),
        field: 0,
    };
    assert!(err.backtrace().is_some());
    assert_bt!(==, err);
}

#[test]
fn named_explicit_backtrace_by_field_type_redundant() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr {
        #[error(backtrace)]
        implicit_backtrace: Backtrace,
        field: i32,
    }

    let err = TestErr {
        implicit_backtrace: Backtrace::force_capture(),
        field: 0,
    };
    assert!(err.backtrace().is_some());
    assert_bt!(==, err, implicit_backtrace);
}

#[test]
fn named_explicit_supresses_implicit() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr {
        #[error(backtrace)]
        not_backtrace: MyBacktrace,
        backtrace: Backtrace,
        field: i32,
    }

    type MyBacktrace = Backtrace;

    let err = TestErr {
        not_backtrace: Backtrace::force_capture(),
        backtrace: (|| Backtrace::force_capture())(), // ensure backtraces are different
        field: 0,
    };

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, not_backtrace);
    assert_bt!(!=, err);
}

#[test]
fn unnamed_implicit_no_backtrace() {
    derive_display!(TestErr);
    #[derive(Default, Debug, Error)]
    struct TestErr(i32, i32);

    assert!(TestErr::default().backtrace().is_none());
}

#[test]
fn unnamed_implicit_backtrace() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr(Backtrace, i32, i32);

    let err = TestErr(Backtrace::force_capture(), 0, 0);
    assert!(err.backtrace().is_some());
    assert_bt!(==, err, 0);
}

#[test]
fn unnamed_explicit_no_backtrace() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr(#[error(not(backtrace))] Backtrace, i32);

    assert!(TestErr(Backtrace::force_capture(), 0).backtrace().is_none());
}

#[test]
fn unnamed_explicit_backtrace() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr(#[error(backtrace)] MyBacktrace, i32, i32);

    type MyBacktrace = Backtrace;

    let err = TestErr(Backtrace::force_capture(), 0, 0);
    assert!(err.backtrace().is_some());
    assert_bt!(==, err, 0);
}

#[test]
fn unnamed_explicit_no_backtrace_redundant() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr(
        #[error(not(backtrace))] MyBacktrace,
        #[error(not(backtrace))] i32,
    );

    type MyBacktrace = Backtrace;

    assert!(TestErr(Backtrace::force_capture(), 0).backtrace().is_none());
}

#[test]
fn unnamed_explicit_backtrace_redundant() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr(#[error(backtrace)] Backtrace, i32, i32);

    let err = TestErr(Backtrace::force_capture(), 0, 0);
    assert!(err.backtrace().is_some());
    assert_bt!(==, err, 0);
}

#[test]
fn unnamed_explicit_supresses_implicit() {
    derive_display!(TestErr);
    #[derive(Debug, Error)]
    struct TestErr(#[error(backtrace)] MyBacktrace, Backtrace, i32);

    type MyBacktrace = Backtrace;

    let err = TestErr(
        Backtrace::force_capture(),
        (|| Backtrace::force_capture())(), // ensure backtraces are different
        0,
    );

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, 0);
    assert_bt!(!=, err, 1);
}
