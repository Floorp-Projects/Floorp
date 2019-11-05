mod auxiliary;

macro_rules! pos {
    () => {
        (file!(), line!())
    };
}

macro_rules! check {
    ($($pos:expr),*) => ({
        verify(&[$($pos,)* pos!()]);
    })
}

type Pos = (&'static str, u32);

#[test]
fn doit() {
    outer(pos!());
}

#[inline(never)]
fn outer(main_pos: Pos) {
    inner(main_pos, pos!());
    inner_inlined(main_pos, pos!());
}

#[inline(never)]
#[rustfmt::skip]
fn inner(main_pos: Pos, outer_pos: Pos) {
    check!(main_pos, outer_pos);
    check!(main_pos, outer_pos);
    let inner_pos = pos!(); auxiliary::callback(|aux_pos| {
        check!(main_pos, outer_pos, inner_pos, aux_pos);
    });
    let inner_pos = pos!(); auxiliary::callback_inlined(|aux_pos| {
        check!(main_pos, outer_pos, inner_pos, aux_pos);
    });
}

#[inline(always)]
#[cfg_attr(feature = "coresymbolication", inline(never))]
#[rustfmt::skip]
fn inner_inlined(main_pos: Pos, outer_pos: Pos) {
    check!(main_pos, outer_pos);
    check!(main_pos, outer_pos);

    #[inline(always)]
    #[cfg_attr(feature = "coresymbolication", inline(never))]
    fn inner_further_inlined(main_pos: Pos, outer_pos: Pos, inner_pos: Pos) {
        check!(main_pos, outer_pos, inner_pos);
    }
    inner_further_inlined(main_pos, outer_pos, pos!());

    let inner_pos = pos!(); auxiliary::callback(|aux_pos| {
        check!(main_pos, outer_pos, inner_pos, aux_pos);
    });
    let inner_pos = pos!(); auxiliary::callback_inlined(|aux_pos| {
        check!(main_pos, outer_pos, inner_pos, aux_pos);
    });

    // this tests a distinction between two independent calls to the inlined function.
    // (un)fortunately, LLVM somehow merges two consecutive such calls into one node.
    inner_further_inlined(main_pos, outer_pos, pos!());
}

fn verify(filelines: &[Pos]) {
    let trace = backtrace::Backtrace::new();
    println!("-----------------------------------");
    println!("looking for:");
    for (file, line) in filelines.iter().rev() {
        println!("\t{}:{}", file, line);
    }
    println!("found:\n{:?}", trace);
    let mut symbols = trace.frames().iter().flat_map(|frame| frame.symbols());
    let mut iter = filelines.iter().rev();
    while let Some((file, line)) = iter.next() {
        loop {
            let sym = match symbols.next() {
                Some(sym) => sym,
                None => panic!("failed to find {}:{}", file, line),
            };
            if let Some(filename) = sym.filename() {
                if let Some(lineno) = sym.lineno() {
                    if filename.ends_with(file) && lineno == *line {
                        break;
                    }
                }
            }
        }
    }
}
