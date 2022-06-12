use crate::data::*;

use nom::{
    branch::alt,
    bytes::complete::{tag, take_while},
    character::complete::{alphanumeric1, char, multispace0, multispace1},
    combinator::success,
    multi::fold_many0,
    sequence::{delimited, preceded, terminated, tuple},
    IResult, Parser,
};

/// Parse a single value, which is either a quoted string, or a word without whitespace
fn val(input: &str) -> IResult<&str, &str, ()> {
    alt((delimited(char('"'), take_while(|c| c != '"'), char('"')), take_while(|c| c != '\n' && c != ' '))).parse(input)
}

/// Parse a key followed by a value, separated by =
fn keyval(input: &str) -> IResult<&str, (&str, &str), ()> {
    terminated(alphanumeric1, char('=')).and(val).parse(input)
}

/// Parser any number of key-value pairs, separated by 0 or more whitespace
fn keyvals(input: &str) -> IResult<&str, BTreeMap<String, String>, ()> {
    fold_many0(terminated(keyval, multispace0), BTreeMap::new, |mut map, (key, value)| {
        map.insert(key.to_owned(), value.to_owned());
        map
    })
    .parse(input)
}

/// Parse a key-value pair, where the key is a specific tag, separated by =,
/// terminated by whitespace
fn keyed_val<'i>(key: &'static str) -> impl Parser<&'i str, &'i str, ()> {
    terminated(preceded(terminated(tag(key), char('=')), val), multispace1)
}

fn notify(input: &str) -> IResult<&str, Event, ()> {
    preceded(char('!'), tuple((keyed_val("system"), keyed_val("subsystem"), keyed_val("type"), keyvals)))
        .map(|(sys, subsys, kind, data)| Event::Notify {
            system: sys.to_owned(),
            subsystem: subsys.to_owned(),
            kind: kind.to_owned(),
            data,
        })
        .parse(input)
}

/// Parse a key-value pair, where the key is a specific tag, separated by
/// whitespace
fn event_param<'i, T>(key: &'static str, value: impl Parser<&'i str, T, ()>) -> impl Parser<&'i str, T, ()> {
    preceded(terminated(tag(key), multispace1), value)
}

fn generic_event<'i, T>(prefix: char, dev: impl Parser<&'i str, T, ()>) -> impl Parser<&'i str, (T, BTreeMap<String, String>, &'i str), ()> {
    tuple((
        terminated(preceded(char(prefix), dev), multispace1),
        event_param("at", keyvals),
        event_param("on", val),
    ))
}

fn attach(input: &str) -> IResult<&str, Event, ()> {
    generic_event('+', alphanumeric1).map(|(dev, parent, loc)| Event::Attach { dev: dev.to_owned(), parent, location: loc.to_owned() }).parse(input)
}

fn detach(input: &str) -> IResult<&str, Event, ()> {
    generic_event('-', alphanumeric1).map(|(dev, parent, loc)| Event::Detach { dev: dev.to_owned(), parent, location: loc.to_owned() }).parse(input)
}

fn nomatch(input: &str) -> IResult<&str, Event, ()> {
    generic_event('?', success(())).map(|((), parent, loc)| Event::Nomatch { parent, location: loc.to_owned() }).parse(input)
}

pub fn event(input: &str) -> IResult<&str, Event, ()> {
    alt((notify, attach, detach, nomatch)).parse(input)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_notify() {
        let txt = "!system=USB subsystem=INTERFACE type=ATTACH ugen=ugen0.2 vendor=0x1050 sernum=\"\" mode=host\n";
        let res = event(txt);
        let mut data = BTreeMap::new();
        data.insert("ugen".to_owned(), "ugen0.2".to_owned());
        data.insert("vendor".to_owned(), "0x1050".to_owned());
        data.insert("sernum".to_owned(), "".to_owned());
        data.insert("mode".to_owned(), "host".to_owned());
        assert_eq!(
            res,
            Ok((
                "",
                Event::Notify {
                    system: "USB".to_owned(),
                    subsystem: "INTERFACE".to_owned(),
                    kind: "ATTACH".to_owned(),
                    data,
                }
            ))
        )
    }

    #[test]
    fn test_attach() {
        let txt = "+uhid1 at bus=0 sernum=\"\" on uhub1";
        let res = event(txt);
        let mut data = BTreeMap::new();
        data.insert("bus".to_owned(), "0".to_owned());
        data.insert("sernum".to_owned(), "".to_owned());
        assert_eq!(
            res,
            Ok((
                "",
                Event::Attach {
                    dev: "uhid1".to_owned(),
                    parent: data,
                    location: "uhub1".to_owned(),
                }
            ))
        )
    }

    #[test]
    fn test_detach() {
        let txt = "-uhid1 at  on uhub1";
        let res = event(txt);
        let data = BTreeMap::new();
        assert_eq!(
            res,
            Ok((
                "",
                Event::Detach {
                    dev: "uhid1".to_owned(),
                    parent: data,
                    location: "uhub1".to_owned(),
                }
            ))
        )
    }

    #[test]
    fn test_nomatch() {
        let txt = "? at bus=0 on uhub1";
        let res = event(txt);
        let mut data = BTreeMap::new();
        data.insert("bus".to_owned(), "0".to_owned());

        assert_eq!(res, Ok(("", Event::Nomatch { parent: data, location: "uhub1".to_owned() })))
    }
}
