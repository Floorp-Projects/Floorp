/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim set:sw=2 ts=2 et lcs=trail\:.,tab\:>~ : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is storage test code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "storage_test_harness.h"

#include "nsThreadUtils.h"

/**
 * This file tests that the storage service can be initialized off of the main
 * thread without issue.
 */

////////////////////////////////////////////////////////////////////////////////
//// Helpers

class ServiceInitializer : public nsRunnable
{
public:
  NS_IMETHOD Run()
  {
    nsCOMPtr<mozIStorageService> service = getService();
    do_check_true(service);
    return NS_OK;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

void
test_service_initialization_on_background_thread()
{
  nsCOMPtr<nsIRunnable> event = new ServiceInitializer();
  do_check_true(event);

  nsCOMPtr<nsIThread> thread;
  do_check_success(NS_NewThread(getter_AddRefs(thread)));

  do_check_success(thread->Dispatch(event, NS_DISPATCH_NORMAL));

  // Shutting down the thread will spin the event loop until all work in its
  // event queue is completed.  This will act as our thread synchronization.
  do_check_success(thread->Shutdown());
}

void (*gTests[])(void) = {
  test_service_initialization_on_background_thread,
};

const char *file = __FILE__;
#define TEST_NAME "Background Thread Initialization"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
