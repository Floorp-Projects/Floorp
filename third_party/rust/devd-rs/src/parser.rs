use std::str;
use nom::{alphanumeric, multispace};
pub use nom::IResult;
use data::*;

named!(key<&str>, map_res!(alphanumeric, str::from_utf8));

named!(
    val<&str>,
    alt!(delimited!(char!('"'), map_res!(take_while!(call!(|c| c != '"' as u8)), str::from_utf8), char!('"')) | map_res!(take_while!(call!(|c| c != '\n' as u8 && c != ' ' as u8)), str::from_utf8))
);

named!(keyval <&[u8], (String, String)>,
   do_parse!(
           key: key
        >> char!('=')
        >> val: val
        >> (key.to_owned(), val.to_owned())
        )
    );

named!(keyvals <&[u8], BTreeMap<String, String> >,
       map!(
           many0!(terminated!(keyval, opt!(multispace))),
           |vec: Vec<_>| vec.into_iter().collect()
       )
      );

named!(pub event <&[u8], Event>,
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
           (Event::Notify { system: sys.to_owned(), subsystem: subsys.to_owned(), kind: kind.to_owned(), data: data })
       ) |
    do_parse!(
        tag!("+") >>
        dev: key >>
        multispace >>
        tag!("at") >>
        multispace >>
        parent: keyvals >>
        tag!("on") >>
        multispace >>
        loc: val >>
        (Event::Attach { dev: dev.to_owned(), parent: parent, location: loc.to_owned() })
    ) |
    do_parse!(
        tag!("-") >>
        dev: key >>
        multispace >>
        tag!("at") >>
        multispace >>
        parent: keyvals >>
        tag!("on") >>
        multispace >>
        loc: val >>
        (Event::Detach { dev: dev.to_owned(), parent: parent, location: loc.to_owned() })
    ) |
    do_parse!(
        tag!("?") >>
        multispace >>
        tag!("at") >>
        multispace >>
        parent: keyvals >>
        tag!("on") >>
        multispace >>
        loc: val >>
        (Event::Nomatch { parent: parent, location: loc.to_owned() })
    )




       )
      );

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_notify() {
        let txt = b"!system=USB subsystem=INTERFACE type=ATTACH ugen=ugen0.2 vendor=0x1050 sernum=\"\" mode=host\n";
        let res = event(txt);
        let mut data = BTreeMap::new();
        data.insert("ugen".to_owned(), "ugen0.2".to_owned());
        data.insert("vendor".to_owned(), "0x1050".to_owned());
        data.insert("sernum".to_owned(), "".to_owned());
        data.insert("mode".to_owned(), "host".to_owned());
        assert_eq!(
            res,
            IResult::Done(
                &b""[..],
                Event::Notify {
                    system: "USB".to_owned(),
                    subsystem: "INTERFACE".to_owned(),
                    kind: "ATTACH".to_owned(),
                    data: data,
                }
            )
        )
    }

    #[test]
    fn test_attach() {
        let txt = b"+uhid1 at bus=0 sernum=\"\" on uhub1";
        let res = event(txt);
        let mut data = BTreeMap::new();
        data.insert("bus".to_owned(), "0".to_owned());
        data.insert("sernum".to_owned(), "".to_owned());
        assert_eq!(
            res,
            IResult::Done(
                &b""[..],
                Event::Attach {
                    dev: "uhid1".to_owned(),
                    parent: data,
                    location: "uhub1".to_owned(),
                }
            )
        )
    }

    #[test]
    fn test_detach() {
        let txt = b"-uhid1 at  on uhub1";
        let res = event(txt);
        let data = BTreeMap::new();
        assert_eq!(
            res,
            IResult::Done(
                &b""[..],
                Event::Detach {
                    dev: "uhid1".to_owned(),
                    parent: data.to_owned(),
                    location: "uhub1".to_owned(),
                }
            )
        )
    }

    #[test]
    fn test_nomatch() {
        let txt = b"? at bus=0 on uhub1";
        let res = event(txt);
        let mut data = BTreeMap::new();
        data.insert("bus".to_owned(), "0".to_owned());
        assert_eq!(
            res,
            IResult::Done(&b""[..], Event::Nomatch { parent: data, location: "uhub1".to_owned() })
        )
    }

}
