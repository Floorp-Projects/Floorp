/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * Kathleen Brade <brade@pearlcrescent.com>
 * Mark Smith <mcs@pearlcrescent.com>
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "TestHarness.h"
#include "nsILocalFile.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsCOMArray.h"
#include "nsArrayEnumerator.h"
#include "nsXULAppAPI.h"
#include "nsIComponentRegistrar.h"

#define SERVICE_A_CONTRACT_ID  "@mozilla.org/RegTestServiceA;1"
#define SERVICE_B_CONTRACT_ID  "@mozilla.org/RegTestServiceB;1"

// {56ab1cd4-ac44-4f86-8104-171f8b8f2fc7}
#define CORE_SERVICE_A_CID             \
        { 0x56ab1cd4, 0xac44, 0x4f86, \
        { 0x81, 0x04, 0x17, 0x1f, 0x8b, 0x8f, 0x2f, 0xc7} }
NS_DEFINE_CID(kCoreServiceA_CID, CORE_SERVICE_A_CID);

// {fe64efb7-c5ab-41a6-b639-e6c0f483181e}
#define EXT_SERVICE_A_CID             \
        { 0xfe64efb7, 0xc5ab, 0x41a6, \
        { 0xb6, 0x39, 0xe6, 0xc0, 0xf4, 0x83, 0x18, 0x1e} }
NS_DEFINE_CID(kExtServiceA_CID, EXT_SERVICE_A_CID);

// {d04d1298-6dac-459b-a13b-bcab235730a0}
#define CORE_SERVICE_B_CID             \
        { 0xd04d1298, 0x6dac, 0x459b, \
        { 0xa1, 0x3b, 0xbc, 0xab, 0x23, 0x57, 0x30, 0xa0 } }
NS_DEFINE_CID(kCoreServiceB_CID, CORE_SERVICE_B_CID);

// {e93dadeb-a6f6-4667-bbbc-ac8c6d440b20}
#define EXT_SERVICE_B_CID             \
        { 0xe93dadeb, 0xa6f6, 0x4667, \
        { 0xbb, 0xbc, 0xac, 0x8c, 0x6d, 0x44, 0x0b, 0x20 } }
NS_DEFINE_CID(kExtServiceB_CID, EXT_SERVICE_B_CID);

nsresult execRegOrderTest(const char *aTestName, const char *aContractID,
                      const nsCID &aCoreCID, const nsCID &aExtCID)
{
  // Make sure the core service loaded (it won't be found using contract ID).
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsISupports> coreService = do_CreateInstance(aCoreCID, &rv);
#ifdef DEBUG_brade
  if (rv) fprintf(stderr, "rv: %d (%x)\n", rv, rv);
  fprintf(stderr, "coreService: %p\n", coreService.get());
#endif
  if (NS_FAILED(rv))
  {
    fail("%s FAILED - cannot create core service\n", aTestName);
    return rv;
  }

  // Get the extension service.
  nsCOMPtr<nsISupports> extService = do_CreateInstance(aExtCID, &rv);
#ifdef DEBUG_brade
  if (rv) fprintf(stderr, "rv: %d (%x)\n", rv, rv);
  fprintf(stderr, "extService: %p\n", extService.get());
#endif
  if (NS_FAILED(rv))
  {
    fail("%s FAILED - cannot create extension service\n", aTestName);
    return rv;
  }

  /*
   * Get the service by contract ID and make sure it is the extension
   * service (it should be, since the extension directory was registered
   * after the core one).
   */
  nsCOMPtr<nsISupports> service = do_CreateInstance(aContractID, &rv);
#ifdef DEBUG_brade
  if (rv) fprintf(stderr, "rv: %d (%x)\n", rv, rv);
  fprintf(stderr, "service: %p\n", service.get());
#endif
  if (NS_FAILED(rv))
  {
    fail("%s FAILED - cannot create service\n", aTestName);
    return rv;
  }

  if (service != extService)
  {
    fail("%s FAILED - wrong service registered\n", aTestName);
    return NS_ERROR_FAILURE;
  }

  passed(aTestName);
  return NS_OK;
}

nsresult TestRegular()
{
  return execRegOrderTest("TestRegular", SERVICE_A_CONTRACT_ID,
                          kCoreServiceA_CID, kExtServiceA_CID);
}

nsresult TestJar()
{
  return execRegOrderTest("TestJar", SERVICE_B_CONTRACT_ID,
                          kCoreServiceB_CID, kExtServiceB_CID);
}

bool TestContractFirst()
{
  nsCOMPtr<nsIComponentRegistrar> r;
  NS_GetComponentRegistrar(getter_AddRefs(r));

  nsCID* cid = NULL;
  nsresult rv = r->ContractIDToCID("@mozilla.org/RegTestOrderC;1", &cid);
  if (NS_FAILED(rv)) {
    fail("RegTestOrderC: contract not registered");
    return false;
  }

  nsCID goodcid;
  goodcid.Parse("{ada15884-bb89-473c-8b50-dcfbb8447ff4}");

  if (!goodcid.Equals(*cid)) {
    fail("RegTestOrderC: CID doesn't match");
    return false;
  }

  passed("RegTestOrderC");
  return true;
}

static already_AddRefed<nsILocalFile>
GetRegDirectory(const char* basename, const char* dirname, const char* leafname)
{
    nsCOMPtr<nsILocalFile> f;
    nsresult rv = NS_NewNativeLocalFile(nsDependentCString(basename), PR_TRUE,
                                        getter_AddRefs(f));
    if (NS_FAILED(rv))
        return NULL;

    f->AppendNative(nsDependentCString(dirname));
    if (leafname)
        f->AppendNative(nsDependentCString(leafname));
    return f.forget();
}



int main(int argc, char** argv)
{
  if (argc < 2)
  {
    fprintf(stderr, "not enough arguments -- need registration dir path\n");
    return 1;
  }

  ScopedLogging logging;
  
  const char *regPath = argv[1];
  XRE_AddManifestLocation(NS_COMPONENT_LOCATION,
                          nsCOMPtr<nsILocalFile>(GetRegDirectory(regPath, "core", "component.manifest")));
  XRE_AddManifestLocation(NS_COMPONENT_LOCATION,
                          nsCOMPtr<nsILocalFile>(GetRegDirectory(regPath, "extension", "extComponent.manifest")));
  XRE_AddJarManifestLocation(NS_COMPONENT_LOCATION,
                          nsCOMPtr<nsILocalFile>(GetRegDirectory(regPath, "extension2.jar", NULL)));
  ScopedXPCOM xpcom("RegistrationOrder");
  if (xpcom.failed())
    return 1;

  int rv = 0;
  if (NS_FAILED(TestRegular()))
    rv = 1;

  if (NS_FAILED(TestJar()))
    rv = 1;

  if (!TestContractFirst())
    rv = 1;

  return rv;
}
