${license_cpp}

#ifndef ${implementation_class_name_uc}_H___
#define ${implementation_class_name_uc}_H___

#include "${interface_h_file}"

#define ${implementation_class_name_uc}_CID                            \
${implementation_guid_define}

class ${implementation_class_name} : public ${interface_name}
{
  public:
    NS_DECL_ISUPPORTS;
    NS_DECL_${interface_name_uc};

    ${implementation_class_name}()
    {
        NS_INIT_ISUPPORTS();
    }
    virtual ~${implementation_class_name}() { }
    
  private:

};

#endif /* ${implementation_class_name_uc}_H___ */

