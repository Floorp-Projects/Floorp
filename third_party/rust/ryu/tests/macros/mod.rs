macro_rules! check {
    ($ryu:tt, $pretty:tt) => {
        assert_eq!($ryu, $pretty);
        assert_eq!(print($ryu), stringify!($ryu));
        assert_eq!(pretty($pretty), stringify!($pretty));
    };
    (-$ryu:tt, -$pretty:tt) => {
        assert_eq!(-$ryu, -$pretty);
        assert_eq!(print(-$ryu), concat!("-", stringify!($ryu)));
        assert_eq!(pretty(-$pretty), concat!("-", stringify!($pretty)));
    };
}
