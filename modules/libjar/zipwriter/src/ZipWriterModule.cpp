/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "mozilla/ModuleUtils.h"
#include "nsDeflateConverter.h"
#include "nsZipWriter.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeflateConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZipWriter)

NS_DEFINE_NAMED_CID(DEFLATECONVERTER_CID);
NS_DEFINE_NAMED_CID(ZIPWRITER_CID);

static const mozilla::Module::CIDEntry kZipWriterCIDs[] = {
  { &kDEFLATECONVERTER_CID, false, NULL, nsDeflateConverterConstructor },
  { &kZIPWRITER_CID, false, NULL, nsZipWriterConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kZipWriterContracts[] = {
  { "@mozilla.org/streamconv;1?from=uncompressed&to=deflate", &kDEFLATECONVERTER_CID },
  { "@mozilla.org/streamconv;1?from=uncompressed&to=gzip", &kDEFLATECONVERTER_CID },
  { "@mozilla.org/streamconv;1?from=uncompressed&to=x-gzip", &kDEFLATECONVERTER_CID },
  { "@mozilla.org/streamconv;1?from=uncompressed&to=rawdeflate", &kDEFLATECONVERTER_CID },
  { ZIPWRITER_CONTRACTID, &kZIPWRITER_CID },
  { NULL }
};

static const mozilla::Module kZipWriterModule = {
  mozilla::Module::kVersion,
  kZipWriterCIDs,
  kZipWriterContracts
};

NSMODULE_DEFN(ZipWriterModule) = &kZipWriterModule;
