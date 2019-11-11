use failure::{bail, Error};
use lucet_runtime::{DlModule, Limits, MmapRegion, Module, Region};
use lucet_wasi::host::__wasi_exitcode_t;
use lucet_wasi::{self, WasiCtx, WasiCtxBuilder};
use lucet_wasi_sdk::{CompileOpts, Link};
use lucetc::{Lucetc, LucetcOpts};
use std::fs::File;
use std::io::Read;
use std::os::unix::io::{FromRawFd, IntoRawFd};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use tempfile::TempDir;

pub const LUCET_WASI_ROOT: &'static str = env!("CARGO_MANIFEST_DIR");

pub fn test_module_wasi<P: AsRef<Path>>(cfile: P) -> Result<Arc<dyn Module>, Error> {
    let c_path = guest_file(&cfile);
    wasi_test(c_path)
}

pub fn guest_file<P: AsRef<Path>>(path: P) -> PathBuf {
    let p = if path.as_ref().is_absolute() {
        path.as_ref().to_owned()
    } else {
        Path::new(LUCET_WASI_ROOT)
            .join("tests")
            .join("guests")
            .join(path)
    };
    assert!(p.exists(), "test case source file {} exists", p.display());
    p
}

pub fn wasi_test<P: AsRef<Path>>(file: P) -> Result<Arc<dyn Module>, Error> {
    let workdir = TempDir::new().expect("create working directory");

    let wasm_path = match file.as_ref().extension().and_then(|x| x.to_str()) {
        Some("c") => {
            // some tests are .c, and must be compiled/linked to .wasm we can run
            let wasm_build = Link::new(&[file])
                .with_cflag("-Wall")
                .with_cflag("-Werror")
                .with_print_output(true);

            let wasm_file = workdir.path().join("out.wasm");
            wasm_build.link(wasm_file.clone())?;

            wasm_file
        }
        Some("wasm") | Some("wat") => {
            // others are just wasm we can run directly
            file.as_ref().to_owned()
        }
        Some(ext) => {
            panic!("unknown test file extension: .{}", ext);
        }
        None => {
            panic!("unknown test file, has no extension");
        }
    };

    wasi_load(&workdir, wasm_path)
}

pub fn wasi_load<P: AsRef<Path>>(
    workdir: &TempDir,
    wasm_file: P,
) -> Result<Arc<dyn Module>, Error> {
    let native_build = Lucetc::new(wasm_file).with_bindings(lucet_wasi::bindings());

    let so_file = workdir.path().join("out.so");

    native_build.shared_object_file(so_file.clone())?;

    let dlmodule = DlModule::load(so_file)?;

    Ok(dlmodule as Arc<dyn Module>)
}

pub fn run<P: AsRef<Path>>(path: P, ctx: WasiCtx) -> Result<__wasi_exitcode_t, Error> {
    let region = MmapRegion::create(1, &Limits::default())?;
    let module = test_module_wasi(path)?;

    let mut inst = region
        .new_instance_builder(module)
        .with_embed_ctx(ctx)
        .build()?;

    match inst.run("_start", &[]) {
        // normal termination implies 0 exit code
        Ok(_) => Ok(0),
        Err(lucet_runtime::Error::RuntimeTerminated(
            lucet_runtime::TerminationDetails::Provided(any),
        )) => Ok(*any
            .downcast_ref::<__wasi_exitcode_t>()
            .expect("termination yields an exitcode")),
        Err(e) => bail!("runtime error: {}", e),
    }
}

pub fn run_with_stdout<P: AsRef<Path>>(
    path: P,
    ctx: WasiCtxBuilder,
) -> Result<(__wasi_exitcode_t, String), Error> {
    let (pipe_out, pipe_in) = nix::unistd::pipe()?;

    let ctx = unsafe { ctx.raw_fd(1, pipe_in) }.build()?;

    let exitcode = run(path, ctx)?;

    let mut stdout_file = unsafe { File::from_raw_fd(pipe_out) };
    let mut stdout = String::new();
    stdout_file.read_to_string(&mut stdout)?;
    nix::unistd::close(stdout_file.into_raw_fd())?;

    Ok((exitcode, stdout))
}

/// Call this if you're having trouble with `__wasi_*` symbols not being exported.
///
/// This is pretty hackish; we will hopefully be able to avoid this altogether once [this
/// issue](https://github.com/rust-lang/rust/issues/58037) is addressed.
#[no_mangle]
#[doc(hidden)]
pub extern "C" fn lucet_wasi_tests_internal_ensure_linked() {
    lucet_wasi::hostcalls::ensure_linked();
}
