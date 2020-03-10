use data::*;
use nom::branch::alt;
use nom::bytes::complete::take_while;
use nom::character::complete::{alphanumeric1, char, multispace1};
use nom::sequence::delimited;

fn val(i: &str) -> nom::IResult<&str, &str> {
    alt((delimited(char('"'), take_while(|c| c != '"'), char('"')), take_while(|c| c != '\n' && c != ' ')))(i)
}

named!(keyval <&str, (&str, &str)>,
   do_parse!(
           key: alphanumeric1
        >> char!('=')
        >> val: val
        >> (key, val)
        )
    );

named!(keyvals <&str, BTreeMap<String, String> >,
       map!(
           many0!(terminated!(keyval, opt!(multispace1))),
           |vec: Vec<_>| vec.into_iter().map(|(k, v)| (k.to_string(), v.to_string())).collect()
       )
      );

named!(pub event <&str, Event>,
    alt!(
        do_parse!(
            tag!("!") >>
            tag!("system=") >>
            sys: val >>
            multispace1 >>
            tag!("subsystem=") >>
            subsys: val >>
            multispace1 >>
            tag!("type=") >>
            kind: val >>
            multispace1 >>
            data: keyvals >>
            (Event::Notify { system: sys.to_string(), subsystem: subsys.to_string(), kind: kind.to_string(), data: data })
        )
        |
        do_parse!(
            tag!("+") >>
            dev: alphanumeric1 >>
            multispace1 >>
            tag!("at") >>
            multispace1 >>
            parent: keyvals >>
            tag!("on") >>
            multispace1 >>
            loc: val >>
            (Event::Attach { dev: dev.to_string(), parent: parent, location: loc.to_string() })
        )
        |
        do_parse!(
            tag!("-") >>
            dev: alphanumeric1 >>
            multispace1 >>
            tag!("at") >>
            multispace1 >>
            parent: keyvals >>
            tag!("on") >>
            multispace1 >>
            loc: val >>
            (Event::Detach { dev: dev.to_string(), parent: parent, location: loc.to_string() })
        )
        |
        do_parse!(
            tag!("?") >>
            multispace1 >>
            tag!("at") >>
            multispace1 >>
            parent: keyvals >>
            tag!("on") >>
            multispace1 >>
            loc: val >>
            (Event::Nomatch { parent: parent, location: loc.to_string() })
        )
    )
);

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
                    data: data,
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
                    parent: data.to_owned(),
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
