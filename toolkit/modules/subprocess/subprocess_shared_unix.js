/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported LIBC, libc */

// ctypes is either already available in the chrome worker scope, or defined
// in scope via loadSubScript.
/* global ctypes */

// This file is loaded into the same scope as subprocess_shared.js.
/* import-globals-from subprocess_shared.js */

const LIBC = {
  get EAGAIN() {
    const os = Services.appinfo.OS;

    if (["Linux", "Android"].includes(os)) {
      // https://github.com/torvalds/linux/blob/9a48d604672220545d209e9996c2a1edbb5637f6/include/uapi/asm-generic/errno-base.h#L15
      return 11;
    } else if (
      ["Darwin", "DragonFly", "FreeBSD", "OpenBSD", "NetBSD"].includes(os)
    ) {
      /*
       * Darwin: https://opensource.apple.com/source/xnu/xnu-201/bsd/sys/errno.h.auto.html
       * DragonFly: https://github.com/DragonFlyBSD/DragonFlyBSD/blob/5e488df32cb01056a5b714a522e51c69ab7b4612/sys/sys/errno.h#L105
       * FreeBSD: https://github.com/freebsd/freebsd-src/blob/7232e6dcc89b978825b30a537bca2e7d3a9b71bb/sys/sys/errno.h#L94
       * OpenBSD: https://github.com/openbsd/src/blob/025fffe4c6e0113862ce4e1927e67517a2841502/sys/sys/errno.h#L83
       * NetBSD: https://github.com/NetBSD/src/blob/ff24f695f5f53540b23b6bb4fa5c0b9d79b369e4/sys/sys/errno.h#L81
       */
      return 35;
    }
    throw new Error("Unsupported OS");
  },

  EINTR: 4,

  F_SETFD: 2,
  F_SETFL: 4,

  FD_CLOEXEC: 1,

  get O_NONBLOCK() {
    const os = Services.appinfo.OS;

    if (["Linux", "Android"].includes(os)) {
      // https://github.com/torvalds/linux/blob/9a48d604672220545d209e9996c2a1edbb5637f6/include/uapi/asm-generic/fcntl.h
      return 0o4000;
    } else if (
      ["Darwin", "DragonFly", "FreeBSD", "OpenBSD", "NetBSD"].includes(os)
    ) {
      /*
       * Darwin: https://opensource.apple.com/source/xnu/xnu-201/bsd/sys/fcntl.h.auto.html
       * DragonFly: https://github.com/DragonFlyBSD/DragonFlyBSD/blob/5e488df32cb01056a5b714a522e51c69ab7b4612/sys/sys/fcntl.h#L71
       * FreeBSD: https://github.com/freebsd/freebsd-src/blob/7232e6dcc89b978825b30a537bca2e7d3a9b71bb/sys/sys/fcntl.h
       * OpenBSD: https://github.com/openbsd/src/blob/025fffe4c6e0113862ce4e1927e67517a2841502/sys/sys/fcntl.h#L79
       * NetBSD: https://github.com/NetBSD/src/blob/ff24f695f5f53540b23b6bb4fa5c0b9d79b369e4/sys/sys/fcntl.h
       */
      return 4;
    }
    throw new Error("Unsupported OS");
  },

  POLLIN: 0x01,
  POLLOUT: 0x04,
  POLLERR: 0x08,
  POLLHUP: 0x10,
  POLLNVAL: 0x20,

  WNOHANG: 1,
};

const LIBC_CHOICES = ["libc.so", "libSystem.B.dylib", "a.out"];

const unix = {
  pid_t: ctypes.int32_t,

  pollfd: new ctypes.StructType("pollfd", [
    { fd: ctypes.int },
    { events: ctypes.short },
    { revents: ctypes.short },
  ]),

  WEXITSTATUS(status) {
    return (status >> 8) & 0xff;
  },

  WTERMSIG(status) {
    return status & 0x7f;
  },
};

var libc = new Library("libc", LIBC_CHOICES, {
  environ: [ctypes.char.ptr.ptr],

  // Darwin-only.
  _NSGetEnviron: [ctypes.default_abi, ctypes.char.ptr.ptr.ptr],

  setenv: [
    ctypes.default_abi,
    ctypes.int,
    ctypes.char.ptr,
    ctypes.char.ptr,
    ctypes.int,
  ],

  chdir: [ctypes.default_abi, ctypes.int, ctypes.char.ptr /* path */],

  close: [ctypes.default_abi, ctypes.int, ctypes.int /* fildes */],

  fcntl: [
    ctypes.default_abi,
    ctypes.int,
    ctypes.int /* fildes */,
    ctypes.int /* cmd */,
    ctypes.int /* ... */,
  ],

  getcwd: [
    ctypes.default_abi,
    ctypes.char.ptr,
    ctypes.char.ptr /* buf */,
    ctypes.size_t /* size */,
  ],

  kill: [
    ctypes.default_abi,
    ctypes.int,
    unix.pid_t /* pid */,
    ctypes.int /* signal */,
  ],

  pipe: [ctypes.default_abi, ctypes.int, ctypes.int.array(2) /* pipefd */],

  poll: [
    ctypes.default_abi,
    ctypes.int,
    unix.pollfd.array() /* fds */,
    ctypes.unsigned_int /* nfds */,
    ctypes.int /* timeout */,
  ],

  read: [
    ctypes.default_abi,
    ctypes.ssize_t,
    ctypes.int /* fildes */,
    ctypes.char.ptr /* buf */,
    ctypes.size_t /* nbyte */,
  ],

  waitpid: [
    ctypes.default_abi,
    unix.pid_t,
    unix.pid_t /* pid */,
    ctypes.int.ptr /* status */,
    ctypes.int /* options */,
  ],

  write: [
    ctypes.default_abi,
    ctypes.ssize_t,
    ctypes.int /* fildes */,
    ctypes.char.ptr /* buf */,
    ctypes.size_t /* nbyte */,
  ],
});

unix.Fd = function (fd) {
  return ctypes.CDataFinalizer(ctypes.int(fd), libc.close);
};
