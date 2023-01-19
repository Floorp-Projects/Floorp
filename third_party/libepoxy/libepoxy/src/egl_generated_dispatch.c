/* GL dispatch code.
 * This is code-generated from the GL API XML files from Khronos.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dispatch_common.h"
#include "epoxy/egl.h"

#ifdef __GNUC__
#define EPOXY_NOINLINE __attribute__((noinline))
#elif defined (_MSC_VER)
#define EPOXY_NOINLINE __declspec(noinline)
#endif
struct dispatch_table {
    PFNEGLBINDAPIPROC epoxy_eglBindAPI;
    PFNEGLBINDTEXIMAGEPROC epoxy_eglBindTexImage;
    PFNEGLCHOOSECONFIGPROC epoxy_eglChooseConfig;
    PFNEGLCLIENTSIGNALSYNCEXTPROC epoxy_eglClientSignalSyncEXT;
    PFNEGLCLIENTWAITSYNCPROC epoxy_eglClientWaitSync;
    PFNEGLCLIENTWAITSYNCKHRPROC epoxy_eglClientWaitSyncKHR;
    PFNEGLCLIENTWAITSYNCNVPROC epoxy_eglClientWaitSyncNV;
    PFNEGLCOMPOSITORBINDTEXWINDOWEXTPROC epoxy_eglCompositorBindTexWindowEXT;
    PFNEGLCOMPOSITORSETCONTEXTATTRIBUTESEXTPROC epoxy_eglCompositorSetContextAttributesEXT;
    PFNEGLCOMPOSITORSETCONTEXTLISTEXTPROC epoxy_eglCompositorSetContextListEXT;
    PFNEGLCOMPOSITORSETSIZEEXTPROC epoxy_eglCompositorSetSizeEXT;
    PFNEGLCOMPOSITORSETWINDOWATTRIBUTESEXTPROC epoxy_eglCompositorSetWindowAttributesEXT;
    PFNEGLCOMPOSITORSETWINDOWLISTEXTPROC epoxy_eglCompositorSetWindowListEXT;
    PFNEGLCOMPOSITORSWAPPOLICYEXTPROC epoxy_eglCompositorSwapPolicyEXT;
    PFNEGLCOPYBUFFERSPROC epoxy_eglCopyBuffers;
    PFNEGLCREATECONTEXTPROC epoxy_eglCreateContext;
    PFNEGLCREATEDRMIMAGEMESAPROC epoxy_eglCreateDRMImageMESA;
    PFNEGLCREATEFENCESYNCNVPROC epoxy_eglCreateFenceSyncNV;
    PFNEGLCREATEIMAGEPROC epoxy_eglCreateImage;
    PFNEGLCREATEIMAGEKHRPROC epoxy_eglCreateImageKHR;
    PFNEGLCREATENATIVECLIENTBUFFERANDROIDPROC epoxy_eglCreateNativeClientBufferANDROID;
    PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC epoxy_eglCreatePbufferFromClientBuffer;
    PFNEGLCREATEPBUFFERSURFACEPROC epoxy_eglCreatePbufferSurface;
    PFNEGLCREATEPIXMAPSURFACEPROC epoxy_eglCreatePixmapSurface;
    PFNEGLCREATEPIXMAPSURFACEHIPROC epoxy_eglCreatePixmapSurfaceHI;
    PFNEGLCREATEPLATFORMPIXMAPSURFACEPROC epoxy_eglCreatePlatformPixmapSurface;
    PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC epoxy_eglCreatePlatformPixmapSurfaceEXT;
    PFNEGLCREATEPLATFORMWINDOWSURFACEPROC epoxy_eglCreatePlatformWindowSurface;
    PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC epoxy_eglCreatePlatformWindowSurfaceEXT;
    PFNEGLCREATESTREAMATTRIBKHRPROC epoxy_eglCreateStreamAttribKHR;
    PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC epoxy_eglCreateStreamFromFileDescriptorKHR;
    PFNEGLCREATESTREAMKHRPROC epoxy_eglCreateStreamKHR;
    PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC epoxy_eglCreateStreamProducerSurfaceKHR;
    PFNEGLCREATESTREAMSYNCNVPROC epoxy_eglCreateStreamSyncNV;
    PFNEGLCREATESYNCPROC epoxy_eglCreateSync;
    PFNEGLCREATESYNC64KHRPROC epoxy_eglCreateSync64KHR;
    PFNEGLCREATESYNCKHRPROC epoxy_eglCreateSyncKHR;
    PFNEGLCREATEWINDOWSURFACEPROC epoxy_eglCreateWindowSurface;
    PFNEGLDEBUGMESSAGECONTROLKHRPROC epoxy_eglDebugMessageControlKHR;
    PFNEGLDESTROYCONTEXTPROC epoxy_eglDestroyContext;
    PFNEGLDESTROYIMAGEPROC epoxy_eglDestroyImage;
    PFNEGLDESTROYIMAGEKHRPROC epoxy_eglDestroyImageKHR;
    PFNEGLDESTROYSTREAMKHRPROC epoxy_eglDestroyStreamKHR;
    PFNEGLDESTROYSURFACEPROC epoxy_eglDestroySurface;
    PFNEGLDESTROYSYNCPROC epoxy_eglDestroySync;
    PFNEGLDESTROYSYNCKHRPROC epoxy_eglDestroySyncKHR;
    PFNEGLDESTROYSYNCNVPROC epoxy_eglDestroySyncNV;
    PFNEGLDUPNATIVEFENCEFDANDROIDPROC epoxy_eglDupNativeFenceFDANDROID;
    PFNEGLEXPORTDMABUFIMAGEMESAPROC epoxy_eglExportDMABUFImageMESA;
    PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC epoxy_eglExportDMABUFImageQueryMESA;
    PFNEGLEXPORTDRMIMAGEMESAPROC epoxy_eglExportDRMImageMESA;
    PFNEGLFENCENVPROC epoxy_eglFenceNV;
    PFNEGLGETCOMPOSITORTIMINGANDROIDPROC epoxy_eglGetCompositorTimingANDROID;
    PFNEGLGETCOMPOSITORTIMINGSUPPORTEDANDROIDPROC epoxy_eglGetCompositorTimingSupportedANDROID;
    PFNEGLGETCONFIGATTRIBPROC epoxy_eglGetConfigAttrib;
    PFNEGLGETCONFIGSPROC epoxy_eglGetConfigs;
    PFNEGLGETCURRENTCONTEXTPROC epoxy_eglGetCurrentContext;
    PFNEGLGETCURRENTDISPLAYPROC epoxy_eglGetCurrentDisplay;
    PFNEGLGETCURRENTSURFACEPROC epoxy_eglGetCurrentSurface;
    PFNEGLGETDISPLAYPROC epoxy_eglGetDisplay;
    PFNEGLGETDISPLAYDRIVERCONFIGPROC epoxy_eglGetDisplayDriverConfig;
    PFNEGLGETDISPLAYDRIVERNAMEPROC epoxy_eglGetDisplayDriverName;
    PFNEGLGETERRORPROC epoxy_eglGetError;
    PFNEGLGETFRAMETIMESTAMPSUPPORTEDANDROIDPROC epoxy_eglGetFrameTimestampSupportedANDROID;
    PFNEGLGETFRAMETIMESTAMPSANDROIDPROC epoxy_eglGetFrameTimestampsANDROID;
    PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC epoxy_eglGetNativeClientBufferANDROID;
    PFNEGLGETNEXTFRAMEIDANDROIDPROC epoxy_eglGetNextFrameIdANDROID;
    PFNEGLGETOUTPUTLAYERSEXTPROC epoxy_eglGetOutputLayersEXT;
    PFNEGLGETOUTPUTPORTSEXTPROC epoxy_eglGetOutputPortsEXT;
    PFNEGLGETPLATFORMDISPLAYPROC epoxy_eglGetPlatformDisplay;
    PFNEGLGETPLATFORMDISPLAYEXTPROC epoxy_eglGetPlatformDisplayEXT;
    PFNEGLGETPROCADDRESSPROC epoxy_eglGetProcAddress;
    PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC epoxy_eglGetStreamFileDescriptorKHR;
    PFNEGLGETSYNCATTRIBPROC epoxy_eglGetSyncAttrib;
    PFNEGLGETSYNCATTRIBKHRPROC epoxy_eglGetSyncAttribKHR;
    PFNEGLGETSYNCATTRIBNVPROC epoxy_eglGetSyncAttribNV;
    PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC epoxy_eglGetSystemTimeFrequencyNV;
    PFNEGLGETSYSTEMTIMENVPROC epoxy_eglGetSystemTimeNV;
    PFNEGLINITIALIZEPROC epoxy_eglInitialize;
    PFNEGLLABELOBJECTKHRPROC epoxy_eglLabelObjectKHR;
    PFNEGLLOCKSURFACEKHRPROC epoxy_eglLockSurfaceKHR;
    PFNEGLMAKECURRENTPROC epoxy_eglMakeCurrent;
    PFNEGLOUTPUTLAYERATTRIBEXTPROC epoxy_eglOutputLayerAttribEXT;
    PFNEGLOUTPUTPORTATTRIBEXTPROC epoxy_eglOutputPortAttribEXT;
    PFNEGLPOSTSUBBUFFERNVPROC epoxy_eglPostSubBufferNV;
    PFNEGLPRESENTATIONTIMEANDROIDPROC epoxy_eglPresentationTimeANDROID;
    PFNEGLQUERYAPIPROC epoxy_eglQueryAPI;
    PFNEGLQUERYCONTEXTPROC epoxy_eglQueryContext;
    PFNEGLQUERYDEBUGKHRPROC epoxy_eglQueryDebugKHR;
    PFNEGLQUERYDEVICEATTRIBEXTPROC epoxy_eglQueryDeviceAttribEXT;
    PFNEGLQUERYDEVICESTRINGEXTPROC epoxy_eglQueryDeviceStringEXT;
    PFNEGLQUERYDEVICESEXTPROC epoxy_eglQueryDevicesEXT;
    PFNEGLQUERYDISPLAYATTRIBEXTPROC epoxy_eglQueryDisplayAttribEXT;
    PFNEGLQUERYDISPLAYATTRIBKHRPROC epoxy_eglQueryDisplayAttribKHR;
    PFNEGLQUERYDISPLAYATTRIBNVPROC epoxy_eglQueryDisplayAttribNV;
    PFNEGLQUERYDMABUFFORMATSEXTPROC epoxy_eglQueryDmaBufFormatsEXT;
    PFNEGLQUERYDMABUFMODIFIERSEXTPROC epoxy_eglQueryDmaBufModifiersEXT;
    PFNEGLQUERYNATIVEDISPLAYNVPROC epoxy_eglQueryNativeDisplayNV;
    PFNEGLQUERYNATIVEPIXMAPNVPROC epoxy_eglQueryNativePixmapNV;
    PFNEGLQUERYNATIVEWINDOWNVPROC epoxy_eglQueryNativeWindowNV;
    PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC epoxy_eglQueryOutputLayerAttribEXT;
    PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC epoxy_eglQueryOutputLayerStringEXT;
    PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC epoxy_eglQueryOutputPortAttribEXT;
    PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC epoxy_eglQueryOutputPortStringEXT;
    PFNEGLQUERYSTREAMATTRIBKHRPROC epoxy_eglQueryStreamAttribKHR;
    PFNEGLQUERYSTREAMKHRPROC epoxy_eglQueryStreamKHR;
    PFNEGLQUERYSTREAMMETADATANVPROC epoxy_eglQueryStreamMetadataNV;
    PFNEGLQUERYSTREAMTIMEKHRPROC epoxy_eglQueryStreamTimeKHR;
    PFNEGLQUERYSTREAMU64KHRPROC epoxy_eglQueryStreamu64KHR;
    PFNEGLQUERYSTRINGPROC epoxy_eglQueryString;
    PFNEGLQUERYSURFACEPROC epoxy_eglQuerySurface;
    PFNEGLQUERYSURFACE64KHRPROC epoxy_eglQuerySurface64KHR;
    PFNEGLQUERYSURFACEPOINTERANGLEPROC epoxy_eglQuerySurfacePointerANGLE;
    PFNEGLRELEASETEXIMAGEPROC epoxy_eglReleaseTexImage;
    PFNEGLRELEASETHREADPROC epoxy_eglReleaseThread;
    PFNEGLRESETSTREAMNVPROC epoxy_eglResetStreamNV;
    PFNEGLSETBLOBCACHEFUNCSANDROIDPROC epoxy_eglSetBlobCacheFuncsANDROID;
    PFNEGLSETDAMAGEREGIONKHRPROC epoxy_eglSetDamageRegionKHR;
    PFNEGLSETSTREAMATTRIBKHRPROC epoxy_eglSetStreamAttribKHR;
    PFNEGLSETSTREAMMETADATANVPROC epoxy_eglSetStreamMetadataNV;
    PFNEGLSIGNALSYNCKHRPROC epoxy_eglSignalSyncKHR;
    PFNEGLSIGNALSYNCNVPROC epoxy_eglSignalSyncNV;
    PFNEGLSTREAMATTRIBKHRPROC epoxy_eglStreamAttribKHR;
    PFNEGLSTREAMCONSUMERACQUIREATTRIBKHRPROC epoxy_eglStreamConsumerAcquireAttribKHR;
    PFNEGLSTREAMCONSUMERACQUIREKHRPROC epoxy_eglStreamConsumerAcquireKHR;
    PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALATTRIBSNVPROC epoxy_eglStreamConsumerGLTextureExternalAttribsNV;
    PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC epoxy_eglStreamConsumerGLTextureExternalKHR;
    PFNEGLSTREAMCONSUMEROUTPUTEXTPROC epoxy_eglStreamConsumerOutputEXT;
    PFNEGLSTREAMCONSUMERRELEASEATTRIBKHRPROC epoxy_eglStreamConsumerReleaseAttribKHR;
    PFNEGLSTREAMCONSUMERRELEASEKHRPROC epoxy_eglStreamConsumerReleaseKHR;
    PFNEGLSTREAMFLUSHNVPROC epoxy_eglStreamFlushNV;
    PFNEGLSURFACEATTRIBPROC epoxy_eglSurfaceAttrib;
    PFNEGLSWAPBUFFERSPROC epoxy_eglSwapBuffers;
    PFNEGLSWAPBUFFERSREGION2NOKPROC epoxy_eglSwapBuffersRegion2NOK;
    PFNEGLSWAPBUFFERSREGIONNOKPROC epoxy_eglSwapBuffersRegionNOK;
    PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC epoxy_eglSwapBuffersWithDamageEXT;
    PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC epoxy_eglSwapBuffersWithDamageKHR;
    PFNEGLSWAPINTERVALPROC epoxy_eglSwapInterval;
    PFNEGLTERMINATEPROC epoxy_eglTerminate;
    PFNEGLUNLOCKSURFACEKHRPROC epoxy_eglUnlockSurfaceKHR;
    PFNEGLUNSIGNALSYNCEXTPROC epoxy_eglUnsignalSyncEXT;
    PFNEGLWAITCLIENTPROC epoxy_eglWaitClient;
    PFNEGLWAITGLPROC epoxy_eglWaitGL;
    PFNEGLWAITNATIVEPROC epoxy_eglWaitNative;
    PFNEGLWAITSYNCPROC epoxy_eglWaitSync;
    PFNEGLWAITSYNCKHRPROC epoxy_eglWaitSyncKHR;
};

#if USING_DISPATCH_TABLE
static inline struct dispatch_table *
get_dispatch_table(void);

#endif

enum egl_provider {
    egl_provider_terminator = 0,
    PROVIDER_EGL_10,
    PROVIDER_EGL_11,
    PROVIDER_EGL_12,
    PROVIDER_EGL_14,
    PROVIDER_EGL_15,
    PROVIDER_EGL_ANDROID_blob_cache,
    PROVIDER_EGL_ANDROID_create_native_client_buffer,
    PROVIDER_EGL_ANDROID_get_frame_timestamps,
    PROVIDER_EGL_ANDROID_get_native_client_buffer,
    PROVIDER_EGL_ANDROID_native_fence_sync,
    PROVIDER_EGL_ANDROID_presentation_time,
    PROVIDER_EGL_ANGLE_query_surface_pointer,
    PROVIDER_EGL_EXT_client_sync,
    PROVIDER_EGL_EXT_compositor,
    PROVIDER_EGL_EXT_device_base,
    PROVIDER_EGL_EXT_device_enumeration,
    PROVIDER_EGL_EXT_device_query,
    PROVIDER_EGL_EXT_image_dma_buf_import_modifiers,
    PROVIDER_EGL_EXT_output_base,
    PROVIDER_EGL_EXT_platform_base,
    PROVIDER_EGL_EXT_stream_consumer_egloutput,
    PROVIDER_EGL_EXT_swap_buffers_with_damage,
    PROVIDER_EGL_EXT_sync_reuse,
    PROVIDER_EGL_HI_clientpixmap,
    PROVIDER_EGL_KHR_cl_event2,
    PROVIDER_EGL_KHR_debug,
    PROVIDER_EGL_KHR_display_reference,
    PROVIDER_EGL_KHR_fence_sync,
    PROVIDER_EGL_KHR_image,
    PROVIDER_EGL_KHR_image_base,
    PROVIDER_EGL_KHR_lock_surface,
    PROVIDER_EGL_KHR_lock_surface3,
    PROVIDER_EGL_KHR_partial_update,
    PROVIDER_EGL_KHR_reusable_sync,
    PROVIDER_EGL_KHR_stream,
    PROVIDER_EGL_KHR_stream_attrib,
    PROVIDER_EGL_KHR_stream_consumer_gltexture,
    PROVIDER_EGL_KHR_stream_cross_process_fd,
    PROVIDER_EGL_KHR_stream_fifo,
    PROVIDER_EGL_KHR_stream_producer_eglsurface,
    PROVIDER_EGL_KHR_swap_buffers_with_damage,
    PROVIDER_EGL_KHR_wait_sync,
    PROVIDER_EGL_MESA_drm_image,
    PROVIDER_EGL_MESA_image_dma_buf_export,
    PROVIDER_EGL_MESA_query_driver,
    PROVIDER_EGL_NOK_swap_region,
    PROVIDER_EGL_NOK_swap_region2,
    PROVIDER_EGL_NV_native_query,
    PROVIDER_EGL_NV_post_sub_buffer,
    PROVIDER_EGL_NV_stream_consumer_gltexture_yuv,
    PROVIDER_EGL_NV_stream_flush,
    PROVIDER_EGL_NV_stream_metadata,
    PROVIDER_EGL_NV_stream_reset,
    PROVIDER_EGL_NV_stream_sync,
    PROVIDER_EGL_NV_sync,
    PROVIDER_EGL_NV_system_time,
} PACKED;
ENDPACKED

static const char *enum_string =
    "EGL 10\0"
    "EGL 11\0"
    "EGL 12\0"
    "EGL 14\0"
    "EGL 15\0"
    "EGL_ANDROID_blob_cache\0"
    "EGL_ANDROID_create_native_client_buffer\0"
    "EGL_ANDROID_get_frame_timestamps\0"
    "EGL_ANDROID_get_native_client_buffer\0"
    "EGL_ANDROID_native_fence_sync\0"
    "EGL_ANDROID_presentation_time\0"
    "EGL_ANGLE_query_surface_pointer\0"
    "EGL_EXT_client_sync\0"
    "EGL_EXT_compositor\0"
    "EGL_EXT_device_base\0"
    "EGL_EXT_device_enumeration\0"
    "EGL_EXT_device_query\0"
    "EGL_EXT_image_dma_buf_import_modifiers\0"
    "EGL_EXT_output_base\0"
    "EGL_EXT_platform_base\0"
    "EGL_EXT_stream_consumer_egloutput\0"
    "EGL_EXT_swap_buffers_with_damage\0"
    "EGL_EXT_sync_reuse\0"
    "EGL_HI_clientpixmap\0"
    "EGL_KHR_cl_event2\0"
    "EGL_KHR_debug\0"
    "EGL_KHR_display_reference\0"
    "EGL_KHR_fence_sync\0"
    "EGL_KHR_image\0"
    "EGL_KHR_image_base\0"
    "EGL_KHR_lock_surface\0"
    "EGL_KHR_lock_surface3\0"
    "EGL_KHR_partial_update\0"
    "EGL_KHR_reusable_sync\0"
    "EGL_KHR_stream\0"
    "EGL_KHR_stream_attrib\0"
    "EGL_KHR_stream_consumer_gltexture\0"
    "EGL_KHR_stream_cross_process_fd\0"
    "EGL_KHR_stream_fifo\0"
    "EGL_KHR_stream_producer_eglsurface\0"
    "EGL_KHR_swap_buffers_with_damage\0"
    "EGL_KHR_wait_sync\0"
    "EGL_MESA_drm_image\0"
    "EGL_MESA_image_dma_buf_export\0"
    "EGL_MESA_query_driver\0"
    "EGL_NOK_swap_region\0"
    "EGL_NOK_swap_region2\0"
    "EGL_NV_native_query\0"
    "EGL_NV_post_sub_buffer\0"
    "EGL_NV_stream_consumer_gltexture_yuv\0"
    "EGL_NV_stream_flush\0"
    "EGL_NV_stream_metadata\0"
    "EGL_NV_stream_reset\0"
    "EGL_NV_stream_sync\0"
    "EGL_NV_sync\0"
    "EGL_NV_system_time\0"
     ;

static const uint16_t enum_string_offsets[] = {
    -1, /* egl_provider_terminator, unused */
    0, /* EGL 10 */
    7, /* EGL 11 */
    14, /* EGL 12 */
    21, /* EGL 14 */
    28, /* EGL 15 */
    35, /* EGL_ANDROID_blob_cache */
    58, /* EGL_ANDROID_create_native_client_buffer */
    98, /* EGL_ANDROID_get_frame_timestamps */
    131, /* EGL_ANDROID_get_native_client_buffer */
    168, /* EGL_ANDROID_native_fence_sync */
    198, /* EGL_ANDROID_presentation_time */
    228, /* EGL_ANGLE_query_surface_pointer */
    260, /* EGL_EXT_client_sync */
    280, /* EGL_EXT_compositor */
    299, /* EGL_EXT_device_base */
    319, /* EGL_EXT_device_enumeration */
    346, /* EGL_EXT_device_query */
    367, /* EGL_EXT_image_dma_buf_import_modifiers */
    406, /* EGL_EXT_output_base */
    426, /* EGL_EXT_platform_base */
    448, /* EGL_EXT_stream_consumer_egloutput */
    482, /* EGL_EXT_swap_buffers_with_damage */
    515, /* EGL_EXT_sync_reuse */
    534, /* EGL_HI_clientpixmap */
    554, /* EGL_KHR_cl_event2 */
    572, /* EGL_KHR_debug */
    586, /* EGL_KHR_display_reference */
    612, /* EGL_KHR_fence_sync */
    631, /* EGL_KHR_image */
    645, /* EGL_KHR_image_base */
    664, /* EGL_KHR_lock_surface */
    685, /* EGL_KHR_lock_surface3 */
    707, /* EGL_KHR_partial_update */
    730, /* EGL_KHR_reusable_sync */
    752, /* EGL_KHR_stream */
    767, /* EGL_KHR_stream_attrib */
    789, /* EGL_KHR_stream_consumer_gltexture */
    823, /* EGL_KHR_stream_cross_process_fd */
    855, /* EGL_KHR_stream_fifo */
    875, /* EGL_KHR_stream_producer_eglsurface */
    910, /* EGL_KHR_swap_buffers_with_damage */
    943, /* EGL_KHR_wait_sync */
    961, /* EGL_MESA_drm_image */
    980, /* EGL_MESA_image_dma_buf_export */
    1010, /* EGL_MESA_query_driver */
    1032, /* EGL_NOK_swap_region */
    1052, /* EGL_NOK_swap_region2 */
    1073, /* EGL_NV_native_query */
    1093, /* EGL_NV_post_sub_buffer */
    1116, /* EGL_NV_stream_consumer_gltexture_yuv */
    1153, /* EGL_NV_stream_flush */
    1173, /* EGL_NV_stream_metadata */
    1196, /* EGL_NV_stream_reset */
    1216, /* EGL_NV_stream_sync */
    1235, /* EGL_NV_sync */
    1247, /* EGL_NV_system_time */
};

static const char entrypoint_strings[] = {
   'e',
   'g',
   'l',
   'B',
   'i',
   'n',
   'd',
   'A',
   'P',
   'I',
   0, // eglBindAPI
   'e',
   'g',
   'l',
   'B',
   'i',
   'n',
   'd',
   'T',
   'e',
   'x',
   'I',
   'm',
   'a',
   'g',
   'e',
   0, // eglBindTexImage
   'e',
   'g',
   'l',
   'C',
   'h',
   'o',
   'o',
   's',
   'e',
   'C',
   'o',
   'n',
   'f',
   'i',
   'g',
   0, // eglChooseConfig
   'e',
   'g',
   'l',
   'C',
   'l',
   'i',
   'e',
   'n',
   't',
   'S',
   'i',
   'g',
   'n',
   'a',
   'l',
   'S',
   'y',
   'n',
   'c',
   'E',
   'X',
   'T',
   0, // eglClientSignalSyncEXT
   'e',
   'g',
   'l',
   'C',
   'l',
   'i',
   'e',
   'n',
   't',
   'W',
   'a',
   'i',
   't',
   'S',
   'y',
   'n',
   'c',
   0, // eglClientWaitSync
   'e',
   'g',
   'l',
   'C',
   'l',
   'i',
   'e',
   'n',
   't',
   'W',
   'a',
   'i',
   't',
   'S',
   'y',
   'n',
   'c',
   'K',
   'H',
   'R',
   0, // eglClientWaitSyncKHR
   'e',
   'g',
   'l',
   'C',
   'l',
   'i',
   'e',
   'n',
   't',
   'W',
   'a',
   'i',
   't',
   'S',
   'y',
   'n',
   'c',
   'N',
   'V',
   0, // eglClientWaitSyncNV
   'e',
   'g',
   'l',
   'C',
   'o',
   'm',
   'p',
   'o',
   's',
   'i',
   't',
   'o',
   'r',
   'B',
   'i',
   'n',
   'd',
   'T',
   'e',
   'x',
   'W',
   'i',
   'n',
   'd',
   'o',
   'w',
   'E',
   'X',
   'T',
   0, // eglCompositorBindTexWindowEXT
   'e',
   'g',
   'l',
   'C',
   'o',
   'm',
   'p',
   'o',
   's',
   'i',
   't',
   'o',
   'r',
   'S',
   'e',
   't',
   'C',
   'o',
   'n',
   't',
   'e',
   'x',
   't',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'u',
   't',
   'e',
   's',
   'E',
   'X',
   'T',
   0, // eglCompositorSetContextAttributesEXT
   'e',
   'g',
   'l',
   'C',
   'o',
   'm',
   'p',
   'o',
   's',
   'i',
   't',
   'o',
   'r',
   'S',
   'e',
   't',
   'C',
   'o',
   'n',
   't',
   'e',
   'x',
   't',
   'L',
   'i',
   's',
   't',
   'E',
   'X',
   'T',
   0, // eglCompositorSetContextListEXT
   'e',
   'g',
   'l',
   'C',
   'o',
   'm',
   'p',
   'o',
   's',
   'i',
   't',
   'o',
   'r',
   'S',
   'e',
   't',
   'S',
   'i',
   'z',
   'e',
   'E',
   'X',
   'T',
   0, // eglCompositorSetSizeEXT
   'e',
   'g',
   'l',
   'C',
   'o',
   'm',
   'p',
   'o',
   's',
   'i',
   't',
   'o',
   'r',
   'S',
   'e',
   't',
   'W',
   'i',
   'n',
   'd',
   'o',
   'w',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'u',
   't',
   'e',
   's',
   'E',
   'X',
   'T',
   0, // eglCompositorSetWindowAttributesEXT
   'e',
   'g',
   'l',
   'C',
   'o',
   'm',
   'p',
   'o',
   's',
   'i',
   't',
   'o',
   'r',
   'S',
   'e',
   't',
   'W',
   'i',
   'n',
   'd',
   'o',
   'w',
   'L',
   'i',
   's',
   't',
   'E',
   'X',
   'T',
   0, // eglCompositorSetWindowListEXT
   'e',
   'g',
   'l',
   'C',
   'o',
   'm',
   'p',
   'o',
   's',
   'i',
   't',
   'o',
   'r',
   'S',
   'w',
   'a',
   'p',
   'P',
   'o',
   'l',
   'i',
   'c',
   'y',
   'E',
   'X',
   'T',
   0, // eglCompositorSwapPolicyEXT
   'e',
   'g',
   'l',
   'C',
   'o',
   'p',
   'y',
   'B',
   'u',
   'f',
   'f',
   'e',
   'r',
   's',
   0, // eglCopyBuffers
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'C',
   'o',
   'n',
   't',
   'e',
   'x',
   't',
   0, // eglCreateContext
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'D',
   'R',
   'M',
   'I',
   'm',
   'a',
   'g',
   'e',
   'M',
   'E',
   'S',
   'A',
   0, // eglCreateDRMImageMESA
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'F',
   'e',
   'n',
   'c',
   'e',
   'S',
   'y',
   'n',
   'c',
   'N',
   'V',
   0, // eglCreateFenceSyncNV
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'I',
   'm',
   'a',
   'g',
   'e',
   0, // eglCreateImage
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'I',
   'm',
   'a',
   'g',
   'e',
   'K',
   'H',
   'R',
   0, // eglCreateImageKHR
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'N',
   'a',
   't',
   'i',
   'v',
   'e',
   'C',
   'l',
   'i',
   'e',
   'n',
   't',
   'B',
   'u',
   'f',
   'f',
   'e',
   'r',
   'A',
   'N',
   'D',
   'R',
   'O',
   'I',
   'D',
   0, // eglCreateNativeClientBufferANDROID
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'P',
   'b',
   'u',
   'f',
   'f',
   'e',
   'r',
   'F',
   'r',
   'o',
   'm',
   'C',
   'l',
   'i',
   'e',
   'n',
   't',
   'B',
   'u',
   'f',
   'f',
   'e',
   'r',
   0, // eglCreatePbufferFromClientBuffer
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'P',
   'b',
   'u',
   'f',
   'f',
   'e',
   'r',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   0, // eglCreatePbufferSurface
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'P',
   'i',
   'x',
   'm',
   'a',
   'p',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   0, // eglCreatePixmapSurface
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'P',
   'i',
   'x',
   'm',
   'a',
   'p',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   'H',
   'I',
   0, // eglCreatePixmapSurfaceHI
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'P',
   'l',
   'a',
   't',
   'f',
   'o',
   'r',
   'm',
   'P',
   'i',
   'x',
   'm',
   'a',
   'p',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   0, // eglCreatePlatformPixmapSurface
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'P',
   'l',
   'a',
   't',
   'f',
   'o',
   'r',
   'm',
   'P',
   'i',
   'x',
   'm',
   'a',
   'p',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   'E',
   'X',
   'T',
   0, // eglCreatePlatformPixmapSurfaceEXT
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'P',
   'l',
   'a',
   't',
   'f',
   'o',
   'r',
   'm',
   'W',
   'i',
   'n',
   'd',
   'o',
   'w',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   0, // eglCreatePlatformWindowSurface
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'P',
   'l',
   'a',
   't',
   'f',
   'o',
   'r',
   'm',
   'W',
   'i',
   'n',
   'd',
   'o',
   'w',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   'E',
   'X',
   'T',
   0, // eglCreatePlatformWindowSurfaceEXT
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'K',
   'H',
   'R',
   0, // eglCreateStreamAttribKHR
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'F',
   'r',
   'o',
   'm',
   'F',
   'i',
   'l',
   'e',
   'D',
   'e',
   's',
   'c',
   'r',
   'i',
   'p',
   't',
   'o',
   'r',
   'K',
   'H',
   'R',
   0, // eglCreateStreamFromFileDescriptorKHR
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'K',
   'H',
   'R',
   0, // eglCreateStreamKHR
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'P',
   'r',
   'o',
   'd',
   'u',
   'c',
   'e',
   'r',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   'K',
   'H',
   'R',
   0, // eglCreateStreamProducerSurfaceKHR
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'S',
   'y',
   'n',
   'c',
   'N',
   'V',
   0, // eglCreateStreamSyncNV
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'S',
   'y',
   'n',
   'c',
   0, // eglCreateSync
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'S',
   'y',
   'n',
   'c',
   '6',
   '4',
   'K',
   'H',
   'R',
   0, // eglCreateSync64KHR
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'S',
   'y',
   'n',
   'c',
   'K',
   'H',
   'R',
   0, // eglCreateSyncKHR
   'e',
   'g',
   'l',
   'C',
   'r',
   'e',
   'a',
   't',
   'e',
   'W',
   'i',
   'n',
   'd',
   'o',
   'w',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   0, // eglCreateWindowSurface
   'e',
   'g',
   'l',
   'D',
   'e',
   'b',
   'u',
   'g',
   'M',
   'e',
   's',
   's',
   'a',
   'g',
   'e',
   'C',
   'o',
   'n',
   't',
   'r',
   'o',
   'l',
   'K',
   'H',
   'R',
   0, // eglDebugMessageControlKHR
   'e',
   'g',
   'l',
   'D',
   'e',
   's',
   't',
   'r',
   'o',
   'y',
   'C',
   'o',
   'n',
   't',
   'e',
   'x',
   't',
   0, // eglDestroyContext
   'e',
   'g',
   'l',
   'D',
   'e',
   's',
   't',
   'r',
   'o',
   'y',
   'I',
   'm',
   'a',
   'g',
   'e',
   0, // eglDestroyImage
   'e',
   'g',
   'l',
   'D',
   'e',
   's',
   't',
   'r',
   'o',
   'y',
   'I',
   'm',
   'a',
   'g',
   'e',
   'K',
   'H',
   'R',
   0, // eglDestroyImageKHR
   'e',
   'g',
   'l',
   'D',
   'e',
   's',
   't',
   'r',
   'o',
   'y',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'K',
   'H',
   'R',
   0, // eglDestroyStreamKHR
   'e',
   'g',
   'l',
   'D',
   'e',
   's',
   't',
   'r',
   'o',
   'y',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   0, // eglDestroySurface
   'e',
   'g',
   'l',
   'D',
   'e',
   's',
   't',
   'r',
   'o',
   'y',
   'S',
   'y',
   'n',
   'c',
   0, // eglDestroySync
   'e',
   'g',
   'l',
   'D',
   'e',
   's',
   't',
   'r',
   'o',
   'y',
   'S',
   'y',
   'n',
   'c',
   'K',
   'H',
   'R',
   0, // eglDestroySyncKHR
   'e',
   'g',
   'l',
   'D',
   'e',
   's',
   't',
   'r',
   'o',
   'y',
   'S',
   'y',
   'n',
   'c',
   'N',
   'V',
   0, // eglDestroySyncNV
   'e',
   'g',
   'l',
   'D',
   'u',
   'p',
   'N',
   'a',
   't',
   'i',
   'v',
   'e',
   'F',
   'e',
   'n',
   'c',
   'e',
   'F',
   'D',
   'A',
   'N',
   'D',
   'R',
   'O',
   'I',
   'D',
   0, // eglDupNativeFenceFDANDROID
   'e',
   'g',
   'l',
   'E',
   'x',
   'p',
   'o',
   'r',
   't',
   'D',
   'M',
   'A',
   'B',
   'U',
   'F',
   'I',
   'm',
   'a',
   'g',
   'e',
   'M',
   'E',
   'S',
   'A',
   0, // eglExportDMABUFImageMESA
   'e',
   'g',
   'l',
   'E',
   'x',
   'p',
   'o',
   'r',
   't',
   'D',
   'M',
   'A',
   'B',
   'U',
   'F',
   'I',
   'm',
   'a',
   'g',
   'e',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'M',
   'E',
   'S',
   'A',
   0, // eglExportDMABUFImageQueryMESA
   'e',
   'g',
   'l',
   'E',
   'x',
   'p',
   'o',
   'r',
   't',
   'D',
   'R',
   'M',
   'I',
   'm',
   'a',
   'g',
   'e',
   'M',
   'E',
   'S',
   'A',
   0, // eglExportDRMImageMESA
   'e',
   'g',
   'l',
   'F',
   'e',
   'n',
   'c',
   'e',
   'N',
   'V',
   0, // eglFenceNV
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'C',
   'o',
   'm',
   'p',
   'o',
   's',
   'i',
   't',
   'o',
   'r',
   'T',
   'i',
   'm',
   'i',
   'n',
   'g',
   'A',
   'N',
   'D',
   'R',
   'O',
   'I',
   'D',
   0, // eglGetCompositorTimingANDROID
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'C',
   'o',
   'm',
   'p',
   'o',
   's',
   'i',
   't',
   'o',
   'r',
   'T',
   'i',
   'm',
   'i',
   'n',
   'g',
   'S',
   'u',
   'p',
   'p',
   'o',
   'r',
   't',
   'e',
   'd',
   'A',
   'N',
   'D',
   'R',
   'O',
   'I',
   'D',
   0, // eglGetCompositorTimingSupportedANDROID
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'C',
   'o',
   'n',
   'f',
   'i',
   'g',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   0, // eglGetConfigAttrib
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'C',
   'o',
   'n',
   'f',
   'i',
   'g',
   's',
   0, // eglGetConfigs
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'C',
   'u',
   'r',
   'r',
   'e',
   'n',
   't',
   'C',
   'o',
   'n',
   't',
   'e',
   'x',
   't',
   0, // eglGetCurrentContext
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'C',
   'u',
   'r',
   'r',
   'e',
   'n',
   't',
   'D',
   'i',
   's',
   'p',
   'l',
   'a',
   'y',
   0, // eglGetCurrentDisplay
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'C',
   'u',
   'r',
   'r',
   'e',
   'n',
   't',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   0, // eglGetCurrentSurface
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'D',
   'i',
   's',
   'p',
   'l',
   'a',
   'y',
   0, // eglGetDisplay
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'D',
   'i',
   's',
   'p',
   'l',
   'a',
   'y',
   'D',
   'r',
   'i',
   'v',
   'e',
   'r',
   'C',
   'o',
   'n',
   'f',
   'i',
   'g',
   0, // eglGetDisplayDriverConfig
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'D',
   'i',
   's',
   'p',
   'l',
   'a',
   'y',
   'D',
   'r',
   'i',
   'v',
   'e',
   'r',
   'N',
   'a',
   'm',
   'e',
   0, // eglGetDisplayDriverName
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'E',
   'r',
   'r',
   'o',
   'r',
   0, // eglGetError
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'F',
   'r',
   'a',
   'm',
   'e',
   'T',
   'i',
   'm',
   'e',
   's',
   't',
   'a',
   'm',
   'p',
   'S',
   'u',
   'p',
   'p',
   'o',
   'r',
   't',
   'e',
   'd',
   'A',
   'N',
   'D',
   'R',
   'O',
   'I',
   'D',
   0, // eglGetFrameTimestampSupportedANDROID
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'F',
   'r',
   'a',
   'm',
   'e',
   'T',
   'i',
   'm',
   'e',
   's',
   't',
   'a',
   'm',
   'p',
   's',
   'A',
   'N',
   'D',
   'R',
   'O',
   'I',
   'D',
   0, // eglGetFrameTimestampsANDROID
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'N',
   'a',
   't',
   'i',
   'v',
   'e',
   'C',
   'l',
   'i',
   'e',
   'n',
   't',
   'B',
   'u',
   'f',
   'f',
   'e',
   'r',
   'A',
   'N',
   'D',
   'R',
   'O',
   'I',
   'D',
   0, // eglGetNativeClientBufferANDROID
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'N',
   'e',
   'x',
   't',
   'F',
   'r',
   'a',
   'm',
   'e',
   'I',
   'd',
   'A',
   'N',
   'D',
   'R',
   'O',
   'I',
   'D',
   0, // eglGetNextFrameIdANDROID
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'O',
   'u',
   't',
   'p',
   'u',
   't',
   'L',
   'a',
   'y',
   'e',
   'r',
   's',
   'E',
   'X',
   'T',
   0, // eglGetOutputLayersEXT
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'O',
   'u',
   't',
   'p',
   'u',
   't',
   'P',
   'o',
   'r',
   't',
   's',
   'E',
   'X',
   'T',
   0, // eglGetOutputPortsEXT
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'P',
   'l',
   'a',
   't',
   'f',
   'o',
   'r',
   'm',
   'D',
   'i',
   's',
   'p',
   'l',
   'a',
   'y',
   0, // eglGetPlatformDisplay
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'P',
   'l',
   'a',
   't',
   'f',
   'o',
   'r',
   'm',
   'D',
   'i',
   's',
   'p',
   'l',
   'a',
   'y',
   'E',
   'X',
   'T',
   0, // eglGetPlatformDisplayEXT
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'P',
   'r',
   'o',
   'c',
   'A',
   'd',
   'd',
   'r',
   'e',
   's',
   's',
   0, // eglGetProcAddress
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'F',
   'i',
   'l',
   'e',
   'D',
   'e',
   's',
   'c',
   'r',
   'i',
   'p',
   't',
   'o',
   'r',
   'K',
   'H',
   'R',
   0, // eglGetStreamFileDescriptorKHR
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'S',
   'y',
   'n',
   'c',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   0, // eglGetSyncAttrib
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'S',
   'y',
   'n',
   'c',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'K',
   'H',
   'R',
   0, // eglGetSyncAttribKHR
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'S',
   'y',
   'n',
   'c',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'N',
   'V',
   0, // eglGetSyncAttribNV
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'S',
   'y',
   's',
   't',
   'e',
   'm',
   'T',
   'i',
   'm',
   'e',
   'F',
   'r',
   'e',
   'q',
   'u',
   'e',
   'n',
   'c',
   'y',
   'N',
   'V',
   0, // eglGetSystemTimeFrequencyNV
   'e',
   'g',
   'l',
   'G',
   'e',
   't',
   'S',
   'y',
   's',
   't',
   'e',
   'm',
   'T',
   'i',
   'm',
   'e',
   'N',
   'V',
   0, // eglGetSystemTimeNV
   'e',
   'g',
   'l',
   'I',
   'n',
   'i',
   't',
   'i',
   'a',
   'l',
   'i',
   'z',
   'e',
   0, // eglInitialize
   'e',
   'g',
   'l',
   'L',
   'a',
   'b',
   'e',
   'l',
   'O',
   'b',
   'j',
   'e',
   'c',
   't',
   'K',
   'H',
   'R',
   0, // eglLabelObjectKHR
   'e',
   'g',
   'l',
   'L',
   'o',
   'c',
   'k',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   'K',
   'H',
   'R',
   0, // eglLockSurfaceKHR
   'e',
   'g',
   'l',
   'M',
   'a',
   'k',
   'e',
   'C',
   'u',
   'r',
   'r',
   'e',
   'n',
   't',
   0, // eglMakeCurrent
   'e',
   'g',
   'l',
   'O',
   'u',
   't',
   'p',
   'u',
   't',
   'L',
   'a',
   'y',
   'e',
   'r',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'E',
   'X',
   'T',
   0, // eglOutputLayerAttribEXT
   'e',
   'g',
   'l',
   'O',
   'u',
   't',
   'p',
   'u',
   't',
   'P',
   'o',
   'r',
   't',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'E',
   'X',
   'T',
   0, // eglOutputPortAttribEXT
   'e',
   'g',
   'l',
   'P',
   'o',
   's',
   't',
   'S',
   'u',
   'b',
   'B',
   'u',
   'f',
   'f',
   'e',
   'r',
   'N',
   'V',
   0, // eglPostSubBufferNV
   'e',
   'g',
   'l',
   'P',
   'r',
   'e',
   's',
   'e',
   'n',
   't',
   'a',
   't',
   'i',
   'o',
   'n',
   'T',
   'i',
   'm',
   'e',
   'A',
   'N',
   'D',
   'R',
   'O',
   'I',
   'D',
   0, // eglPresentationTimeANDROID
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'A',
   'P',
   'I',
   0, // eglQueryAPI
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'C',
   'o',
   'n',
   't',
   'e',
   'x',
   't',
   0, // eglQueryContext
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'D',
   'e',
   'b',
   'u',
   'g',
   'K',
   'H',
   'R',
   0, // eglQueryDebugKHR
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'D',
   'e',
   'v',
   'i',
   'c',
   'e',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'E',
   'X',
   'T',
   0, // eglQueryDeviceAttribEXT
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'D',
   'e',
   'v',
   'i',
   'c',
   'e',
   'S',
   't',
   'r',
   'i',
   'n',
   'g',
   'E',
   'X',
   'T',
   0, // eglQueryDeviceStringEXT
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'D',
   'e',
   'v',
   'i',
   'c',
   'e',
   's',
   'E',
   'X',
   'T',
   0, // eglQueryDevicesEXT
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'D',
   'i',
   's',
   'p',
   'l',
   'a',
   'y',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'E',
   'X',
   'T',
   0, // eglQueryDisplayAttribEXT
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'D',
   'i',
   's',
   'p',
   'l',
   'a',
   'y',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'K',
   'H',
   'R',
   0, // eglQueryDisplayAttribKHR
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'D',
   'i',
   's',
   'p',
   'l',
   'a',
   'y',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'N',
   'V',
   0, // eglQueryDisplayAttribNV
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'D',
   'm',
   'a',
   'B',
   'u',
   'f',
   'F',
   'o',
   'r',
   'm',
   'a',
   't',
   's',
   'E',
   'X',
   'T',
   0, // eglQueryDmaBufFormatsEXT
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'D',
   'm',
   'a',
   'B',
   'u',
   'f',
   'M',
   'o',
   'd',
   'i',
   'f',
   'i',
   'e',
   'r',
   's',
   'E',
   'X',
   'T',
   0, // eglQueryDmaBufModifiersEXT
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'N',
   'a',
   't',
   'i',
   'v',
   'e',
   'D',
   'i',
   's',
   'p',
   'l',
   'a',
   'y',
   'N',
   'V',
   0, // eglQueryNativeDisplayNV
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'N',
   'a',
   't',
   'i',
   'v',
   'e',
   'P',
   'i',
   'x',
   'm',
   'a',
   'p',
   'N',
   'V',
   0, // eglQueryNativePixmapNV
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'N',
   'a',
   't',
   'i',
   'v',
   'e',
   'W',
   'i',
   'n',
   'd',
   'o',
   'w',
   'N',
   'V',
   0, // eglQueryNativeWindowNV
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'O',
   'u',
   't',
   'p',
   'u',
   't',
   'L',
   'a',
   'y',
   'e',
   'r',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'E',
   'X',
   'T',
   0, // eglQueryOutputLayerAttribEXT
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'O',
   'u',
   't',
   'p',
   'u',
   't',
   'L',
   'a',
   'y',
   'e',
   'r',
   'S',
   't',
   'r',
   'i',
   'n',
   'g',
   'E',
   'X',
   'T',
   0, // eglQueryOutputLayerStringEXT
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'O',
   'u',
   't',
   'p',
   'u',
   't',
   'P',
   'o',
   'r',
   't',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'E',
   'X',
   'T',
   0, // eglQueryOutputPortAttribEXT
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'O',
   'u',
   't',
   'p',
   'u',
   't',
   'P',
   'o',
   'r',
   't',
   'S',
   't',
   'r',
   'i',
   'n',
   'g',
   'E',
   'X',
   'T',
   0, // eglQueryOutputPortStringEXT
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'K',
   'H',
   'R',
   0, // eglQueryStreamAttribKHR
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'K',
   'H',
   'R',
   0, // eglQueryStreamKHR
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'M',
   'e',
   't',
   'a',
   'd',
   'a',
   't',
   'a',
   'N',
   'V',
   0, // eglQueryStreamMetadataNV
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'T',
   'i',
   'm',
   'e',
   'K',
   'H',
   'R',
   0, // eglQueryStreamTimeKHR
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'u',
   '6',
   '4',
   'K',
   'H',
   'R',
   0, // eglQueryStreamu64KHR
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'S',
   't',
   'r',
   'i',
   'n',
   'g',
   0, // eglQueryString
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   0, // eglQuerySurface
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   '6',
   '4',
   'K',
   'H',
   'R',
   0, // eglQuerySurface64KHR
   'e',
   'g',
   'l',
   'Q',
   'u',
   'e',
   'r',
   'y',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   'P',
   'o',
   'i',
   'n',
   't',
   'e',
   'r',
   'A',
   'N',
   'G',
   'L',
   'E',
   0, // eglQuerySurfacePointerANGLE
   'e',
   'g',
   'l',
   'R',
   'e',
   'l',
   'e',
   'a',
   's',
   'e',
   'T',
   'e',
   'x',
   'I',
   'm',
   'a',
   'g',
   'e',
   0, // eglReleaseTexImage
   'e',
   'g',
   'l',
   'R',
   'e',
   'l',
   'e',
   'a',
   's',
   'e',
   'T',
   'h',
   'r',
   'e',
   'a',
   'd',
   0, // eglReleaseThread
   'e',
   'g',
   'l',
   'R',
   'e',
   's',
   'e',
   't',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'N',
   'V',
   0, // eglResetStreamNV
   'e',
   'g',
   'l',
   'S',
   'e',
   't',
   'B',
   'l',
   'o',
   'b',
   'C',
   'a',
   'c',
   'h',
   'e',
   'F',
   'u',
   'n',
   'c',
   's',
   'A',
   'N',
   'D',
   'R',
   'O',
   'I',
   'D',
   0, // eglSetBlobCacheFuncsANDROID
   'e',
   'g',
   'l',
   'S',
   'e',
   't',
   'D',
   'a',
   'm',
   'a',
   'g',
   'e',
   'R',
   'e',
   'g',
   'i',
   'o',
   'n',
   'K',
   'H',
   'R',
   0, // eglSetDamageRegionKHR
   'e',
   'g',
   'l',
   'S',
   'e',
   't',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'K',
   'H',
   'R',
   0, // eglSetStreamAttribKHR
   'e',
   'g',
   'l',
   'S',
   'e',
   't',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'M',
   'e',
   't',
   'a',
   'd',
   'a',
   't',
   'a',
   'N',
   'V',
   0, // eglSetStreamMetadataNV
   'e',
   'g',
   'l',
   'S',
   'i',
   'g',
   'n',
   'a',
   'l',
   'S',
   'y',
   'n',
   'c',
   'K',
   'H',
   'R',
   0, // eglSignalSyncKHR
   'e',
   'g',
   'l',
   'S',
   'i',
   'g',
   'n',
   'a',
   'l',
   'S',
   'y',
   'n',
   'c',
   'N',
   'V',
   0, // eglSignalSyncNV
   'e',
   'g',
   'l',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'K',
   'H',
   'R',
   0, // eglStreamAttribKHR
   'e',
   'g',
   'l',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'C',
   'o',
   'n',
   's',
   'u',
   'm',
   'e',
   'r',
   'A',
   'c',
   'q',
   'u',
   'i',
   'r',
   'e',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'K',
   'H',
   'R',
   0, // eglStreamConsumerAcquireAttribKHR
   'e',
   'g',
   'l',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'C',
   'o',
   'n',
   's',
   'u',
   'm',
   'e',
   'r',
   'A',
   'c',
   'q',
   'u',
   'i',
   'r',
   'e',
   'K',
   'H',
   'R',
   0, // eglStreamConsumerAcquireKHR
   'e',
   'g',
   'l',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'C',
   'o',
   'n',
   's',
   'u',
   'm',
   'e',
   'r',
   'G',
   'L',
   'T',
   'e',
   'x',
   't',
   'u',
   'r',
   'e',
   'E',
   'x',
   't',
   'e',
   'r',
   'n',
   'a',
   'l',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   's',
   'N',
   'V',
   0, // eglStreamConsumerGLTextureExternalAttribsNV
   'e',
   'g',
   'l',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'C',
   'o',
   'n',
   's',
   'u',
   'm',
   'e',
   'r',
   'G',
   'L',
   'T',
   'e',
   'x',
   't',
   'u',
   'r',
   'e',
   'E',
   'x',
   't',
   'e',
   'r',
   'n',
   'a',
   'l',
   'K',
   'H',
   'R',
   0, // eglStreamConsumerGLTextureExternalKHR
   'e',
   'g',
   'l',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'C',
   'o',
   'n',
   's',
   'u',
   'm',
   'e',
   'r',
   'O',
   'u',
   't',
   'p',
   'u',
   't',
   'E',
   'X',
   'T',
   0, // eglStreamConsumerOutputEXT
   'e',
   'g',
   'l',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'C',
   'o',
   'n',
   's',
   'u',
   'm',
   'e',
   'r',
   'R',
   'e',
   'l',
   'e',
   'a',
   's',
   'e',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   'K',
   'H',
   'R',
   0, // eglStreamConsumerReleaseAttribKHR
   'e',
   'g',
   'l',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'C',
   'o',
   'n',
   's',
   'u',
   'm',
   'e',
   'r',
   'R',
   'e',
   'l',
   'e',
   'a',
   's',
   'e',
   'K',
   'H',
   'R',
   0, // eglStreamConsumerReleaseKHR
   'e',
   'g',
   'l',
   'S',
   't',
   'r',
   'e',
   'a',
   'm',
   'F',
   'l',
   'u',
   's',
   'h',
   'N',
   'V',
   0, // eglStreamFlushNV
   'e',
   'g',
   'l',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   'A',
   't',
   't',
   'r',
   'i',
   'b',
   0, // eglSurfaceAttrib
   'e',
   'g',
   'l',
   'S',
   'w',
   'a',
   'p',
   'B',
   'u',
   'f',
   'f',
   'e',
   'r',
   's',
   0, // eglSwapBuffers
   'e',
   'g',
   'l',
   'S',
   'w',
   'a',
   'p',
   'B',
   'u',
   'f',
   'f',
   'e',
   'r',
   's',
   'R',
   'e',
   'g',
   'i',
   'o',
   'n',
   '2',
   'N',
   'O',
   'K',
   0, // eglSwapBuffersRegion2NOK
   'e',
   'g',
   'l',
   'S',
   'w',
   'a',
   'p',
   'B',
   'u',
   'f',
   'f',
   'e',
   'r',
   's',
   'R',
   'e',
   'g',
   'i',
   'o',
   'n',
   'N',
   'O',
   'K',
   0, // eglSwapBuffersRegionNOK
   'e',
   'g',
   'l',
   'S',
   'w',
   'a',
   'p',
   'B',
   'u',
   'f',
   'f',
   'e',
   'r',
   's',
   'W',
   'i',
   't',
   'h',
   'D',
   'a',
   'm',
   'a',
   'g',
   'e',
   'E',
   'X',
   'T',
   0, // eglSwapBuffersWithDamageEXT
   'e',
   'g',
   'l',
   'S',
   'w',
   'a',
   'p',
   'B',
   'u',
   'f',
   'f',
   'e',
   'r',
   's',
   'W',
   'i',
   't',
   'h',
   'D',
   'a',
   'm',
   'a',
   'g',
   'e',
   'K',
   'H',
   'R',
   0, // eglSwapBuffersWithDamageKHR
   'e',
   'g',
   'l',
   'S',
   'w',
   'a',
   'p',
   'I',
   'n',
   't',
   'e',
   'r',
   'v',
   'a',
   'l',
   0, // eglSwapInterval
   'e',
   'g',
   'l',
   'T',
   'e',
   'r',
   'm',
   'i',
   'n',
   'a',
   't',
   'e',
   0, // eglTerminate
   'e',
   'g',
   'l',
   'U',
   'n',
   'l',
   'o',
   'c',
   'k',
   'S',
   'u',
   'r',
   'f',
   'a',
   'c',
   'e',
   'K',
   'H',
   'R',
   0, // eglUnlockSurfaceKHR
   'e',
   'g',
   'l',
   'U',
   'n',
   's',
   'i',
   'g',
   'n',
   'a',
   'l',
   'S',
   'y',
   'n',
   'c',
   'E',
   'X',
   'T',
   0, // eglUnsignalSyncEXT
   'e',
   'g',
   'l',
   'W',
   'a',
   'i',
   't',
   'C',
   'l',
   'i',
   'e',
   'n',
   't',
   0, // eglWaitClient
   'e',
   'g',
   'l',
   'W',
   'a',
   'i',
   't',
   'G',
   'L',
   0, // eglWaitGL
   'e',
   'g',
   'l',
   'W',
   'a',
   'i',
   't',
   'N',
   'a',
   't',
   'i',
   'v',
   'e',
   0, // eglWaitNative
   'e',
   'g',
   'l',
   'W',
   'a',
   'i',
   't',
   'S',
   'y',
   'n',
   'c',
   0, // eglWaitSync
   'e',
   'g',
   'l',
   'W',
   'a',
   'i',
   't',
   'S',
   'y',
   'n',
   'c',
   'K',
   'H',
   'R',
   0, // eglWaitSyncKHR
    0 };

static void *egl_provider_resolver(const char *name,
                                   const enum egl_provider *providers,
                                   const uint32_t *entrypoints)
{
    int i;
    for (i = 0; providers[i] != egl_provider_terminator; i++) {
        const char *provider_name = enum_string + enum_string_offsets[providers[i]];
        switch (providers[i]) {

        case PROVIDER_EGL_10:
            if (true)
                return epoxy_egl_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_11:
            if (epoxy_conservative_egl_version() >= 11)
                return epoxy_egl_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_12:
            if (epoxy_conservative_egl_version() >= 12)
                return epoxy_egl_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_14:
            if (epoxy_conservative_egl_version() >= 14)
                return epoxy_egl_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_15:
            if (epoxy_conservative_egl_version() >= 15)
                return epoxy_egl_dlsym(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_ANDROID_blob_cache:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_ANDROID_create_native_client_buffer:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_ANDROID_get_frame_timestamps:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_ANDROID_get_native_client_buffer:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_ANDROID_native_fence_sync:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_ANDROID_presentation_time:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_ANGLE_query_surface_pointer:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_client_sync:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_compositor:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_device_base:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_device_enumeration:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_device_query:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_image_dma_buf_import_modifiers:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_output_base:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_platform_base:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_stream_consumer_egloutput:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_swap_buffers_with_damage:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_EXT_sync_reuse:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_HI_clientpixmap:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_cl_event2:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_debug:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_display_reference:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_fence_sync:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_image:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_image_base:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_lock_surface:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_lock_surface3:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_partial_update:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_reusable_sync:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_stream:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_stream_attrib:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_stream_consumer_gltexture:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_stream_cross_process_fd:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_stream_fifo:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_stream_producer_eglsurface:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_swap_buffers_with_damage:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_KHR_wait_sync:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_MESA_drm_image:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_MESA_image_dma_buf_export:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_MESA_query_driver:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NOK_swap_region:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NOK_swap_region2:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NV_native_query:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NV_post_sub_buffer:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NV_stream_consumer_gltexture_yuv:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NV_stream_flush:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NV_stream_metadata:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NV_stream_reset:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NV_stream_sync:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NV_sync:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case PROVIDER_EGL_NV_system_time:
            if (epoxy_conservative_has_egl_extension(provider_name))
                return eglGetProcAddress(entrypoint_strings + entrypoints[i]);
            break;
        case egl_provider_terminator:
            abort(); /* Not reached */
        }
    }

    if (epoxy_resolver_failure_handler)
        return epoxy_resolver_failure_handler(name);

    fprintf(stderr, "No provider of %s found.  Requires one of:\n", name);
    for (i = 0; providers[i] != egl_provider_terminator; i++) {
        fprintf(stderr, "    %s\n", enum_string + enum_string_offsets[providers[i]]);
    }
    if (providers[0] == egl_provider_terminator) {
        fprintf(stderr, "    No known providers.  This is likely a bug "
                        "in libepoxy code generation\n");
    }
    abort();
}

EPOXY_NOINLINE static void *
egl_single_resolver(enum egl_provider provider, uint32_t entrypoint_offset);

static void *
egl_single_resolver(enum egl_provider provider, uint32_t entrypoint_offset)
{
    enum egl_provider providers[] = {
        provider,
        egl_provider_terminator
    };
    return egl_provider_resolver(entrypoint_strings + entrypoint_offset,
                                providers, &entrypoint_offset);
}

static PFNEGLBINDAPIPROC
epoxy_eglBindAPI_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_12, 0 /* eglBindAPI */);
}

static PFNEGLBINDTEXIMAGEPROC
epoxy_eglBindTexImage_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_11, 11 /* eglBindTexImage */);
}

static PFNEGLCHOOSECONFIGPROC
epoxy_eglChooseConfig_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 27 /* eglChooseConfig */);
}

static PFNEGLCLIENTSIGNALSYNCEXTPROC
epoxy_eglClientSignalSyncEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_client_sync, 43 /* eglClientSignalSyncEXT */);
}

static PFNEGLCLIENTWAITSYNCPROC
epoxy_eglClientWaitSync_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_15,
        PROVIDER_EGL_KHR_fence_sync,
        PROVIDER_EGL_KHR_reusable_sync,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        66 /* "eglClientWaitSync" */,
        84 /* "eglClientWaitSyncKHR" */,
        84 /* "eglClientWaitSyncKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 66 /* "eglClientWaitSync" */,
                                providers, entrypoints);
}

static PFNEGLCLIENTWAITSYNCKHRPROC
epoxy_eglClientWaitSyncKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_KHR_fence_sync,
        PROVIDER_EGL_KHR_reusable_sync,
        PROVIDER_EGL_15,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        84 /* "eglClientWaitSyncKHR" */,
        84 /* "eglClientWaitSyncKHR" */,
        66 /* "eglClientWaitSync" */,
    };
    return egl_provider_resolver(entrypoint_strings + 84 /* "eglClientWaitSyncKHR" */,
                                providers, entrypoints);
}

static PFNEGLCLIENTWAITSYNCNVPROC
epoxy_eglClientWaitSyncNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_sync, 105 /* eglClientWaitSyncNV */);
}

static PFNEGLCOMPOSITORBINDTEXWINDOWEXTPROC
epoxy_eglCompositorBindTexWindowEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_compositor, 125 /* eglCompositorBindTexWindowEXT */);
}

static PFNEGLCOMPOSITORSETCONTEXTATTRIBUTESEXTPROC
epoxy_eglCompositorSetContextAttributesEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_compositor, 155 /* eglCompositorSetContextAttributesEXT */);
}

static PFNEGLCOMPOSITORSETCONTEXTLISTEXTPROC
epoxy_eglCompositorSetContextListEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_compositor, 192 /* eglCompositorSetContextListEXT */);
}

static PFNEGLCOMPOSITORSETSIZEEXTPROC
epoxy_eglCompositorSetSizeEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_compositor, 223 /* eglCompositorSetSizeEXT */);
}

static PFNEGLCOMPOSITORSETWINDOWATTRIBUTESEXTPROC
epoxy_eglCompositorSetWindowAttributesEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_compositor, 247 /* eglCompositorSetWindowAttributesEXT */);
}

static PFNEGLCOMPOSITORSETWINDOWLISTEXTPROC
epoxy_eglCompositorSetWindowListEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_compositor, 283 /* eglCompositorSetWindowListEXT */);
}

static PFNEGLCOMPOSITORSWAPPOLICYEXTPROC
epoxy_eglCompositorSwapPolicyEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_compositor, 313 /* eglCompositorSwapPolicyEXT */);
}

static PFNEGLCOPYBUFFERSPROC
epoxy_eglCopyBuffers_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 340 /* eglCopyBuffers */);
}

static PFNEGLCREATECONTEXTPROC
epoxy_eglCreateContext_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 355 /* eglCreateContext */);
}

static PFNEGLCREATEDRMIMAGEMESAPROC
epoxy_eglCreateDRMImageMESA_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_MESA_drm_image, 372 /* eglCreateDRMImageMESA */);
}

static PFNEGLCREATEFENCESYNCNVPROC
epoxy_eglCreateFenceSyncNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_sync, 394 /* eglCreateFenceSyncNV */);
}

static PFNEGLCREATEIMAGEPROC
epoxy_eglCreateImage_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_15, 415 /* eglCreateImage */);
}

static PFNEGLCREATEIMAGEKHRPROC
epoxy_eglCreateImageKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_KHR_image,
        PROVIDER_EGL_KHR_image_base,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        430 /* "eglCreateImageKHR" */,
        430 /* "eglCreateImageKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 430 /* "eglCreateImageKHR" */,
                                providers, entrypoints);
}

static PFNEGLCREATENATIVECLIENTBUFFERANDROIDPROC
epoxy_eglCreateNativeClientBufferANDROID_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANDROID_create_native_client_buffer, 448 /* eglCreateNativeClientBufferANDROID */);
}

static PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC
epoxy_eglCreatePbufferFromClientBuffer_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_12, 483 /* eglCreatePbufferFromClientBuffer */);
}

static PFNEGLCREATEPBUFFERSURFACEPROC
epoxy_eglCreatePbufferSurface_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 516 /* eglCreatePbufferSurface */);
}

static PFNEGLCREATEPIXMAPSURFACEPROC
epoxy_eglCreatePixmapSurface_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 540 /* eglCreatePixmapSurface */);
}

static PFNEGLCREATEPIXMAPSURFACEHIPROC
epoxy_eglCreatePixmapSurfaceHI_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_HI_clientpixmap, 563 /* eglCreatePixmapSurfaceHI */);
}

static PFNEGLCREATEPLATFORMPIXMAPSURFACEPROC
epoxy_eglCreatePlatformPixmapSurface_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_15, 588 /* eglCreatePlatformPixmapSurface */);
}

static PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC
epoxy_eglCreatePlatformPixmapSurfaceEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_platform_base, 619 /* eglCreatePlatformPixmapSurfaceEXT */);
}

static PFNEGLCREATEPLATFORMWINDOWSURFACEPROC
epoxy_eglCreatePlatformWindowSurface_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_15, 653 /* eglCreatePlatformWindowSurface */);
}

static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC
epoxy_eglCreatePlatformWindowSurfaceEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_platform_base, 684 /* eglCreatePlatformWindowSurfaceEXT */);
}

static PFNEGLCREATESTREAMATTRIBKHRPROC
epoxy_eglCreateStreamAttribKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_attrib, 718 /* eglCreateStreamAttribKHR */);
}

static PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC
epoxy_eglCreateStreamFromFileDescriptorKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_cross_process_fd, 743 /* eglCreateStreamFromFileDescriptorKHR */);
}

static PFNEGLCREATESTREAMKHRPROC
epoxy_eglCreateStreamKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream, 780 /* eglCreateStreamKHR */);
}

static PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC
epoxy_eglCreateStreamProducerSurfaceKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_producer_eglsurface, 799 /* eglCreateStreamProducerSurfaceKHR */);
}

static PFNEGLCREATESTREAMSYNCNVPROC
epoxy_eglCreateStreamSyncNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_stream_sync, 833 /* eglCreateStreamSyncNV */);
}

static PFNEGLCREATESYNCPROC
epoxy_eglCreateSync_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_15,
        PROVIDER_EGL_KHR_cl_event2,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        855 /* "eglCreateSync" */,
        869 /* "eglCreateSync64KHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 855 /* "eglCreateSync" */,
                                providers, entrypoints);
}

static PFNEGLCREATESYNC64KHRPROC
epoxy_eglCreateSync64KHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_KHR_cl_event2,
        PROVIDER_EGL_15,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        869 /* "eglCreateSync64KHR" */,
        855 /* "eglCreateSync" */,
    };
    return egl_provider_resolver(entrypoint_strings + 869 /* "eglCreateSync64KHR" */,
                                providers, entrypoints);
}

static PFNEGLCREATESYNCKHRPROC
epoxy_eglCreateSyncKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_KHR_fence_sync,
        PROVIDER_EGL_KHR_reusable_sync,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        888 /* "eglCreateSyncKHR" */,
        888 /* "eglCreateSyncKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 888 /* "eglCreateSyncKHR" */,
                                providers, entrypoints);
}

static PFNEGLCREATEWINDOWSURFACEPROC
epoxy_eglCreateWindowSurface_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 905 /* eglCreateWindowSurface */);
}

static PFNEGLDEBUGMESSAGECONTROLKHRPROC
epoxy_eglDebugMessageControlKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_debug, 928 /* eglDebugMessageControlKHR */);
}

static PFNEGLDESTROYCONTEXTPROC
epoxy_eglDestroyContext_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 954 /* eglDestroyContext */);
}

static PFNEGLDESTROYIMAGEPROC
epoxy_eglDestroyImage_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_15,
        PROVIDER_EGL_KHR_image,
        PROVIDER_EGL_KHR_image_base,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        972 /* "eglDestroyImage" */,
        988 /* "eglDestroyImageKHR" */,
        988 /* "eglDestroyImageKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 972 /* "eglDestroyImage" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYIMAGEKHRPROC
epoxy_eglDestroyImageKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_KHR_image,
        PROVIDER_EGL_KHR_image_base,
        PROVIDER_EGL_15,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        988 /* "eglDestroyImageKHR" */,
        988 /* "eglDestroyImageKHR" */,
        972 /* "eglDestroyImage" */,
    };
    return egl_provider_resolver(entrypoint_strings + 988 /* "eglDestroyImageKHR" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYSTREAMKHRPROC
epoxy_eglDestroyStreamKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream, 1007 /* eglDestroyStreamKHR */);
}

static PFNEGLDESTROYSURFACEPROC
epoxy_eglDestroySurface_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1027 /* eglDestroySurface */);
}

static PFNEGLDESTROYSYNCPROC
epoxy_eglDestroySync_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_15,
        PROVIDER_EGL_KHR_fence_sync,
        PROVIDER_EGL_KHR_reusable_sync,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        1045 /* "eglDestroySync" */,
        1060 /* "eglDestroySyncKHR" */,
        1060 /* "eglDestroySyncKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1045 /* "eglDestroySync" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYSYNCKHRPROC
epoxy_eglDestroySyncKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_KHR_fence_sync,
        PROVIDER_EGL_KHR_reusable_sync,
        PROVIDER_EGL_15,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        1060 /* "eglDestroySyncKHR" */,
        1060 /* "eglDestroySyncKHR" */,
        1045 /* "eglDestroySync" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1060 /* "eglDestroySyncKHR" */,
                                providers, entrypoints);
}

static PFNEGLDESTROYSYNCNVPROC
epoxy_eglDestroySyncNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_sync, 1078 /* eglDestroySyncNV */);
}

static PFNEGLDUPNATIVEFENCEFDANDROIDPROC
epoxy_eglDupNativeFenceFDANDROID_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANDROID_native_fence_sync, 1095 /* eglDupNativeFenceFDANDROID */);
}

static PFNEGLEXPORTDMABUFIMAGEMESAPROC
epoxy_eglExportDMABUFImageMESA_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_MESA_image_dma_buf_export, 1122 /* eglExportDMABUFImageMESA */);
}

static PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC
epoxy_eglExportDMABUFImageQueryMESA_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_MESA_image_dma_buf_export, 1147 /* eglExportDMABUFImageQueryMESA */);
}

static PFNEGLEXPORTDRMIMAGEMESAPROC
epoxy_eglExportDRMImageMESA_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_MESA_drm_image, 1177 /* eglExportDRMImageMESA */);
}

static PFNEGLFENCENVPROC
epoxy_eglFenceNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_sync, 1199 /* eglFenceNV */);
}

static PFNEGLGETCOMPOSITORTIMINGANDROIDPROC
epoxy_eglGetCompositorTimingANDROID_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANDROID_get_frame_timestamps, 1210 /* eglGetCompositorTimingANDROID */);
}

static PFNEGLGETCOMPOSITORTIMINGSUPPORTEDANDROIDPROC
epoxy_eglGetCompositorTimingSupportedANDROID_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANDROID_get_frame_timestamps, 1240 /* eglGetCompositorTimingSupportedANDROID */);
}

static PFNEGLGETCONFIGATTRIBPROC
epoxy_eglGetConfigAttrib_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1279 /* eglGetConfigAttrib */);
}

static PFNEGLGETCONFIGSPROC
epoxy_eglGetConfigs_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1298 /* eglGetConfigs */);
}

static PFNEGLGETCURRENTCONTEXTPROC
epoxy_eglGetCurrentContext_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_14, 1312 /* eglGetCurrentContext */);
}

static PFNEGLGETCURRENTDISPLAYPROC
epoxy_eglGetCurrentDisplay_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1333 /* eglGetCurrentDisplay */);
}

static PFNEGLGETCURRENTSURFACEPROC
epoxy_eglGetCurrentSurface_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1354 /* eglGetCurrentSurface */);
}

static PFNEGLGETDISPLAYPROC
epoxy_eglGetDisplay_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1375 /* eglGetDisplay */);
}

static PFNEGLGETDISPLAYDRIVERCONFIGPROC
epoxy_eglGetDisplayDriverConfig_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_MESA_query_driver, 1389 /* eglGetDisplayDriverConfig */);
}

static PFNEGLGETDISPLAYDRIVERNAMEPROC
epoxy_eglGetDisplayDriverName_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_MESA_query_driver, 1415 /* eglGetDisplayDriverName */);
}

static PFNEGLGETERRORPROC
epoxy_eglGetError_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1439 /* eglGetError */);
}

static PFNEGLGETFRAMETIMESTAMPSUPPORTEDANDROIDPROC
epoxy_eglGetFrameTimestampSupportedANDROID_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANDROID_get_frame_timestamps, 1451 /* eglGetFrameTimestampSupportedANDROID */);
}

static PFNEGLGETFRAMETIMESTAMPSANDROIDPROC
epoxy_eglGetFrameTimestampsANDROID_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANDROID_get_frame_timestamps, 1488 /* eglGetFrameTimestampsANDROID */);
}

static PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC
epoxy_eglGetNativeClientBufferANDROID_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANDROID_get_native_client_buffer, 1517 /* eglGetNativeClientBufferANDROID */);
}

static PFNEGLGETNEXTFRAMEIDANDROIDPROC
epoxy_eglGetNextFrameIdANDROID_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANDROID_get_frame_timestamps, 1549 /* eglGetNextFrameIdANDROID */);
}

static PFNEGLGETOUTPUTLAYERSEXTPROC
epoxy_eglGetOutputLayersEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_output_base, 1574 /* eglGetOutputLayersEXT */);
}

static PFNEGLGETOUTPUTPORTSEXTPROC
epoxy_eglGetOutputPortsEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_output_base, 1596 /* eglGetOutputPortsEXT */);
}

static PFNEGLGETPLATFORMDISPLAYPROC
epoxy_eglGetPlatformDisplay_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_15, 1617 /* eglGetPlatformDisplay */);
}

static PFNEGLGETPLATFORMDISPLAYEXTPROC
epoxy_eglGetPlatformDisplayEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_platform_base, 1639 /* eglGetPlatformDisplayEXT */);
}

static PFNEGLGETPROCADDRESSPROC
epoxy_eglGetProcAddress_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1664 /* eglGetProcAddress */);
}

static PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC
epoxy_eglGetStreamFileDescriptorKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_cross_process_fd, 1682 /* eglGetStreamFileDescriptorKHR */);
}

static PFNEGLGETSYNCATTRIBPROC
epoxy_eglGetSyncAttrib_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_15, 1712 /* eglGetSyncAttrib */);
}

static PFNEGLGETSYNCATTRIBKHRPROC
epoxy_eglGetSyncAttribKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_KHR_fence_sync,
        PROVIDER_EGL_KHR_reusable_sync,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        1729 /* "eglGetSyncAttribKHR" */,
        1729 /* "eglGetSyncAttribKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1729 /* "eglGetSyncAttribKHR" */,
                                providers, entrypoints);
}

static PFNEGLGETSYNCATTRIBNVPROC
epoxy_eglGetSyncAttribNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_sync, 1749 /* eglGetSyncAttribNV */);
}

static PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC
epoxy_eglGetSystemTimeFrequencyNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_system_time, 1768 /* eglGetSystemTimeFrequencyNV */);
}

static PFNEGLGETSYSTEMTIMENVPROC
epoxy_eglGetSystemTimeNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_system_time, 1796 /* eglGetSystemTimeNV */);
}

static PFNEGLINITIALIZEPROC
epoxy_eglInitialize_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1815 /* eglInitialize */);
}

static PFNEGLLABELOBJECTKHRPROC
epoxy_eglLabelObjectKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_debug, 1829 /* eglLabelObjectKHR */);
}

static PFNEGLLOCKSURFACEKHRPROC
epoxy_eglLockSurfaceKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_KHR_lock_surface,
        PROVIDER_EGL_KHR_lock_surface3,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        1847 /* "eglLockSurfaceKHR" */,
        1847 /* "eglLockSurfaceKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 1847 /* "eglLockSurfaceKHR" */,
                                providers, entrypoints);
}

static PFNEGLMAKECURRENTPROC
epoxy_eglMakeCurrent_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1865 /* eglMakeCurrent */);
}

static PFNEGLOUTPUTLAYERATTRIBEXTPROC
epoxy_eglOutputLayerAttribEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_output_base, 1880 /* eglOutputLayerAttribEXT */);
}

static PFNEGLOUTPUTPORTATTRIBEXTPROC
epoxy_eglOutputPortAttribEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_output_base, 1904 /* eglOutputPortAttribEXT */);
}

static PFNEGLPOSTSUBBUFFERNVPROC
epoxy_eglPostSubBufferNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_post_sub_buffer, 1927 /* eglPostSubBufferNV */);
}

static PFNEGLPRESENTATIONTIMEANDROIDPROC
epoxy_eglPresentationTimeANDROID_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANDROID_presentation_time, 1946 /* eglPresentationTimeANDROID */);
}

static PFNEGLQUERYAPIPROC
epoxy_eglQueryAPI_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_12, 1973 /* eglQueryAPI */);
}

static PFNEGLQUERYCONTEXTPROC
epoxy_eglQueryContext_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 1985 /* eglQueryContext */);
}

static PFNEGLQUERYDEBUGKHRPROC
epoxy_eglQueryDebugKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_debug, 2001 /* eglQueryDebugKHR */);
}

static PFNEGLQUERYDEVICEATTRIBEXTPROC
epoxy_eglQueryDeviceAttribEXT_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_EXT_device_base,
        PROVIDER_EGL_EXT_device_query,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        2018 /* "eglQueryDeviceAttribEXT" */,
        2018 /* "eglQueryDeviceAttribEXT" */,
    };
    return egl_provider_resolver(entrypoint_strings + 2018 /* "eglQueryDeviceAttribEXT" */,
                                providers, entrypoints);
}

static PFNEGLQUERYDEVICESTRINGEXTPROC
epoxy_eglQueryDeviceStringEXT_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_EXT_device_base,
        PROVIDER_EGL_EXT_device_query,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        2042 /* "eglQueryDeviceStringEXT" */,
        2042 /* "eglQueryDeviceStringEXT" */,
    };
    return egl_provider_resolver(entrypoint_strings + 2042 /* "eglQueryDeviceStringEXT" */,
                                providers, entrypoints);
}

static PFNEGLQUERYDEVICESEXTPROC
epoxy_eglQueryDevicesEXT_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_EXT_device_base,
        PROVIDER_EGL_EXT_device_enumeration,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        2066 /* "eglQueryDevicesEXT" */,
        2066 /* "eglQueryDevicesEXT" */,
    };
    return egl_provider_resolver(entrypoint_strings + 2066 /* "eglQueryDevicesEXT" */,
                                providers, entrypoints);
}

static PFNEGLQUERYDISPLAYATTRIBEXTPROC
epoxy_eglQueryDisplayAttribEXT_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_EXT_device_base,
        PROVIDER_EGL_EXT_device_query,
        PROVIDER_EGL_KHR_display_reference,
        PROVIDER_EGL_NV_stream_metadata,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        2085 /* "eglQueryDisplayAttribEXT" */,
        2085 /* "eglQueryDisplayAttribEXT" */,
        2110 /* "eglQueryDisplayAttribKHR" */,
        2135 /* "eglQueryDisplayAttribNV" */,
    };
    return egl_provider_resolver(entrypoint_strings + 2085 /* "eglQueryDisplayAttribEXT" */,
                                providers, entrypoints);
}

static PFNEGLQUERYDISPLAYATTRIBKHRPROC
epoxy_eglQueryDisplayAttribKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_KHR_display_reference,
        PROVIDER_EGL_EXT_device_base,
        PROVIDER_EGL_EXT_device_query,
        PROVIDER_EGL_NV_stream_metadata,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        2110 /* "eglQueryDisplayAttribKHR" */,
        2085 /* "eglQueryDisplayAttribEXT" */,
        2085 /* "eglQueryDisplayAttribEXT" */,
        2135 /* "eglQueryDisplayAttribNV" */,
    };
    return egl_provider_resolver(entrypoint_strings + 2110 /* "eglQueryDisplayAttribKHR" */,
                                providers, entrypoints);
}

static PFNEGLQUERYDISPLAYATTRIBNVPROC
epoxy_eglQueryDisplayAttribNV_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_NV_stream_metadata,
        PROVIDER_EGL_EXT_device_base,
        PROVIDER_EGL_EXT_device_query,
        PROVIDER_EGL_KHR_display_reference,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        2135 /* "eglQueryDisplayAttribNV" */,
        2085 /* "eglQueryDisplayAttribEXT" */,
        2085 /* "eglQueryDisplayAttribEXT" */,
        2110 /* "eglQueryDisplayAttribKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 2135 /* "eglQueryDisplayAttribNV" */,
                                providers, entrypoints);
}

static PFNEGLQUERYDMABUFFORMATSEXTPROC
epoxy_eglQueryDmaBufFormatsEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_image_dma_buf_import_modifiers, 2159 /* eglQueryDmaBufFormatsEXT */);
}

static PFNEGLQUERYDMABUFMODIFIERSEXTPROC
epoxy_eglQueryDmaBufModifiersEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_image_dma_buf_import_modifiers, 2184 /* eglQueryDmaBufModifiersEXT */);
}

static PFNEGLQUERYNATIVEDISPLAYNVPROC
epoxy_eglQueryNativeDisplayNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_native_query, 2211 /* eglQueryNativeDisplayNV */);
}

static PFNEGLQUERYNATIVEPIXMAPNVPROC
epoxy_eglQueryNativePixmapNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_native_query, 2235 /* eglQueryNativePixmapNV */);
}

static PFNEGLQUERYNATIVEWINDOWNVPROC
epoxy_eglQueryNativeWindowNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_native_query, 2258 /* eglQueryNativeWindowNV */);
}

static PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC
epoxy_eglQueryOutputLayerAttribEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_output_base, 2281 /* eglQueryOutputLayerAttribEXT */);
}

static PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC
epoxy_eglQueryOutputLayerStringEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_output_base, 2310 /* eglQueryOutputLayerStringEXT */);
}

static PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC
epoxy_eglQueryOutputPortAttribEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_output_base, 2339 /* eglQueryOutputPortAttribEXT */);
}

static PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC
epoxy_eglQueryOutputPortStringEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_output_base, 2367 /* eglQueryOutputPortStringEXT */);
}

static PFNEGLQUERYSTREAMATTRIBKHRPROC
epoxy_eglQueryStreamAttribKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_attrib, 2395 /* eglQueryStreamAttribKHR */);
}

static PFNEGLQUERYSTREAMKHRPROC
epoxy_eglQueryStreamKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream, 2419 /* eglQueryStreamKHR */);
}

static PFNEGLQUERYSTREAMMETADATANVPROC
epoxy_eglQueryStreamMetadataNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_stream_metadata, 2437 /* eglQueryStreamMetadataNV */);
}

static PFNEGLQUERYSTREAMTIMEKHRPROC
epoxy_eglQueryStreamTimeKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_fifo, 2462 /* eglQueryStreamTimeKHR */);
}

static PFNEGLQUERYSTREAMU64KHRPROC
epoxy_eglQueryStreamu64KHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream, 2484 /* eglQueryStreamu64KHR */);
}

static PFNEGLQUERYSTRINGPROC
epoxy_eglQueryString_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 2505 /* eglQueryString */);
}

static PFNEGLQUERYSURFACEPROC
epoxy_eglQuerySurface_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 2520 /* eglQuerySurface */);
}

static PFNEGLQUERYSURFACE64KHRPROC
epoxy_eglQuerySurface64KHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_lock_surface3, 2536 /* eglQuerySurface64KHR */);
}

static PFNEGLQUERYSURFACEPOINTERANGLEPROC
epoxy_eglQuerySurfacePointerANGLE_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANGLE_query_surface_pointer, 2557 /* eglQuerySurfacePointerANGLE */);
}

static PFNEGLRELEASETEXIMAGEPROC
epoxy_eglReleaseTexImage_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_11, 2585 /* eglReleaseTexImage */);
}

static PFNEGLRELEASETHREADPROC
epoxy_eglReleaseThread_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_12, 2604 /* eglReleaseThread */);
}

static PFNEGLRESETSTREAMNVPROC
epoxy_eglResetStreamNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_stream_reset, 2621 /* eglResetStreamNV */);
}

static PFNEGLSETBLOBCACHEFUNCSANDROIDPROC
epoxy_eglSetBlobCacheFuncsANDROID_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_ANDROID_blob_cache, 2638 /* eglSetBlobCacheFuncsANDROID */);
}

static PFNEGLSETDAMAGEREGIONKHRPROC
epoxy_eglSetDamageRegionKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_partial_update, 2666 /* eglSetDamageRegionKHR */);
}

static PFNEGLSETSTREAMATTRIBKHRPROC
epoxy_eglSetStreamAttribKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_attrib, 2688 /* eglSetStreamAttribKHR */);
}

static PFNEGLSETSTREAMMETADATANVPROC
epoxy_eglSetStreamMetadataNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_stream_metadata, 2710 /* eglSetStreamMetadataNV */);
}

static PFNEGLSIGNALSYNCKHRPROC
epoxy_eglSignalSyncKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_reusable_sync, 2733 /* eglSignalSyncKHR */);
}

static PFNEGLSIGNALSYNCNVPROC
epoxy_eglSignalSyncNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_sync, 2750 /* eglSignalSyncNV */);
}

static PFNEGLSTREAMATTRIBKHRPROC
epoxy_eglStreamAttribKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream, 2766 /* eglStreamAttribKHR */);
}

static PFNEGLSTREAMCONSUMERACQUIREATTRIBKHRPROC
epoxy_eglStreamConsumerAcquireAttribKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_attrib, 2785 /* eglStreamConsumerAcquireAttribKHR */);
}

static PFNEGLSTREAMCONSUMERACQUIREKHRPROC
epoxy_eglStreamConsumerAcquireKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_consumer_gltexture, 2819 /* eglStreamConsumerAcquireKHR */);
}

static PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALATTRIBSNVPROC
epoxy_eglStreamConsumerGLTextureExternalAttribsNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_stream_consumer_gltexture_yuv, 2847 /* eglStreamConsumerGLTextureExternalAttribsNV */);
}

static PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC
epoxy_eglStreamConsumerGLTextureExternalKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_consumer_gltexture, 2891 /* eglStreamConsumerGLTextureExternalKHR */);
}

static PFNEGLSTREAMCONSUMEROUTPUTEXTPROC
epoxy_eglStreamConsumerOutputEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_stream_consumer_egloutput, 2929 /* eglStreamConsumerOutputEXT */);
}

static PFNEGLSTREAMCONSUMERRELEASEATTRIBKHRPROC
epoxy_eglStreamConsumerReleaseAttribKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_attrib, 2956 /* eglStreamConsumerReleaseAttribKHR */);
}

static PFNEGLSTREAMCONSUMERRELEASEKHRPROC
epoxy_eglStreamConsumerReleaseKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_stream_consumer_gltexture, 2990 /* eglStreamConsumerReleaseKHR */);
}

static PFNEGLSTREAMFLUSHNVPROC
epoxy_eglStreamFlushNV_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NV_stream_flush, 3018 /* eglStreamFlushNV */);
}

static PFNEGLSURFACEATTRIBPROC
epoxy_eglSurfaceAttrib_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_11, 3035 /* eglSurfaceAttrib */);
}

static PFNEGLSWAPBUFFERSPROC
epoxy_eglSwapBuffers_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 3052 /* eglSwapBuffers */);
}

static PFNEGLSWAPBUFFERSREGION2NOKPROC
epoxy_eglSwapBuffersRegion2NOK_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NOK_swap_region2, 3067 /* eglSwapBuffersRegion2NOK */);
}

static PFNEGLSWAPBUFFERSREGIONNOKPROC
epoxy_eglSwapBuffersRegionNOK_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_NOK_swap_region, 3092 /* eglSwapBuffersRegionNOK */);
}

static PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC
epoxy_eglSwapBuffersWithDamageEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_swap_buffers_with_damage, 3116 /* eglSwapBuffersWithDamageEXT */);
}

static PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC
epoxy_eglSwapBuffersWithDamageKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_swap_buffers_with_damage, 3144 /* eglSwapBuffersWithDamageKHR */);
}

static PFNEGLSWAPINTERVALPROC
epoxy_eglSwapInterval_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_11, 3172 /* eglSwapInterval */);
}

static PFNEGLTERMINATEPROC
epoxy_eglTerminate_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 3188 /* eglTerminate */);
}

static PFNEGLUNLOCKSURFACEKHRPROC
epoxy_eglUnlockSurfaceKHR_resolver(void)
{
    static const enum egl_provider providers[] = {
        PROVIDER_EGL_KHR_lock_surface,
        PROVIDER_EGL_KHR_lock_surface3,
        egl_provider_terminator
    };
    static const uint32_t entrypoints[] = {
        3201 /* "eglUnlockSurfaceKHR" */,
        3201 /* "eglUnlockSurfaceKHR" */,
    };
    return egl_provider_resolver(entrypoint_strings + 3201 /* "eglUnlockSurfaceKHR" */,
                                providers, entrypoints);
}

static PFNEGLUNSIGNALSYNCEXTPROC
epoxy_eglUnsignalSyncEXT_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_EXT_sync_reuse, 3221 /* eglUnsignalSyncEXT */);
}

static PFNEGLWAITCLIENTPROC
epoxy_eglWaitClient_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_12, 3240 /* eglWaitClient */);
}

static PFNEGLWAITGLPROC
epoxy_eglWaitGL_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 3254 /* eglWaitGL */);
}

static PFNEGLWAITNATIVEPROC
epoxy_eglWaitNative_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_10, 3264 /* eglWaitNative */);
}

static PFNEGLWAITSYNCPROC
epoxy_eglWaitSync_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_15, 3278 /* eglWaitSync */);
}

static PFNEGLWAITSYNCKHRPROC
epoxy_eglWaitSyncKHR_resolver(void)
{
    return egl_single_resolver(PROVIDER_EGL_KHR_wait_sync, 3290 /* eglWaitSyncKHR */);
}

GEN_THUNKS_RET(EGLBoolean, eglBindAPI, (EGLenum api), (api))
GEN_THUNKS_RET(EGLBoolean, eglBindTexImage, (EGLDisplay dpy, EGLSurface surface, EGLint buffer), (dpy, surface, buffer))
GEN_THUNKS_RET(EGLBoolean, eglChooseConfig, (EGLDisplay dpy, const EGLint * attrib_list, EGLConfig * configs, EGLint config_size, EGLint * num_config), (dpy, attrib_list, configs, config_size, num_config))
GEN_THUNKS_RET(EGLBoolean, eglClientSignalSyncEXT, (EGLDisplay dpy, EGLSync sync, const EGLAttrib * attrib_list), (dpy, sync, attrib_list))
GEN_THUNKS_RET(EGLint, eglClientWaitSync, (EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout), (dpy, sync, flags, timeout))
GEN_THUNKS_RET(EGLint, eglClientWaitSyncKHR, (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout), (dpy, sync, flags, timeout))
GEN_THUNKS_RET(EGLint, eglClientWaitSyncNV, (EGLSyncNV sync, EGLint flags, EGLTimeNV timeout), (sync, flags, timeout))
GEN_THUNKS_RET(EGLBoolean, eglCompositorBindTexWindowEXT, (EGLint external_win_id), (external_win_id))
GEN_THUNKS_RET(EGLBoolean, eglCompositorSetContextAttributesEXT, (EGLint external_ref_id, const EGLint * context_attributes, EGLint num_entries), (external_ref_id, context_attributes, num_entries))
GEN_THUNKS_RET(EGLBoolean, eglCompositorSetContextListEXT, (const EGLint * external_ref_ids, EGLint num_entries), (external_ref_ids, num_entries))
GEN_THUNKS_RET(EGLBoolean, eglCompositorSetSizeEXT, (EGLint external_win_id, EGLint width, EGLint height), (external_win_id, width, height))
GEN_THUNKS_RET(EGLBoolean, eglCompositorSetWindowAttributesEXT, (EGLint external_win_id, const EGLint * window_attributes, EGLint num_entries), (external_win_id, window_attributes, num_entries))
GEN_THUNKS_RET(EGLBoolean, eglCompositorSetWindowListEXT, (EGLint external_ref_id, const EGLint * external_win_ids, EGLint num_entries), (external_ref_id, external_win_ids, num_entries))
GEN_THUNKS_RET(EGLBoolean, eglCompositorSwapPolicyEXT, (EGLint external_win_id, EGLint policy), (external_win_id, policy))
GEN_THUNKS_RET(EGLBoolean, eglCopyBuffers, (EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target), (dpy, surface, target))
GEN_THUNKS_RET(EGLContext, eglCreateContext, (EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint * attrib_list), (dpy, config, share_context, attrib_list))
GEN_THUNKS_RET(EGLImageKHR, eglCreateDRMImageMESA, (EGLDisplay dpy, const EGLint * attrib_list), (dpy, attrib_list))
GEN_THUNKS_RET(EGLSyncNV, eglCreateFenceSyncNV, (EGLDisplay dpy, EGLenum condition, const EGLint * attrib_list), (dpy, condition, attrib_list))
GEN_THUNKS_RET(EGLImage, eglCreateImage, (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib * attrib_list), (dpy, ctx, target, buffer, attrib_list))
GEN_THUNKS_RET(EGLImageKHR, eglCreateImageKHR, (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint * attrib_list), (dpy, ctx, target, buffer, attrib_list))
GEN_THUNKS_RET(EGLClientBuffer, eglCreateNativeClientBufferANDROID, (const EGLint * attrib_list), (attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePbufferFromClientBuffer, (EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint * attrib_list), (dpy, buftype, buffer, config, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePbufferSurface, (EGLDisplay dpy, EGLConfig config, const EGLint * attrib_list), (dpy, config, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePixmapSurface, (EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint * attrib_list), (dpy, config, pixmap, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePixmapSurfaceHI, (EGLDisplay dpy, EGLConfig config, struct EGLClientPixmapHI * pixmap), (dpy, config, pixmap))
GEN_THUNKS_RET(EGLSurface, eglCreatePlatformPixmapSurface, (EGLDisplay dpy, EGLConfig config, void * native_pixmap, const EGLAttrib * attrib_list), (dpy, config, native_pixmap, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePlatformPixmapSurfaceEXT, (EGLDisplay dpy, EGLConfig config, void * native_pixmap, const EGLint * attrib_list), (dpy, config, native_pixmap, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePlatformWindowSurface, (EGLDisplay dpy, EGLConfig config, void * native_window, const EGLAttrib * attrib_list), (dpy, config, native_window, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreatePlatformWindowSurfaceEXT, (EGLDisplay dpy, EGLConfig config, void * native_window, const EGLint * attrib_list), (dpy, config, native_window, attrib_list))
GEN_THUNKS_RET(EGLStreamKHR, eglCreateStreamAttribKHR, (EGLDisplay dpy, const EGLAttrib * attrib_list), (dpy, attrib_list))
GEN_THUNKS_RET(EGLStreamKHR, eglCreateStreamFromFileDescriptorKHR, (EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor), (dpy, file_descriptor))
GEN_THUNKS_RET(EGLStreamKHR, eglCreateStreamKHR, (EGLDisplay dpy, const EGLint * attrib_list), (dpy, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreateStreamProducerSurfaceKHR, (EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint * attrib_list), (dpy, config, stream, attrib_list))
GEN_THUNKS_RET(EGLSyncKHR, eglCreateStreamSyncNV, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum type, const EGLint * attrib_list), (dpy, stream, type, attrib_list))
GEN_THUNKS_RET(EGLSync, eglCreateSync, (EGLDisplay dpy, EGLenum type, const EGLAttrib * attrib_list), (dpy, type, attrib_list))
GEN_THUNKS_RET(EGLSyncKHR, eglCreateSync64KHR, (EGLDisplay dpy, EGLenum type, const EGLAttribKHR * attrib_list), (dpy, type, attrib_list))
GEN_THUNKS_RET(EGLSyncKHR, eglCreateSyncKHR, (EGLDisplay dpy, EGLenum type, const EGLint * attrib_list), (dpy, type, attrib_list))
GEN_THUNKS_RET(EGLSurface, eglCreateWindowSurface, (EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint * attrib_list), (dpy, config, win, attrib_list))
GEN_THUNKS_RET(EGLint, eglDebugMessageControlKHR, (EGLDEBUGPROCKHR callback, const EGLAttrib * attrib_list), (callback, attrib_list))
GEN_THUNKS_RET(EGLBoolean, eglDestroyContext, (EGLDisplay dpy, EGLContext ctx), (dpy, ctx))
GEN_THUNKS_RET(EGLBoolean, eglDestroyImage, (EGLDisplay dpy, EGLImage image), (dpy, image))
GEN_THUNKS_RET(EGLBoolean, eglDestroyImageKHR, (EGLDisplay dpy, EGLImageKHR image), (dpy, image))
GEN_THUNKS_RET(EGLBoolean, eglDestroyStreamKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglDestroySurface, (EGLDisplay dpy, EGLSurface surface), (dpy, surface))
GEN_THUNKS_RET(EGLBoolean, eglDestroySync, (EGLDisplay dpy, EGLSync sync), (dpy, sync))
GEN_THUNKS_RET(EGLBoolean, eglDestroySyncKHR, (EGLDisplay dpy, EGLSyncKHR sync), (dpy, sync))
GEN_THUNKS_RET(EGLBoolean, eglDestroySyncNV, (EGLSyncNV sync), (sync))
GEN_THUNKS_RET(EGLint, eglDupNativeFenceFDANDROID, (EGLDisplay dpy, EGLSyncKHR sync), (dpy, sync))
GEN_THUNKS_RET(EGLBoolean, eglExportDMABUFImageMESA, (EGLDisplay dpy, EGLImageKHR image, int * fds, EGLint * strides, EGLint * offsets), (dpy, image, fds, strides, offsets))
GEN_THUNKS_RET(EGLBoolean, eglExportDMABUFImageQueryMESA, (EGLDisplay dpy, EGLImageKHR image, int * fourcc, int * num_planes, EGLuint64KHR * modifiers), (dpy, image, fourcc, num_planes, modifiers))
GEN_THUNKS_RET(EGLBoolean, eglExportDRMImageMESA, (EGLDisplay dpy, EGLImageKHR image, EGLint * name, EGLint * handle, EGLint * stride), (dpy, image, name, handle, stride))
GEN_THUNKS_RET(EGLBoolean, eglFenceNV, (EGLSyncNV sync), (sync))
GEN_THUNKS_RET(EGLBoolean, eglGetCompositorTimingANDROID, (EGLDisplay dpy, EGLSurface surface, EGLint numTimestamps, const EGLint * names, EGLnsecsANDROID * values), (dpy, surface, numTimestamps, names, values))
GEN_THUNKS_RET(EGLBoolean, eglGetCompositorTimingSupportedANDROID, (EGLDisplay dpy, EGLSurface surface, EGLint name), (dpy, surface, name))
GEN_THUNKS_RET(EGLBoolean, eglGetConfigAttrib, (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint * value), (dpy, config, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglGetConfigs, (EGLDisplay dpy, EGLConfig * configs, EGLint config_size, EGLint * num_config), (dpy, configs, config_size, num_config))
GEN_THUNKS_RET(EGLContext, eglGetCurrentContext, (void), ())
GEN_THUNKS_RET(EGLDisplay, eglGetCurrentDisplay, (void), ())
GEN_THUNKS_RET(EGLSurface, eglGetCurrentSurface, (EGLint readdraw), (readdraw))
GEN_THUNKS_RET(EGLDisplay, eglGetDisplay, (EGLNativeDisplayType display_id), (display_id))
GEN_THUNKS_RET(char *, eglGetDisplayDriverConfig, (EGLDisplay dpy), (dpy))
GEN_THUNKS_RET(const char *, eglGetDisplayDriverName, (EGLDisplay dpy), (dpy))
GEN_THUNKS_RET(EGLint, eglGetError, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglGetFrameTimestampSupportedANDROID, (EGLDisplay dpy, EGLSurface surface, EGLint timestamp), (dpy, surface, timestamp))
GEN_THUNKS_RET(EGLBoolean, eglGetFrameTimestampsANDROID, (EGLDisplay dpy, EGLSurface surface, EGLuint64KHR frameId, EGLint numTimestamps, const EGLint * timestamps, EGLnsecsANDROID * values), (dpy, surface, frameId, numTimestamps, timestamps, values))
GEN_THUNKS_RET(EGLClientBuffer, eglGetNativeClientBufferANDROID, (const struct AHardwareBuffer * buffer), (buffer))
GEN_THUNKS_RET(EGLBoolean, eglGetNextFrameIdANDROID, (EGLDisplay dpy, EGLSurface surface, EGLuint64KHR * frameId), (dpy, surface, frameId))
GEN_THUNKS_RET(EGLBoolean, eglGetOutputLayersEXT, (EGLDisplay dpy, const EGLAttrib * attrib_list, EGLOutputLayerEXT * layers, EGLint max_layers, EGLint * num_layers), (dpy, attrib_list, layers, max_layers, num_layers))
GEN_THUNKS_RET(EGLBoolean, eglGetOutputPortsEXT, (EGLDisplay dpy, const EGLAttrib * attrib_list, EGLOutputPortEXT * ports, EGLint max_ports, EGLint * num_ports), (dpy, attrib_list, ports, max_ports, num_ports))
GEN_THUNKS_RET(EGLDisplay, eglGetPlatformDisplay, (EGLenum platform, void * native_display, const EGLAttrib * attrib_list), (platform, native_display, attrib_list))
GEN_THUNKS_RET(EGLDisplay, eglGetPlatformDisplayEXT, (EGLenum platform, void * native_display, const EGLint * attrib_list), (platform, native_display, attrib_list))
GEN_THUNKS_RET(__eglMustCastToProperFunctionPointerType, eglGetProcAddress, (const char * procname), (procname))
GEN_THUNKS_RET(EGLNativeFileDescriptorKHR, eglGetStreamFileDescriptorKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglGetSyncAttrib, (EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib * value), (dpy, sync, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglGetSyncAttribKHR, (EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint * value), (dpy, sync, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglGetSyncAttribNV, (EGLSyncNV sync, EGLint attribute, EGLint * value), (sync, attribute, value))
GEN_THUNKS_RET(EGLuint64NV, eglGetSystemTimeFrequencyNV, (void), ())
GEN_THUNKS_RET(EGLuint64NV, eglGetSystemTimeNV, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglInitialize, (EGLDisplay dpy, EGLint * major, EGLint * minor), (dpy, major, minor))
GEN_THUNKS_RET(EGLint, eglLabelObjectKHR, (EGLDisplay display, EGLenum objectType, EGLObjectKHR object, EGLLabelKHR label), (display, objectType, object, label))
GEN_THUNKS_RET(EGLBoolean, eglLockSurfaceKHR, (EGLDisplay dpy, EGLSurface surface, const EGLint * attrib_list), (dpy, surface, attrib_list))
GEN_THUNKS_RET(EGLBoolean, eglMakeCurrent, (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx), (dpy, draw, read, ctx))
GEN_THUNKS_RET(EGLBoolean, eglOutputLayerAttribEXT, (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib value), (dpy, layer, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglOutputPortAttribEXT, (EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib value), (dpy, port, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglPostSubBufferNV, (EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height), (dpy, surface, x, y, width, height))
GEN_THUNKS_RET(EGLBoolean, eglPresentationTimeANDROID, (EGLDisplay dpy, EGLSurface surface, EGLnsecsANDROID time), (dpy, surface, time))
GEN_THUNKS_RET(EGLenum, eglQueryAPI, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglQueryContext, (EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint * value), (dpy, ctx, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryDebugKHR, (EGLint attribute, EGLAttrib * value), (attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryDeviceAttribEXT, (EGLDeviceEXT device, EGLint attribute, EGLAttrib * value), (device, attribute, value))
GEN_THUNKS_RET(const char *, eglQueryDeviceStringEXT, (EGLDeviceEXT device, EGLint name), (device, name))
GEN_THUNKS_RET(EGLBoolean, eglQueryDevicesEXT, (EGLint max_devices, EGLDeviceEXT * devices, EGLint * num_devices), (max_devices, devices, num_devices))
GEN_THUNKS_RET(EGLBoolean, eglQueryDisplayAttribEXT, (EGLDisplay dpy, EGLint attribute, EGLAttrib * value), (dpy, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryDisplayAttribKHR, (EGLDisplay dpy, EGLint name, EGLAttrib * value), (dpy, name, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryDisplayAttribNV, (EGLDisplay dpy, EGLint attribute, EGLAttrib * value), (dpy, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryDmaBufFormatsEXT, (EGLDisplay dpy, EGLint max_formats, EGLint * formats, EGLint * num_formats), (dpy, max_formats, formats, num_formats))
GEN_THUNKS_RET(EGLBoolean, eglQueryDmaBufModifiersEXT, (EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR * modifiers, EGLBoolean * external_only, EGLint * num_modifiers), (dpy, format, max_modifiers, modifiers, external_only, num_modifiers))
GEN_THUNKS_RET(EGLBoolean, eglQueryNativeDisplayNV, (EGLDisplay dpy, EGLNativeDisplayType * display_id), (dpy, display_id))
GEN_THUNKS_RET(EGLBoolean, eglQueryNativePixmapNV, (EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType * pixmap), (dpy, surf, pixmap))
GEN_THUNKS_RET(EGLBoolean, eglQueryNativeWindowNV, (EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType * window), (dpy, surf, window))
GEN_THUNKS_RET(EGLBoolean, eglQueryOutputLayerAttribEXT, (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib * value), (dpy, layer, attribute, value))
GEN_THUNKS_RET(const char *, eglQueryOutputLayerStringEXT, (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint name), (dpy, layer, name))
GEN_THUNKS_RET(EGLBoolean, eglQueryOutputPortAttribEXT, (EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib * value), (dpy, port, attribute, value))
GEN_THUNKS_RET(const char *, eglQueryOutputPortStringEXT, (EGLDisplay dpy, EGLOutputPortEXT port, EGLint name), (dpy, port, name))
GEN_THUNKS_RET(EGLBoolean, eglQueryStreamAttribKHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib * value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryStreamKHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint * value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryStreamMetadataNV, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum name, EGLint n, EGLint offset, EGLint size, void * data), (dpy, stream, name, n, offset, size, data))
GEN_THUNKS_RET(EGLBoolean, eglQueryStreamTimeKHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLTimeKHR * value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQueryStreamu64KHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR * value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(const char *, eglQueryString, (EGLDisplay dpy, EGLint name), (dpy, name))
GEN_THUNKS_RET(EGLBoolean, eglQuerySurface, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint * value), (dpy, surface, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQuerySurface64KHR, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLAttribKHR * value), (dpy, surface, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglQuerySurfacePointerANGLE, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, void ** value), (dpy, surface, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglReleaseTexImage, (EGLDisplay dpy, EGLSurface surface, EGLint buffer), (dpy, surface, buffer))
GEN_THUNKS_RET(EGLBoolean, eglReleaseThread, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglResetStreamNV, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS(eglSetBlobCacheFuncsANDROID, (EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get), (dpy, set, get))
GEN_THUNKS_RET(EGLBoolean, eglSetDamageRegionKHR, (EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects), (dpy, surface, rects, n_rects))
GEN_THUNKS_RET(EGLBoolean, eglSetStreamAttribKHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglSetStreamMetadataNV, (EGLDisplay dpy, EGLStreamKHR stream, EGLint n, EGLint offset, EGLint size, const void * data), (dpy, stream, n, offset, size, data))
GEN_THUNKS_RET(EGLBoolean, eglSignalSyncKHR, (EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode), (dpy, sync, mode))
GEN_THUNKS_RET(EGLBoolean, eglSignalSyncNV, (EGLSyncNV sync, EGLenum mode), (sync, mode))
GEN_THUNKS_RET(EGLBoolean, eglStreamAttribKHR, (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value), (dpy, stream, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerAcquireAttribKHR, (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib * attrib_list), (dpy, stream, attrib_list))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerAcquireKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerGLTextureExternalAttribsNV, (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib * attrib_list), (dpy, stream, attrib_list))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerGLTextureExternalKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerOutputEXT, (EGLDisplay dpy, EGLStreamKHR stream, EGLOutputLayerEXT layer), (dpy, stream, layer))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerReleaseAttribKHR, (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib * attrib_list), (dpy, stream, attrib_list))
GEN_THUNKS_RET(EGLBoolean, eglStreamConsumerReleaseKHR, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglStreamFlushNV, (EGLDisplay dpy, EGLStreamKHR stream), (dpy, stream))
GEN_THUNKS_RET(EGLBoolean, eglSurfaceAttrib, (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value), (dpy, surface, attribute, value))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffers, (EGLDisplay dpy, EGLSurface surface), (dpy, surface))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersRegion2NOK, (EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint * rects), (dpy, surface, numRects, rects))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersRegionNOK, (EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint * rects), (dpy, surface, numRects, rects))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersWithDamageEXT, (EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects), (dpy, surface, rects, n_rects))
GEN_THUNKS_RET(EGLBoolean, eglSwapBuffersWithDamageKHR, (EGLDisplay dpy, EGLSurface surface, EGLint * rects, EGLint n_rects), (dpy, surface, rects, n_rects))
GEN_THUNKS_RET(EGLBoolean, eglSwapInterval, (EGLDisplay dpy, EGLint interval), (dpy, interval))
GEN_THUNKS_RET(EGLBoolean, eglTerminate, (EGLDisplay dpy), (dpy))
GEN_THUNKS_RET(EGLBoolean, eglUnlockSurfaceKHR, (EGLDisplay dpy, EGLSurface surface), (dpy, surface))
GEN_THUNKS_RET(EGLBoolean, eglUnsignalSyncEXT, (EGLDisplay dpy, EGLSync sync, const EGLAttrib * attrib_list), (dpy, sync, attrib_list))
GEN_THUNKS_RET(EGLBoolean, eglWaitClient, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglWaitGL, (void), ())
GEN_THUNKS_RET(EGLBoolean, eglWaitNative, (EGLint engine), (engine))
GEN_THUNKS_RET(EGLBoolean, eglWaitSync, (EGLDisplay dpy, EGLSync sync, EGLint flags), (dpy, sync, flags))
GEN_THUNKS_RET(EGLint, eglWaitSyncKHR, (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags), (dpy, sync, flags))

#if USING_DISPATCH_TABLE
static struct dispatch_table resolver_table = {
    epoxy_eglBindAPI_dispatch_table_rewrite_ptr, /* eglBindAPI */
    epoxy_eglBindTexImage_dispatch_table_rewrite_ptr, /* eglBindTexImage */
    epoxy_eglChooseConfig_dispatch_table_rewrite_ptr, /* eglChooseConfig */
    epoxy_eglClientSignalSyncEXT_dispatch_table_rewrite_ptr, /* eglClientSignalSyncEXT */
    epoxy_eglClientWaitSync_dispatch_table_rewrite_ptr, /* eglClientWaitSync */
    epoxy_eglClientWaitSyncKHR_dispatch_table_rewrite_ptr, /* eglClientWaitSyncKHR */
    epoxy_eglClientWaitSyncNV_dispatch_table_rewrite_ptr, /* eglClientWaitSyncNV */
    epoxy_eglCompositorBindTexWindowEXT_dispatch_table_rewrite_ptr, /* eglCompositorBindTexWindowEXT */
    epoxy_eglCompositorSetContextAttributesEXT_dispatch_table_rewrite_ptr, /* eglCompositorSetContextAttributesEXT */
    epoxy_eglCompositorSetContextListEXT_dispatch_table_rewrite_ptr, /* eglCompositorSetContextListEXT */
    epoxy_eglCompositorSetSizeEXT_dispatch_table_rewrite_ptr, /* eglCompositorSetSizeEXT */
    epoxy_eglCompositorSetWindowAttributesEXT_dispatch_table_rewrite_ptr, /* eglCompositorSetWindowAttributesEXT */
    epoxy_eglCompositorSetWindowListEXT_dispatch_table_rewrite_ptr, /* eglCompositorSetWindowListEXT */
    epoxy_eglCompositorSwapPolicyEXT_dispatch_table_rewrite_ptr, /* eglCompositorSwapPolicyEXT */
    epoxy_eglCopyBuffers_dispatch_table_rewrite_ptr, /* eglCopyBuffers */
    epoxy_eglCreateContext_dispatch_table_rewrite_ptr, /* eglCreateContext */
    epoxy_eglCreateDRMImageMESA_dispatch_table_rewrite_ptr, /* eglCreateDRMImageMESA */
    epoxy_eglCreateFenceSyncNV_dispatch_table_rewrite_ptr, /* eglCreateFenceSyncNV */
    epoxy_eglCreateImage_dispatch_table_rewrite_ptr, /* eglCreateImage */
    epoxy_eglCreateImageKHR_dispatch_table_rewrite_ptr, /* eglCreateImageKHR */
    epoxy_eglCreateNativeClientBufferANDROID_dispatch_table_rewrite_ptr, /* eglCreateNativeClientBufferANDROID */
    epoxy_eglCreatePbufferFromClientBuffer_dispatch_table_rewrite_ptr, /* eglCreatePbufferFromClientBuffer */
    epoxy_eglCreatePbufferSurface_dispatch_table_rewrite_ptr, /* eglCreatePbufferSurface */
    epoxy_eglCreatePixmapSurface_dispatch_table_rewrite_ptr, /* eglCreatePixmapSurface */
    epoxy_eglCreatePixmapSurfaceHI_dispatch_table_rewrite_ptr, /* eglCreatePixmapSurfaceHI */
    epoxy_eglCreatePlatformPixmapSurface_dispatch_table_rewrite_ptr, /* eglCreatePlatformPixmapSurface */
    epoxy_eglCreatePlatformPixmapSurfaceEXT_dispatch_table_rewrite_ptr, /* eglCreatePlatformPixmapSurfaceEXT */
    epoxy_eglCreatePlatformWindowSurface_dispatch_table_rewrite_ptr, /* eglCreatePlatformWindowSurface */
    epoxy_eglCreatePlatformWindowSurfaceEXT_dispatch_table_rewrite_ptr, /* eglCreatePlatformWindowSurfaceEXT */
    epoxy_eglCreateStreamAttribKHR_dispatch_table_rewrite_ptr, /* eglCreateStreamAttribKHR */
    epoxy_eglCreateStreamFromFileDescriptorKHR_dispatch_table_rewrite_ptr, /* eglCreateStreamFromFileDescriptorKHR */
    epoxy_eglCreateStreamKHR_dispatch_table_rewrite_ptr, /* eglCreateStreamKHR */
    epoxy_eglCreateStreamProducerSurfaceKHR_dispatch_table_rewrite_ptr, /* eglCreateStreamProducerSurfaceKHR */
    epoxy_eglCreateStreamSyncNV_dispatch_table_rewrite_ptr, /* eglCreateStreamSyncNV */
    epoxy_eglCreateSync_dispatch_table_rewrite_ptr, /* eglCreateSync */
    epoxy_eglCreateSync64KHR_dispatch_table_rewrite_ptr, /* eglCreateSync64KHR */
    epoxy_eglCreateSyncKHR_dispatch_table_rewrite_ptr, /* eglCreateSyncKHR */
    epoxy_eglCreateWindowSurface_dispatch_table_rewrite_ptr, /* eglCreateWindowSurface */
    epoxy_eglDebugMessageControlKHR_dispatch_table_rewrite_ptr, /* eglDebugMessageControlKHR */
    epoxy_eglDestroyContext_dispatch_table_rewrite_ptr, /* eglDestroyContext */
    epoxy_eglDestroyImage_dispatch_table_rewrite_ptr, /* eglDestroyImage */
    epoxy_eglDestroyImageKHR_dispatch_table_rewrite_ptr, /* eglDestroyImageKHR */
    epoxy_eglDestroyStreamKHR_dispatch_table_rewrite_ptr, /* eglDestroyStreamKHR */
    epoxy_eglDestroySurface_dispatch_table_rewrite_ptr, /* eglDestroySurface */
    epoxy_eglDestroySync_dispatch_table_rewrite_ptr, /* eglDestroySync */
    epoxy_eglDestroySyncKHR_dispatch_table_rewrite_ptr, /* eglDestroySyncKHR */
    epoxy_eglDestroySyncNV_dispatch_table_rewrite_ptr, /* eglDestroySyncNV */
    epoxy_eglDupNativeFenceFDANDROID_dispatch_table_rewrite_ptr, /* eglDupNativeFenceFDANDROID */
    epoxy_eglExportDMABUFImageMESA_dispatch_table_rewrite_ptr, /* eglExportDMABUFImageMESA */
    epoxy_eglExportDMABUFImageQueryMESA_dispatch_table_rewrite_ptr, /* eglExportDMABUFImageQueryMESA */
    epoxy_eglExportDRMImageMESA_dispatch_table_rewrite_ptr, /* eglExportDRMImageMESA */
    epoxy_eglFenceNV_dispatch_table_rewrite_ptr, /* eglFenceNV */
    epoxy_eglGetCompositorTimingANDROID_dispatch_table_rewrite_ptr, /* eglGetCompositorTimingANDROID */
    epoxy_eglGetCompositorTimingSupportedANDROID_dispatch_table_rewrite_ptr, /* eglGetCompositorTimingSupportedANDROID */
    epoxy_eglGetConfigAttrib_dispatch_table_rewrite_ptr, /* eglGetConfigAttrib */
    epoxy_eglGetConfigs_dispatch_table_rewrite_ptr, /* eglGetConfigs */
    epoxy_eglGetCurrentContext_dispatch_table_rewrite_ptr, /* eglGetCurrentContext */
    epoxy_eglGetCurrentDisplay_dispatch_table_rewrite_ptr, /* eglGetCurrentDisplay */
    epoxy_eglGetCurrentSurface_dispatch_table_rewrite_ptr, /* eglGetCurrentSurface */
    epoxy_eglGetDisplay_dispatch_table_rewrite_ptr, /* eglGetDisplay */
    epoxy_eglGetDisplayDriverConfig_dispatch_table_rewrite_ptr, /* eglGetDisplayDriverConfig */
    epoxy_eglGetDisplayDriverName_dispatch_table_rewrite_ptr, /* eglGetDisplayDriverName */
    epoxy_eglGetError_dispatch_table_rewrite_ptr, /* eglGetError */
    epoxy_eglGetFrameTimestampSupportedANDROID_dispatch_table_rewrite_ptr, /* eglGetFrameTimestampSupportedANDROID */
    epoxy_eglGetFrameTimestampsANDROID_dispatch_table_rewrite_ptr, /* eglGetFrameTimestampsANDROID */
    epoxy_eglGetNativeClientBufferANDROID_dispatch_table_rewrite_ptr, /* eglGetNativeClientBufferANDROID */
    epoxy_eglGetNextFrameIdANDROID_dispatch_table_rewrite_ptr, /* eglGetNextFrameIdANDROID */
    epoxy_eglGetOutputLayersEXT_dispatch_table_rewrite_ptr, /* eglGetOutputLayersEXT */
    epoxy_eglGetOutputPortsEXT_dispatch_table_rewrite_ptr, /* eglGetOutputPortsEXT */
    epoxy_eglGetPlatformDisplay_dispatch_table_rewrite_ptr, /* eglGetPlatformDisplay */
    epoxy_eglGetPlatformDisplayEXT_dispatch_table_rewrite_ptr, /* eglGetPlatformDisplayEXT */
    epoxy_eglGetProcAddress_dispatch_table_rewrite_ptr, /* eglGetProcAddress */
    epoxy_eglGetStreamFileDescriptorKHR_dispatch_table_rewrite_ptr, /* eglGetStreamFileDescriptorKHR */
    epoxy_eglGetSyncAttrib_dispatch_table_rewrite_ptr, /* eglGetSyncAttrib */
    epoxy_eglGetSyncAttribKHR_dispatch_table_rewrite_ptr, /* eglGetSyncAttribKHR */
    epoxy_eglGetSyncAttribNV_dispatch_table_rewrite_ptr, /* eglGetSyncAttribNV */
    epoxy_eglGetSystemTimeFrequencyNV_dispatch_table_rewrite_ptr, /* eglGetSystemTimeFrequencyNV */
    epoxy_eglGetSystemTimeNV_dispatch_table_rewrite_ptr, /* eglGetSystemTimeNV */
    epoxy_eglInitialize_dispatch_table_rewrite_ptr, /* eglInitialize */
    epoxy_eglLabelObjectKHR_dispatch_table_rewrite_ptr, /* eglLabelObjectKHR */
    epoxy_eglLockSurfaceKHR_dispatch_table_rewrite_ptr, /* eglLockSurfaceKHR */
    epoxy_eglMakeCurrent_dispatch_table_rewrite_ptr, /* eglMakeCurrent */
    epoxy_eglOutputLayerAttribEXT_dispatch_table_rewrite_ptr, /* eglOutputLayerAttribEXT */
    epoxy_eglOutputPortAttribEXT_dispatch_table_rewrite_ptr, /* eglOutputPortAttribEXT */
    epoxy_eglPostSubBufferNV_dispatch_table_rewrite_ptr, /* eglPostSubBufferNV */
    epoxy_eglPresentationTimeANDROID_dispatch_table_rewrite_ptr, /* eglPresentationTimeANDROID */
    epoxy_eglQueryAPI_dispatch_table_rewrite_ptr, /* eglQueryAPI */
    epoxy_eglQueryContext_dispatch_table_rewrite_ptr, /* eglQueryContext */
    epoxy_eglQueryDebugKHR_dispatch_table_rewrite_ptr, /* eglQueryDebugKHR */
    epoxy_eglQueryDeviceAttribEXT_dispatch_table_rewrite_ptr, /* eglQueryDeviceAttribEXT */
    epoxy_eglQueryDeviceStringEXT_dispatch_table_rewrite_ptr, /* eglQueryDeviceStringEXT */
    epoxy_eglQueryDevicesEXT_dispatch_table_rewrite_ptr, /* eglQueryDevicesEXT */
    epoxy_eglQueryDisplayAttribEXT_dispatch_table_rewrite_ptr, /* eglQueryDisplayAttribEXT */
    epoxy_eglQueryDisplayAttribKHR_dispatch_table_rewrite_ptr, /* eglQueryDisplayAttribKHR */
    epoxy_eglQueryDisplayAttribNV_dispatch_table_rewrite_ptr, /* eglQueryDisplayAttribNV */
    epoxy_eglQueryDmaBufFormatsEXT_dispatch_table_rewrite_ptr, /* eglQueryDmaBufFormatsEXT */
    epoxy_eglQueryDmaBufModifiersEXT_dispatch_table_rewrite_ptr, /* eglQueryDmaBufModifiersEXT */
    epoxy_eglQueryNativeDisplayNV_dispatch_table_rewrite_ptr, /* eglQueryNativeDisplayNV */
    epoxy_eglQueryNativePixmapNV_dispatch_table_rewrite_ptr, /* eglQueryNativePixmapNV */
    epoxy_eglQueryNativeWindowNV_dispatch_table_rewrite_ptr, /* eglQueryNativeWindowNV */
    epoxy_eglQueryOutputLayerAttribEXT_dispatch_table_rewrite_ptr, /* eglQueryOutputLayerAttribEXT */
    epoxy_eglQueryOutputLayerStringEXT_dispatch_table_rewrite_ptr, /* eglQueryOutputLayerStringEXT */
    epoxy_eglQueryOutputPortAttribEXT_dispatch_table_rewrite_ptr, /* eglQueryOutputPortAttribEXT */
    epoxy_eglQueryOutputPortStringEXT_dispatch_table_rewrite_ptr, /* eglQueryOutputPortStringEXT */
    epoxy_eglQueryStreamAttribKHR_dispatch_table_rewrite_ptr, /* eglQueryStreamAttribKHR */
    epoxy_eglQueryStreamKHR_dispatch_table_rewrite_ptr, /* eglQueryStreamKHR */
    epoxy_eglQueryStreamMetadataNV_dispatch_table_rewrite_ptr, /* eglQueryStreamMetadataNV */
    epoxy_eglQueryStreamTimeKHR_dispatch_table_rewrite_ptr, /* eglQueryStreamTimeKHR */
    epoxy_eglQueryStreamu64KHR_dispatch_table_rewrite_ptr, /* eglQueryStreamu64KHR */
    epoxy_eglQueryString_dispatch_table_rewrite_ptr, /* eglQueryString */
    epoxy_eglQuerySurface_dispatch_table_rewrite_ptr, /* eglQuerySurface */
    epoxy_eglQuerySurface64KHR_dispatch_table_rewrite_ptr, /* eglQuerySurface64KHR */
    epoxy_eglQuerySurfacePointerANGLE_dispatch_table_rewrite_ptr, /* eglQuerySurfacePointerANGLE */
    epoxy_eglReleaseTexImage_dispatch_table_rewrite_ptr, /* eglReleaseTexImage */
    epoxy_eglReleaseThread_dispatch_table_rewrite_ptr, /* eglReleaseThread */
    epoxy_eglResetStreamNV_dispatch_table_rewrite_ptr, /* eglResetStreamNV */
    epoxy_eglSetBlobCacheFuncsANDROID_dispatch_table_rewrite_ptr, /* eglSetBlobCacheFuncsANDROID */
    epoxy_eglSetDamageRegionKHR_dispatch_table_rewrite_ptr, /* eglSetDamageRegionKHR */
    epoxy_eglSetStreamAttribKHR_dispatch_table_rewrite_ptr, /* eglSetStreamAttribKHR */
    epoxy_eglSetStreamMetadataNV_dispatch_table_rewrite_ptr, /* eglSetStreamMetadataNV */
    epoxy_eglSignalSyncKHR_dispatch_table_rewrite_ptr, /* eglSignalSyncKHR */
    epoxy_eglSignalSyncNV_dispatch_table_rewrite_ptr, /* eglSignalSyncNV */
    epoxy_eglStreamAttribKHR_dispatch_table_rewrite_ptr, /* eglStreamAttribKHR */
    epoxy_eglStreamConsumerAcquireAttribKHR_dispatch_table_rewrite_ptr, /* eglStreamConsumerAcquireAttribKHR */
    epoxy_eglStreamConsumerAcquireKHR_dispatch_table_rewrite_ptr, /* eglStreamConsumerAcquireKHR */
    epoxy_eglStreamConsumerGLTextureExternalAttribsNV_dispatch_table_rewrite_ptr, /* eglStreamConsumerGLTextureExternalAttribsNV */
    epoxy_eglStreamConsumerGLTextureExternalKHR_dispatch_table_rewrite_ptr, /* eglStreamConsumerGLTextureExternalKHR */
    epoxy_eglStreamConsumerOutputEXT_dispatch_table_rewrite_ptr, /* eglStreamConsumerOutputEXT */
    epoxy_eglStreamConsumerReleaseAttribKHR_dispatch_table_rewrite_ptr, /* eglStreamConsumerReleaseAttribKHR */
    epoxy_eglStreamConsumerReleaseKHR_dispatch_table_rewrite_ptr, /* eglStreamConsumerReleaseKHR */
    epoxy_eglStreamFlushNV_dispatch_table_rewrite_ptr, /* eglStreamFlushNV */
    epoxy_eglSurfaceAttrib_dispatch_table_rewrite_ptr, /* eglSurfaceAttrib */
    epoxy_eglSwapBuffers_dispatch_table_rewrite_ptr, /* eglSwapBuffers */
    epoxy_eglSwapBuffersRegion2NOK_dispatch_table_rewrite_ptr, /* eglSwapBuffersRegion2NOK */
    epoxy_eglSwapBuffersRegionNOK_dispatch_table_rewrite_ptr, /* eglSwapBuffersRegionNOK */
    epoxy_eglSwapBuffersWithDamageEXT_dispatch_table_rewrite_ptr, /* eglSwapBuffersWithDamageEXT */
    epoxy_eglSwapBuffersWithDamageKHR_dispatch_table_rewrite_ptr, /* eglSwapBuffersWithDamageKHR */
    epoxy_eglSwapInterval_dispatch_table_rewrite_ptr, /* eglSwapInterval */
    epoxy_eglTerminate_dispatch_table_rewrite_ptr, /* eglTerminate */
    epoxy_eglUnlockSurfaceKHR_dispatch_table_rewrite_ptr, /* eglUnlockSurfaceKHR */
    epoxy_eglUnsignalSyncEXT_dispatch_table_rewrite_ptr, /* eglUnsignalSyncEXT */
    epoxy_eglWaitClient_dispatch_table_rewrite_ptr, /* eglWaitClient */
    epoxy_eglWaitGL_dispatch_table_rewrite_ptr, /* eglWaitGL */
    epoxy_eglWaitNative_dispatch_table_rewrite_ptr, /* eglWaitNative */
    epoxy_eglWaitSync_dispatch_table_rewrite_ptr, /* eglWaitSync */
    epoxy_eglWaitSyncKHR_dispatch_table_rewrite_ptr, /* eglWaitSyncKHR */
};

uint32_t egl_tls_index;
uint32_t egl_tls_size = sizeof(struct dispatch_table);

static inline struct dispatch_table *
get_dispatch_table(void)
{
	return TlsGetValue(egl_tls_index);
}

void
egl_init_dispatch_table(void)
{
    struct dispatch_table *dispatch_table = get_dispatch_table();
    memcpy(dispatch_table, &resolver_table, sizeof(resolver_table));
}

void
egl_switch_to_dispatch_table(void)
{
    epoxy_eglBindAPI = epoxy_eglBindAPI_dispatch_table_thunk;
    epoxy_eglBindTexImage = epoxy_eglBindTexImage_dispatch_table_thunk;
    epoxy_eglChooseConfig = epoxy_eglChooseConfig_dispatch_table_thunk;
    epoxy_eglClientSignalSyncEXT = epoxy_eglClientSignalSyncEXT_dispatch_table_thunk;
    epoxy_eglClientWaitSync = epoxy_eglClientWaitSync_dispatch_table_thunk;
    epoxy_eglClientWaitSyncKHR = epoxy_eglClientWaitSyncKHR_dispatch_table_thunk;
    epoxy_eglClientWaitSyncNV = epoxy_eglClientWaitSyncNV_dispatch_table_thunk;
    epoxy_eglCompositorBindTexWindowEXT = epoxy_eglCompositorBindTexWindowEXT_dispatch_table_thunk;
    epoxy_eglCompositorSetContextAttributesEXT = epoxy_eglCompositorSetContextAttributesEXT_dispatch_table_thunk;
    epoxy_eglCompositorSetContextListEXT = epoxy_eglCompositorSetContextListEXT_dispatch_table_thunk;
    epoxy_eglCompositorSetSizeEXT = epoxy_eglCompositorSetSizeEXT_dispatch_table_thunk;
    epoxy_eglCompositorSetWindowAttributesEXT = epoxy_eglCompositorSetWindowAttributesEXT_dispatch_table_thunk;
    epoxy_eglCompositorSetWindowListEXT = epoxy_eglCompositorSetWindowListEXT_dispatch_table_thunk;
    epoxy_eglCompositorSwapPolicyEXT = epoxy_eglCompositorSwapPolicyEXT_dispatch_table_thunk;
    epoxy_eglCopyBuffers = epoxy_eglCopyBuffers_dispatch_table_thunk;
    epoxy_eglCreateContext = epoxy_eglCreateContext_dispatch_table_thunk;
    epoxy_eglCreateDRMImageMESA = epoxy_eglCreateDRMImageMESA_dispatch_table_thunk;
    epoxy_eglCreateFenceSyncNV = epoxy_eglCreateFenceSyncNV_dispatch_table_thunk;
    epoxy_eglCreateImage = epoxy_eglCreateImage_dispatch_table_thunk;
    epoxy_eglCreateImageKHR = epoxy_eglCreateImageKHR_dispatch_table_thunk;
    epoxy_eglCreateNativeClientBufferANDROID = epoxy_eglCreateNativeClientBufferANDROID_dispatch_table_thunk;
    epoxy_eglCreatePbufferFromClientBuffer = epoxy_eglCreatePbufferFromClientBuffer_dispatch_table_thunk;
    epoxy_eglCreatePbufferSurface = epoxy_eglCreatePbufferSurface_dispatch_table_thunk;
    epoxy_eglCreatePixmapSurface = epoxy_eglCreatePixmapSurface_dispatch_table_thunk;
    epoxy_eglCreatePixmapSurfaceHI = epoxy_eglCreatePixmapSurfaceHI_dispatch_table_thunk;
    epoxy_eglCreatePlatformPixmapSurface = epoxy_eglCreatePlatformPixmapSurface_dispatch_table_thunk;
    epoxy_eglCreatePlatformPixmapSurfaceEXT = epoxy_eglCreatePlatformPixmapSurfaceEXT_dispatch_table_thunk;
    epoxy_eglCreatePlatformWindowSurface = epoxy_eglCreatePlatformWindowSurface_dispatch_table_thunk;
    epoxy_eglCreatePlatformWindowSurfaceEXT = epoxy_eglCreatePlatformWindowSurfaceEXT_dispatch_table_thunk;
    epoxy_eglCreateStreamAttribKHR = epoxy_eglCreateStreamAttribKHR_dispatch_table_thunk;
    epoxy_eglCreateStreamFromFileDescriptorKHR = epoxy_eglCreateStreamFromFileDescriptorKHR_dispatch_table_thunk;
    epoxy_eglCreateStreamKHR = epoxy_eglCreateStreamKHR_dispatch_table_thunk;
    epoxy_eglCreateStreamProducerSurfaceKHR = epoxy_eglCreateStreamProducerSurfaceKHR_dispatch_table_thunk;
    epoxy_eglCreateStreamSyncNV = epoxy_eglCreateStreamSyncNV_dispatch_table_thunk;
    epoxy_eglCreateSync = epoxy_eglCreateSync_dispatch_table_thunk;
    epoxy_eglCreateSync64KHR = epoxy_eglCreateSync64KHR_dispatch_table_thunk;
    epoxy_eglCreateSyncKHR = epoxy_eglCreateSyncKHR_dispatch_table_thunk;
    epoxy_eglCreateWindowSurface = epoxy_eglCreateWindowSurface_dispatch_table_thunk;
    epoxy_eglDebugMessageControlKHR = epoxy_eglDebugMessageControlKHR_dispatch_table_thunk;
    epoxy_eglDestroyContext = epoxy_eglDestroyContext_dispatch_table_thunk;
    epoxy_eglDestroyImage = epoxy_eglDestroyImage_dispatch_table_thunk;
    epoxy_eglDestroyImageKHR = epoxy_eglDestroyImageKHR_dispatch_table_thunk;
    epoxy_eglDestroyStreamKHR = epoxy_eglDestroyStreamKHR_dispatch_table_thunk;
    epoxy_eglDestroySurface = epoxy_eglDestroySurface_dispatch_table_thunk;
    epoxy_eglDestroySync = epoxy_eglDestroySync_dispatch_table_thunk;
    epoxy_eglDestroySyncKHR = epoxy_eglDestroySyncKHR_dispatch_table_thunk;
    epoxy_eglDestroySyncNV = epoxy_eglDestroySyncNV_dispatch_table_thunk;
    epoxy_eglDupNativeFenceFDANDROID = epoxy_eglDupNativeFenceFDANDROID_dispatch_table_thunk;
    epoxy_eglExportDMABUFImageMESA = epoxy_eglExportDMABUFImageMESA_dispatch_table_thunk;
    epoxy_eglExportDMABUFImageQueryMESA = epoxy_eglExportDMABUFImageQueryMESA_dispatch_table_thunk;
    epoxy_eglExportDRMImageMESA = epoxy_eglExportDRMImageMESA_dispatch_table_thunk;
    epoxy_eglFenceNV = epoxy_eglFenceNV_dispatch_table_thunk;
    epoxy_eglGetCompositorTimingANDROID = epoxy_eglGetCompositorTimingANDROID_dispatch_table_thunk;
    epoxy_eglGetCompositorTimingSupportedANDROID = epoxy_eglGetCompositorTimingSupportedANDROID_dispatch_table_thunk;
    epoxy_eglGetConfigAttrib = epoxy_eglGetConfigAttrib_dispatch_table_thunk;
    epoxy_eglGetConfigs = epoxy_eglGetConfigs_dispatch_table_thunk;
    epoxy_eglGetCurrentContext = epoxy_eglGetCurrentContext_dispatch_table_thunk;
    epoxy_eglGetCurrentDisplay = epoxy_eglGetCurrentDisplay_dispatch_table_thunk;
    epoxy_eglGetCurrentSurface = epoxy_eglGetCurrentSurface_dispatch_table_thunk;
    epoxy_eglGetDisplay = epoxy_eglGetDisplay_dispatch_table_thunk;
    epoxy_eglGetDisplayDriverConfig = epoxy_eglGetDisplayDriverConfig_dispatch_table_thunk;
    epoxy_eglGetDisplayDriverName = epoxy_eglGetDisplayDriverName_dispatch_table_thunk;
    epoxy_eglGetError = epoxy_eglGetError_dispatch_table_thunk;
    epoxy_eglGetFrameTimestampSupportedANDROID = epoxy_eglGetFrameTimestampSupportedANDROID_dispatch_table_thunk;
    epoxy_eglGetFrameTimestampsANDROID = epoxy_eglGetFrameTimestampsANDROID_dispatch_table_thunk;
    epoxy_eglGetNativeClientBufferANDROID = epoxy_eglGetNativeClientBufferANDROID_dispatch_table_thunk;
    epoxy_eglGetNextFrameIdANDROID = epoxy_eglGetNextFrameIdANDROID_dispatch_table_thunk;
    epoxy_eglGetOutputLayersEXT = epoxy_eglGetOutputLayersEXT_dispatch_table_thunk;
    epoxy_eglGetOutputPortsEXT = epoxy_eglGetOutputPortsEXT_dispatch_table_thunk;
    epoxy_eglGetPlatformDisplay = epoxy_eglGetPlatformDisplay_dispatch_table_thunk;
    epoxy_eglGetPlatformDisplayEXT = epoxy_eglGetPlatformDisplayEXT_dispatch_table_thunk;
    epoxy_eglGetProcAddress = epoxy_eglGetProcAddress_dispatch_table_thunk;
    epoxy_eglGetStreamFileDescriptorKHR = epoxy_eglGetStreamFileDescriptorKHR_dispatch_table_thunk;
    epoxy_eglGetSyncAttrib = epoxy_eglGetSyncAttrib_dispatch_table_thunk;
    epoxy_eglGetSyncAttribKHR = epoxy_eglGetSyncAttribKHR_dispatch_table_thunk;
    epoxy_eglGetSyncAttribNV = epoxy_eglGetSyncAttribNV_dispatch_table_thunk;
    epoxy_eglGetSystemTimeFrequencyNV = epoxy_eglGetSystemTimeFrequencyNV_dispatch_table_thunk;
    epoxy_eglGetSystemTimeNV = epoxy_eglGetSystemTimeNV_dispatch_table_thunk;
    epoxy_eglInitialize = epoxy_eglInitialize_dispatch_table_thunk;
    epoxy_eglLabelObjectKHR = epoxy_eglLabelObjectKHR_dispatch_table_thunk;
    epoxy_eglLockSurfaceKHR = epoxy_eglLockSurfaceKHR_dispatch_table_thunk;
    epoxy_eglMakeCurrent = epoxy_eglMakeCurrent_dispatch_table_thunk;
    epoxy_eglOutputLayerAttribEXT = epoxy_eglOutputLayerAttribEXT_dispatch_table_thunk;
    epoxy_eglOutputPortAttribEXT = epoxy_eglOutputPortAttribEXT_dispatch_table_thunk;
    epoxy_eglPostSubBufferNV = epoxy_eglPostSubBufferNV_dispatch_table_thunk;
    epoxy_eglPresentationTimeANDROID = epoxy_eglPresentationTimeANDROID_dispatch_table_thunk;
    epoxy_eglQueryAPI = epoxy_eglQueryAPI_dispatch_table_thunk;
    epoxy_eglQueryContext = epoxy_eglQueryContext_dispatch_table_thunk;
    epoxy_eglQueryDebugKHR = epoxy_eglQueryDebugKHR_dispatch_table_thunk;
    epoxy_eglQueryDeviceAttribEXT = epoxy_eglQueryDeviceAttribEXT_dispatch_table_thunk;
    epoxy_eglQueryDeviceStringEXT = epoxy_eglQueryDeviceStringEXT_dispatch_table_thunk;
    epoxy_eglQueryDevicesEXT = epoxy_eglQueryDevicesEXT_dispatch_table_thunk;
    epoxy_eglQueryDisplayAttribEXT = epoxy_eglQueryDisplayAttribEXT_dispatch_table_thunk;
    epoxy_eglQueryDisplayAttribKHR = epoxy_eglQueryDisplayAttribKHR_dispatch_table_thunk;
    epoxy_eglQueryDisplayAttribNV = epoxy_eglQueryDisplayAttribNV_dispatch_table_thunk;
    epoxy_eglQueryDmaBufFormatsEXT = epoxy_eglQueryDmaBufFormatsEXT_dispatch_table_thunk;
    epoxy_eglQueryDmaBufModifiersEXT = epoxy_eglQueryDmaBufModifiersEXT_dispatch_table_thunk;
    epoxy_eglQueryNativeDisplayNV = epoxy_eglQueryNativeDisplayNV_dispatch_table_thunk;
    epoxy_eglQueryNativePixmapNV = epoxy_eglQueryNativePixmapNV_dispatch_table_thunk;
    epoxy_eglQueryNativeWindowNV = epoxy_eglQueryNativeWindowNV_dispatch_table_thunk;
    epoxy_eglQueryOutputLayerAttribEXT = epoxy_eglQueryOutputLayerAttribEXT_dispatch_table_thunk;
    epoxy_eglQueryOutputLayerStringEXT = epoxy_eglQueryOutputLayerStringEXT_dispatch_table_thunk;
    epoxy_eglQueryOutputPortAttribEXT = epoxy_eglQueryOutputPortAttribEXT_dispatch_table_thunk;
    epoxy_eglQueryOutputPortStringEXT = epoxy_eglQueryOutputPortStringEXT_dispatch_table_thunk;
    epoxy_eglQueryStreamAttribKHR = epoxy_eglQueryStreamAttribKHR_dispatch_table_thunk;
    epoxy_eglQueryStreamKHR = epoxy_eglQueryStreamKHR_dispatch_table_thunk;
    epoxy_eglQueryStreamMetadataNV = epoxy_eglQueryStreamMetadataNV_dispatch_table_thunk;
    epoxy_eglQueryStreamTimeKHR = epoxy_eglQueryStreamTimeKHR_dispatch_table_thunk;
    epoxy_eglQueryStreamu64KHR = epoxy_eglQueryStreamu64KHR_dispatch_table_thunk;
    epoxy_eglQueryString = epoxy_eglQueryString_dispatch_table_thunk;
    epoxy_eglQuerySurface = epoxy_eglQuerySurface_dispatch_table_thunk;
    epoxy_eglQuerySurface64KHR = epoxy_eglQuerySurface64KHR_dispatch_table_thunk;
    epoxy_eglQuerySurfacePointerANGLE = epoxy_eglQuerySurfacePointerANGLE_dispatch_table_thunk;
    epoxy_eglReleaseTexImage = epoxy_eglReleaseTexImage_dispatch_table_thunk;
    epoxy_eglReleaseThread = epoxy_eglReleaseThread_dispatch_table_thunk;
    epoxy_eglResetStreamNV = epoxy_eglResetStreamNV_dispatch_table_thunk;
    epoxy_eglSetBlobCacheFuncsANDROID = epoxy_eglSetBlobCacheFuncsANDROID_dispatch_table_thunk;
    epoxy_eglSetDamageRegionKHR = epoxy_eglSetDamageRegionKHR_dispatch_table_thunk;
    epoxy_eglSetStreamAttribKHR = epoxy_eglSetStreamAttribKHR_dispatch_table_thunk;
    epoxy_eglSetStreamMetadataNV = epoxy_eglSetStreamMetadataNV_dispatch_table_thunk;
    epoxy_eglSignalSyncKHR = epoxy_eglSignalSyncKHR_dispatch_table_thunk;
    epoxy_eglSignalSyncNV = epoxy_eglSignalSyncNV_dispatch_table_thunk;
    epoxy_eglStreamAttribKHR = epoxy_eglStreamAttribKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerAcquireAttribKHR = epoxy_eglStreamConsumerAcquireAttribKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerAcquireKHR = epoxy_eglStreamConsumerAcquireKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerGLTextureExternalAttribsNV = epoxy_eglStreamConsumerGLTextureExternalAttribsNV_dispatch_table_thunk;
    epoxy_eglStreamConsumerGLTextureExternalKHR = epoxy_eglStreamConsumerGLTextureExternalKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerOutputEXT = epoxy_eglStreamConsumerOutputEXT_dispatch_table_thunk;
    epoxy_eglStreamConsumerReleaseAttribKHR = epoxy_eglStreamConsumerReleaseAttribKHR_dispatch_table_thunk;
    epoxy_eglStreamConsumerReleaseKHR = epoxy_eglStreamConsumerReleaseKHR_dispatch_table_thunk;
    epoxy_eglStreamFlushNV = epoxy_eglStreamFlushNV_dispatch_table_thunk;
    epoxy_eglSurfaceAttrib = epoxy_eglSurfaceAttrib_dispatch_table_thunk;
    epoxy_eglSwapBuffers = epoxy_eglSwapBuffers_dispatch_table_thunk;
    epoxy_eglSwapBuffersRegion2NOK = epoxy_eglSwapBuffersRegion2NOK_dispatch_table_thunk;
    epoxy_eglSwapBuffersRegionNOK = epoxy_eglSwapBuffersRegionNOK_dispatch_table_thunk;
    epoxy_eglSwapBuffersWithDamageEXT = epoxy_eglSwapBuffersWithDamageEXT_dispatch_table_thunk;
    epoxy_eglSwapBuffersWithDamageKHR = epoxy_eglSwapBuffersWithDamageKHR_dispatch_table_thunk;
    epoxy_eglSwapInterval = epoxy_eglSwapInterval_dispatch_table_thunk;
    epoxy_eglTerminate = epoxy_eglTerminate_dispatch_table_thunk;
    epoxy_eglUnlockSurfaceKHR = epoxy_eglUnlockSurfaceKHR_dispatch_table_thunk;
    epoxy_eglUnsignalSyncEXT = epoxy_eglUnsignalSyncEXT_dispatch_table_thunk;
    epoxy_eglWaitClient = epoxy_eglWaitClient_dispatch_table_thunk;
    epoxy_eglWaitGL = epoxy_eglWaitGL_dispatch_table_thunk;
    epoxy_eglWaitNative = epoxy_eglWaitNative_dispatch_table_thunk;
    epoxy_eglWaitSync = epoxy_eglWaitSync_dispatch_table_thunk;
    epoxy_eglWaitSyncKHR = epoxy_eglWaitSyncKHR_dispatch_table_thunk;
}

#endif /* !USING_DISPATCH_TABLE */
PFNEGLBINDAPIPROC epoxy_eglBindAPI = epoxy_eglBindAPI_global_rewrite_ptr;

PFNEGLBINDTEXIMAGEPROC epoxy_eglBindTexImage = epoxy_eglBindTexImage_global_rewrite_ptr;

PFNEGLCHOOSECONFIGPROC epoxy_eglChooseConfig = epoxy_eglChooseConfig_global_rewrite_ptr;

PFNEGLCLIENTSIGNALSYNCEXTPROC epoxy_eglClientSignalSyncEXT = epoxy_eglClientSignalSyncEXT_global_rewrite_ptr;

PFNEGLCLIENTWAITSYNCPROC epoxy_eglClientWaitSync = epoxy_eglClientWaitSync_global_rewrite_ptr;

PFNEGLCLIENTWAITSYNCKHRPROC epoxy_eglClientWaitSyncKHR = epoxy_eglClientWaitSyncKHR_global_rewrite_ptr;

PFNEGLCLIENTWAITSYNCNVPROC epoxy_eglClientWaitSyncNV = epoxy_eglClientWaitSyncNV_global_rewrite_ptr;

PFNEGLCOMPOSITORBINDTEXWINDOWEXTPROC epoxy_eglCompositorBindTexWindowEXT = epoxy_eglCompositorBindTexWindowEXT_global_rewrite_ptr;

PFNEGLCOMPOSITORSETCONTEXTATTRIBUTESEXTPROC epoxy_eglCompositorSetContextAttributesEXT = epoxy_eglCompositorSetContextAttributesEXT_global_rewrite_ptr;

PFNEGLCOMPOSITORSETCONTEXTLISTEXTPROC epoxy_eglCompositorSetContextListEXT = epoxy_eglCompositorSetContextListEXT_global_rewrite_ptr;

PFNEGLCOMPOSITORSETSIZEEXTPROC epoxy_eglCompositorSetSizeEXT = epoxy_eglCompositorSetSizeEXT_global_rewrite_ptr;

PFNEGLCOMPOSITORSETWINDOWATTRIBUTESEXTPROC epoxy_eglCompositorSetWindowAttributesEXT = epoxy_eglCompositorSetWindowAttributesEXT_global_rewrite_ptr;

PFNEGLCOMPOSITORSETWINDOWLISTEXTPROC epoxy_eglCompositorSetWindowListEXT = epoxy_eglCompositorSetWindowListEXT_global_rewrite_ptr;

PFNEGLCOMPOSITORSWAPPOLICYEXTPROC epoxy_eglCompositorSwapPolicyEXT = epoxy_eglCompositorSwapPolicyEXT_global_rewrite_ptr;

PFNEGLCOPYBUFFERSPROC epoxy_eglCopyBuffers = epoxy_eglCopyBuffers_global_rewrite_ptr;

PFNEGLCREATECONTEXTPROC epoxy_eglCreateContext = epoxy_eglCreateContext_global_rewrite_ptr;

PFNEGLCREATEDRMIMAGEMESAPROC epoxy_eglCreateDRMImageMESA = epoxy_eglCreateDRMImageMESA_global_rewrite_ptr;

PFNEGLCREATEFENCESYNCNVPROC epoxy_eglCreateFenceSyncNV = epoxy_eglCreateFenceSyncNV_global_rewrite_ptr;

PFNEGLCREATEIMAGEPROC epoxy_eglCreateImage = epoxy_eglCreateImage_global_rewrite_ptr;

PFNEGLCREATEIMAGEKHRPROC epoxy_eglCreateImageKHR = epoxy_eglCreateImageKHR_global_rewrite_ptr;

PFNEGLCREATENATIVECLIENTBUFFERANDROIDPROC epoxy_eglCreateNativeClientBufferANDROID = epoxy_eglCreateNativeClientBufferANDROID_global_rewrite_ptr;

PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC epoxy_eglCreatePbufferFromClientBuffer = epoxy_eglCreatePbufferFromClientBuffer_global_rewrite_ptr;

PFNEGLCREATEPBUFFERSURFACEPROC epoxy_eglCreatePbufferSurface = epoxy_eglCreatePbufferSurface_global_rewrite_ptr;

PFNEGLCREATEPIXMAPSURFACEPROC epoxy_eglCreatePixmapSurface = epoxy_eglCreatePixmapSurface_global_rewrite_ptr;

PFNEGLCREATEPIXMAPSURFACEHIPROC epoxy_eglCreatePixmapSurfaceHI = epoxy_eglCreatePixmapSurfaceHI_global_rewrite_ptr;

PFNEGLCREATEPLATFORMPIXMAPSURFACEPROC epoxy_eglCreatePlatformPixmapSurface = epoxy_eglCreatePlatformPixmapSurface_global_rewrite_ptr;

PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC epoxy_eglCreatePlatformPixmapSurfaceEXT = epoxy_eglCreatePlatformPixmapSurfaceEXT_global_rewrite_ptr;

PFNEGLCREATEPLATFORMWINDOWSURFACEPROC epoxy_eglCreatePlatformWindowSurface = epoxy_eglCreatePlatformWindowSurface_global_rewrite_ptr;

PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC epoxy_eglCreatePlatformWindowSurfaceEXT = epoxy_eglCreatePlatformWindowSurfaceEXT_global_rewrite_ptr;

PFNEGLCREATESTREAMATTRIBKHRPROC epoxy_eglCreateStreamAttribKHR = epoxy_eglCreateStreamAttribKHR_global_rewrite_ptr;

PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC epoxy_eglCreateStreamFromFileDescriptorKHR = epoxy_eglCreateStreamFromFileDescriptorKHR_global_rewrite_ptr;

PFNEGLCREATESTREAMKHRPROC epoxy_eglCreateStreamKHR = epoxy_eglCreateStreamKHR_global_rewrite_ptr;

PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC epoxy_eglCreateStreamProducerSurfaceKHR = epoxy_eglCreateStreamProducerSurfaceKHR_global_rewrite_ptr;

PFNEGLCREATESTREAMSYNCNVPROC epoxy_eglCreateStreamSyncNV = epoxy_eglCreateStreamSyncNV_global_rewrite_ptr;

PFNEGLCREATESYNCPROC epoxy_eglCreateSync = epoxy_eglCreateSync_global_rewrite_ptr;

PFNEGLCREATESYNC64KHRPROC epoxy_eglCreateSync64KHR = epoxy_eglCreateSync64KHR_global_rewrite_ptr;

PFNEGLCREATESYNCKHRPROC epoxy_eglCreateSyncKHR = epoxy_eglCreateSyncKHR_global_rewrite_ptr;

PFNEGLCREATEWINDOWSURFACEPROC epoxy_eglCreateWindowSurface = epoxy_eglCreateWindowSurface_global_rewrite_ptr;

PFNEGLDEBUGMESSAGECONTROLKHRPROC epoxy_eglDebugMessageControlKHR = epoxy_eglDebugMessageControlKHR_global_rewrite_ptr;

PFNEGLDESTROYCONTEXTPROC epoxy_eglDestroyContext = epoxy_eglDestroyContext_global_rewrite_ptr;

PFNEGLDESTROYIMAGEPROC epoxy_eglDestroyImage = epoxy_eglDestroyImage_global_rewrite_ptr;

PFNEGLDESTROYIMAGEKHRPROC epoxy_eglDestroyImageKHR = epoxy_eglDestroyImageKHR_global_rewrite_ptr;

PFNEGLDESTROYSTREAMKHRPROC epoxy_eglDestroyStreamKHR = epoxy_eglDestroyStreamKHR_global_rewrite_ptr;

PFNEGLDESTROYSURFACEPROC epoxy_eglDestroySurface = epoxy_eglDestroySurface_global_rewrite_ptr;

PFNEGLDESTROYSYNCPROC epoxy_eglDestroySync = epoxy_eglDestroySync_global_rewrite_ptr;

PFNEGLDESTROYSYNCKHRPROC epoxy_eglDestroySyncKHR = epoxy_eglDestroySyncKHR_global_rewrite_ptr;

PFNEGLDESTROYSYNCNVPROC epoxy_eglDestroySyncNV = epoxy_eglDestroySyncNV_global_rewrite_ptr;

PFNEGLDUPNATIVEFENCEFDANDROIDPROC epoxy_eglDupNativeFenceFDANDROID = epoxy_eglDupNativeFenceFDANDROID_global_rewrite_ptr;

PFNEGLEXPORTDMABUFIMAGEMESAPROC epoxy_eglExportDMABUFImageMESA = epoxy_eglExportDMABUFImageMESA_global_rewrite_ptr;

PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC epoxy_eglExportDMABUFImageQueryMESA = epoxy_eglExportDMABUFImageQueryMESA_global_rewrite_ptr;

PFNEGLEXPORTDRMIMAGEMESAPROC epoxy_eglExportDRMImageMESA = epoxy_eglExportDRMImageMESA_global_rewrite_ptr;

PFNEGLFENCENVPROC epoxy_eglFenceNV = epoxy_eglFenceNV_global_rewrite_ptr;

PFNEGLGETCOMPOSITORTIMINGANDROIDPROC epoxy_eglGetCompositorTimingANDROID = epoxy_eglGetCompositorTimingANDROID_global_rewrite_ptr;

PFNEGLGETCOMPOSITORTIMINGSUPPORTEDANDROIDPROC epoxy_eglGetCompositorTimingSupportedANDROID = epoxy_eglGetCompositorTimingSupportedANDROID_global_rewrite_ptr;

PFNEGLGETCONFIGATTRIBPROC epoxy_eglGetConfigAttrib = epoxy_eglGetConfigAttrib_global_rewrite_ptr;

PFNEGLGETCONFIGSPROC epoxy_eglGetConfigs = epoxy_eglGetConfigs_global_rewrite_ptr;

PFNEGLGETCURRENTCONTEXTPROC epoxy_eglGetCurrentContext = epoxy_eglGetCurrentContext_global_rewrite_ptr;

PFNEGLGETCURRENTDISPLAYPROC epoxy_eglGetCurrentDisplay = epoxy_eglGetCurrentDisplay_global_rewrite_ptr;

PFNEGLGETCURRENTSURFACEPROC epoxy_eglGetCurrentSurface = epoxy_eglGetCurrentSurface_global_rewrite_ptr;

PFNEGLGETDISPLAYPROC epoxy_eglGetDisplay = epoxy_eglGetDisplay_global_rewrite_ptr;

PFNEGLGETDISPLAYDRIVERCONFIGPROC epoxy_eglGetDisplayDriverConfig = epoxy_eglGetDisplayDriverConfig_global_rewrite_ptr;

PFNEGLGETDISPLAYDRIVERNAMEPROC epoxy_eglGetDisplayDriverName = epoxy_eglGetDisplayDriverName_global_rewrite_ptr;

PFNEGLGETERRORPROC epoxy_eglGetError = epoxy_eglGetError_global_rewrite_ptr;

PFNEGLGETFRAMETIMESTAMPSUPPORTEDANDROIDPROC epoxy_eglGetFrameTimestampSupportedANDROID = epoxy_eglGetFrameTimestampSupportedANDROID_global_rewrite_ptr;

PFNEGLGETFRAMETIMESTAMPSANDROIDPROC epoxy_eglGetFrameTimestampsANDROID = epoxy_eglGetFrameTimestampsANDROID_global_rewrite_ptr;

PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC epoxy_eglGetNativeClientBufferANDROID = epoxy_eglGetNativeClientBufferANDROID_global_rewrite_ptr;

PFNEGLGETNEXTFRAMEIDANDROIDPROC epoxy_eglGetNextFrameIdANDROID = epoxy_eglGetNextFrameIdANDROID_global_rewrite_ptr;

PFNEGLGETOUTPUTLAYERSEXTPROC epoxy_eglGetOutputLayersEXT = epoxy_eglGetOutputLayersEXT_global_rewrite_ptr;

PFNEGLGETOUTPUTPORTSEXTPROC epoxy_eglGetOutputPortsEXT = epoxy_eglGetOutputPortsEXT_global_rewrite_ptr;

PFNEGLGETPLATFORMDISPLAYPROC epoxy_eglGetPlatformDisplay = epoxy_eglGetPlatformDisplay_global_rewrite_ptr;

PFNEGLGETPLATFORMDISPLAYEXTPROC epoxy_eglGetPlatformDisplayEXT = epoxy_eglGetPlatformDisplayEXT_global_rewrite_ptr;

PFNEGLGETPROCADDRESSPROC epoxy_eglGetProcAddress = epoxy_eglGetProcAddress_global_rewrite_ptr;

PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC epoxy_eglGetStreamFileDescriptorKHR = epoxy_eglGetStreamFileDescriptorKHR_global_rewrite_ptr;

PFNEGLGETSYNCATTRIBPROC epoxy_eglGetSyncAttrib = epoxy_eglGetSyncAttrib_global_rewrite_ptr;

PFNEGLGETSYNCATTRIBKHRPROC epoxy_eglGetSyncAttribKHR = epoxy_eglGetSyncAttribKHR_global_rewrite_ptr;

PFNEGLGETSYNCATTRIBNVPROC epoxy_eglGetSyncAttribNV = epoxy_eglGetSyncAttribNV_global_rewrite_ptr;

PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC epoxy_eglGetSystemTimeFrequencyNV = epoxy_eglGetSystemTimeFrequencyNV_global_rewrite_ptr;

PFNEGLGETSYSTEMTIMENVPROC epoxy_eglGetSystemTimeNV = epoxy_eglGetSystemTimeNV_global_rewrite_ptr;

PFNEGLINITIALIZEPROC epoxy_eglInitialize = epoxy_eglInitialize_global_rewrite_ptr;

PFNEGLLABELOBJECTKHRPROC epoxy_eglLabelObjectKHR = epoxy_eglLabelObjectKHR_global_rewrite_ptr;

PFNEGLLOCKSURFACEKHRPROC epoxy_eglLockSurfaceKHR = epoxy_eglLockSurfaceKHR_global_rewrite_ptr;

PFNEGLMAKECURRENTPROC epoxy_eglMakeCurrent = epoxy_eglMakeCurrent_global_rewrite_ptr;

PFNEGLOUTPUTLAYERATTRIBEXTPROC epoxy_eglOutputLayerAttribEXT = epoxy_eglOutputLayerAttribEXT_global_rewrite_ptr;

PFNEGLOUTPUTPORTATTRIBEXTPROC epoxy_eglOutputPortAttribEXT = epoxy_eglOutputPortAttribEXT_global_rewrite_ptr;

PFNEGLPOSTSUBBUFFERNVPROC epoxy_eglPostSubBufferNV = epoxy_eglPostSubBufferNV_global_rewrite_ptr;

PFNEGLPRESENTATIONTIMEANDROIDPROC epoxy_eglPresentationTimeANDROID = epoxy_eglPresentationTimeANDROID_global_rewrite_ptr;

PFNEGLQUERYAPIPROC epoxy_eglQueryAPI = epoxy_eglQueryAPI_global_rewrite_ptr;

PFNEGLQUERYCONTEXTPROC epoxy_eglQueryContext = epoxy_eglQueryContext_global_rewrite_ptr;

PFNEGLQUERYDEBUGKHRPROC epoxy_eglQueryDebugKHR = epoxy_eglQueryDebugKHR_global_rewrite_ptr;

PFNEGLQUERYDEVICEATTRIBEXTPROC epoxy_eglQueryDeviceAttribEXT = epoxy_eglQueryDeviceAttribEXT_global_rewrite_ptr;

PFNEGLQUERYDEVICESTRINGEXTPROC epoxy_eglQueryDeviceStringEXT = epoxy_eglQueryDeviceStringEXT_global_rewrite_ptr;

PFNEGLQUERYDEVICESEXTPROC epoxy_eglQueryDevicesEXT = epoxy_eglQueryDevicesEXT_global_rewrite_ptr;

PFNEGLQUERYDISPLAYATTRIBEXTPROC epoxy_eglQueryDisplayAttribEXT = epoxy_eglQueryDisplayAttribEXT_global_rewrite_ptr;

PFNEGLQUERYDISPLAYATTRIBKHRPROC epoxy_eglQueryDisplayAttribKHR = epoxy_eglQueryDisplayAttribKHR_global_rewrite_ptr;

PFNEGLQUERYDISPLAYATTRIBNVPROC epoxy_eglQueryDisplayAttribNV = epoxy_eglQueryDisplayAttribNV_global_rewrite_ptr;

PFNEGLQUERYDMABUFFORMATSEXTPROC epoxy_eglQueryDmaBufFormatsEXT = epoxy_eglQueryDmaBufFormatsEXT_global_rewrite_ptr;

PFNEGLQUERYDMABUFMODIFIERSEXTPROC epoxy_eglQueryDmaBufModifiersEXT = epoxy_eglQueryDmaBufModifiersEXT_global_rewrite_ptr;

PFNEGLQUERYNATIVEDISPLAYNVPROC epoxy_eglQueryNativeDisplayNV = epoxy_eglQueryNativeDisplayNV_global_rewrite_ptr;

PFNEGLQUERYNATIVEPIXMAPNVPROC epoxy_eglQueryNativePixmapNV = epoxy_eglQueryNativePixmapNV_global_rewrite_ptr;

PFNEGLQUERYNATIVEWINDOWNVPROC epoxy_eglQueryNativeWindowNV = epoxy_eglQueryNativeWindowNV_global_rewrite_ptr;

PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC epoxy_eglQueryOutputLayerAttribEXT = epoxy_eglQueryOutputLayerAttribEXT_global_rewrite_ptr;

PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC epoxy_eglQueryOutputLayerStringEXT = epoxy_eglQueryOutputLayerStringEXT_global_rewrite_ptr;

PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC epoxy_eglQueryOutputPortAttribEXT = epoxy_eglQueryOutputPortAttribEXT_global_rewrite_ptr;

PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC epoxy_eglQueryOutputPortStringEXT = epoxy_eglQueryOutputPortStringEXT_global_rewrite_ptr;

PFNEGLQUERYSTREAMATTRIBKHRPROC epoxy_eglQueryStreamAttribKHR = epoxy_eglQueryStreamAttribKHR_global_rewrite_ptr;

PFNEGLQUERYSTREAMKHRPROC epoxy_eglQueryStreamKHR = epoxy_eglQueryStreamKHR_global_rewrite_ptr;

PFNEGLQUERYSTREAMMETADATANVPROC epoxy_eglQueryStreamMetadataNV = epoxy_eglQueryStreamMetadataNV_global_rewrite_ptr;

PFNEGLQUERYSTREAMTIMEKHRPROC epoxy_eglQueryStreamTimeKHR = epoxy_eglQueryStreamTimeKHR_global_rewrite_ptr;

PFNEGLQUERYSTREAMU64KHRPROC epoxy_eglQueryStreamu64KHR = epoxy_eglQueryStreamu64KHR_global_rewrite_ptr;

PFNEGLQUERYSTRINGPROC epoxy_eglQueryString = epoxy_eglQueryString_global_rewrite_ptr;

PFNEGLQUERYSURFACEPROC epoxy_eglQuerySurface = epoxy_eglQuerySurface_global_rewrite_ptr;

PFNEGLQUERYSURFACE64KHRPROC epoxy_eglQuerySurface64KHR = epoxy_eglQuerySurface64KHR_global_rewrite_ptr;

PFNEGLQUERYSURFACEPOINTERANGLEPROC epoxy_eglQuerySurfacePointerANGLE = epoxy_eglQuerySurfacePointerANGLE_global_rewrite_ptr;

PFNEGLRELEASETEXIMAGEPROC epoxy_eglReleaseTexImage = epoxy_eglReleaseTexImage_global_rewrite_ptr;

PFNEGLRELEASETHREADPROC epoxy_eglReleaseThread = epoxy_eglReleaseThread_global_rewrite_ptr;

PFNEGLRESETSTREAMNVPROC epoxy_eglResetStreamNV = epoxy_eglResetStreamNV_global_rewrite_ptr;

PFNEGLSETBLOBCACHEFUNCSANDROIDPROC epoxy_eglSetBlobCacheFuncsANDROID = epoxy_eglSetBlobCacheFuncsANDROID_global_rewrite_ptr;

PFNEGLSETDAMAGEREGIONKHRPROC epoxy_eglSetDamageRegionKHR = epoxy_eglSetDamageRegionKHR_global_rewrite_ptr;

PFNEGLSETSTREAMATTRIBKHRPROC epoxy_eglSetStreamAttribKHR = epoxy_eglSetStreamAttribKHR_global_rewrite_ptr;

PFNEGLSETSTREAMMETADATANVPROC epoxy_eglSetStreamMetadataNV = epoxy_eglSetStreamMetadataNV_global_rewrite_ptr;

PFNEGLSIGNALSYNCKHRPROC epoxy_eglSignalSyncKHR = epoxy_eglSignalSyncKHR_global_rewrite_ptr;

PFNEGLSIGNALSYNCNVPROC epoxy_eglSignalSyncNV = epoxy_eglSignalSyncNV_global_rewrite_ptr;

PFNEGLSTREAMATTRIBKHRPROC epoxy_eglStreamAttribKHR = epoxy_eglStreamAttribKHR_global_rewrite_ptr;

PFNEGLSTREAMCONSUMERACQUIREATTRIBKHRPROC epoxy_eglStreamConsumerAcquireAttribKHR = epoxy_eglStreamConsumerAcquireAttribKHR_global_rewrite_ptr;

PFNEGLSTREAMCONSUMERACQUIREKHRPROC epoxy_eglStreamConsumerAcquireKHR = epoxy_eglStreamConsumerAcquireKHR_global_rewrite_ptr;

PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALATTRIBSNVPROC epoxy_eglStreamConsumerGLTextureExternalAttribsNV = epoxy_eglStreamConsumerGLTextureExternalAttribsNV_global_rewrite_ptr;

PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC epoxy_eglStreamConsumerGLTextureExternalKHR = epoxy_eglStreamConsumerGLTextureExternalKHR_global_rewrite_ptr;

PFNEGLSTREAMCONSUMEROUTPUTEXTPROC epoxy_eglStreamConsumerOutputEXT = epoxy_eglStreamConsumerOutputEXT_global_rewrite_ptr;

PFNEGLSTREAMCONSUMERRELEASEATTRIBKHRPROC epoxy_eglStreamConsumerReleaseAttribKHR = epoxy_eglStreamConsumerReleaseAttribKHR_global_rewrite_ptr;

PFNEGLSTREAMCONSUMERRELEASEKHRPROC epoxy_eglStreamConsumerReleaseKHR = epoxy_eglStreamConsumerReleaseKHR_global_rewrite_ptr;

PFNEGLSTREAMFLUSHNVPROC epoxy_eglStreamFlushNV = epoxy_eglStreamFlushNV_global_rewrite_ptr;

PFNEGLSURFACEATTRIBPROC epoxy_eglSurfaceAttrib = epoxy_eglSurfaceAttrib_global_rewrite_ptr;

PFNEGLSWAPBUFFERSPROC epoxy_eglSwapBuffers = epoxy_eglSwapBuffers_global_rewrite_ptr;

PFNEGLSWAPBUFFERSREGION2NOKPROC epoxy_eglSwapBuffersRegion2NOK = epoxy_eglSwapBuffersRegion2NOK_global_rewrite_ptr;

PFNEGLSWAPBUFFERSREGIONNOKPROC epoxy_eglSwapBuffersRegionNOK = epoxy_eglSwapBuffersRegionNOK_global_rewrite_ptr;

PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC epoxy_eglSwapBuffersWithDamageEXT = epoxy_eglSwapBuffersWithDamageEXT_global_rewrite_ptr;

PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC epoxy_eglSwapBuffersWithDamageKHR = epoxy_eglSwapBuffersWithDamageKHR_global_rewrite_ptr;

PFNEGLSWAPINTERVALPROC epoxy_eglSwapInterval = epoxy_eglSwapInterval_global_rewrite_ptr;

PFNEGLTERMINATEPROC epoxy_eglTerminate = epoxy_eglTerminate_global_rewrite_ptr;

PFNEGLUNLOCKSURFACEKHRPROC epoxy_eglUnlockSurfaceKHR = epoxy_eglUnlockSurfaceKHR_global_rewrite_ptr;

PFNEGLUNSIGNALSYNCEXTPROC epoxy_eglUnsignalSyncEXT = epoxy_eglUnsignalSyncEXT_global_rewrite_ptr;

PFNEGLWAITCLIENTPROC epoxy_eglWaitClient = epoxy_eglWaitClient_global_rewrite_ptr;

PFNEGLWAITGLPROC epoxy_eglWaitGL = epoxy_eglWaitGL_global_rewrite_ptr;

PFNEGLWAITNATIVEPROC epoxy_eglWaitNative = epoxy_eglWaitNative_global_rewrite_ptr;

PFNEGLWAITSYNCPROC epoxy_eglWaitSync = epoxy_eglWaitSync_global_rewrite_ptr;

PFNEGLWAITSYNCKHRPROC epoxy_eglWaitSyncKHR = epoxy_eglWaitSyncKHR_global_rewrite_ptr;

