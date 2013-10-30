
/* this file contains the definitions for DisplayInformation related interfaces
  copied over from Windows.Graphics.Display.h file in the windows 8.1 SDK 
  This file can be deleted once our build system moves to 8.1. */

 /* File created by MIDL compiler version 8.00.0603 */
/* @@MIDL_FILE_HEADING(  ) */

/* Forward Declarations */ 

#ifndef ____FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_FWD_DEFINED__
#define ____FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_FWD_DEFINED__
typedef interface __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable;

#endif 	/* ____FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics_FWD_DEFINED__
#define ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics __x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics;

#ifdef __cplusplus
namespace ABI {
    namespace Windows {
        namespace Graphics {
            namespace Display {
                interface IDisplayInformationStatics;
            } /* end namespace */
        } /* end namespace */
    } /* end namespace */
} /* end namespace */

#endif /* __cplusplus */

#endif 	/* ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics_FWD_DEFINED__ */


#ifndef ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation_FWD_DEFINED__
#define ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation __x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation;

#ifdef __cplusplus
namespace ABI {
    namespace Windows {
        namespace Graphics {
            namespace Display {
                interface IDisplayInformation;
            } /* end namespace */
        } /* end namespace */
    } /* end namespace */
} /* end namespace */

#endif /* __cplusplus */

#endif 	/* ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation_FWD_DEFINED__ */


#ifdef __cplusplus
namespace ABI {
namespace Windows {
namespace Graphics {
namespace Display {
class DisplayInformation;
} /*Display*/
} /*Graphics*/
} /*Windows*/
}
#endif

#ifdef __cplusplus
namespace ABI {
namespace Windows {
namespace Graphics {
namespace Display {
interface IDisplayInformation;
} /*Display*/
} /*Graphics*/
} /*Windows*/
}
#endif

interface IInspectable;


/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0000 */
/* [local] */ 





extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0000_v0_0_s_ifspec;

/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0580 */




/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0580 */




extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0580_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0580_v0_0_s_ifspec;

/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0001 */
/* [local] */ 

#ifndef DEF___FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_USE
#define DEF___FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_USE
#if defined(__cplusplus) && !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("86c4f619-67b6-51c7-b30d-d8cf13625327"))
ITypedEventHandler<ABI::Windows::Graphics::Display::DisplayInformation*,IInspectable*> : ITypedEventHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Windows::Graphics::Display::DisplayInformation*, ABI::Windows::Graphics::Display::IDisplayInformation*>,IInspectable*> {
static const wchar_t* z_get_rc_name_impl() {
return L"Windows.Foundation.TypedEventHandler`2<Windows.Graphics.Display.DisplayInformation, Object>"; }
};
typedef ITypedEventHandler<ABI::Windows::Graphics::Display::DisplayInformation*,IInspectable*> __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_t;
#define ____FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_FWD_DEFINED__
#define __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable ABI::Windows::Foundation::__FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_t

/* ABI */ } /* Windows */ } /* Foundation */ }
#endif //__cplusplus
#endif /* DEF___FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_USE */

#ifndef DEF___FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable
#define DEF___FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable
#if !defined(__cplusplus) || defined(RO_NO_TEMPLATE_NAME)


/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0004 */
/* [local] */ 



extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0004_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0004_v0_0_s_ifspec;

#ifndef ____FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_INTERFACE_DEFINED__
#define ____FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_INTERFACE_DEFINED__

/* interface __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable */
/* [unique][uuid][object] */ 



/* interface __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable */
/* [unique][uuid][object] */ 


EXTERN_C const IID IID___FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("86c4f619-67b6-51c7-b30d-d8cf13625327")
    __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Invoke( 
            /* [in] */ __RPC__in_opt ABI::Windows::Graphics::Display::IDisplayInformation *sender,
            /* [in] */ __RPC__in_opt IInspectable *e) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectableVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __RPC__in __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __RPC__in __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __RPC__in __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable * This);
        
        HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            __RPC__in __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable * This,
            /* [in] */ __RPC__in_opt __x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation *sender,
            /* [in] */ __RPC__in_opt IInspectable *e);
        
        END_INTERFACE
    } __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectableVtbl;

    interface __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable
    {
        CONST_VTBL struct __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectableVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_Invoke(This,sender,e)	\
    ( (This)->lpVtbl -> Invoke(This,sender,e) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* ____FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0005 */
/* [local] */ 

#endif /* pinterface */
#endif /* DEF___FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable */


/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0005 */
/* [local] */ 


/* interface __x_ABI_CWindows_CGraphics_CDisplay_CIDisplayPropertiesEventHandler */
/* [uuid][object] */ 



/* interface ABI::Windows::Graphics::Display::IDisplayPropertiesEventHandler */
/* [uuid][object] */ 




/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0006 */
/* [local] */ 

#if !defined(____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics_INTERFACE_DEFINED__)
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Windows_Graphics_Display_IDisplayInformationStatics[] = L"Windows.Graphics.Display.IDisplayInformationStatics";
#endif /* !defined(____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics_INTERFACE_DEFINED__) */


/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0006 */
/* [local] */ 



extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0006_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0006_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics */
/* [uuid][object] */ 



/* interface ABI::Windows::Graphics::Display::IDisplayInformationStatics */
/* [uuid][object] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics;

#if defined(__cplusplus) && !defined(CINTERFACE)
    namespace ABI {
        namespace Windows {
            namespace Graphics {
                namespace Display {
                    
                    MIDL_INTERFACE("C6A02A6C-D452-44DC-BA07-96F3C6ADF9D1")
                    IDisplayInformationStatics : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE GetForCurrentView( 
                            /* [out][retval] */ __RPC__deref_out_opt ABI::Windows::Graphics::Display::IDisplayInformation **current) = 0;
                        
                        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_AutoRotationPreferences( 
                            /* [out][retval] */ __RPC__out ABI::Windows::Graphics::Display::DisplayOrientations *value) = 0;
                        
                        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_AutoRotationPreferences( 
                            /* [in] */ ABI::Windows::Graphics::Display::DisplayOrientations value) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE add_DisplayContentsInvalidated( 
                            /* [in] */ __RPC__in_opt __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable *handler,
                            /* [out][retval] */ __RPC__out EventRegistrationToken *token) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE remove_DisplayContentsInvalidated( 
                            /* [in] */ EventRegistrationToken token) = 0;
                        
                    };

                    extern const __declspec(selectany) IID & IID_IDisplayInformationStatics = __uuidof(IDisplayInformationStatics);

                    
                }  /* end namespace */
            }  /* end namespace */
        }  /* end namespace */
    }  /* end namespace */

#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformationStatics_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0007 */
/* [local] */ 

#if !defined(____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation_INTERFACE_DEFINED__)
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Windows_Graphics_Display_IDisplayInformation[] = L"Windows.Graphics.Display.IDisplayInformation";
#endif /* !defined(____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation_INTERFACE_DEFINED__) */


/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0007 */
/* [local] */ 



extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0007_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0007_v0_0_s_ifspec;

#ifndef ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation_INTERFACE_DEFINED__
#define ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation_INTERFACE_DEFINED__

/* interface __x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation */
/* [uuid][object] */ 



/* interface ABI::Windows::Graphics::Display::IDisplayInformation */
/* [uuid][object] */ 


EXTERN_C const IID IID___x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation;

#if defined(__cplusplus) && !defined(CINTERFACE)
    namespace ABI {
        namespace Windows {
            namespace Graphics {
                namespace Display {
                    
                    MIDL_INTERFACE("BED112AE-ADC3-4DC9-AE65-851F4D7D4799")
                    IDisplayInformation : public IInspectable
                    {
                    public:
                        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_CurrentOrientation( 
                            /* [out][retval] */ __RPC__out ABI::Windows::Graphics::Display::DisplayOrientations *value) = 0;
                        
                        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_NativeOrientation( 
                            /* [out][retval] */ __RPC__out ABI::Windows::Graphics::Display::DisplayOrientations *value) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE add_OrientationChanged( 
                            /* [in] */ __RPC__in_opt __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable *handler,
                            /* [out][retval] */ __RPC__out EventRegistrationToken *token) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE remove_OrientationChanged( 
                            /* [in] */ EventRegistrationToken token) = 0;
                        
                        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ResolutionScale( 
                            /* [out][retval] */ __RPC__out ABI::Windows::Graphics::Display::ResolutionScale *value) = 0;
                        
                        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_LogicalDpi( 
                            /* [out][retval] */ __RPC__out FLOAT *value) = 0;
                        
                        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_RawDpiX( 
                            /* [out][retval] */ __RPC__out FLOAT *value) = 0;
                        
                        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_RawDpiY( 
                            /* [out][retval] */ __RPC__out FLOAT *value) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE add_DpiChanged( 
                            /* [in] */ __RPC__in_opt __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable *handler,
                            /* [out][retval] */ __RPC__out EventRegistrationToken *token) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE remove_DpiChanged( 
                            /* [in] */ EventRegistrationToken token) = 0;
                        
                        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_StereoEnabled( 
                            /* [out][retval] */ __RPC__out boolean *value) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE add_StereoEnabledChanged( 
                            /* [in] */ __RPC__in_opt __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable *handler,
                            /* [out][retval] */ __RPC__out EventRegistrationToken *token) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE remove_StereoEnabledChanged( 
                            /* [in] */ EventRegistrationToken token) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE GetColorProfileAsync( 
                            /* [out][retval] */ __RPC__deref_out_opt __FIAsyncOperation_1_Windows__CStorage__CStreams__CIRandomAccessStream **asyncInfo) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE add_ColorProfileChanged( 
                            /* [in] */ __RPC__in_opt __FITypedEventHandler_2_Windows__CGraphics__CDisplay__CDisplayInformation_IInspectable *handler,
                            /* [out][retval] */ __RPC__out EventRegistrationToken *token) = 0;
                        
                        virtual HRESULT STDMETHODCALLTYPE remove_ColorProfileChanged( 
                            /* [in] */ EventRegistrationToken token) = 0;
                        
                    };

                    extern const __declspec(selectany) IID & IID_IDisplayInformation = __uuidof(IDisplayInformation);

                    
                }  /* end namespace */
            }  /* end namespace */
        }  /* end namespace */
    }  /* end namespace */

#endif 	/* C style interface */




#endif 	/* ____x_ABI_CWindows_CGraphics_CDisplay_CIDisplayInformation_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0008 */
/* [local] */ 


/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0009 */
/* [local] */ 

#ifndef RUNTIMECLASS_Windows_Graphics_Display_DisplayInformation_DEFINED
#define RUNTIMECLASS_Windows_Graphics_Display_DisplayInformation_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Windows_Graphics_Display_DisplayInformation[] = L"Windows.Graphics.Display.DisplayInformation";
#endif


/* interface __MIDL_itf_windows2Egraphics2Edisplay_0000_0009 */
/* [local] */ 



extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0009_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_windows2Egraphics2Edisplay_0000_0009_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */
