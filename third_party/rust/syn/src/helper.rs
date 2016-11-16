use nom::IResult;
use space::{whitespace, word_break};

macro_rules! punct {
    ($i:expr, $punct:expr) => {
        $crate::helper::punct($i, $punct)
    };
}

pub fn punct<'a>(input: &'a str, token: &'static str) -> IResult<&'a str, &'a str> {
    let input = match whitespace(input) {
        IResult::Done(rest, _) => rest,
        IResult::Error => input,
    };
    if input.starts_with(token) {
        IResult::Done(&input[token.len()..], token)
    } else {
        IResult::Error
    }
}

macro_rules! keyword {
    ($i:expr, $keyword:expr) => {
        $crate::helper::keyword($i, $keyword)
    };
}

pub fn keyword<'a>(input: &'a str, token: &'static str) -> IResult<&'a str, &'a str> {
    match punct(input, token) {
        IResult::Done(rest, _) => {
            match word_break(rest) {
                IResult::Done(_, _) => IResult::Done(rest, token),
                IResult::Error => IResult::Error,
            }
        }
        IResult::Error => IResult::Error,
    }
}

macro_rules! option {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Done(i, o) => $crate::nom::IResult::Done(i, Some(o)),
            $crate::nom::IResult::Error => $crate::nom::IResult::Done($i, None),
        }
    };

    ($i:expr, $f:expr) => {
        option!($i, call!($f));
    };
}

macro_rules! opt_vec {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Done(i, o) => $crate::nom::IResult::Done(i, o),
            $crate::nom::IResult::Error => $crate::nom::IResult::Done($i, Vec::new()),
        }
    };
}

macro_rules! epsilon {
    ($i:expr,) => {
        $crate::nom::IResult::Done($i, ())
    };
}

macro_rules! tap {
    ($i:expr, $name:ident : $submac:ident!( $($args:tt)* ) => $e:expr) => {
        match $submac!($i, $($args)*) {
            $crate::nom::IResult::Done(i, o) => {
                let $name = o;
                $e;
                $crate::nom::IResult::Done(i, ())
            }
            $crate::nom::IResult::Error => $crate::nom::IResult::Error,
        }
    };

    ($i:expr, $name:ident : $f:expr => $e:expr) => {
        tap!($i, $name: call!($f) => $e);
    };
}

macro_rules! separated_list {
    ($i:expr, punct!($sep:expr), $f:expr) => {
        $crate::helper::separated_list($i, $sep, $f, false)
    };
}

macro_rules! terminated_list {
    ($i:expr, punct!($sep:expr), $f:expr) => {
        $crate::helper::separated_list($i, $sep, $f, true)
    };
}

pub fn separated_list<'a, T>(
    mut input: &'a str,
    sep: &'static str,
    f: fn(&'a str) -> IResult<&'a str, T>,
    terminated: bool,
) -> IResult<&'a str, Vec<T>> {
    let mut res = Vec::new();

    // get the first element
    match f(input) {
        IResult::Error => IResult::Done(input, Vec::new()),
        IResult::Done(i, o) => {
            if i.len() == input.len() {
                IResult::Error
            } else {
                res.push(o);
                input = i;

                // get the separator first
                while let IResult::Done(i2, _) = punct(input, sep) {
                    if i2.len() == input.len() {
                        break;
                    }

                    // get the element next
                    if let IResult::Done(i3, o3) = f(i2) {
                        if i3.len() == i2.len() {
                            break;
                        }
                        res.push(o3);
                        input = i3;
                    } else {
                        break;
                    }
                }
                if terminated {
                    if let IResult::Done(after, _) = punct(input, sep) {
                        input = after;
                    }
                }
                IResult::Done(input, res)
            }
        }
    }
}
