/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DEFAULT_BROWSER_SET_DEFAULT_BROWSER_H__
#define DEFAULT_BROWSER_SET_DEFAULT_BROWSER_H__

/*
 * Set the default browser by writing the UserChoice registry keys.
 *
 * This sets the associations for https, http, .html, and .htm.
 *
 * When the agent is run with set-default-browser-user-choice,
 * the exit code is the result of this function.
 *
 * @param aAumi The AUMI of the installation to set as default.
 *
 * @return S_OK             All associations set and checked successfully.
 *         MOZ_E_NO_PROGID  The ProgID classes had not been registered.
 *         MOZ_E_HASH_CHECK The existing UserChoice Hash could not be verified.
 *         MOZ_E_REJECTED   UserChoice was set, but checking the default
 *                          did not return our ProgID.
 *         E_FAIL           other failure
 */
HRESULT SetDefaultBrowserUserChoice(const wchar_t* aAumi);

/*
 * Additional HRESULT error codes from SetDefaultBrowserUserChoice
 *
 * 0x20000000 is set to put these in the customer-defined range.
 */
const HRESULT MOZ_E_NO_PROGID = 0xa0000001L;
const HRESULT MOZ_E_HASH_CHECK = 0xa0000002L;
const HRESULT MOZ_E_REJECTED = 0xa0000003L;

#endif  // DEFAULT_BROWSER_SET_DEFAULT_BROWSER_H__
