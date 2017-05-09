//! Generating Graphviz `dot` files from our IR.

use super::context::{BindgenContext, ItemId};
use super::traversal::Trace;
use std::fs::File;
use std::io::{self, Write};
use std::path::Path;

/// A trait for anything that can write attributes as `<table>` rows to a dot
/// file.
pub trait DotAttributes {
    /// Write this thing's attributes to the given output. Each attribute must
    /// be its own `<tr>...</tr>`.
    fn dot_attributes<W>(&self,
                         ctx: &BindgenContext,
                         out: &mut W)
                         -> io::Result<()>
        where W: io::Write;
}

/// Write a graphviz dot file containing our IR.
pub fn write_dot_file<P>(ctx: &BindgenContext, path: P) -> io::Result<()>
    where P: AsRef<Path>,
{
    let file = try!(File::create(path));
    let mut dot_file = io::BufWriter::new(file);
    try!(writeln!(&mut dot_file, "digraph {{"));

    let mut err: Option<io::Result<_>> = None;

    for (id, item) in ctx.items() {
        try!(writeln!(&mut dot_file,
                      r#"{} [fontname="courier", label=< <table border="0">"#,
                      id.as_usize()));
        try!(item.dot_attributes(ctx, &mut dot_file));
        try!(writeln!(&mut dot_file, r#"</table> >];"#));

        item.trace(ctx,
                   &mut |sub_id: ItemId, edge_kind| {
            if err.is_some() {
                return;
            }

            match writeln!(&mut dot_file,
                           "{} -> {} [label={:?}];",
                           id.as_usize(),
                           sub_id.as_usize(),
                           edge_kind) {
                Ok(_) => {}
                Err(e) => err = Some(Err(e)),
            }
        },
                   &());

        if let Some(err) = err {
            return err;
        }
    }

    try!(writeln!(&mut dot_file, "}}"));
    Ok(())
}
