${license_cpp}

#include "nsIGenericFactory.h"
#include "${implementation_class_name}.h"

const char ${implementation_class_name}ContractID[] =
    "${implementation_contract_id}";

NS_IMPL_THREADSAFE_ISUPPORTS1(${implementation_class_name}, ${interface_name});

NS_IMETHODIMP
${implementation_class_name}::Method (void)
{
    printf ("${implementation_class_name}::method() called.\n");
    return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(${implementation_class_name});

static nsModuleComponentInfo components[] = {
    { "${component_module_desc}", ${implementation_class_name_uc}_CID,
      ${implementation_class_name}ContractID,
      ${implementation_class_name}Constructor},
};

NS_IMPL_NSGETMODULE("${component_module_desc}", components);

