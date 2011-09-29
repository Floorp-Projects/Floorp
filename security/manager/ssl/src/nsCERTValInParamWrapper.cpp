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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
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

#include "nsCERTValInParamWrapper.h"

NS_IMPL_THREADSAFE_ADDREF(nsCERTValInParamWrapper)
NS_IMPL_THREADSAFE_RELEASE(nsCERTValInParamWrapper)

nsCERTValInParamWrapper::nsCERTValInParamWrapper()
:mAlreadyConstructed(PR_FALSE)
,mCVIN(nsnull)
,mRev(nsnull)
{
  MOZ_COUNT_CTOR(nsCERTValInParamWrapper);
}

nsCERTValInParamWrapper::~nsCERTValInParamWrapper()
{
  MOZ_COUNT_DTOR(nsCERTValInParamWrapper);
  if (mRev) {
    CERT_DestroyCERTRevocationFlags(mRev);
  }
  if (mCVIN)
    PORT_Free(mCVIN);
}

nsresult nsCERTValInParamWrapper::Construct(missing_cert_download_config mcdc,
                                            crl_download_config cdc,
                                            ocsp_download_config odc,
                                            ocsp_strict_config osc,
                                            any_revo_fresh_config arfc,
                                            const char *firstNetworkRevocationMethod)
{
  if (mAlreadyConstructed)
    return NS_ERROR_FAILURE;

  CERTValInParam *p = (CERTValInParam*)PORT_Alloc(3 * sizeof(CERTValInParam));
  if (!p)
    return NS_ERROR_OUT_OF_MEMORY;

  CERTRevocationFlags *rev = CERT_AllocCERTRevocationFlags(
      cert_revocation_method_ocsp +1, 1,
      cert_revocation_method_ocsp +1, 1);
  
  if (!rev) {
    PORT_Free(p);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  p[0].type = cert_pi_useAIACertFetch;
  p[0].value.scalar.b = (mcdc == missing_cert_download_on);
  p[1].type = cert_pi_revocationFlags;
  p[1].value.pointer.revocation = rev;
  p[2].type = cert_pi_end;
  
  rev->leafTests.cert_rev_flags_per_method[cert_revocation_method_crl] =
  rev->chainTests.cert_rev_flags_per_method[cert_revocation_method_crl] =
    // implicit default source - makes no sense for CRLs
    CERT_REV_M_IGNORE_IMPLICIT_DEFAULT_SOURCE

    // let's not stop on fresh CRL. If OCSP is enabled, too, let's check it
    | CERT_REV_M_CONTINUE_TESTING_ON_FRESH_INFO

    // no fresh CRL? well, let other flag decide whether to fail or not
    | CERT_REV_M_IGNORE_MISSING_FRESH_INFO

    // testing using local CRLs is always allowed
    | CERT_REV_M_TEST_USING_THIS_METHOD

    // no local crl and don't know where to get it from? ignore
    | CERT_REV_M_SKIP_TEST_ON_MISSING_SOURCE

    // crl download based on parameter
    | ((cdc == crl_download_allowed) ?
        CERT_REV_M_ALLOW_NETWORK_FETCHING : CERT_REV_M_FORBID_NETWORK_FETCHING)
    ;

  rev->leafTests.cert_rev_flags_per_method[cert_revocation_method_ocsp] =
  rev->chainTests.cert_rev_flags_per_method[cert_revocation_method_ocsp] =
    // is ocsp enabled at all?
    ((odc == ocsp_on) ?
      CERT_REV_M_TEST_USING_THIS_METHOD : CERT_REV_M_DO_NOT_TEST_USING_THIS_METHOD)

    // ocsp enabled controls network fetching, too
    | ((odc == ocsp_on) ?
        CERT_REV_M_ALLOW_NETWORK_FETCHING : CERT_REV_M_FORBID_NETWORK_FETCHING)

    // ocsp set to strict==required?
    | ((osc == ocsp_strict) ?
        CERT_REV_M_FAIL_ON_MISSING_FRESH_INFO : CERT_REV_M_IGNORE_MISSING_FRESH_INFO)

    // if app has a default OCSP responder configured, let's use it
    | CERT_REV_M_ALLOW_IMPLICIT_DEFAULT_SOURCE

    // of course OCSP doesn't work without a source. let's accept such certs
    | CERT_REV_M_SKIP_TEST_ON_MISSING_SOURCE

    // ocsp success is sufficient
    | CERT_REV_M_STOP_TESTING_ON_FRESH_INFO
    ;

  bool wantsCrlFirst = (firstNetworkRevocationMethod != nsnull)
                          && (strcmp("crl", firstNetworkRevocationMethod) == 0);
    
  rev->leafTests.preferred_methods[0] =
  rev->chainTests.preferred_methods[0] =
    wantsCrlFirst ? cert_revocation_method_crl : cert_revocation_method_ocsp;

  rev->leafTests.cert_rev_method_independent_flags =
  rev->chainTests.cert_rev_method_independent_flags =
    // avoiding the network is good, let's try local first
    CERT_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST

    // is overall revocation requirement strict or relaxed?
    | ((arfc == any_revo_strict) ?
        CERT_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE : CERT_REV_MI_NO_OVERALL_INFO_REQUIREMENT)
    ;

  mAlreadyConstructed = PR_TRUE;
  mCVIN = p;
  mRev = rev;
  return NS_OK;
}
