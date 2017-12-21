"use strict";

/* eslint-disable no-lone-blocks */

Components.utils.import("resource://gre/modules/osfile.jsm");

/**
 * A test to ensure that OS.File.setDates and OS.File.prototype.setDates are
 * working correctly.
 * (see bug 924916)
 */

// Non-prototypical tests, operating on path names.
add_task(async function test_nonproto() {
  // First, create a file we can mess with.
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                              "test_osfile_async_setDates_nonproto.tmp");
  await OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    // 1. Try to set some well known dates.
    // We choose multiples of 2000ms, because the time stamp resolution of
    // the underlying OS might not support something more precise.
    const accDate = 2000;
    const modDate = 4000;
    {
      await OS.File.setDates(path, accDate, modDate);
      let stat = await OS.File.stat(path);
      Assert.equal(accDate, stat.lastAccessDate.getTime());
      Assert.equal(modDate, stat.lastModificationDate.getTime());
    }

    // 2.1 Try to omit modificationDate (which should then default to
    // |Date.now()|, expect for resolution differences).
    {
      await OS.File.setDates(path, accDate);
      let stat = await OS.File.stat(path);
      Assert.equal(accDate, stat.lastAccessDate.getTime());
      Assert.notEqual(modDate, stat.lastModificationDate.getTime());
    }

    // 2.2 Try to omit accessDate as well (which should then default to
    // |Date.now()|, expect for resolution differences).
    {
      await OS.File.setDates(path);
      let stat = await OS.File.stat(path);
      Assert.notEqual(accDate, stat.lastAccessDate.getTime());
      Assert.notEqual(modDate, stat.lastModificationDate.getTime());
    }

    // 3. Repeat 1., but with Date objects this time
    {
      await OS.File.setDates(path, new Date(accDate), new Date(modDate));
      let stat = await OS.File.stat(path);
      Assert.equal(accDate, stat.lastAccessDate.getTime());
      Assert.equal(modDate, stat.lastModificationDate.getTime());
    }

    // 4. Check that invalid params will cause an exception/rejection.
    {
      for (let p of ["invalid", new Uint8Array(1), NaN]) {
        try {
          await OS.File.setDates(path, p, modDate);
          do_throw("Invalid access date should have thrown for: " + p);
        } catch (ex) {
          let stat = await OS.File.stat(path);
          Assert.equal(accDate, stat.lastAccessDate.getTime());
          Assert.equal(modDate, stat.lastModificationDate.getTime());
        }
        try {
          await OS.File.setDates(path, accDate, p);
          do_throw("Invalid modification date should have thrown for: " + p);
        } catch (ex) {
          let stat = await OS.File.stat(path);
          Assert.equal(accDate, stat.lastAccessDate.getTime());
          Assert.equal(modDate, stat.lastModificationDate.getTime());
        }
        try {
          await OS.File.setDates(path, p, p);
          do_throw("Invalid dates should have thrown for: " + p);
        } catch (ex) {
          let stat = await OS.File.stat(path);
          Assert.equal(accDate, stat.lastAccessDate.getTime());
          Assert.equal(modDate, stat.lastModificationDate.getTime());
        }
      }
    }
  } finally {
    // Remove the temp file again
    await OS.File.remove(path);
  }
});

// Prototypical tests, operating on |File| handles.
add_task(async function test_proto() {
  if (OS.Constants.Sys.Name == "Android") {
    do_print("File.prototype.setDates is not implemented for Android");
    Assert.equal(OS.File.prototype.setDates, undefined);
    return;
  }

  // First, create a file we can mess with.
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                              "test_osfile_async_setDates_proto.tmp");
  await OS.File.writeAtomic(path, new Uint8Array(1));

  try {
    let fd = await OS.File.open(path, {write: true});

    try {
      // 1. Try to set some well known dates.
      // We choose multiples of 2000ms, because the time stamp resolution of
      // the underlying OS might not support something more precise.
      const accDate = 2000;
      const modDate = 4000;
      {
        await fd.setDates(accDate, modDate);
        let stat = await fd.stat();
        Assert.equal(accDate, stat.lastAccessDate.getTime());
        Assert.equal(modDate, stat.lastModificationDate.getTime());
      }

      // 2.1 Try to omit modificationDate (which should then default to
      // |Date.now()|, expect for resolution differences).
      {
        await fd.setDates(accDate);
        let stat = await fd.stat();
        Assert.equal(accDate, stat.lastAccessDate.getTime());
        Assert.notEqual(modDate, stat.lastModificationDate.getTime());
      }

      // 2.2 Try to omit accessDate as well (which should then default to
      // |Date.now()|, expect for resolution differences).
      {
        await fd.setDates();
        let stat = await fd.stat();
        Assert.notEqual(accDate, stat.lastAccessDate.getTime());
        Assert.notEqual(modDate, stat.lastModificationDate.getTime());
      }

      // 3. Repeat 1., but with Date objects this time
      {
        await fd.setDates(new Date(accDate), new Date(modDate));
        let stat = await fd.stat();
        Assert.equal(accDate, stat.lastAccessDate.getTime());
        Assert.equal(modDate, stat.lastModificationDate.getTime());
      }

      // 4. Check that invalid params will cause an exception/rejection.
      {
        for (let p of ["invalid", new Uint8Array(1), NaN]) {
          try {
            await fd.setDates(p, modDate);
            do_throw("Invalid access date should have thrown for: " + p);
          } catch (ex) {
            let stat = await fd.stat();
            Assert.equal(accDate, stat.lastAccessDate.getTime());
            Assert.equal(modDate, stat.lastModificationDate.getTime());
          }
          try {
            await fd.setDates(accDate, p);
            do_throw("Invalid modification date should have thrown for: " + p);
          } catch (ex) {
            let stat = await fd.stat();
            Assert.equal(accDate, stat.lastAccessDate.getTime());
            Assert.equal(modDate, stat.lastModificationDate.getTime());
          }
          try {
            await fd.setDates(p, p);
            do_throw("Invalid dates should have thrown for: " + p);
          } catch (ex) {
            let stat = await fd.stat();
            Assert.equal(accDate, stat.lastAccessDate.getTime());
            Assert.equal(modDate, stat.lastModificationDate.getTime());
          }
        }
      }
    } finally {
      await fd.close();
    }
  } finally {
    // Remove the temp file again
    await OS.File.remove(path);
  }
});

// Tests setting dates on directories.
add_task(async function test_dirs() {
  let path = OS.Path.join(OS.Constants.Path.tmpDir,
                              "test_osfile_async_setDates_dir");
  await OS.File.makeDir(path);

  try {
    // 1. Try to set some well known dates.
    // We choose multiples of 2000ms, because the time stamp resolution of
    // the underlying OS might not support something more precise.
    const accDate = 2000;
    const modDate = 4000;
    {
      await OS.File.setDates(path, accDate, modDate);
      let stat = await OS.File.stat(path);
      Assert.equal(accDate, stat.lastAccessDate.getTime());
      Assert.equal(modDate, stat.lastModificationDate.getTime());
    }
  } finally {
    await OS.File.removeEmptyDir(path);
  }
});
