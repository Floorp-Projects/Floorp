/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DEFAULT_BROWSER_SET_DEFAULT_BROWSER_H__
#define DEFAULT_BROWSER_SET_DEFAULT_BROWSER_H__

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
 * @param aExtraFileExtensions Optional null-terminated list of extra file
 * association pairs to set as default, like `{ L".pdf", "FirefoxPDF", nullptr
 * }`.
 *
 * @return S_OK             All associations set and checked successfully.
 *         MOZ_E_NO_PROGID  The ProgID classes had not been registered.
 *         MOZ_E_HASH_CHECK The existing UserChoice Hash could not be verified.
 *         MOZ_E_REJECTED   UserChoice was set, but checking the default
 *                          did not return our ProgID.
 *         MOZ_E_BUILD      The existing UserChoice Hash was verified, but
 *                          we're on an older, unsupported Windows build,
 *                          so do not attempt to update the UserChoice hash.
 *         E_FAIL           other failure
 */
HRESULT SetDefaultBrowserUserChoice(
    const wchar_t* aAumi, const wchar_t* const* aExtraFileExtensions = nullptr);

/*
 * Set the default extension handlers for the given file extensions by writing
 * the UserChoice registry keys.
 *
 * @param aAumi The AUMI of the installation to set as default.
 *
 * @param aFileExtensions Optional null-terminated list of file association
 * pairs to set as default, like `{ L".pdf", "FirefoxPDF", nullptr }`.
 *
 * @returns S_OK           All associations set and checked successfully.
 *          MOZ_E_REJECTED UserChoice was set, but checking the default did not
 *                         return our ProgID.
 *          E_FAIL         Failed to set at least one association.
 */
HRESULT SetDefaultExtensionHandlersUserChoice(
    const wchar_t* aAumi, const wchar_t* const* aFileExtensions = nullptr);

/*
 * Additional HRESULT error codes from SetDefaultBrowserUserChoice
 *
 * 0x20000000 is set to put these in the customer-defined range.
 */
const HRESULT MOZ_E_NO_PROGID = 0xa0000001L;
const HRESULT MOZ_E_HASH_CHECK = 0xa0000002L;
const HRESULT MOZ_E_REJECTED = 0xa0000003L;
const HRESULT MOZ_E_BUILD = 0xa0000004L;

#endif  // DEFAULT_BROWSER_SET_DEFAULT_BROWSER_H__
