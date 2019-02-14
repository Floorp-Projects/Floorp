#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "PageThumbsProtocol.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(PageThumbsProtocol)
NS_DEFINE_NAMED_CID(PAGETHUMBSPROTOCOL_CID);

const mozilla::Module::CIDEntry kPageThumbsCIDs[] = {
    {&kPAGETHUMBSPROTOCOL_CID, false, nullptr, PageThumbsProtocolConstructor},
    {nullptr}};

const mozilla::Module::ContractIDEntry kPageThumbsContracts[] = {
    {NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-page-thumb",
     &kPAGETHUMBSPROTOCOL_CID},
    {nullptr}};

const mozilla::Module kPageThumbsModule = {
    mozilla::Module::kVersion, kPageThumbsCIDs, kPageThumbsContracts};

NSMODULE_DEFN(nsPageThumbsModule) = &kPageThumbsModule;
