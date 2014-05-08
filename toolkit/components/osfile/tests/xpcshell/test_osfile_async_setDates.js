"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");

/**
 * A test to ensure that OS.File.setDates and OS.File.prototype.setDates are
 * working correctly.
 * (see bug 924916)
 */

function run_test() {
  do_test_pending();
  run_next_test();
}

// Non-prototypical tests, operating on path names.
add_task(function* test_nonproto() {
  // First, create a file we can mess with.
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                              "test_osfile_async_setDates_nonproto.tmp");
  yield OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    // 1. Try to set some well known dates.
    // We choose multiples of 2000ms, because the time stamp resolution of
    // the underlying OS might not support something more precise.
    const accDate = 2000;
    const modDate = 4000;
    {
      yield OS.File.setDates(path, accDate, modDate);
      let stat = yield OS.File.stat(path);
      do_check_eq(accDate, stat.lastAccessDate.getTime());
      do_check_eq(modDate, stat.lastModificationDate.getTime());
    }

    // 2.1 Try to omit modificationDate (which should then default to
    // |Date.now()|, expect for resolution differences).
    {
      yield OS.File.setDates(path, accDate);
      let stat = yield OS.File.stat(path);
      do_check_eq(accDate, stat.lastAccessDate.getTime());
      do_check_neq(modDate, stat.lastModificationDate.getTime());
    }

    // 2.2 Try to omit accessDate as well (which should then default to
    // |Date.now()|, expect for resolution differences).
    {
      yield OS.File.setDates(path);
      let stat = yield OS.File.stat(path);
      do_check_neq(accDate, stat.lastAccessDate.getTime());
      do_check_neq(modDate, stat.lastModificationDate.getTime());
    }

    // 3. Repeat 1., but with Date objects this time
    {
      yield OS.File.setDates(path, new Date(accDate), new Date(modDate));
      let stat = yield OS.File.stat(path);
      do_check_eq(accDate, stat.lastAccessDate.getTime());
      do_check_eq(modDate, stat.lastModificationDate.getTime());
    }

    // 4. Check that invalid params will cause an exception/rejection.
    {
      for (let p of ["invalid", new Uint8Array(1), NaN]) {
        try {
          yield OS.File.setDates(path, p, modDate);
          do_throw("Invalid access date should have thrown for: " + p);
        } catch (ex) {
          let stat = yield OS.File.stat(path);
          do_check_eq(accDate, stat.lastAccessDate.getTime());
          do_check_eq(modDate, stat.lastModificationDate.getTime());
        }
        try {
          yield OS.File.setDates(path, accDate, p);
          do_throw("Invalid modification date should have thrown for: " + p);
        } catch (ex) {
          let stat = yield OS.File.stat(path);
          do_check_eq(accDate, stat.lastAccessDate.getTime());
          do_check_eq(modDate, stat.lastModificationDate.getTime());
        }
        try {
          yield OS.File.setDates(path, p, p);
          do_throw("Invalid dates should have thrown for: " + p);
        } catch (ex) {
          let stat = yield OS.File.stat(path);
          do_check_eq(accDate, stat.lastAccessDate.getTime());
          do_check_eq(modDate, stat.lastModificationDate.getTime());
        }
      }
    }
  } finally {
    // Remove the temp file again
    yield OS.File.remove(path);
  }
});

// Prototypical tests, operating on |File| handles.
add_task(function* test_proto() {
  if (OS.Constants.Sys.Name == "Android") {
    do_print("File.prototype.setDates is not implemented for Android/B2G");
    do_check_eq(OS.File.prototype.setDates, undefined);
    return;
  }

  // First, create a file we can mess with.
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                              "test_osfile_async_setDates_proto.tmp");
  yield OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    let fd = yield OS.File.open(path, {write: true});

    try {
      // 1. Try to set some well known dates.
      // We choose multiples of 2000ms, because the time stamp resolution of
      // the underlying OS might not support something more precise.
      const accDate = 2000;
      const modDate = 4000;
      {
        yield fd.setDates(accDate, modDate);
        let stat = yield fd.stat();
        do_check_eq(accDate, stat.lastAccessDate.getTime());
        do_check_eq(modDate, stat.lastModificationDate.getTime());
      }

      // 2.1 Try to omit modificationDate (which should then default to
      // |Date.now()|, expect for resolution differences).
      {
        yield fd.setDates(accDate);
        let stat = yield fd.stat();
        do_check_eq(accDate, stat.lastAccessDate.getTime());
        do_check_neq(modDate, stat.lastModificationDate.getTime());
      }

      // 2.2 Try to omit accessDate as well (which should then default to
      // |Date.now()|, expect for resolution differences).
      {
        yield fd.setDates();
        let stat = yield fd.stat();
        do_check_neq(accDate, stat.lastAccessDate.getTime());
        do_check_neq(modDate, stat.lastModificationDate.getTime());
      }

      // 3. Repeat 1., but with Date objects this time
      {
        yield fd.setDates(new Date(accDate), new Date(modDate));
        let stat = yield fd.stat();
        do_check_eq(accDate, stat.lastAccessDate.getTime());
        do_check_eq(modDate, stat.lastModificationDate.getTime());
      }

      // 4. Check that invalid params will cause an exception/rejection.
      {
        for (let p of ["invalid", new Uint8Array(1), NaN]) {
          try {
            yield fd.setDates(p, modDate);
            do_throw("Invalid access date should have thrown for: " + p);
          } catch (ex) {
            let stat = yield fd.stat();
            do_check_eq(accDate, stat.lastAccessDate.getTime());
            do_check_eq(modDate, stat.lastModificationDate.getTime());
          }
          try {
            yield fd.setDates(accDate, p);
            do_throw("Invalid modification date should have thrown for: " + p);
          } catch (ex) {
            let stat = yield fd.stat();
            do_check_eq(accDate, stat.lastAccessDate.getTime());
            do_check_eq(modDate, stat.lastModificationDate.getTime());
          }
          try {
            yield fd.setDates(p, p);
            do_throw("Invalid dates should have thrown for: " + p);
          } catch (ex) {
            let stat = yield fd.stat();
            do_check_eq(accDate, stat.lastAccessDate.getTime());
            do_check_eq(modDate, stat.lastModificationDate.getTime());
          }
        }
      }
    } finally {
      yield fd.close();
    }
  } finally {
    // Remove the temp file again
    yield OS.File.remove(path);
  }
});

// Tests setting dates on directories.
add_task(function test_dirs() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                              "test_osfile_async_setDates_dir");
  yield OS.File.makeDir(path);

  try {
    // 1. Try to set some well known dates.
    // We choose multiples of 2000ms, because the time stamp resolution of
    // the underlying OS might not support something more precise.
    const accDate = 2000;
    const modDate = 4000;
    {
      yield OS.File.setDates(path, accDate, modDate);
      let stat = yield OS.File.stat(path);
      do_check_eq(accDate, stat.lastAccessDate.getTime());
      do_check_eq(modDate, stat.lastModificationDate.getTime());
    }
  } finally {
    yield OS.File.removeEmptyDir(path);
  }
});

add_task(do_test_finished);
