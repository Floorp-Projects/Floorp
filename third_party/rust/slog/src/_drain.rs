/// Logging drain
///
/// Drains generally mean destination for logs, but slog generalize the
/// term. `Drain`s are responsible for filtering, modifying, formatting
/// and writing the log records into given destination.
///
/// Implementing this trait allows writing own Drains, that can be combined
/// with other drains.
pub trait Drain {
    /// Type of potential errors returned during logging
    type Error;
    /// Log one logging record
    ///
    /// Every logging `Record` built from a logging statement (eg.
    /// `info!(...)`), and key-value lists of a `Logger` it was executed on
    /// will be passed to the root drain registered during `Logger::root`.
    ///
    /// Typically `Drain`s:
    ///
    /// * pass this information (or not) to the sub-logger(s) (filters)
    /// * format and write the information the a destination (writers)
    /// * deal with the errors returned from the sub-logger(s)
    fn log(&self, info: &Record, &OwnedKeyValueList) -> result::Result<(), Self::Error>;
}

impl<D: Drain+?Sized> Drain for Box<D> {
    type Error = D::Error;
    fn log(&self, info: &Record, o: &OwnedKeyValueList) -> result::Result<(), D::Error> {
        (**self).log(info, o)
    }
}

impl<D: Drain+?Sized> Drain for Arc<D> {
    type Error = D::Error;
    fn log(&self, info: &Record, o: &OwnedKeyValueList) -> result::Result<(), D::Error> {
        (**self).log(info, o)
    }
}

/// Convenience methods for building drains
///
/// `DrainExt` is implemented for every `Drain` and contains
/// convenience methods.
pub trait DrainExt: Sized + Drain {
    /// Map logging errors returned by this drain
    ///
    /// `f` is a closure that takes `Drain::Error` returned by a given
    /// drain, and returns new error of potentially different type
    fn map_err<F, E>(self, f : F) -> MapError<Self, E> where F : 'static + Sync + Send + Fn(<Self as Drain>::Error) -> E {
        MapError::new(self, f)
    }

    /// Make `Self` ignore and not report any error
    fn ignore_err(self) -> IgnoreErr<Self> {
        IgnoreErr::new(self)
    }

    /// Make `Self` panic when returning any errors
    fn fuse(self) -> Fuse<Self> where <Self as Drain>::Error : fmt::Display {
       Fuse::new(self)
    }
}

impl<D : Drain> DrainExt for D {}

/// `Drain` discarding everything
///
/// `/dev/null` of `Drain`s
pub struct Discard;

impl Drain for Discard {
    type Error = Never;
    fn log(&self, _: &Record, _: &OwnedKeyValueList) -> result::Result<(), Never> {
        Ok(())
    }
}

/// `Drain` filtering records
///
/// Wraps another `Drain` and passes `Record`s to it, only if they satisfy a
/// given condition.
pub struct Filter<D: Drain> {
    drain: D,
    // eliminated dynamic dispatch, after rust learns `-> impl Trait`
    cond: Box<Fn(&Record) -> bool + 'static + Send + Sync>,
}

impl<D: Drain> Filter<D> {
    /// Create `Filter` wrapping given `drain`
    pub fn new<F: 'static + Sync + Send + Fn(&Record) -> bool>(drain: D, cond: F) -> Self {
        Filter {
            drain: drain,
            cond: Box::new(cond),
        }
    }
}

impl<D: Drain> Drain for Filter<D> {
    type Error = D::Error;
    fn log(&self,
           info: &Record,
           logger_values: &OwnedKeyValueList)
           -> result::Result<(), Self::Error> {
        if (self.cond)(&info) {
            self.drain.log(info, logger_values)
        } else {
            Ok(())
        }
    }
}


/// `Drain` mapping error returned by another `Drain`
///
/// See `DrainExt::map_err` for convenience function.
pub struct MapError<D: Drain, E> {
    drain: D,
    // eliminated dynamic dispatch, after rust learns `-> impl Trait`
    map_fn: Box<Fn(D::Error) -> E + 'static+ Send+Sync>,
}

impl<D: Drain, E> MapError<D, E> {
    /// Create `Filter` wrapping given `drain`
    pub fn new<F: 'static + Sync + Send + Fn(<D as Drain>::Error) -> E>(drain: D, map_fn: F) -> Self {
        MapError {
            drain: drain,
            map_fn: Box::new(map_fn),
        }
    }
}

impl<D: Drain, E> Drain for MapError<D, E> {
    type Error = E;
    fn log(&self,
           info: &Record,
           logger_values: &OwnedKeyValueList)
           -> result::Result<(), Self::Error> {
            self.drain.log(info, logger_values).map_err(|e| (self.map_fn)(e))
    }
}


/// `Drain` filtering records by `Record` logging level
///
/// Wraps a drain and passes records to it, only
/// if their level is at least given level.
///
/// TODO: Remove this type. This drain is a special case of `Filter`, but
/// because `Filter` can not use static dispatch ATM due to Rust limitations
/// that will be lifted in the future, it is a standalone type.
pub struct LevelFilter<D: Drain> {
    level: Level,
    drain: D,
}

impl<D: Drain> LevelFilter<D> {
    /// Create `LevelFilter`
    pub fn new(drain: D, level: Level) -> Self {
        LevelFilter {
            level: level,
            drain: drain,
        }
    }
}

impl<D: Drain> Drain for LevelFilter<D> {
    type Error = D::Error;
    fn log(&self,
           info: &Record,
           logger_values: &OwnedKeyValueList)
           -> result::Result<(), Self::Error> {
        if info.level().is_at_least(self.level) {
            self.drain.log(info, logger_values)
        } else {
            Ok(())
        }
    }
}

/// `Drain` duplicating records into two other `Drain`s
///
/// Can be nested for more than two outputs.
pub struct Duplicate<D1: Drain, D2: Drain> {
    drain1: D1,
    drain2: D2,
}


impl<D1: Drain, D2: Drain> Duplicate<D1, D2> {
    /// Create `Duplicate`
    pub fn new(drain1: D1, drain2: D2) -> Self {
        Duplicate {
            drain1: drain1,
            drain2: drain2,
        }
    }
}

/// Logging error returned by `Duplicate` drain
pub enum DuplicateError<E1, E2> {
    /// First `Drain` has returned error
    First(E1),
    /// Second `Drain` has returned error
    Second(E2),
    /// Both `Drain`s have returned error
    Both((E1, E2))
}

impl<E1 : fmt::Display, E2 : fmt::Display> fmt::Display for DuplicateError<E1, E2> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            &DuplicateError::First(ref e) => write!(f, "{})", *e),
            &DuplicateError::Second(ref e) => write!(f, "{})", *e),
            &DuplicateError::Both((ref e1, ref e2)) => write!(f, "({}, {})", *e1, *e2),
        }
    }
}

impl<D1 : Drain, D2 : Drain> Drain for Duplicate<D1, D2> {
    type Error = DuplicateError<D1::Error, D2::Error>;
    fn log(&self,
           info: &Record,
           logger_values: &OwnedKeyValueList)
           -> result::Result<(), Self::Error> {
        let res1 = self.drain1.log(info, logger_values);
        let res2 = self.drain2.log(info, logger_values);

        match (res1, res2) {
            (Ok(_), Ok(_)) => Ok(()),
            (Err(e), Ok(_)) => Err(DuplicateError::First(e)),
            (Ok(_), Err(e)) => Err(DuplicateError::Second(e)),
            (Err(e1), Err(e2)) => Err(DuplicateError::Both((e1, e2))),
        }
    }
}


/// `Drain` panicking on error
///
/// `Logger` requires a root drain to handle all errors (`Drain::Error == ()`),
/// `Fuse` will wrap a `Drain` and panic if it returns any errors.
///
/// Note: `Drain::Error` must implement `Display`. It's easy to create own
/// `Fuse` drain if this requirement can't be fulfilled.
pub struct Fuse<D: Drain> {
    drain: D,
}

impl<D: Drain> Fuse<D> {
    /// Create `Fuse` wrapping given `drain`
    pub fn new(drain: D) -> Self {
        Fuse {
            drain: drain,
        }
    }
}

impl<D: Drain> Drain for Fuse<D> where D::Error : fmt::Display {
    type Error = Never;
    fn log(&self,
           info: &Record,
           logger_values: &OwnedKeyValueList)
        -> result::Result<(), Never> {
            Ok(
                self.drain.log(info, logger_values).unwrap_or_else(
                    |e| panic!("Fuse: {}", e)
                    )
                )
        }
}


/// `Drain` ignoring errors
///
/// `Logger` requires a root drain to handle all errors (`Drain::Error == ()`),
/// `IgnoreErr` will ignore all errors of the drain it wraps.
pub struct IgnoreErr<D: Drain> {
    drain: D,
}

impl<D: Drain> IgnoreErr<D> {
    /// Create `IgnoreErr` wrapping `drain`
    pub fn new(drain: D) -> Self {
        IgnoreErr {
            drain: drain,
        }
    }
}

impl<D: Drain> Drain for IgnoreErr<D> {
    type Error = Never;
    fn log(&self,
           info: &Record,
           logger_values: &OwnedKeyValueList)
        -> result::Result<(), Never> {
            let _ = self.drain.log(info, logger_values);
            Ok(())
        }
}

/// Filter by `cond` closure
pub fn filter<D: Drain, F: 'static + Send + Sync + Fn(&Record) -> bool>(
    cond: F,
    d: D
    ) -> Filter<D> {
    Filter::new(d, cond)
}

/// Filter by log level
pub fn level_filter<D: Drain>(level: Level, d: D) -> LevelFilter<D> {
    LevelFilter::new(d, level)
}

/// Duplicate records to two drains
///
/// Create `Duplicate` drain.
///
/// Can be nested for multiple outputs.
pub fn duplicate<D1: Drain, D2: Drain>(d1: D1, d2: D2) -> Duplicate<D1, D2> {
    Duplicate::new(d1, d2)
}
