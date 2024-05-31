/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate bindgen;
extern crate nom;

use bindgen::callbacks::*;
use bindgen::*;

use mozbuild::TOPSRCDIR;

use nom::branch::alt;
use nom::bytes::complete::{tag, take_until};
use nom::character::complete::{
    char, multispace0, newline, not_line_ending, one_of, space0, space1,
};
use nom::combinator::{fail, recognize};
use nom::multi::{many1, separated_list0};
use nom::sequence::{delimited, separated_pair, terminated, tuple};
use nom::IResult;

use std::collections::HashMap;
use std::env;
use std::fmt;
use std::fs::File;
use std::io::{BufWriter, Write};
use std::path::PathBuf;

fn octal_block_to_vec_u8(octal_block: &str) -> Vec<u8> {
    octal_block
        .lines()
        .flat_map(|x| x.split('\\').skip(1))
        .map(|x| u8::from_str_radix(x, 8).expect("octal value out of range."))
        .collect()
}

fn octal_block_to_hex_string(octal: &str) -> String {
    octal_block_to_vec_u8(octal)
        .iter()
        .map(|x| format!("0x{:02X}, ", x))
        .collect()
}

// Wrapper around values parsed out of certdata.txt
enum Ck<'a> {
    Class(&'a str),
    Comment(&'a str),
    DistrustAfter(Option<&'a str>),
    Empty,
    MultilineOctal(&'a str),
    OptionBool(&'a str),
    Trust(&'a str),
    Utf8(&'a str),
}

// Translation of parsed values into the output rust code
impl fmt::Display for Ck<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Ck::Class(s) => write!(f, "{s}_BYTES"),
            Ck::Comment(s) => write!(f, "{}", s.replace('#', "//")),
            Ck::DistrustAfter(None) => write!(f, "Some(CK_FALSE_BYTES)"),
            Ck::DistrustAfter(Some(s)) => write!(f, "Some(&[{}])", octal_block_to_hex_string(s)),
            Ck::Empty => write!(f, "None"),
            Ck::MultilineOctal(s) => write!(f, "&[{}]", octal_block_to_hex_string(s)),
            Ck::OptionBool(s) => write!(f, "Some({s}_BYTES)"),
            Ck::Trust(s) => write!(f, "{s}_BYTES"),
            Ck::Utf8(s) => write!(f, "\"{s}\\0\""),
        }
    }
}

impl PartialEq for Ck<'_> {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Ck::Class(s), Ck::Class(t)) => s.eq(t),
            (Ck::Comment(s), Ck::Comment(t)) => s.eq(t),
            (Ck::DistrustAfter(None), Ck::DistrustAfter(None)) => true,
            (Ck::DistrustAfter(Some(s)), Ck::DistrustAfter(Some(t))) => {
                // compare the data rather than the presentation
                let vec_s = octal_block_to_vec_u8(s);
                let vec_t = octal_block_to_vec_u8(t);
                vec_s.eq(&vec_t)
            }
            (Ck::Empty, Ck::Empty) => true,
            (Ck::MultilineOctal(s), Ck::MultilineOctal(t)) => {
                // compare the data rather than the presentation
                let vec_s = octal_block_to_vec_u8(s);
                let vec_t = octal_block_to_vec_u8(t);
                vec_s.eq(&vec_t)
            }
            (Ck::Trust(s), Ck::Trust(t)) => s.eq(t),
            (Ck::Utf8(s), Ck::Utf8(t)) => s.eq(t),
            _ => false,
        }
    }
}

fn class(i: &str) -> IResult<&str, Ck> {
    let (i, _) = tag("CK_OBJECT_CLASS")(i)?;
    let (i, _) = space1(i)?;
    let (i, class) = alt((
        tag("CKO_NSS_BUILTIN_ROOT_LIST"),
        tag("CKO_CERTIFICATE"),
        tag("CKO_NSS_TRUST"),
    ))(i)?;
    let (i, _) = space0(i)?;
    let (i, _) = newline(i)?;
    Ok((i, Ck::Class(class)))
}

fn trust(i: &str) -> IResult<&str, Ck> {
    let (i, _) = tag("CK_TRUST")(i)?;
    let (i, _) = space1(i)?;
    let (i, trust) = alt((
        tag("CKT_NSS_TRUSTED_DELEGATOR"),
        tag("CKT_NSS_MUST_VERIFY_TRUST"),
        tag("CKT_NSS_NOT_TRUSTED"),
    ))(i)?;
    let (i, _) = space0(i)?;
    let (i, _) = newline(i)?;
    Ok((i, Ck::Trust(trust)))
}

// Parses a CK_BBOOL and wraps it with Ck::OptionBool so that it gets printed as
// "Some(CK_TRUE_BYTES)" instead of "CK_TRUE_BYTES".
fn option_bbool(i: &str) -> IResult<&str, Ck> {
    let (i, _) = tag("CK_BBOOL")(i)?;
    let (i, _) = space1(i)?;
    let (i, b) = alt((tag("CK_TRUE"), tag("CK_FALSE")))(i)?;
    let (i, _) = space0(i)?;
    let (i, _) = newline(i)?;
    Ok((i, Ck::OptionBool(b)))
}

fn bbool_true(i: &str) -> IResult<&str, Ck> {
    let (i, _) = tag("CK_BBOOL")(i)?;
    let (i, _) = space1(i)?;
    let (i, _) = tag("CK_TRUE")(i)?;
    let (i, _) = space0(i)?;
    let (i, _) = newline(i)?;
    Ok((i, Ck::Empty))
}

fn bbool_false(i: &str) -> IResult<&str, Ck> {
    let (i, _) = tag("CK_BBOOL")(i)?;
    let (i, _) = space1(i)?;
    let (i, _) = tag("CK_FALSE")(i)?;
    let (i, _) = space0(i)?;
    let (i, _) = newline(i)?;
    Ok((i, Ck::Empty))
}

fn utf8(i: &str) -> IResult<&str, Ck> {
    let (i, _) = tag("UTF8")(i)?;
    let (i, _) = space1(i)?;
    let (i, _) = char('"')(i)?;
    let (i, utf8) = take_until("\"")(i)?;
    let (i, _) = char('"')(i)?;
    let (i, _) = space0(i)?;
    let (i, _) = newline(i)?;
    Ok((i, Ck::Utf8(utf8)))
}

fn certificate_type(i: &str) -> IResult<&str, Ck> {
    let (i, _) = tag("CK_CERTIFICATE_TYPE")(i)?;
    let (i, _) = space1(i)?;
    let (i, _) = tag("CKC_X_509")(i)?;
    let (i, _) = space0(i)?;
    let (i, _) = newline(i)?;
    Ok((i, Ck::Empty))
}

// A CKA_NSS_{EMAIL,SERVER}_DISTRUST_AFTER line in certdata.txt is encoded either as a CK_BBOOL
// with value CK_FALSE (when there is no distrust after date) or as a MULTILINE_OCTAL block.
fn distrust_after(i: &str) -> IResult<&str, Ck> {
    let (i, value) = alt((multiline_octal, bbool_false))(i)?;
    match value {
        Ck::Empty => Ok((i, Ck::DistrustAfter(None))),
        Ck::MultilineOctal(data) => Ok((i, Ck::DistrustAfter(Some(data)))),
        _ => unreachable!(),
    }
}

fn octal_octet(i: &str) -> IResult<&str, &str> {
    recognize(tuple((
        tag("\\"),
        one_of("0123"), // 255 = \377
        one_of("01234567"),
        one_of("01234567"),
    )))(i)
}

fn multiline_octal(i: &str) -> IResult<&str, Ck> {
    let (i, _) = tag("MULTILINE_OCTAL")(i)?;
    let (i, _) = space0(i)?;
    let (i, _) = newline(i)?;
    let (i, lines) = recognize(many1(terminated(many1(octal_octet), newline)))(i)?;
    let (i, _) = tag("END")(i)?;
    let (i, _) = space0(i)?;
    let (i, _) = newline(i)?;
    return Ok((i, Ck::MultilineOctal(lines)));
}

fn distrust_comment(i: &str) -> IResult<&str, (&str, Ck)> {
    let (i, comment) = recognize(delimited(
        alt((
            tag("# For Email Distrust After: "),
            tag("# For Server Distrust After: "),
        )),
        not_line_ending,
        newline,
    ))(i)?;
    Ok((i, ("DISTRUST_COMMENT", Ck::Comment(comment))))
}

fn comment(i: &str) -> IResult<&str, (&str, Ck)> {
    let (i, comment) = recognize(many1(delimited(char('#'), not_line_ending, newline)))(i)?;
    Ok((i, ("COMMENT", Ck::Comment(comment))))
}

fn certdata_line(i: &str) -> IResult<&str, (&str, Ck)> {
    let (i, (attr, value)) = alt((
        distrust_comment, // must be listed before `comment`
        comment,
        separated_pair(tag("CKA_CLASS"), space1, class),
        separated_pair(tag("CKA_CERTIFICATE_TYPE"), space1, certificate_type),
        separated_pair(alt((tag("CKA_ID"), tag("CKA_LABEL"))), space1, utf8),
        separated_pair(
            alt((
                tag("CKA_ISSUER"),
                tag("CKA_CERT_SHA1_HASH"),
                tag("CKA_CERT_MD5_HASH"),
                tag("CKA_SERIAL_NUMBER"),
                tag("CKA_SUBJECT"),
                tag("CKA_VALUE"),
            )),
            space1,
            multiline_octal,
        ),
        separated_pair(
            alt((
                tag("CKA_NSS_SERVER_DISTRUST_AFTER"),
                tag("CKA_NSS_EMAIL_DISTRUST_AFTER"),
            )),
            space1,
            distrust_after,
        ),
        separated_pair(
            alt((
                tag("CKA_TRUST_EMAIL_PROTECTION"),
                tag("CKA_TRUST_CODE_SIGNING"),
                tag("CKA_TRUST_SERVER_AUTH"),
            )),
            space1,
            trust,
        ),
        separated_pair(tag("CKA_NSS_MOZILLA_CA_POLICY"), space1, option_bbool),
        separated_pair(tag("CKA_TOKEN"), space1, bbool_true),
        separated_pair(
            alt((
                tag("CKA_TRUST_STEP_UP_APPROVED"),
                tag("CKA_PRIVATE"),
                tag("CKA_MODIFIABLE"),
            )),
            space1,
            bbool_false,
        ),
    ))(i)?;
    Ok((i, (attr, value)))
}

type Block<'a> = HashMap<&'a str, Ck<'a>>;

fn attr<'a>(block: &'a Block, attr: &str) -> &'a Ck<'a> {
    block.get(attr).unwrap_or(&Ck::Empty)
}

fn parse(i: &str) -> IResult<&str, Vec<Block>> {
    let mut out: Vec<Block> = vec![];
    let (i, _) = take_until("BEGINDATA\n")(i)?;
    let (i, _) = tag("BEGINDATA\n")(i)?;
    let (i, mut raw_blocks) = separated_list0(many1(char('\n')), many1(certdata_line))(i)?;
    let (i, _) = multispace0(i)?; // allow trailing whitespace
    if !i.is_empty() {
        // The first line of i contains an error.
        let (line, _) = i.split_once('\n').unwrap_or((i, ""));
        fail::<_, &str, _>(line)?;
    }
    for raw_block in raw_blocks.drain(..) {
        out.push(raw_block.into_iter().collect())
    }
    Ok((i, out))
}

#[derive(Debug)]
struct PKCS11TypesParseCallbacks;

impl ParseCallbacks for PKCS11TypesParseCallbacks {
    fn int_macro(&self, _name: &str, _value: i64) -> Option<IntKind> {
        Some(IntKind::U8)
    }
}

// If we encounter a problem parsing certdata.txt we'll try to turn it into a compile time
// error in builtins.rs. We need to output definitions for ROOT_LIST_LABEL and BUILTINS to
// cut down on the number of errors the compiler produces.
macro_rules! emit_build_error {
    ($out:ident, $err:expr) => {
        writeln!($out, "std::compile_error!(\"{}\");", $err)?;
        writeln!($out, "pub static ROOT_LIST_LABEL: [u8; 0] = [];")?;
        writeln!($out, "pub static BUILTINS: [Root; 0] = [];")?;
    };
}

fn main() -> std::io::Result<()> {
    let testlib_certdata =
        TOPSRCDIR.join("security/manager/ssl/tests/unit/test_builtins/certdata.txt");
    let mozilla_certdata = TOPSRCDIR.join("security/nss/lib/ckfw/builtins/certdata.txt");
    let nssckbi_header = TOPSRCDIR.join("security/nss/lib/ckfw/builtins/nssckbi.h");
    println!("cargo:rerun-if-changed={}", testlib_certdata.display());
    println!("cargo:rerun-if-changed={}", mozilla_certdata.display());
    println!("cargo:rerun-if-changed={}", nssckbi_header.display());

    let bindings = Builder::default()
        .header(nssckbi_header.display().to_string())
        .allowlist_var("NSS_BUILTINS_CRYPTOKI_VERSION_MAJOR")
        .allowlist_var("NSS_BUILTINS_CRYPTOKI_VERSION_MINOR")
        .allowlist_var("NSS_BUILTINS_LIBRARY_VERSION_MAJOR")
        .allowlist_var("NSS_BUILTINS_LIBRARY_VERSION_MINOR")
        .allowlist_var("NSS_BUILTINS_HARDWARE_VERSION_MAJOR")
        .allowlist_var("NSS_BUILTINS_HARDWARE_VERSION_MINOR")
        .allowlist_var("NSS_BUILTINS_FIRMWARE_VERSION_MAJOR")
        .allowlist_var("NSS_BUILTINS_FIRMWARE_VERSION_MINOR")
        .parse_callbacks(Box::new(PKCS11TypesParseCallbacks))
        .generate()
        .expect("Unable to generate bindings.");

    let out_path = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR should be set in env."));
    bindings
        .write_to_file(out_path.join("version.rs"))
        .expect("Could not write version.rs.");

    let mut out = BufWriter::new(
        File::create(out_path.join("builtins.rs")).expect("Could not write builtins.rs."),
    );

    // If we are building the test module, use the certdata.txt in the test directory.
    #[cfg(feature = "testlib")]
    let mut input =
        std::fs::read_to_string(testlib_certdata).expect("Unable to read certdata.txt.");

    // Otherwise, use the official certdata.txt for the Mozilla root store.
    #[cfg(not(feature = "testlib"))]
    let mut input =
        std::fs::read_to_string(mozilla_certdata).expect("Unable to read certdata.txt.");

    // Add a trailing newline to simplify parsing.
    input.push('\n');

    let blocks = match parse(&input) {
        Ok((_, blocks)) => blocks,
        Err(e) => {
            let input = match e {
                nom::Err::Error(nom::error::Error { input, .. }) => input,
                _ => "Unknown",
            };
            emit_build_error!(
                out,
                &format!(
                    "Could not parse certdata.txt. Failed at: \'{}\');",
                    input.escape_debug().to_string().escape_debug()
                )
            );
            return Ok(());
        }
    };

    let root_lists: Vec<&Block> = blocks
        .iter()
        .filter(|x| attr(x, "CKA_CLASS") == &Ck::Class("CKO_NSS_BUILTIN_ROOT_LIST"))
        .collect();

    if root_lists.len() != 1 {
        emit_build_error!(
            out,
            "certdata.txt does not define a CKO_NSS_BUILTIN_ROOT_LIST object."
        );
        return Ok(());
    }

    let mut certs: Vec<&Block> = blocks
        .iter()
        .filter(|x| attr(x, "CKA_CLASS") == &Ck::Class("CKO_CERTIFICATE"))
        .collect();

    let trusts: Vec<&Block> = blocks
        .iter()
        .filter(|x| attr(x, "CKA_CLASS") == &Ck::Class("CKO_NSS_TRUST"))
        .collect();

    if certs.len() != trusts.len() {
        emit_build_error!(
            out,
            "certdata.txt has a mismatched number of certificate and trust objects"
        );
        return Ok(());
    }

    // Ensure that every certificate has a CKA_SUBJECT attribute for the sort
    for (i, cert) in certs.iter().enumerate() {
        match cert.get("CKA_SUBJECT") {
            Some(Ck::MultilineOctal(_)) => (),
            _ => {
                emit_build_error!(
                    out,
                    format!("Certificate {i} in certdata.txt has no CKA_SUBJECT attribute.")
                );
                return Ok(());
            }
        }
    }

    certs.sort_by_cached_key(|x| match x.get("CKA_SUBJECT") {
        Some(Ck::MultilineOctal(data)) => octal_block_to_vec_u8(data),
        _ => unreachable!(),
    });

    // Write out arrays for the DER encoded certificate, serial number, and subject of each root.
    // Since the serial number and the subject are in the DER cert, we don't need to store
    // additional data for them.
    for (i, cert) in certs.iter().enumerate() {
        // Preserve the comment from certdata.txt
        match attr(cert, "COMMENT") {
            Ck::Empty => (),
            comment => write!(out, "{comment}")?,
        };

        let der = attr(cert, "CKA_VALUE");
        writeln!(out, "static ROOT_{i}: &[u8] = {der};")?;

        // Search for the serial number and subject in the DER cert. We want to search on the raw
        // bytes, not the octal presentation, so we have to unpack the enums.
        let der_data = match der {
            Ck::MultilineOctal(x) => octal_block_to_vec_u8(x),
            _ => unreachable!(),
        };
        let serial_data = match attr(cert, "CKA_SERIAL_NUMBER") {
            Ck::MultilineOctal(x) => octal_block_to_vec_u8(x),
            _ => unreachable!(),
        };
        let subject_data = match attr(cert, "CKA_SUBJECT") {
            Ck::MultilineOctal(x) => octal_block_to_vec_u8(x),
            _ => unreachable!(),
        };

        fn need_u16(out: &mut impl Write, attr: &str, what: &str, i: usize) -> std::io::Result<()> {
            emit_build_error!(
                out,
                format!("Certificate {i} in certdata.txt has a {attr} whose {what} doesn't fit in a u8. Time to upgrade to u16 at the expense of size?")
            );
            Ok(())
        }

        let serial_len = serial_data.len();
        if let Some(serial_offset) = &der_data.windows(serial_len).position(|s| s == serial_data) {
            if *serial_offset > u8::MAX.into() {
                return need_u16(&mut out, "CKA_SERIAL_NUMBER", "offset", i);
            }
            if serial_len > u8::MAX.into() {
                return need_u16(&mut out, "CKA_SERIAL_NUMBER", "length", i);
            }
            writeln!(
                out,
                "const SERIAL_{i}: (u8, u8) = ({serial_offset}, {serial_len});"
            )?;
        } else {
            emit_build_error!(
                out,
                format!("Certificate {i} in certdata.txt has a CKA_SERIAL_NUMBER that does not match its CKA_VALUE.")
            );
            return Ok(());
        }

        let subject_len = subject_data.len();
        if let Some(subject_offset) = &der_data
            .windows(subject_len)
            .position(|s| s == subject_data)
        {
            if *subject_offset > u8::MAX.into() {
                return need_u16(&mut out, "CKA_SUBJECT", "offset", i);
            }
            if subject_len > u8::MAX.into() {
                return need_u16(&mut out, "CKA_SUBJECT", "length", i);
            }
            writeln!(
                out,
                "const SUBJECT_{i}: (u8, u8) = ({subject_offset}, {subject_len});"
            )?;
        } else {
            emit_build_error!(
                out,
                format!("Certificate {i} in certdata.txt has a CKA_SUBJECT that does not match its CKA_VALUE.")
            );
            return Ok(());
        }
    }

    let root_list_label = attr(root_lists[0], "CKA_LABEL");
    let root_list_label_len = match root_list_label {
        Ck::Utf8(x) => x.len() + 1,
        _ => unreachable!(),
    };
    writeln!(
        out,
        "pub const ROOT_LIST_LABEL: [u8; {root_list_label_len}] = *b{root_list_label};"
    )?;

    writeln!(out, "pub static BUILTINS: [Root; {}] = [", certs.len())?;
    for (i, cert) in certs.iter().enumerate() {
        let subject = attr(cert, "CKA_SUBJECT");
        let issuer = attr(cert, "CKA_ISSUER");
        let label = attr(cert, "CKA_LABEL");
        if !subject.eq(issuer) {
            writeln!(out, "];")?; // end the definition of BUILTINS
            let label = format!("{}", label);
            writeln!(
                out,
                "std::compile_error!(\"Certificate with label {} is not self-signed\");",
                label.escape_debug()
            )?;
            return Ok(());
        }
        let mozpol = attr(cert, "CKA_NSS_MOZILLA_CA_POLICY");
        let server_distrust = attr(cert, "CKA_NSS_SERVER_DISTRUST_AFTER");
        let email_distrust = attr(cert, "CKA_NSS_EMAIL_DISTRUST_AFTER");
        let matching_trusts: Vec<&&Block> = trusts
            .iter()
            .filter(|trust| {
                (attr(cert, "CKA_ISSUER") == attr(trust, "CKA_ISSUER"))
                    && (attr(cert, "CKA_SERIAL_NUMBER") == attr(trust, "CKA_SERIAL_NUMBER"))
            })
            .collect();
        if matching_trusts.len() != 1 {
            writeln!(out, "];")?; // end the definition of BUILTINS
            let label = format!("{}", label);
            writeln!(out, "std::compile_error!(\"Could not find unique trust object for {} in certdata.txt\");", label.escape_debug())?;
            return Ok(());
        }
        let trust = *matching_trusts[0];
        let sha1 = match attr(trust, "CKA_CERT_SHA1_HASH") {
            Ck::MultilineOctal(x) => octal_block_to_hex_string(x),
            _ => unreachable!(),
        };
        let md5 = match attr(trust, "CKA_CERT_MD5_HASH") {
            Ck::MultilineOctal(x) => octal_block_to_hex_string(x),
            _ => unreachable!(),
        };
        let server = attr(trust, "CKA_TRUST_SERVER_AUTH");
        let email = attr(trust, "CKA_TRUST_EMAIL_PROTECTION");

        writeln!(
            out,
            "        Root {{
            label: {label},
            der_name: SUBJECT_{i},
            der_serial: SERIAL_{i},
            der_cert: ROOT_{i},
            mozilla_ca_policy: {mozpol},
            server_distrust_after: {server_distrust},
            email_distrust_after: {email_distrust},
            sha1: [{sha1}],
            md5: [{md5}],
            trust_server: {server},
            trust_email: {email},
        }},"
        )?;
    }
    writeln!(out, "];")?;

    let _ = out.flush();
    Ok(())
}
