/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSDialogHelper_h
#define nsNSSDialogHelper_h

class mozIDOMWindowProxy;
class nsISupports;

/**
 * Common class that uses the window watcher service to open a
 * standard dialog, with or without a parent context. The params
 * parameter can be an nsISupportsArray so any number of additional
 * arguments can be used.
 */
class nsNSSDialogHelper
{
public:
  // params is a nsIDialogParamBlock or a nsIKeygenThread
  static nsresult openDialog(mozIDOMWindowProxy* window, const char* url,
                             nsISupports* params, bool modal = true);
};

#endif
