/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DEFAULT_BROWSER_SET_DEFAULT_BROWSER_H__
#define DEFAULT_BROWSER_SET_DEFAULT_BROWSER_H__

#include "nsStringFwd.h"
#include "nsTArray.h"

namespace mozilla::default_agent {

/*
 * Set the default browser by writing the UserChoice registry keys.
 *
 * This sets the associations for https, http, .html, and .htm, and
 * optionally for additional extra file extensions.
 *
 * When the agent is run with set-default-browser-user-choice,
 * the exit code is the result of this function.
 *
 * @param aAumi The AUMI of the installation to set as default.
 *
 * @param aExtraFileExtensions Optional array of extra file association pairs to
 * set as default, like `[ ".pdf", "FirefoxPDF" ]`.
 *
 * @return NS_OK                    All associations set and checked
 *                                  successfully.
 *         NS_ERROR_WDBA_NO_PROGID  The ProgID classes had not been registered.
 *         NS_ERROR_WDBA_HASH_CHECK The existing UserChoice Hash could not be
 *                                  verified.
 *         NS_ERROR_WDBA_REJECTED   UserChoice was set, but checking the default
 *                                  did not return our ProgID.
 *         NS_ERROR_WDBA_BUILD      The existing UserChoice Hash was verified,
 *                                  but we're on an older, unsupported Windows
 *                                  build, so do not attempt to update the
 *                                  UserChoice hash.
 *         NS_ERROR_FAILURE         other failure
 */
nsresult SetDefaultBrowserUserChoice(
    const wchar_t* aAumi,
    const nsTArray<nsString>& aExtraFileExtensions = nsTArray<nsString>());

/*
 * Set the default extension handlers for the given file extensions by writing
 * the UserChoice registry keys.
 *
 * @param aAumi The AUMI of the installation to set as default.
 *
 * @param aExtraFileExtensions Optional array of extra file association pairs to
 * set as default, like `[ ".pdf", "FirefoxPDF" ]`.
 *
 * @returns NS_OK                  All associations set and checked
 *                                 successfully.
 *          NS_ERROR_WDBA_REJECTED UserChoice was set, but checking the default
 *                                 did not return our ProgID.
 *          NS_ERROR_FAILURE       Failed to set at least one association.
 */
nsresult SetDefaultExtensionHandlersUserChoice(
    const wchar_t* aAumi, const nsTArray<nsString>& aFileExtensions);

}  // namespace mozilla::default_agent

#endif  // DEFAULT_BROWSER_SET_DEFAULT_BROWSER_H__
