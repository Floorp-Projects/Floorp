// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_IPC_TAGS_H__
#define SANDBOX_SRC_IPC_TAGS_H__

namespace sandbox {

enum {
  IPC_UNUSED_TAG = 0,
  IPC_PING1_TAG,  // Takes a cookie in parameters and returns the cookie
                  // multiplied by 2 and the tick_count. Used for testing only.
  IPC_PING2_TAG,  // Takes an in/out cookie in parameters and modify the cookie
                  // to be multiplied by 3. Used for testing only.
  IPC_NTCREATEFILE_TAG,
  IPC_NTOPENFILE_TAG,
  IPC_NTQUERYATTRIBUTESFILE_TAG,
  IPC_NTQUERYFULLATTRIBUTESFILE_TAG,
  IPC_NTSETINFO_RENAME_TAG,
  IPC_CREATENAMEDPIPEW_TAG,
  IPC_NTOPENTHREAD_TAG,
  IPC_NTOPENPROCESS_TAG,
  IPC_NTOPENPROCESSTOKEN_TAG,
  IPC_NTOPENPROCESSTOKENEX_TAG,
  IPC_CREATEPROCESSW_TAG,
  IPC_CREATEEVENT_TAG,
  IPC_OPENEVENT_TAG,
  IPC_NTCREATEKEY_TAG,
  IPC_NTOPENKEY_TAG,
  IPC_DUPLICATEHANDLEPROXY_TAG,
  IPC_GDI_GDIDLLINITIALIZE_TAG,
  IPC_GDI_GETSTOCKOBJECT_TAG,
  IPC_USER_REGISTERCLASSW_TAG,
  IPC_LAST_TAG
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_IPC_TAGS_H__
