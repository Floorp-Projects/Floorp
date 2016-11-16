// Adapted from nom <https://github.com/Geal/nom> by removing the
// IResult::Incomplete variant, which we don't use and which unfortunately more
// than doubles the compilation time.

#[derive(Debug, PartialEq, Eq, Clone)]
pub enum IResult<I, O> {
    /// indicates a correct parsing, the first field containing the rest of the
    /// unparsed data, the second field contains the parsed data
    Done(I, O),
    Error,
}

macro_rules! named {
    ($name:ident -> $o:ty, $submac:ident!( $($args:tt)* )) => {
        fn $name(i: &str) -> $crate::nom::IResult<&str, $o> {
            $submac!(i, $($args)*)
        }
    };

    (pub $name:ident -> $o:ty, $submac:ident!( $($args:tt)* )) => {
        pub fn $name(i: &str) -> $crate::nom::IResult<&str, $o> {
            $submac!(i, $($args)*)
        }
    };
}

macro_rules! call {
    ($i:expr, $fun:expr) => {
        $fun($i)
    };
}

macro_rules! map {
    ($i:expr, $submac:ident!( $($args:tt)* ), $g:expr) => {
        map_impl!($i, $submac!($($args)*), call!($g));
    };

    ($i:expr, $f:expr, $g:expr) => {
        map_impl!($i, call!($f), call!($g));
    };
}

/// Internal parser, do not use directly
macro_rules! map_impl {
    ($i:expr, $submac:ident!( $($args:tt)* ), $submac2:ident!( $($args2:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
            $crate::nom::IResult::Done(i, o) => {
                $crate::nom::IResult::Done(i, $submac2!(o, $($args2)*))
            }
        }
    };
}

macro_rules! not {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Done(_, _) => $crate::nom::IResult::Error,
            $crate::nom::IResult::Error => $crate::nom::IResult::Done($i, ""),
        }
    };
}

macro_rules! cond {
    ($i:expr, $cond:expr, $submac:ident!( $($args:tt)* )) => {
        if $cond {
            match $submac!($i, $($args)*) {
                $crate::nom::IResult::Done(i, o) => $crate::nom::IResult::Done(i, ::std::option::Option::Some(o)),
                $crate::nom::IResult::Error => $crate::nom::IResult::Done($i, ::std::option::Option::None),
            }
        } else {
            $crate::nom::IResult::Done($i, ::std::option::Option::None)
        }
    };

    ($i:expr, $cond:expr, $f:expr) => {
        cond!($i, $cond, call!($f));
    };
}

macro_rules! preceded {
    ($i:expr, $submac:ident!( $($args:tt)* ), $submac2:ident!( $($args2:tt)* )) => {
        match tuple!($i, $submac!($($args)*), $submac2!($($args2)*)) {
            $crate::nom::IResult::Done(remaining, (_, o)) => $crate::nom::IResult::Done(remaining, o),
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
        }
    };

    ($i:expr, $submac:ident!( $($args:tt)* ), $g:expr) => {
        preceded!($i, $submac!($($args)*), call!($g));
    };

    ($i:expr, $f:expr, $submac:ident!( $($args:tt)* )) => {
        preceded!($i, call!($f), $submac!($($args)*));
    };

    ($i:expr, $f:expr, $g:expr) => {
        preceded!($i, call!($f), call!($g));
    };
}

macro_rules! terminated {
    ($i:expr, $submac:ident!( $($args:tt)* ), $submac2:ident!( $($args2:tt)* )) => {
        match tuple!($i, $submac!($($args)*), $submac2!($($args2)*)) {
            $crate::nom::IResult::Done(remaining, (o, _)) => $crate::nom::IResult::Done(remaining, o),
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
        }
    };

    ($i:expr, $submac:ident!( $($args:tt)* ), $g:expr) => {
        terminated!($i, $submac!($($args)*), call!($g));
    };

    ($i:expr, $f:expr, $submac:ident!( $($args:tt)* )) => {
        terminated!($i, call!($f), $submac!($($args)*));
    };

    ($i:expr, $f:expr, $g:expr) => {
        terminated!($i, call!($f), call!($g));
    };
}

macro_rules! many0 {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {{
        let ret;
        let mut res   = ::std::vec::Vec::new();
        let mut input = $i;

        loop {
            if input.is_empty() {
                ret = $crate::nom::IResult::Done(input, res);
                break;
            }

            match $submac!(input, $($args)*) {
                $crate::nom::IResult::Error => {
                    ret = $crate::nom::IResult::Done(input, res);
                    break;
                }
                $crate::nom::IResult::Done(i, o) => {
                    // loop trip must always consume (otherwise infinite loops)
                    if i.len() == input.len() {
                        ret = $crate::nom::IResult::Error;
                        break;
                    }

                    res.push(o);
                    input = i;
                }
            }
        }

        ret
    }};

    ($i:expr, $f:expr) => {
        $crate::nom::many0($i, $f)
    };
}

pub fn many0<'a, T>(mut input: &'a str, f: fn(&'a str) -> IResult<&'a str, T>) -> IResult<&'a str, Vec<T>> {
    let mut res = Vec::new();

    loop {
        if input.is_empty() {
            return IResult::Done(input, res);
        }

        match f(input) {
            IResult::Error => {
                return IResult::Done(input, res);
            }
            IResult::Done(i, o) => {
                // loop trip must always consume (otherwise infinite loops)
                if i.len() == input.len() {
                    return IResult::Error;
                }

                res.push(o);
                input = i;
            }
        }
    }
}

macro_rules! peek {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Done(_, o) => $crate::nom::IResult::Done($i, o),
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
        }
    };
}

macro_rules! take_while1 {
    ($input:expr, $submac:ident!( $($args:tt)* )) => {{
        let mut offset = $input.len();
        for (o, c) in $input.char_indices() {
            if !$submac!(c, $($args)*) {
                offset = o;
                break;
            }
        }
        if offset == 0 {
            $crate::nom::IResult::Error
        } else if offset < $input.len() {
            $crate::nom::IResult::Done(&$input[offset..], &$input[..offset])
        } else {
            $crate::nom::IResult::Done("", $input)
        }
    }};

    ($input:expr, $f:expr) => {
        take_while1!($input, call!($f));
    };
}

pub fn str_chars(s: &str) -> Vec<char> {
    // Can't do `s.chars().collect()` because it triggers a compiler bug in 1.12.0
    // https://github.com/dtolnay/syn/issues/20
    let mut result = Vec::new();
    for ch in s.chars() {
        result.push(ch);
    }
    result
}

macro_rules! take_until {
    ($input:expr, $substr:expr) => {{
        if $substr.len() > $input.len() {
            $crate::nom::IResult::Error
        } else {
            let substr_vec: Vec<char> = $crate::nom::str_chars($substr);
            let mut window: Vec<char> = vec![];
            let mut offset = $input.len();
            let mut parsed = false;
            for (o, c) in $input.char_indices() {
                window.push(c);
                if window.len() > substr_vec.len() {
                    window.remove(0);
                }
                if window == substr_vec {
                    parsed = true;
                    window.pop();
                    let window_len: usize = window.iter()
                        .map(|x| x.len_utf8())
                        .fold(0, |x, y| x + y);
                    offset = o - window_len;
                    break;
                }
            }
            if parsed {
                $crate::nom::IResult::Done(&$input[offset..], &$input[..offset])
            } else {
                $crate::nom::IResult::Error
            }
        }
    }};
}

macro_rules! tag {
    ($i:expr, $tag: expr) => {
        if $i.starts_with($tag) {
            $crate::nom::IResult::Done(&$i[$tag.len()..], &$i[0..$tag.len()])
        } else {
            $crate::nom::IResult::Error
        }
    };
}

macro_rules! switch {
    ($i:expr, $submac:ident!( $($args:tt)* ), $($p:pat => $subrule:ident!( $($args2:tt)* ))|* ) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
            $crate::nom::IResult::Done(i, o) => match o {
                $(
                    $p => $subrule!(i, $($args2)*),
                )*
                _ => $crate::nom::IResult::Error,
            }
        }
    };
}

macro_rules! value {
    ($i:expr, $res:expr) => {
        $crate::nom::IResult::Done($i, $res)
    };
}

macro_rules! delimited {
    ($i:expr, $submac:ident!( $($args:tt)* ), $($rest:tt)+) => {
        match tuple_parser!($i, (), $submac!($($args)*), $($rest)*) {
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
            $crate::nom::IResult::Done(i1, (_, o, _)) => $crate::nom::IResult::Done(i1, o)
        }
    };

    ($i:expr, $f:expr, $($rest:tt)+) => {
        delimited!($i, call!($f), $($rest)*);
    };
}

macro_rules! separated_nonempty_list {
    ($i:expr, $sep:ident!( $($args:tt)* ), $submac:ident!( $($args2:tt)* )) => {{
        let mut res   = ::std::vec::Vec::new();
        let mut input = $i;

        // get the first element
        match $submac!(input, $($args2)*) {
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
            $crate::nom::IResult::Done(i, o) => {
                if i.len() == input.len() {
                    $crate::nom::IResult::Error
                } else {
                    res.push(o);
                    input = i;

                    while let $crate::nom::IResult::Done(i2, _) = $sep!(input, $($args)*) {
                        if i2.len() == input.len() {
                            break;
                        }

                        if let $crate::nom::IResult::Done(i3, o3) = $submac!(i2, $($args2)*) {
                            if i3.len() == i2.len() {
                                break;
                            }
                            res.push(o3);
                            input = i3;
                        } else {
                            break;
                        }
                    }
                    $crate::nom::IResult::Done(input, res)
                }
            }
        }
    }};

    ($i:expr, $submac:ident!( $($args:tt)* ), $g:expr) => {
        separated_nonempty_list!($i, $submac!($($args)*), call!($g));
    };

    ($i:expr, $f:expr, $submac:ident!( $($args:tt)* )) => {
        separated_nonempty_list!($i, call!($f), $submac!($($args)*));
    };

    ($i:expr, $f:expr, $g:expr) => {
        separated_nonempty_list!($i, call!($f), call!($g));
    };
}

macro_rules! tuple {
    ($i:expr, $($rest:tt)*) => {
        tuple_parser!($i, (), $($rest)*)
    };
}

/// Internal parser, do not use directly
macro_rules! tuple_parser {
    ($i:expr, ($($parsed:tt),*), $e:ident, $($rest:tt)*) => {
        tuple_parser!($i, ($($parsed),*), call!($e), $($rest)*);
    };

    ($i:expr, (), $submac:ident!( $($args:tt)* ), $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
            $crate::nom::IResult::Done(i, o) =>
                tuple_parser!(i, (o), $($rest)*),
        }
    };

    ($i:expr, ($($parsed:tt)*), $submac:ident!( $($args:tt)* ), $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
            $crate::nom::IResult::Done(i, o) =>
                tuple_parser!(i, ($($parsed)* , o), $($rest)*),
        }
    };

    ($i:expr, ($($parsed:tt),*), $e:ident) => {
        tuple_parser!($i, ($($parsed),*), call!($e));
    };

    ($i:expr, (), $submac:ident!( $($args:tt)* )) => {
        $submac!($i, $($args)*)
    };

    ($i:expr, ($($parsed:expr),*), $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
            $crate::nom::IResult::Done(i, o) => $crate::nom::IResult::Done(i, ($($parsed),*, o))
        }
    };

    ($i:expr, ($($parsed:expr),*)) => {
        $crate::nom::IResult::Done($i, ($($parsed),*))
    };
}

macro_rules! alt {
    ($i:expr, $e:ident | $($rest:tt)*) => {
        alt!($i, call!($e) | $($rest)*);
    };

    ($i:expr, $subrule:ident!( $($args:tt)*) | $($rest:tt)*) => {
        match $subrule!($i, $($args)*) {
            res @ $crate::nom::IResult::Done(_, _) => res,
            _ => alt!($i, $($rest)*)
        }
    };

    ($i:expr, $subrule:ident!( $($args:tt)* ) => { $gen:expr } | $($rest:tt)+) => {
        match $subrule!($i, $($args)*) {
            $crate::nom::IResult::Done(i, o) => $crate::nom::IResult::Done(i, $gen(o)),
            $crate::nom::IResult::Error => alt!($i, $($rest)*)
        }
    };

    ($i:expr, $e:ident => { $gen:expr } | $($rest:tt)*) => {
        alt!($i, call!($e) => { $gen } | $($rest)*);
    };

    ($i:expr, $e:ident => { $gen:expr }) => {
        alt!($i, call!($e) => { $gen });
    };

    ($i:expr, $subrule:ident!( $($args:tt)* ) => { $gen:expr }) => {
        match $subrule!($i, $($args)*) {
            $crate::nom::IResult::Done(i, o) => $crate::nom::IResult::Done(i, $gen(o)),
            $crate::nom::IResult::Error => alt!($i)
        }
    };

    ($i:expr, $e:ident) => {
        alt!($i, call!($e));
    };

    ($i:expr, $subrule:ident!( $($args:tt)*)) => {
        match $subrule!( $i, $($args)* ) {
            $crate::nom::IResult::Done(i, o) => $crate::nom::IResult::Done(i, o),
            $crate::nom::IResult::Error => alt!($i),
        }
    };

    ($i:expr) => {
        $crate::nom::IResult::Error
    };
}

macro_rules! do_parse {
    ($i:expr, ( $($rest:expr),* )) => {
        $crate::nom::IResult::Done($i, ( $($rest),* ))
    };

    ($i:expr, $e:ident >> $($rest:tt)*) => {
        do_parse!($i, call!($e) >> $($rest)*);
    };

    ($i:expr, $submac:ident!( $($args:tt)* ) >> $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
            $crate::nom::IResult::Done(i, _) =>
                do_parse!(i, $($rest)*),
        }
    };

    ($i:expr, $field:ident : $e:ident >> $($rest:tt)*) => {
        do_parse!($i, $field: call!($e) >> $($rest)*);
    };

    ($i:expr, $field:ident : $submac:ident!( $($args:tt)* ) >> $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
            $crate::nom::IResult::Done(i, o) => {
                let $field = o;
                do_parse!(i, $($rest)*)
            },
        }
    };

    ($i:expr, mut $field:ident : $e:ident >> $($rest:tt)*) => {
        do_parse!($i, $field: call!($e) >> $($rest)*);
    };

    ($i:expr, mut $field:ident : $submac:ident!( $($args:tt)* ) >> $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
            $crate::nom::IResult::Done(i, o) => {
                let mut $field = o;
                do_parse!(i, $($rest)*)
            },
        }
    };
}
