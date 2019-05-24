use nom::{alphanumeric, multispace};
use nom::types::CompleteStr;
use data::*;

named!(
    val<CompleteStr, CompleteStr>,
    alt!(
        delimited!(char!('"'), take_while!(call!(|c| c != '"')), char!('"'))
        |
        take_while!(call!(|c| c != '\n' && c != ' '))
    )
);

named!(keyval <CompleteStr, (CompleteStr, CompleteStr)>,
   do_parse!(
           key: alphanumeric
        >> char!('=')
        >> val: val
        >> (key, val)
        )
    );

named!(keyvals <CompleteStr, BTreeMap<String, String> >,
       map!(
           many0!(terminated!(keyval, opt!(multispace))),
           |vec: Vec<_>| vec.into_iter().map(|(k, v)| (k.to_string(), v.to_string())).collect()
       )
      );

named!(pub event <CompleteStr, Event>,
    alt!(
        do_parse!(
            tag!("!") >>
            tag!("system=") >>
            sys: val >>
            multispace >>
            tag!("subsystem=") >>
            subsys: val >>
            multispace >>
            tag!("type=") >>
            kind: val >>
            multispace >>
            data: keyvals >>
            (Event::Notify { system: sys.to_string(), subsystem: subsys.to_string(), kind: kind.to_string(), data: data })
        )
        |
        do_parse!(
            tag!("+") >>
            dev: alphanumeric >>
            multispace >>
            tag!("at") >>
            multispace >>
            parent: keyvals >>
            tag!("on") >>
            multispace >>
            loc: val >>
            (Event::Attach { dev: dev.to_string(), parent: parent, location: loc.to_string() })
        )
        |
        do_parse!(
            tag!("-") >>
            dev: alphanumeric >>
            multispace >>
            tag!("at") >>
            multispace >>
            parent: keyvals >>
            tag!("on") >>
            multispace >>
            loc: val >>
            (Event::Detach { dev: dev.to_string(), parent: parent, location: loc.to_string() })
        )
        |
        do_parse!(
            tag!("?") >>
            multispace >>
            tag!("at") >>
            multispace >>
            parent: keyvals >>
            tag!("on") >>
            multispace >>
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
        let res = event(CompleteStr(txt));
        let mut data = BTreeMap::new();
        data.insert("ugen".to_owned(), "ugen0.2".to_owned());
        data.insert("vendor".to_owned(), "0x1050".to_owned());
        data.insert("sernum".to_owned(), "".to_owned());
        data.insert("mode".to_owned(), "host".to_owned());
        assert_eq!(
            res,
            Ok((
                CompleteStr(""),
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
        let res = event(CompleteStr(txt));
        let mut data = BTreeMap::new();
        data.insert("bus".to_owned(), "0".to_owned());
        data.insert("sernum".to_owned(), "".to_owned());
        assert_eq!(
            res,
            Ok((
                CompleteStr(""),
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
        let res = event(CompleteStr(txt));
        let data = BTreeMap::new();
        assert_eq!(
            res,
            Ok((
                CompleteStr(""),
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
        let res = event(CompleteStr(txt));
        let mut data = BTreeMap::new();
        data.insert("bus".to_owned(), "0".to_owned());

        assert_eq!(
            res,
            Ok((CompleteStr(""), Event::Nomatch { parent: data, location: "uhub1".to_owned() }))
        )
    }

}
