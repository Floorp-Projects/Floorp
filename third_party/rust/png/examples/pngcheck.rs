#![allow(non_upper_case_globals)]

extern crate getopts;
extern crate glob;
extern crate png;
extern crate term;

use std::io;
use std::io::prelude::*;
use std::path::Path;
use std::fs::File;
use std::env;

use getopts::{Matches, Options, ParsingStyle};
use term::{color, Attr};

fn parse_args() -> Option<Matches> {
    let args: Vec<String> = env::args().collect();
    let mut opts = Options::new();
    opts.optflag("c", "", "colorize output (for ANSI terminals)")
        .optflag("q", "", "test quietly (output only errors)")
        //.optflag("t", "", "print contents of tEXt chunks (can be used with -q)");
        .optflag("v", "", "test verbosely (print most chunk data)")
        .parsing_style(ParsingStyle::StopAtFirstFree);
    if args.len() > 1 {
        match opts.parse(&args[1..]) {
            Ok(matches) => return Some(matches),
            Err(err) => println!("{}", err)
        }
    }
    println!("{}", opts.usage(&format!("Usage: pngcheck [-cpt] [file ...]")));
    None
}

#[derive(Clone, Copy)]
struct Config {
    quiet: bool,
    verbose: bool,
    color: bool
}

fn display_interlaced(i: bool) -> &'static str {
    if i {
        "interlaced"
    } else {
        "non-interlaced"
    }
}

fn display_image_type(bits: u8, color: png::ColorType) -> String {
    use png::ColorType::*;
    format!(
        "{}-bit {}",
        bits,
        match color {
            Grayscale => "grayscale",
            RGB => "RGB",
            Indexed => "palette",
            GrayscaleAlpha => "grayscale+alpha",
            RGBA => "RGB+alpha"
        }
    )
}
// channels after expansion of tRNS
fn final_channels(c: png::ColorType, trns: bool) -> u8 {
    use png::ColorType::*;
    match c {
        Grayscale => 1 + if trns { 1 } else { 0 },
        RGB => 3,
        Indexed => 3 + if trns { 1 } else { 0 },
        GrayscaleAlpha => 2,
        RGBA => 4
    }
}
fn check_image<P: AsRef<Path>>(c: Config, fname: P) -> io::Result<()> {
    // TODO improve performance by resusing allocations from decoder
    use png::Decoded::*;
    let mut t = try!(term::stdout().ok_or(io::Error::new(
        io::ErrorKind::Other,
        "could not open terminal"
    )));
    let mut data = vec![0; 10*1024];
    let data_p = data.as_mut_ptr();
    let mut reader = io::BufReader::new(try!(File::open(&fname)));
    let fname = fname.as_ref().to_string_lossy();
    let n = try!(reader.read(&mut data));
    let mut buf = &data[..n];
    let mut pos = 0;
    let mut decoder = png::StreamingDecoder::new();
    // Image data
    let mut width = 0;
    let mut height = 0;
    let mut color = png::ColorType::Grayscale;
    let mut bits = 0;
    let mut trns = false;
    let mut interlaced = false;
    let mut compressed_size = 0;
    let mut n_chunks = 0;
    let mut have_idat = false;
    macro_rules! c_ratio(
        // TODO add palette entries to compressed_size
        () => ({
            compressed_size as f32/(
                height as u64 *
                (width as u64 * final_channels(color, trns) as u64 * bits as u64 + 7)>>3
            ) as f32
        });
    );
    let display_error = |err| -> Result<_, io::Error> {
        let mut t = try!(term::stdout().ok_or(io::Error::new(
            io::ErrorKind::Other,
            "could not open terminal"
        )));
        if c.verbose {
            if c.color {
                print!(": ");
                try!(t.fg(color::RED));
                try!(writeln!(t, "{}", err));
                try!(t.attr(Attr::Bold));
                try!(write!(t, "ERRORS DETECTED"));
                try!(t.reset());
            } else {
                println!(": {}", err);
                print!("ERRORS DETECTED")
            }
            println!(" in {}", fname);
        } else {
            if !c.quiet { if c.color {
                try!(t.fg(color::RED));
                try!(t.attr(Attr::Bold));
                try!(write!(t, "ERROR"));
                try!(t.reset());
                try!(write!(t, ": "));
                try!(t.fg(color::YELLOW));
                try!(writeln!(t, "{}", fname));
                try!(t.reset());
            } else {
                println!("ERROR: {}", fname)
            }}
            print!("{}: ", fname);
            if c.color {
                try!(t.fg(color::RED));
                try!(writeln!(t, "{}", err));
                try!(t.reset());
            } else {
                println!("{}", err);
            }
            
        }
        Ok(())
    };
    
    if c.verbose {
        print!("File: ");
        if c.color {
            try!(t.attr(Attr::Bold));
            try!(write!(t, "{}", fname));
            try!(t.reset());
        } else {
            print!("{}", fname);
        }
        print!(" ({}) bytes", data.len())
            
    }
    loop {
        if buf.len() == 0 {
            // circumvent borrow checker
            let n = try!(reader.read(unsafe {
                ::std::slice::from_raw_parts_mut(data_p, data.len())
            }));
            buf = &data[..n];
        }
        match decoder.update(buf, &mut Vec::new()) {
            Ok((_, ImageEnd)) => {
                if !have_idat {
                    try!(display_error(png::DecodingError::Format("IDAT chunk missing".into())));
                    break;
                }
                if !c.verbose && !c.quiet {
                    if c.color {
                        try!(t.fg(color::GREEN));
                        try!(t.attr(Attr::Bold));
                        try!(write!(t, "OK"));
                        try!(t.reset());
                        try!(write!(t, ": "));
                        try!(t.fg(color::YELLOW));
                        try!(write!(t, "{}", fname));
                        try!(t.reset());
                    } else {
                        print!("OK: {}", fname)
                    }
                    println!(
                        " ({}x{}, {}{}, {}, {:.1}%)",
                        width,
                        height,
                        display_image_type(bits, color),
                        (if trns { "+trns" } else { "" }),
                        display_interlaced(interlaced),
                        100.0*(1.0-c_ratio!())
                    )
                } else if !c.quiet {
                    println!("");
                    if c.color {
                        try!(t.fg(color::GREEN));
                        try!(t.attr(Attr::Bold));
                        try!(write!(t, "No errors detected "));
                        try!(t.reset());
                    } else {
                        print!("No errors detected ");
                    }
                    println!(
                        "in {} ({} chunks, {:.1}% compression)",
                        fname,
                        n_chunks,
                        100.0*(1.0-c_ratio!())
                    )
                }
                break
            },
            Ok((n, res)) => {
                buf = &buf[n..];
                pos += n;
                match res {
                    Header(w, h, b, c, i) => {
                        width = w;
                        height = h;
                        bits = b as u8;
                        color = c;
                        interlaced = i;
                    }
                    ChunkBegin(len, type_str) => {
                        use png::chunk;
                        n_chunks += 1;
                        if c.verbose {
                            let chunk = String::from_utf8_lossy(&type_str);
                            println!("");
                            print!("  chunk ");
                            if c.color {
                                try!(t.fg(color::YELLOW));
                                try!(write!(t, "{}", chunk));
                                try!(t.reset());
                            } else {
                                print!("{}", chunk)
                            }
                            print!(
                                " at offset {:#07x}, length {}",
                                pos - 4, // substract chunk name length
                                len
                            )
                        }
                        match type_str {
                            chunk::IDAT => {
                                have_idat = true;
                                compressed_size += len
                            },
                            chunk::tRNS => {
                                trns = true;
                            },    
                            _ => ()
                        }
                    }
                    ImageData => {
                        //println!("got {} bytes of image data", data.len())
                    }
                    ChunkComplete(_, type_str) if c.verbose => {
                        use png::chunk::*;
                        match type_str {
                            IHDR => {
                                println!("");
                                print!(
                                    "    {} x {} image, {}{}, {}",
                                    width,
                                    height,
                                    display_image_type(bits, color),
                                    (if trns { "+trns" } else { "" }),
                                    display_interlaced(interlaced),
                                );
                            }
                            _ => ()
                        }
                    }
                    AnimationControl(actl) => {
                        println!("");
                        print!(
                            "    {} frames, {} plays",
                            actl.num_frames,
                            actl.num_plays,
                        );
                    }
                    FrameControl(fctl) => {
                        println!("");
                        println!(
                            "    sequence #{}, {} x {} pixels @ ({}, {})",
                            fctl.sequence_number,
                            fctl.width,
                            fctl.height,
                            fctl.x_offset,
                            fctl.y_offset,
                            /*fctl.delay_num,
                            fctl.delay_den,
                            fctl.dispose_op,
                            fctl.blend_op,*/
                        );
                        print!(
                            "    {}/{} s delay, dispose: {}, blend: {}",
                            fctl.delay_num,
                            if fctl.delay_den == 0 { 100 } else {fctl.delay_den},
                            fctl.dispose_op,
                            fctl.blend_op,
                        );
                    }
                    _ => ()
                }
                //println!("{} {:?}", n, res)
            },
            Err(err) => {
                let _ = display_error(err);
                break
            }
        }
    }
    Ok(())
}

fn main() {
    if let Some(m) = parse_args() {
        let config = Config {
            quiet: m.opt_present("q"),
            verbose: m.opt_present("v"),
            color: m.opt_present("c")
        };
        for file in m.free {
            match if file.contains("*") {
                (|| -> io::Result<_> {
                    for entry in try!(glob::glob(&file).map_err(|err| {
                        io::Error::new(io::ErrorKind::Other, err.msg)
                    })) {
                        try!(check_image(config, try!(entry.map_err(|_| {
                            io::Error::new(io::ErrorKind::Other, "glob error")
                        }))))
                    }
                    Ok(())
                })()
            } else {
                check_image(config, &file)
            } {
                Ok(_) => (),
                Err(err) => {
                    println!("{}: {}", file, err);
                    break
                }
            }
            
        }
    }
}
