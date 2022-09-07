/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{Context, Result};
use askama::Template;
use camino::Utf8PathBuf;
use clap::Parser;
use heck::ToTitleCase;
use std::fs::File;
use std::io::Write;

mod ci_list;
mod render;

pub use ci_list::{ComponentInterfaceUniverse, FunctionIds, ObjectIds};
pub use render::cpp::CPPScaffoldingTemplate;
pub use render::js::JSBindingsTemplate;
use uniffi_bindgen::ComponentInterface;

#[derive(Debug, Parser)]
#[clap(name = "uniffi-bindgen-gecko-js")]
#[clap(version = clap::crate_version!())]
#[clap(about = "JS bindings generator for Rust")]
#[clap(propagate_version = true)]
struct CliArgs {
    // This is a really convoluted set of arguments, but we're only expecting to be called by
    // `mach_commands.py`
    #[clap(long, value_name = "FILE")]
    js_dir: Utf8PathBuf,

    #[clap(long, value_name = "FILE")]
    fixture_js_dir: Utf8PathBuf,

    #[clap(long, value_name = "FILE")]
    cpp_path: Utf8PathBuf,

    #[clap(long, value_name = "FILE")]
    fixture_cpp_path: Utf8PathBuf,

    #[clap(long, multiple_values = true, value_name = "FILES")]
    udl_files: Vec<Utf8PathBuf>,

    #[clap(long, multiple_values = true, value_name = "FILES")]
    fixture_udl_files: Vec<Utf8PathBuf>,
}

fn render(out_path: Utf8PathBuf, template: impl Template) -> Result<()> {
    println!("rendering {}", out_path);
    let contents = template.render()?;
    let mut f =
        File::create(&out_path).context(format!("Failed to create {:?}", out_path.file_name()))?;
    write!(f, "{}\n", contents).context(format!("Failed to write to {}", out_path))
}

fn render_cpp(
    path: Utf8PathBuf,
    prefix: &str,
    ci_list: &Vec<ComponentInterface>,
    function_ids: &FunctionIds,
    object_ids: &ObjectIds,
) -> Result<()> {
    render(
        path,
        CPPScaffoldingTemplate {
            prefix,
            ci_list,
            function_ids: &function_ids,
            object_ids: &object_ids,
        },
    )
}

fn render_js(
    out_dir: Utf8PathBuf,
    ci_list: &Vec<ComponentInterface>,
    function_ids: &FunctionIds,
    object_ids: &ObjectIds,
) -> Result<()> {
    for ci in ci_list {
        // The plain namespace name is a bit too generic as a module name for m-c, so we
        // prefix it with "Rust". Later we'll probably allow this to be customized.
        let path = out_dir.join(format!("Rust{}.jsm", ci.namespace().to_title_case()));
        render(
            path,
            JSBindingsTemplate {
                ci,
                function_ids: &function_ids,
                object_ids: &object_ids,
            },
        )?;
    }
    Ok(())
}

pub fn run_main() -> Result<()> {
    let args = CliArgs::parse();
    let cis = ComponentInterfaceUniverse::new(args.udl_files, args.fixture_udl_files)?;
    let function_ids = FunctionIds::new(&cis);
    let object_ids = ObjectIds::new(&cis);

    render_cpp(
        args.cpp_path,
        "UniFFI",
        cis.ci_list(),
        &function_ids,
        &object_ids,
    )?;
    render_cpp(
        args.fixture_cpp_path,
        "UniFFIFixtures",
        cis.fixture_ci_list(),
        &function_ids,
        &object_ids,
    )?;
    render_js(args.js_dir, cis.ci_list(), &function_ids, &object_ids)?;
    render_js(
        args.fixture_js_dir,
        cis.fixture_ci_list(),
        &function_ids,
        &object_ids,
    )?;

    Ok(())
}
