/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported LIBC, libc */

// This file is loaded into the same scope as subprocess_unix.jsm
/* import-globals-from subprocess_shared.js */
/* import-globals-from subprocess_unix.jsm */

var LIBC = OS.Constants.libc;

const LIBC_CHOICES = ["libc.so", "libSystem.B.dylib", "a.out"];

const unix = {
  pid_t: ctypes.int32_t,

  pollfd: new ctypes.StructType("pollfd", [
    { fd: ctypes.int },
    { events: ctypes.short },
    { revents: ctypes.short },
  ]),

  posix_spawn_file_actions_t: ctypes.uint8_t.array(
    LIBC.OSFILE_SIZEOF_POSIX_SPAWN_FILE_ACTIONS_T
  ),

  posix_spawnattr_t: ctypes.uint8_t.array(LIBC.OSFILE_SIZEOF_POSIX_SPAWNATTR_T),

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

  posix_spawn: [
    ctypes.default_abi,
    ctypes.int,
    unix.pid_t.ptr /* pid */,
    ctypes.char.ptr /* path */,
    unix.posix_spawn_file_actions_t.ptr /* file_actions */,
    ctypes.voidptr_t /* attrp */,
    ctypes.char.ptr.ptr /* argv */,
    ctypes.char.ptr.ptr /* envp */,
  ],

  posix_spawnattr_init: [
    ctypes.default_abi,
    ctypes.int,
    unix.posix_spawnattr_t.ptr,
  ],

  posix_spawnattr_destroy: [
    ctypes.default_abi,
    ctypes.int,
    unix.posix_spawnattr_t.ptr,
  ],

  responsibility_spawnattrs_setdisclaim: [
    ctypes.default_abi,
    ctypes.int,
    unix.posix_spawnattr_t.ptr,
    ctypes.int,
  ],

  posix_spawn_file_actions_addclose: [
    ctypes.default_abi,
    ctypes.int,
    unix.posix_spawn_file_actions_t.ptr /* file_actions */,
    ctypes.int /* fildes */,
  ],

  posix_spawn_file_actions_adddup2: [
    ctypes.default_abi,
    ctypes.int,
    unix.posix_spawn_file_actions_t.ptr /* file_actions */,
    ctypes.int /* fildes */,
    ctypes.int /* newfildes */,
  ],

  posix_spawn_file_actions_destroy: [
    ctypes.default_abi,
    ctypes.int,
    unix.posix_spawn_file_actions_t.ptr /* file_actions */,
  ],

  posix_spawn_file_actions_init: [
    ctypes.default_abi,
    ctypes.int,
    unix.posix_spawn_file_actions_t.ptr /* file_actions */,
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

unix.Fd = function(fd) {
  return ctypes.CDataFinalizer(ctypes.int(fd), libc.close);
};
