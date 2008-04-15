/*
 * Modified definition of the IApplicationAssociationRegistration adapted
 * from the Windows SDK version 6.0. (Vista SDK) For use in cases where
 * older SDK's are used during compile.
 */

#ifndef AppAssoc_h__
#define AppAssoc_h__

// 6.0 SDK requires _WIN32_IE_IE60SP2 to expose this interface
#ifdef _WIN32_IE_IE60SP2
#undef _WIN32_IE
#define _WIN32_IE _WIN32_IE_IE60SP2
#endif
#include <shlobj.h>

// Non 6.0 SDK definitions for IApplicationAssociationRegistration
#if !defined(IApplicationAssociationRegistration)

typedef enum tagASSOCIATIONLEVEL {
  AL_MACHINE,
  AL_EFFECTIVE,
  AL_USER
} ASSOCIATIONLEVEL;

typedef enum tagASSOCIATIONTYPE {
  AT_FILEEXTENSION,
  AT_URLPROTOCOL,
  AT_STARTMENUCLIENT,
  AT_MIMETYPE
} ASSOCIATIONTYPE;

MIDL_INTERFACE("4e530b0a-e611-4c77-a3ac-9031d022281b")
IApplicationAssociationRegistration : public IUnknown
{
 public:
  virtual HRESULT STDMETHODCALLTYPE QueryCurrentDefault(LPCWSTR pszQuery,
                                                        ASSOCIATIONTYPE atQueryType,
                                                        ASSOCIATIONLEVEL alQueryLevel,
                                                        LPWSTR *ppszAssociation) = 0;
  virtual HRESULT STDMETHODCALLTYPE QueryAppIsDefault(LPCWSTR pszQuery,
                                                      ASSOCIATIONTYPE atQueryType,
                                                      ASSOCIATIONLEVEL alQueryLevel,
                                                      LPCWSTR pszAppRegistryName,
                                                      BOOL *pfDefault) = 0;
  virtual HRESULT STDMETHODCALLTYPE QueryAppIsDefaultAll(ASSOCIATIONLEVEL alQueryLevel,
                                                         LPCWSTR pszAppRegistryName,
                                                         BOOL *pfDefault) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetAppAsDefault(LPCWSTR pszAppRegistryName,
                                                    LPCWSTR pszSet,
                                                    ASSOCIATIONTYPE atSetType) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetAppAsDefaultAll(LPCWSTR pszAppRegistryName) = 0;
  virtual HRESULT STDMETHODCALLTYPE ClearUserAssociations(void) = 0;
};
#endif

#endif
