use super::*;

derive_display!(TestErr, T);
#[derive(Debug, Error)]
enum TestErr<T> {
    Unit,
    NamedImplicitNoBacktrace {
        field: T,
    },
    NamedImplicitBacktraceByFieldName {
        backtrace: MyBacktrace,
        field: T,
    },
    NamedImplicitBacktraceByFieldType {
        implicit_backtrace: Backtrace,
        field: T,
    },
    NamedExplicitNoBacktraceByFieldName {
        #[error(not(backtrace))]
        backtrace: MyBacktrace,
        field: T,
    },
    NamedExplicitNoBacktraceByFieldType {
        #[error(not(backtrace))]
        implicit_backtrace: Backtrace,
        field: T,
    },
    NamedExplicitBacktrace {
        #[error(backtrace)]
        explicit_backtrace: MyBacktrace,
        field: T,
    },
    NamedExplicitNoBacktraceRedundant {
        #[error(not(backtrace))]
        not_backtrace: MyBacktrace,
        #[error(not(backtrace))]
        field: T,
    },
    NamedExplicitBacktraceByFieldNameRedundant {
        #[error(backtrace)]
        backtrace: MyBacktrace,
        field: T,
    },
    NamedExplicitBacktraceByFieldTypeRedundant {
        #[error(backtrace)]
        implicit_backtrace: Backtrace,
        field: T,
    },
    NamedExplicitSupressesImplicit {
        #[error(backtrace)]
        not_backtrace: MyBacktrace,
        backtrace: Backtrace,
        field: T,
    },
    UnnamedImplicitNoBacktrace(T, T),
    UnnamedImplicitBacktrace(Backtrace, T, T),
    UnnamedExplicitNoBacktrace(#[error(not(backtrace))] Backtrace, T),
    UnnamedExplicitBacktrace(#[error(backtrace)] MyBacktrace, T, T),
    UnnamedExplicitNoBacktraceRedundant(
        #[error(not(backtrace))] MyBacktrace,
        #[error(not(backtrace))] T,
    ),
    UnnamedExplicitBacktraceRedundant(#[error(backtrace)] Backtrace, T, T),
    UnnamedExplicitSupressesImplicit(#[error(backtrace)] MyBacktrace, Backtrace, T),
}

impl<T> TestErr<T> {
    fn get_stored_backtrace(&self) -> &Backtrace {
        match self {
            Self::NamedImplicitBacktraceByFieldName { backtrace, .. } => backtrace,
            Self::NamedImplicitBacktraceByFieldType {
                implicit_backtrace, ..
            } => implicit_backtrace,
            Self::NamedExplicitBacktrace {
                explicit_backtrace, ..
            } => explicit_backtrace,
            Self::NamedExplicitBacktraceByFieldNameRedundant { backtrace, .. } => {
                backtrace
            }
            Self::NamedExplicitBacktraceByFieldTypeRedundant {
                implicit_backtrace,
                ..
            } => implicit_backtrace,
            Self::NamedExplicitSupressesImplicit { not_backtrace, .. } => not_backtrace,
            Self::UnnamedImplicitBacktrace(backtrace, _, _) => backtrace,
            Self::UnnamedExplicitBacktrace(backtrace, _, _) => backtrace,
            Self::UnnamedExplicitBacktraceRedundant(backtrace, _, _) => backtrace,
            Self::UnnamedExplicitSupressesImplicit(backtrace, _, _) => backtrace,
            _ => panic!("ERROR IN TEST IMPLEMENTATION"),
        }
    }

    fn get_unused_backtrace(&self) -> &Backtrace {
        match self {
            Self::NamedExplicitSupressesImplicit { backtrace, .. } => backtrace,
            Self::UnnamedExplicitSupressesImplicit(_, backtrace, _) => backtrace,
            _ => panic!("ERROR IN TEST IMPLEMENTATION"),
        }
    }
}

type MyBacktrace = Backtrace;

#[test]
fn unit() {
    assert!(TestErr::<i32>::Unit.backtrace().is_none());
}

#[test]
fn named_implicit_no_backtrace() {
    let err = TestErr::NamedImplicitNoBacktrace { field: 0 };

    assert!(err.backtrace().is_none());
}

#[test]
fn named_implicit_backtrace_by_field_name() {
    let err = TestErr::NamedImplicitBacktraceByFieldName {
        backtrace: Backtrace::force_capture(),
        field: 0,
    };

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, .get_stored_backtrace);
}

#[test]
fn named_implicit_backtrace_by_field_type() {
    let err = TestErr::NamedImplicitBacktraceByFieldType {
        implicit_backtrace: Backtrace::force_capture(),
        field: 0,
    };

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, .get_stored_backtrace);
}

#[test]
fn named_explicit_no_backtrace_by_field_name() {
    let err = TestErr::NamedExplicitNoBacktraceByFieldName {
        backtrace: Backtrace::force_capture(),
        field: 0,
    };

    assert!(err.backtrace().is_none());
}

#[test]
fn named_explicit_no_backtrace_by_field_type() {
    let err = TestErr::NamedExplicitNoBacktraceByFieldType {
        implicit_backtrace: Backtrace::force_capture(),
        field: 0,
    };

    assert!(err.backtrace().is_none());
}

#[test]
fn named_explicit_backtrace() {
    let err = TestErr::NamedExplicitBacktrace {
        explicit_backtrace: Backtrace::force_capture(),
        field: 0,
    };

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, .get_stored_backtrace);
}

#[test]
fn named_explicit_no_backtrace_redundant() {
    let err = TestErr::NamedExplicitNoBacktraceRedundant {
        not_backtrace: Backtrace::force_capture(),
        field: 0,
    };

    assert!(err.backtrace().is_none());
}

#[test]
fn named_explicit_backtrace_by_field_name_redundant() {
    let err = TestErr::NamedExplicitBacktraceByFieldNameRedundant {
        backtrace: Backtrace::force_capture(),
        field: 0,
    };

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, .get_stored_backtrace);
}

#[test]
fn named_explicit_backtrace_by_field_type_redundant() {
    let err = TestErr::NamedExplicitBacktraceByFieldTypeRedundant {
        implicit_backtrace: Backtrace::force_capture(),
        field: 0,
    };

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, .get_stored_backtrace);
}

#[test]
fn named_explicit_supresses_implicit() {
    let err = TestErr::NamedExplicitSupressesImplicit {
        not_backtrace: Backtrace::force_capture(),
        backtrace: (|| Backtrace::force_capture())(), // ensure backtraces are different
        field: 0,
    };

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, .get_stored_backtrace);
    assert_bt!(!=, err, .get_unused_backtrace);
}

#[test]
fn unnamed_implicit_no_backtrace() {
    let err = TestErr::UnnamedImplicitNoBacktrace(0, 0);

    assert!(err.backtrace().is_none());
}

#[test]
fn unnamed_implicit_backtrace() {
    let err = TestErr::UnnamedImplicitBacktrace(Backtrace::force_capture(), 0, 0);

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, .get_stored_backtrace);
}

#[test]
fn unnamed_explicit_no_backtrace() {
    let err = TestErr::UnnamedExplicitNoBacktrace(Backtrace::force_capture(), 0);

    assert!(err.backtrace().is_none());
}

#[test]
fn unnamed_explicit_backtrace() {
    let err = TestErr::UnnamedExplicitBacktrace(Backtrace::force_capture(), 0, 0);

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, .get_stored_backtrace);
}

#[test]
fn unnamed_explicit_no_backtrace_redundant() {
    let err =
        TestErr::UnnamedExplicitNoBacktraceRedundant(Backtrace::force_capture(), 0);

    assert!(err.backtrace().is_none());
}

#[test]
fn unnamed_explicit_backtrace_redundant() {
    let err =
        TestErr::UnnamedExplicitBacktraceRedundant(Backtrace::force_capture(), 0, 0);

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, .get_stored_backtrace);
}

#[test]
fn unnamed_explicit_supresses_implicit() {
    let err = TestErr::UnnamedExplicitSupressesImplicit(
        Backtrace::force_capture(),
        (|| Backtrace::force_capture())(), // ensure backtraces are different
        0,
    );

    assert!(err.backtrace().is_some());
    assert_bt!(==, err, .get_stored_backtrace);
    assert_bt!(!=, err, .get_unused_backtrace);
}
