
#ifndef AttachmentService_h_
#define AttachmentService_h_

// Support for versions of shlobj.h that don't include this interface
#ifndef __IAttachmentExecute_INTERFACE_DEFINED__
typedef enum tagATTACHMENT_PROMPT {	
ATTACHMENT_PROMPT_NONE	= 0,
ATTACHMENT_PROMPT_SAVE	= 0x1,
ATTACHMENT_PROMPT_EXEC	= 0x2,
ATTACHMENT_PROMPT_EXEC_OR_SAVE	= 0x3
} ATTACHMENT_PROMPT;

typedef enum tagATTACHMENT_ACTION {
ATTACHMENT_ACTION_CANCEL	= 0,
ATTACHMENT_ACTION_SAVE	= 0x1,
ATTACHMENT_ACTION_EXEC	= 0x2
} ATTACHMENT_ACTION;

MIDL_INTERFACE("73db1241-1e85-4581-8e4f-a81e1d0f8c57")
IAttachmentExecute : public IUnknown
{
 public:
  virtual HRESULT STDMETHODCALLTYPE SetClientTitle( 
      LPCWSTR pszTitle) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE SetClientGuid( 
      REFGUID guid) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE SetLocalPath( 
      LPCWSTR pszLocalPath) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE SetFileName( 
      LPCWSTR pszFileName) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE SetSource( 
      LPCWSTR pszSource) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE SetReferrer( 
      LPCWSTR pszReferrer) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE CheckPolicy( void) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE Prompt( 
      HWND hwnd,
      ATTACHMENT_PROMPT prompt,
      ATTACHMENT_ACTION *paction) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE Save( void) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE Execute( 
      HWND hwnd,
      LPCWSTR pszVerb,
      HANDLE *phProcess) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE SaveWithUI( 
      HWND hwnd) = 0;
  
  virtual HRESULT STDMETHODCALLTYPE ClearClientState( void) = 0;
};
static const CLSID CLSID_AttachmentServices = {0x4125dd96,0xe03a,0x4103,{0x8f,0x70,0xe0,0x59,0x7d,0x80,0x3b,0x9c}};
static const IID IID_IAttachmentExecute  = {0x73db1241,0x1e85,0x4581,{0x8e,0x4f,0xa8,0x1e,0x1d,0x0f,0x8c,0x57}};
#endif

#endif
