# Usage: ./update.sh <android_froyo_os_src_directory>
#
# Copies the needed include files from the directory containing the original
# Android Froyo OS source that we need for building the libstagefright support.
cp $1/system/core/include/pixelflinger/pixelflinger.h ./pixelflinger/pixelflinger.h
cp $1/system/core/include/pixelflinger/format.h ./pixelflinger/format.h
cp $1/frameworks/base/include/utils/threads.h ./utils/threads.h
cp $1/frameworks/base/include/utils/List.h ./utils/List.h
cp $1/frameworks/base/include/utils/Errors.h ./utils/Errors.h
cp $1/frameworks/base/include/utils/Vector.h ./utils/Vector.h
cp $1/frameworks/base/include/utils/VectorImpl.h ./utils/VectorImpl.h
cp $1/frameworks/base/include/utils/KeyedVector.h ./utils/KeyedVector.h
cp $1/frameworks/base/include/utils/RefBase.h ./utils/RefBase.h
cp $1/frameworks/base/include/utils/TextOutput.h ./utils/TextOutput.h
cp $1/frameworks/base/include/utils/SharedBuffer.h ./utils/SharedBuffer.h
cp $1/frameworks/base/include/utils/Timers.h ./utils/Timers.h
cp $1/frameworks/base/include/utils/String16.h ./utils/String16.h
cp $1/frameworks/base/include/utils/SortedVector.h ./utils/SortedVector.h
cp $1/frameworks/base/include/utils/Log.h ./utils/Log.h
cp $1/frameworks/base/include/utils/TypeHelpers.h ./utils/TypeHelpers.h
cp $1/frameworks/base/include/utils/Flattenable.h ./utils/Flattenable.h
cp $1/frameworks/base/include/utils/String8.h ./utils/String8.h
cp $1/system/core/include/cutils/uio.h ./cutils/uio.h
cp $1/system/core/include/cutils/atomic.h ./cutils/atomic.h
cp $1/system/core/include/cutils/log.h ./cutils/log.h
cp $1/system/core/include/cutils/native_handle.h ./cutils/native_handle.h
cp $1/system/core/include/cutils/logd.h ./cutils/logd.h
cp $1/frameworks/base/include/binder/IBinder.h ./binder/IBinder.h
cp $1/frameworks/base/include/binder/Binder.h ./binder/Binder.h
cp $1/frameworks/base/include/binder/IInterface.h ./binder/IInterface.h
cp $1/frameworks/base/include/media/stagefright/ColorConverter.h ./stagefright/ColorConverter.h
cp $1/frameworks/base/include/media/stagefright/MediaExtractor.h ./stagefright/MediaExtractor.h
cp $1/frameworks/base/include/media/stagefright/OMXCodec.h ./stagefright/OMXCodec.h
cp $1/frameworks/base/include/media/stagefright/OMXClient.h ./stagefright/OMXClient.h
cp $1/frameworks/base/include/media/stagefright/DataSource.h ./stagefright/DataSource.h
cp $1/frameworks/base/include/media/stagefright/MetaData.h ./stagefright/MetaData.h
cp $1/frameworks/base/include/media/stagefright/MediaErrors.h ./media/stagefright/MediaErrors.h
cp $1/frameworks/base/include/media/stagefright/MediaSource.h ./media/stagefright/MediaSource.h
cp $1/frameworks/base/include/media/stagefright/MediaBuffer.h ./media/stagefright/MediaBuffer.h
cp $1/device/samsung/crespo/sec_mm/sec_omx/sec_omx_include/khronos/OMX_IVCommon.h ./media/stagefright/openmax/OMX_IVCommon.h
cp $1/device/samsung/crespo/sec_mm/sec_omx/sec_omx_include/khronos/OMX_Video.h ./media/stagefright/openmax/OMX_Video.h
cp $1/device/samsung/crespo/sec_mm/sec_omx/sec_omx_include/khronos/OMX_Index.h ./media/stagefright/openmax/OMX_Index.h
cp $1/device/samsung/crespo/sec_mm/sec_omx/sec_omx_include/khronos/OMX_Core.h ./media/stagefright/openmax/OMX_Core.h
cp $1/device/samsung/crespo/sec_mm/sec_omx/sec_omx_include/khronos/OMX_Types.h ./media/stagefright/openmax/OMX_Types.h
cp $1/frameworks/base/include/media/IOMX.h ./media/IOMX.h
cp $1/frameworks/base/include/ui/Rect.h ./ui/Rect.h
cp $1/frameworks/base/include/ui/Point.h ./ui/Point.h
cp $1/frameworks/base/include/ui/PixelFormat.h ./ui/PixelFormat.h
cp $1/frameworks/base/include/ui/egl/android_natives.h ./ui/egl/android_natives.h
cp $1/frameworks/base/include/ui/android_native_buffer.h ./ui/android_native_buffer.h
cp $1/frameworks/base/include/ui/GraphicBuffer.h ./ui/GraphicBuffer.h
cp $1/frameworks/base/media/libstagefright/include/OMX.h ./OMX.h
cp $1/hardware/libhardware/include/hardware/hardware.h ./hardware/hardware.h
cp $1/hardware/libhardware/include/hardware/gralloc.h ./hardware/gralloc.h
patch -R -p5 <update.patch
