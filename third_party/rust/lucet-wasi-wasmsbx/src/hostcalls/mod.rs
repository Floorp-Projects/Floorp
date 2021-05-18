//! Hostcalls that implement
//! [WASI](https://github.com/CraneStation/wasmtime-wasi/blob/wasi/docs/WASI-overview.md).
//!
//! This code borrows heavily from [wasmtime-wasi](https://github.com/CraneStation/wasmtime-wasi),
//! which in turn borrows from cloudabi-utils. See `LICENSE.wasmtime-wasi` for license information.

#![allow(non_camel_case_types)]
#![allow(unused_unsafe)]

mod fs;
mod fs_helpers;
mod misc;
mod timers;

use crate::wasm32;

use fs::*;
use lucet_runtime::lucet_hostcalls;
use misc::*;

lucet_hostcalls! {
    #[no_mangle] pub unsafe extern "C"
    fn __wasi_proc_exit(&mut vmctx, rval: wasm32::__wasi_exitcode_t,) -> ! {
        wasi_proc_exit(vmctx, rval)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_args_get(
        &mut vmctx,
        argv_ptr: wasm32::uintptr_t,
        argv_buf: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_args_get(vmctx, argv_ptr, argv_buf)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_args_sizes_get(&mut vmctx,
        argc_ptr: wasm32::uintptr_t,
        argv_buf_size_ptr: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_args_sizes_get(vmctx, argc_ptr, argv_buf_size_ptr)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_sched_yield(&mut vmctx,) -> wasm32::__wasi_errno_t {
        wasi_sched_yield(vmctx)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_clock_res_get(
        &mut vmctx,
        clock_id: wasm32::__wasi_clockid_t,
        resolution_ptr: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_clock_res_get(vmctx, clock_id, resolution_ptr)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_clock_time_get(
        &mut vmctx,
        clock_id: wasm32::__wasi_clockid_t,
        precision: wasm32::__wasi_timestamp_t,
        time_ptr: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_clock_time_get(vmctx, clock_id, precision, time_ptr)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_environ_get(
        &mut vmctx,
        environ_ptr: wasm32::uintptr_t,
        environ_buf: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_environ_get(vmctx, environ_ptr, environ_buf)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_environ_sizes_get(
        &mut vmctx,
        environ_count_ptr: wasm32::uintptr_t,
        environ_size_ptr: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_environ_sizes_get(vmctx, environ_count_ptr, environ_size_ptr)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_close(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_close(vmctx, fd)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_fdstat_get(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        fdstat_ptr: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_fdstat_get(vmctx, fd, fdstat_ptr)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_fdstat_set_flags(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        fdflags: wasm32::__wasi_fdflags_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_fdstat_set_flags(vmctx, fd, fdflags)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_tell(&mut vmctx,
        fd: wasm32::__wasi_fd_t,
        offset: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_tell(vmctx, fd, offset)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_seek(&mut vmctx,
        fd: wasm32::__wasi_fd_t,
        offset: wasm32::__wasi_filedelta_t,
        whence: wasm32::__wasi_whence_t,
        newoffset: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_seek(vmctx, fd, offset, whence, newoffset)
    }

     #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_prestat_get(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        prestat_ptr: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_prestat_get(vmctx, fd, prestat_ptr)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_prestat_dir_name(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        path_ptr: wasm32::uintptr_t,
        path_len: wasm32::size_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_prestat_dir_name(vmctx, fd, path_ptr, path_len)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_read(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        iovs_ptr: wasm32::uintptr_t,
        iovs_len: wasm32::size_t,
        nread: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_read(vmctx, fd, iovs_ptr, iovs_len, nread)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_write(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        iovs_ptr: wasm32::uintptr_t,
        iovs_len: wasm32::size_t,
        nwritten: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_write(vmctx, fd, iovs_ptr, iovs_len, nwritten)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_path_open(
        &mut vmctx,
        dirfd: wasm32::__wasi_fd_t,
        dirflags: wasm32::__wasi_lookupflags_t,
        path_ptr: wasm32::uintptr_t,
        path_len: wasm32::size_t,
        oflags: wasm32::__wasi_oflags_t,
        fs_rights_base: wasm32::__wasi_rights_t,
        fs_rights_inheriting: wasm32::__wasi_rights_t,
        fs_flags: wasm32::__wasi_fdflags_t,
        fd_out_ptr: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_path_open(vmctx, dirfd, dirflags, path_ptr, path_len,
            oflags, fs_rights_base, fs_rights_inheriting, fs_flags,
            fd_out_ptr)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_random_get(
        &mut vmctx,
        buf_ptr: wasm32::uintptr_t,
        buf_len: wasm32::size_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_random_get(vmctx, buf_ptr, buf_len)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_poll_oneoff(
        &mut vmctx,
        input: wasm32::uintptr_t,
        output: wasm32::uintptr_t,
        nsubscriptions: wasm32::size_t,
        nevents: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
         wasi_poll_oneoff(vmctx, input, output, nsubscriptions, nevents)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_filestat_get(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        filestat_ptr: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_filestat_get(vmctx, fd, filestat_ptr)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_path_filestat_get(
        &mut vmctx,
        dirfd: wasm32::__wasi_fd_t,
        dirflags: wasm32::__wasi_lookupflags_t,
        path_ptr: wasm32::uintptr_t,
        path_len: wasm32::size_t,
        filestat_ptr: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_path_filestat_get(vmctx, dirfd, dirflags, path_ptr,
            path_len, filestat_ptr)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_path_create_directory(
        &mut vmctx,
        dirfd: wasm32::__wasi_fd_t,
        path_ptr: wasm32::uintptr_t,
        path_len: wasm32::size_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_path_create_directory(vmctx, dirfd, path_ptr, path_len)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_path_unlink_file(
        &mut vmctx,
        dirfd: wasm32::__wasi_fd_t,
        path_ptr: wasm32::uintptr_t,
        path_len: wasm32::size_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_path_unlink_file(vmctx, dirfd, path_ptr, path_len)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_allocate(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        offset: wasm32::__wasi_filesize_t,
        len: wasm32::__wasi_filesize_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_allocate(vmctx, fd, offset, len)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_advise(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        offset: wasm32::__wasi_filesize_t,
        len: wasm32::__wasi_filesize_t,
        advice: wasm32::__wasi_advice_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_advise(vmctx, fd, offset, len, advice)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_datasync(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_datasync(vmctx, fd)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_sync(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_sync(vmctx, fd)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_fdstat_set_rights(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        fs_rights_base: wasm32::__wasi_rights_t,
        fs_rights_inheriting: wasm32::__wasi_rights_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_fdstat_set_rights(vmctx, fd, fs_rights_base, fs_rights_inheriting)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_filestat_set_size(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        st_size: wasm32::__wasi_filesize_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_filestat_set_size(vmctx, fd, st_size)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_filestat_set_times(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        st_atim: wasm32::__wasi_timestamp_t,
        st_mtim: wasm32::__wasi_timestamp_t,
        fst_flags: wasm32::__wasi_fstflags_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_filestat_set_times(vmctx, fd, st_atim, st_mtim, fst_flags)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_pread(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        iovs_ptr: wasm32::uintptr_t,
        iovs_len: wasm32::size_t,
        offset: wasm32::__wasi_filesize_t,
        nread: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_pread(vmctx, fd, iovs_ptr, iovs_len, offset, nread)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_pwrite(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        iovs_ptr: wasm32::uintptr_t,
        iovs_len: wasm32::size_t,
        offset: wasm32::__wasi_filesize_t,
        nwritten: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_pwrite(vmctx, fd, iovs_ptr, iovs_len, offset, nwritten)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_readdir(
        &mut vmctx,
        fd: wasm32::__wasi_fd_t,
        buf: wasm32::uintptr_t,
        buf_len: wasm32::size_t,
        cookie: wasm32::__wasi_dircookie_t,
        bufused: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_readdir(vmctx, fd, buf, buf_len, cookie, bufused)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_fd_renumber(
        &mut vmctx,
        from: wasm32::__wasi_fd_t,
        to: wasm32::__wasi_fd_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_fd_renumber(vmctx, from, to)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_path_filestat_set_times(
        &mut vmctx,
        dirfd: wasm32::__wasi_fd_t,
        dirflags: wasm32::__wasi_lookupflags_t,
        path_ptr: wasm32::uintptr_t,
        path_len: wasm32::size_t,
        st_atim: wasm32::__wasi_timestamp_t,
        st_mtim: wasm32::__wasi_timestamp_t,
        fst_flags: wasm32::__wasi_fstflags_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_path_filestat_set_times(vmctx, dirfd, dirflags, path_ptr, path_len, st_atim, st_mtim, fst_flags)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_path_link(
        &mut vmctx,
        old_fd: wasm32::__wasi_fd_t,
        old_flags: wasm32::__wasi_lookupflags_t,
        old_path_ptr: wasm32::uintptr_t,
        old_path_len: wasm32::size_t,
        new_fd: wasm32::__wasi_fd_t,
        new_path_ptr: wasm32::uintptr_t,
        new_path_len: wasm32::size_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_path_link(vmctx, old_fd, old_flags, old_path_ptr, old_path_len,
            new_fd, new_path_ptr, new_path_len)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_path_readlink(
        &mut vmctx,
        dirfd: wasm32::__wasi_fd_t,
        path_ptr: wasm32::uintptr_t,
        path_len: wasm32::size_t,
        buf_ptr: wasm32::uintptr_t,
        buf_len: wasm32::size_t,
        bufused: wasm32::uintptr_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_path_readlink(vmctx, dirfd, path_ptr, path_len, buf_ptr, buf_len, bufused)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_path_remove_directory(
        &mut vmctx,
        dirfd: wasm32::__wasi_fd_t,
        path_ptr: wasm32::uintptr_t,
        path_len: wasm32::size_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_path_remove_directory(vmctx, dirfd, path_ptr, path_len)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_path_rename(
        &mut vmctx,
        old_dirfd: wasm32::__wasi_fd_t,
        old_path_ptr: wasm32::uintptr_t,
        old_path_len: wasm32::size_t,
        new_dirfd: wasm32::__wasi_fd_t,
        new_path_ptr: wasm32::uintptr_t,
        new_path_len: wasm32::size_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_path_rename(vmctx, old_dirfd, old_path_ptr, old_path_len,
            new_dirfd, new_path_ptr, new_path_len)
    }

    #[no_mangle] pub unsafe extern "C"
    fn __wasi_path_symlink(
        &mut vmctx,
        old_path_ptr: wasm32::uintptr_t,
        old_path_len: wasm32::size_t,
        dir_fd: wasm32::__wasi_fd_t,
        new_path_ptr: wasm32::uintptr_t,
        new_path_len: wasm32::size_t,
    ) -> wasm32::__wasi_errno_t {
        wasi_path_symlink(vmctx, old_path_ptr, old_path_len,
            dir_fd, new_path_ptr, new_path_len)
    }
}

#[doc(hidden)]
pub fn ensure_linked() {
    unsafe {
        std::ptr::read_volatile(__wasi_proc_exit as *const extern "C" fn());
    }
}
