#ifndef WEBRTC_MODULES_BROWSER_CAPTURE_MAIN_SOURCE_BROWSER_CAPTURE_IMPL_H_
#define WEBRTC_MODULES_BROWSER_CAPTURE_MAIN_SOURCE_BROWSER_CAPTURE_IMPL_H_

#include "webrtc/modules/video_capture/include/video_capture.h"

using namespace webrtc::videocapturemodule;

namespace webrtc {

  class BrowserDeviceInfoImpl : public VideoCaptureModule::DeviceInfo {
  public:
    virtual uint32_t NumberOfDevices() { return 1; }

    virtual int32_t Refresh() { return 0; }

    virtual int32_t GetDeviceName(uint32_t deviceNumber,
                                  char* deviceNameUTF8,
                                  uint32_t deviceNameLength,
                                  char* deviceUniqueIdUTF8,
                                  uint32_t deviceUniqueIdUTF8Length,
                                  char* productUniqueIdUTF8 = NULL,
                                  uint32_t productUniqueIdUTF8Length = 0) {
      deviceNameUTF8 = const_cast<char*>(kDeviceName);
      deviceUniqueIdUTF8 = const_cast<char*>(kUniqueDeviceName);
      productUniqueIdUTF8 =  const_cast<char*>(kProductUniqueId);
      return 1;
    };


    virtual int32_t NumberOfCapabilities(const char* deviceUniqueIdUTF8) {
      return 0;
    }

    virtual int32_t GetCapability(const char* deviceUniqueIdUTF8,
                                  const uint32_t deviceCapabilityNumber,
                                  VideoCaptureCapability& capability) { return 0;};

    virtual int32_t GetOrientation(const char* deviceUniqueIdUTF8,
                                   VideoRotation& orientation) { return 0; }

    virtual int32_t GetBestMatchedCapability(const char* deviceUniqueIdUTF8,
                                             const VideoCaptureCapability& requested,
                                             VideoCaptureCapability& resulting) { return 0;}

    virtual int32_t DisplayCaptureSettingsDialogBox(const char* deviceUniqueIdUTF8,
                                                    const char* dialogTitleUTF8,
                                                    void* parentWindow,
                                                    uint32_t positionX,
                                                    uint32_t positionY) { return 0; }

    BrowserDeviceInfoImpl() : kDeviceName("browser"), kUniqueDeviceName("browser"), kProductUniqueId("browser") {}

    static BrowserDeviceInfoImpl* CreateDeviceInfo() {
      return new BrowserDeviceInfoImpl();
    }
    virtual ~BrowserDeviceInfoImpl() {}

  private:
    const char* kDeviceName;
    const char* kUniqueDeviceName;
    const char* kProductUniqueId;

  };
}
#endif
