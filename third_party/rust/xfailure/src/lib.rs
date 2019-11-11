#[macro_export]
macro_rules! xbail { ($e:expr) => (Err($e)?); }
