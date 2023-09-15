/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PowerCounters.h"
#include "nsXULAppAPI.h"  // for XRE_IsParentProcess
#include "nsString.h"

#include <windows.h>
#include <devioctl.h>
#include <setupapi.h>  // for SetupDi*
// LogSeverity, defined by setupapi.h to DWORD, messes with other code.
#undef LogSeverity

#include <emi.h>

using namespace mozilla;

// This is a counter to collect power utilization during profiling.
// It cannot be a raw `ProfilerCounter` because we need to manually add/remove
// it while the profiler lock is already held.
class PowerMeterChannel final : public BaseProfilerCount {
 public:
  explicit PowerMeterChannel(const WCHAR* aChannelName, ULONGLONG aInitialValue,
                             ULONGLONG aInitialTime)
      : BaseProfilerCount(nullptr, nullptr, nullptr, "power",
                          "Power utilization"),
        mChannelName(NS_ConvertUTF16toUTF8(aChannelName)),
        mPreviousValue(aInitialValue),
        mPreviousTime(aInitialTime),
        mIsSampleNew(true) {
    if (mChannelName.Equals("RAPL_Package0_PKG")) {
      mLabel = "Power: CPU package";
      mDescription = mChannelName.get();
    } else if (mChannelName.Equals("RAPL_Package0_PP0")) {
      mLabel = "Power: CPU cores";
      mDescription = mChannelName.get();
    } else if (mChannelName.Equals("RAPL_Package0_PP1")) {
      mLabel = "Power: iGPU";
      mDescription = mChannelName.get();
    } else if (mChannelName.Equals("RAPL_Package0_DRAM")) {
      mLabel = "Power: DRAM";
      mDescription = mChannelName.get();
    } else {
      unsigned int coreId;
      if (sscanf(mChannelName.get(), "RAPL_Package0_Core%u_CORE", &coreId) ==
          1) {
        mLabelString = "Power: CPU core ";
        mLabelString.AppendInt(coreId);
        mLabel = mLabelString.get();
        mDescription = mChannelName.get();
      } else {
        mLabel = mChannelName.get();
      }
    }
  }

  CountSample Sample() override {
    CountSample result;
    result.count = mCounter;
    result.number = 0;
    result.isSampleNew = mIsSampleNew;
    mIsSampleNew = false;
    return result;
  }

  void AddSample(ULONGLONG aAbsoluteEnergy, ULONGLONG aAbsoluteTime) {
    // aAbsoluteTime is the time since the system start in 100ns increments.
    if (aAbsoluteTime == mPreviousTime) {
      return;
    }

    if (aAbsoluteEnergy > mPreviousValue) {
      int64_t increment = aAbsoluteEnergy - mPreviousValue;
      mCounter += increment;
      mPreviousValue += increment;
      mPreviousTime = aAbsoluteTime;
    }

    mIsSampleNew = true;
  }

 private:
  int64_t mCounter;
  nsCString mChannelName;

  // Used as a storage when the label can not be a literal string.
  nsCString mLabelString;

  ULONGLONG mPreviousValue;
  ULONGLONG mPreviousTime;
  bool mIsSampleNew;
};

class PowerMeterDevice {
 public:
  explicit PowerMeterDevice(LPCTSTR aDevicePath) {
    mHandle = ::CreateFile(aDevicePath, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (mHandle == INVALID_HANDLE_VALUE) {
      return;
    }

    EMI_VERSION version = {0};
    DWORD dwOut;

    if (!::DeviceIoControl(mHandle, IOCTL_EMI_GET_VERSION, nullptr, 0, &version,
                           sizeof(version), &dwOut, nullptr) ||
        (version.EmiVersion != EMI_VERSION_V1 &&
         version.EmiVersion != EMI_VERSION_V2)) {
      return;
    }

    EMI_METADATA_SIZE size = {0};
    if (!::DeviceIoControl(mHandle, IOCTL_EMI_GET_METADATA_SIZE, nullptr, 0,
                           &size, sizeof(size), &dwOut, nullptr) ||
        !size.MetadataSize) {
      return;
    }

    UniquePtr<uint8_t[]> metadata(new (std::nothrow)
                                      uint8_t[size.MetadataSize]);
    if (!metadata) {
      return;
    }

    if (version.EmiVersion == EMI_VERSION_V2) {
      EMI_METADATA_V2* metadata2 =
          reinterpret_cast<EMI_METADATA_V2*>(metadata.get());
      if (!::DeviceIoControl(mHandle, IOCTL_EMI_GET_METADATA, nullptr, 0,
                             metadata2, size.MetadataSize, &dwOut, nullptr)) {
        return;
      }

      if (!mChannels.reserve(metadata2->ChannelCount)) {
        return;
      }

      mDataBuffer =
          MakeUnique<EMI_CHANNEL_MEASUREMENT_DATA[]>(metadata2->ChannelCount);
      if (!mDataBuffer) {
        return;
      }

      if (!::DeviceIoControl(
              mHandle, IOCTL_EMI_GET_MEASUREMENT, nullptr, 0, mDataBuffer.get(),
              sizeof(EMI_CHANNEL_MEASUREMENT_DATA[metadata2->ChannelCount]),
              &dwOut, nullptr)) {
        return;
      }

      EMI_CHANNEL_V2* channel = &metadata2->Channels[0];
      for (int i = 0; i < metadata2->ChannelCount; ++i) {
        EMI_CHANNEL_MEASUREMENT_DATA* channel_data = &mDataBuffer[i];
        mChannels.infallibleAppend(new PowerMeterChannel(
            channel->ChannelName, channel_data->AbsoluteEnergy,
            channel_data->AbsoluteTime));
        channel = EMI_CHANNEL_V2_NEXT_CHANNEL(channel);
      }
    } else if (version.EmiVersion == EMI_VERSION_V1) {
      EMI_METADATA_V1* metadata1 =
          reinterpret_cast<EMI_METADATA_V1*>(metadata.get());
      if (!::DeviceIoControl(mHandle, IOCTL_EMI_GET_METADATA, nullptr, 0,
                             metadata1, size.MetadataSize, &dwOut, nullptr)) {
        return;
      }

      mDataBuffer = MakeUnique<EMI_CHANNEL_MEASUREMENT_DATA[]>(1);
      if (!mDataBuffer) {
        return;
      }

      if (!::DeviceIoControl(
              mHandle, IOCTL_EMI_GET_MEASUREMENT, nullptr, 0, mDataBuffer.get(),
              sizeof(EMI_CHANNEL_MEASUREMENT_DATA), &dwOut, nullptr)) {
        return;
      }

      (void)mChannels.append(new PowerMeterChannel(
          metadata1->MeteredHardwareName, mDataBuffer[0].AbsoluteEnergy,
          mDataBuffer[0].AbsoluteTime));
    }
  }

  ~PowerMeterDevice() {
    if (mHandle != INVALID_HANDLE_VALUE) {
      ::CloseHandle(mHandle);
    }
  }

  void Sample() {
    MOZ_ASSERT(HasChannels());
    MOZ_ASSERT(mDataBuffer);

    DWORD dwOut;
    if (!::DeviceIoControl(
            mHandle, IOCTL_EMI_GET_MEASUREMENT, nullptr, 0, mDataBuffer.get(),
            sizeof(EMI_CHANNEL_MEASUREMENT_DATA[mChannels.length()]), &dwOut,
            nullptr)) {
      return;
    }

    for (size_t i = 0; i < mChannels.length(); ++i) {
      EMI_CHANNEL_MEASUREMENT_DATA* channel_data = &mDataBuffer[i];
      mChannels[i]->AddSample(channel_data->AbsoluteEnergy,
                              channel_data->AbsoluteTime);
    }
  }

  bool HasChannels() { return mChannels.length() != 0; }
  void AppendCountersTo(PowerCounters::CountVector& aCounters) {
    if (aCounters.reserve(aCounters.length() + mChannels.length())) {
      for (auto& channel : mChannels) {
        aCounters.infallibleAppend(channel.get());
      }
    }
  }

 private:
  Vector<UniquePtr<PowerMeterChannel>, 4> mChannels;
  HANDLE mHandle = INVALID_HANDLE_VALUE;
  UniquePtr<EMI_CHANNEL_MEASUREMENT_DATA[]> mDataBuffer;
};

PowerCounters::PowerCounters() {
  class MOZ_STACK_CLASS HDevInfoHolder final {
   public:
    explicit HDevInfoHolder(HDEVINFO aHandle) : mHandle(aHandle) {}

    ~HDevInfoHolder() { ::SetupDiDestroyDeviceInfoList(mHandle); }

   private:
    HDEVINFO mHandle;
  };

  if (!XRE_IsParentProcess()) {
    // Energy meters are global, so only sample them on the parent.
    return;
  }

  // Energy Metering Device Interface
  // {45BD8344-7ED6-49cf-A440-C276C933B053}
  //
  // Using GUID_DEVICE_ENERGY_METER does not compile as the symbol does not
  // exist before Windows 10.
  GUID my_GUID_DEVICE_ENERGY_METER = {
      0x45bd8344,
      0x7ed6,
      0x49cf,
      {0xa4, 0x40, 0xc2, 0x76, 0xc9, 0x33, 0xb0, 0x53}};

  HDEVINFO hdev =
      ::SetupDiGetClassDevs(&my_GUID_DEVICE_ENERGY_METER, nullptr, nullptr,
                            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (hdev == INVALID_HANDLE_VALUE) {
    return;
  }

  HDevInfoHolder hdevHolder(hdev);

  DWORD i = 0;
  SP_DEVICE_INTERFACE_DATA did = {0};
  did.cbSize = sizeof(did);

  while (::SetupDiEnumDeviceInterfaces(
      hdev, nullptr, &my_GUID_DEVICE_ENERGY_METER, i++, &did)) {
    DWORD bufferSize = 0;
    ::SetupDiGetDeviceInterfaceDetail(hdev, &did, nullptr, 0, &bufferSize,
                                      nullptr);
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      continue;
    }

    UniquePtr<uint8_t[]> buffer(new (std::nothrow) uint8_t[bufferSize]);
    if (!buffer) {
      continue;
    }

    PSP_DEVICE_INTERFACE_DETAIL_DATA pdidd =
        reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(buffer.get());
    MOZ_ASSERT(uintptr_t(buffer.get()) %
                   alignof(PSP_DEVICE_INTERFACE_DETAIL_DATA) ==
               0);
    pdidd->cbSize = sizeof(*pdidd);
    if (!::SetupDiGetDeviceInterfaceDetail(hdev, &did, pdidd, bufferSize,
                                           &bufferSize, nullptr)) {
      continue;
    }

    UniquePtr<PowerMeterDevice> pmd =
        MakeUnique<PowerMeterDevice>(pdidd->DevicePath);
    if (!pmd->HasChannels() ||
        !mPowerMeterDevices.emplaceBack(std::move(pmd))) {
      NS_WARNING("PowerMeterDevice without measurement channel (or OOM)");
    }
  }

  for (auto& device : mPowerMeterDevices) {
    device->AppendCountersTo(mCounters);
  }
}

PowerCounters::~PowerCounters() { mCounters.clear(); }

void PowerCounters::Sample() {
  for (auto& device : mPowerMeterDevices) {
    device->Sample();
  }
}
