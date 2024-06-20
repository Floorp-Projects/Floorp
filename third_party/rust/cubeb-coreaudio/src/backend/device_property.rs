use super::*;

pub fn get_device_uid(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<StringRef, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::DeviceUID, devtype);
    let mut size = mem::size_of::<CFStringRef>();
    let mut uid: CFStringRef = ptr::null();
    let err = audio_object_get_property_data(id, &address, &mut size, &mut uid);
    if err == NO_ERR {
        Ok(StringRef::new(uid as _))
    } else {
        Err(err)
    }
}

pub fn get_devices() -> Vec<AudioObjectID> {
    debug_assert_running_serially();
    let mut size: usize = 0;
    let address = get_property_address(
        Property::HardwareDevices,
        DeviceType::INPUT | DeviceType::OUTPUT,
    );
    let mut ret =
        audio_object_get_property_data_size(kAudioObjectSystemObject, &address, &mut size);
    if ret != NO_ERR {
        return Vec::new();
    }
    // Total number of input and output devices.
    let mut devices: Vec<AudioObjectID> = allocate_array_by_size(size);
    ret = audio_object_get_property_data(
        kAudioObjectSystemObject,
        &address,
        &mut size,
        devices.as_mut_ptr(),
    );
    if ret != NO_ERR {
        return Vec::new();
    }
    devices
}

pub fn get_device_model_uid(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<StringRef, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::ModelUID, devtype);
    let mut size = mem::size_of::<CFStringRef>();
    let mut uid: CFStringRef = ptr::null();
    let err = audio_object_get_property_data(id, &address, &mut size, &mut uid);
    if err == NO_ERR {
        Ok(StringRef::new(uid as _))
    } else {
        Err(err)
    }
}

pub fn get_device_transport_type(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<u32, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::TransportType, devtype);
    let mut size = mem::size_of::<u32>();
    let mut transport: u32 = 0;
    let err = audio_object_get_property_data(id, &address, &mut size, &mut transport);
    if err == NO_ERR {
        Ok(transport)
    } else {
        Err(err)
    }
}

pub fn get_device_source(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<u32, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::DeviceSource, devtype);
    let mut size = mem::size_of::<u32>();
    let mut source: u32 = 0;
    let err = audio_object_get_property_data(id, &address, &mut size, &mut source);
    if err == NO_ERR {
        Ok(source)
    } else {
        Err(err)
    }
}

pub fn get_device_source_name(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<StringRef, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let mut source: u32 = get_device_source(id, devtype)?;
    let address = get_property_address(Property::DeviceSourceName, devtype);
    let mut size = mem::size_of::<AudioValueTranslation>();
    let mut name: CFStringRef = ptr::null();
    let mut trl = AudioValueTranslation {
        mInputData: &mut source as *mut u32 as *mut c_void,
        mInputDataSize: mem::size_of::<u32>() as u32,
        mOutputData: &mut name as *mut CFStringRef as *mut c_void,
        mOutputDataSize: mem::size_of::<CFStringRef>() as u32,
    };
    let err = audio_object_get_property_data(id, &address, &mut size, &mut trl);
    if err == NO_ERR {
        Ok(StringRef::new(name as _))
    } else {
        Err(err)
    }
}

pub fn get_device_name(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<StringRef, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::DeviceName, devtype);
    let mut size = mem::size_of::<CFStringRef>();
    let mut name: CFStringRef = ptr::null();
    let err = audio_object_get_property_data(id, &address, &mut size, &mut name);
    if err == NO_ERR {
        Ok(StringRef::new(name as _))
    } else {
        Err(err)
    }
}

pub fn get_device_manufacturer(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<StringRef, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::DeviceManufacturer, devtype);
    let mut size = mem::size_of::<CFStringRef>();
    let mut manufacturer: CFStringRef = ptr::null();
    let err = audio_object_get_property_data(id, &address, &mut size, &mut manufacturer);
    if err == NO_ERR {
        Ok(StringRef::new(manufacturer as _))
    } else {
        Err(err)
    }
}

pub fn get_device_buffer_frame_size_range(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<AudioValueRange, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::DeviceBufferFrameSizeRange, devtype);
    let mut size = mem::size_of::<AudioValueRange>();
    let mut range = AudioValueRange::default();
    let err = audio_object_get_property_data(id, &address, &mut size, &mut range);
    if err == NO_ERR {
        Ok(range)
    } else {
        Err(err)
    }
}

pub fn get_device_latency(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<u32, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::DeviceLatency, devtype);
    let mut size = mem::size_of::<u32>();
    let mut latency: u32 = 0;
    let err = audio_object_get_property_data(id, &address, &mut size, &mut latency);
    if err == NO_ERR {
        Ok(latency)
    } else {
        Err(err)
    }
}

#[derive(Debug)]
pub struct DeviceStream {
    pub device: AudioDeviceID,
    pub stream: AudioStreamID,
}
pub fn get_device_streams(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<Vec<DeviceStream>, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::DeviceStreams, devtype);

    let mut size: usize = 0;
    let err = audio_object_get_property_data_size(id, &address, &mut size);
    if err != NO_ERR {
        return Err(err);
    }

    let mut streams = vec![AudioObjectID::default(); size / mem::size_of::<AudioObjectID>()];
    let err = audio_object_get_property_data(id, &address, &mut size, streams.as_mut_ptr());
    if err != NO_ERR {
        return Err(err);
    }

    let mut device_streams = streams
        .into_iter()
        .map(|stream| DeviceStream { device: id, stream })
        .collect::<Vec<_>>();
    if devtype.contains(DeviceType::INPUT) {
        // With VPIO, output devices will/may get a Tap that appears as an input stream on the
        // output device id. It is unclear what kind of Tap this is as it cannot be enumerated
        // as a Tap through the public APIs. There is no property on the stream itself that
        // can consistently identify it as originating from another device's output either.
        // TerminalType gets close but is often kAudioStreamTerminalTypeUnknown, and there are
        // cases reported where real input streams have that TerminalType, too.
        // See Firefox bug 1890186.
        // We rely on AudioObjectID order instead. AudioDeviceID and AudioStreamID (and more)
        // are all AudioObjectIDs underneath, and they're all distinct. The Tap streams
        // mentioned above are created when VPIO is created, and their AudioObjectIDs are higher
        // than the VPIO device's AudioObjectID, but lower than the next *real* device's
        // AudioObjectID.
        // Simplified, a device's native streams have AudioObjectIDs higher than their device's
        // AudioObjectID but lower than the next device's AudioObjectID.
        // We use this to filter streams, and hope that it holds across macOS versions.
        // Note that for aggregate devices this does not hold, as their stream IDs seem to be
        // repurposed by VPIO. We sum up the result of the above algorithm for each of their sub
        // devices instead, as that seems to hold.
        let mut devices = get_devices();
        let sub_devices = AggregateDevice::get_sub_devices(id);
        if let Ok(sub_device_ids) = sub_devices {
            cubeb_log!(
                "Getting input device streams for aggregate device {}. Summing over sub devices {:?}.",
                id,
                sub_device_ids
            );
            return Ok(sub_device_ids
                .into_iter()
                .filter_map(|sub_id| match sub_id {
                    #[allow(non_upper_case_globals)]
                    kAudioObjectUnknown => None,
                    i => get_device_streams(i, devtype).ok(),
                })
                .flatten()
                .collect());
        }
        debug_assert!(devices.contains(&id));
        devices.sort();
        let next_id = devices.into_iter().skip_while(|&i| i != id).nth(1);
        cubeb_log!(
            "Filtering input streams {:?} for device {}. Next device is {:?}.",
            device_streams
                .iter()
                .map(|ds| ds.stream)
                .collect::<Vec<_>>(),
            id,
            next_id
        );
        if let Some(next_id) = next_id {
            device_streams.retain(|ds| ds.stream > id && ds.stream < next_id);
        } else {
            device_streams.retain(|ds| ds.stream > id);
        }
        cubeb_log!(
            "Input stream filtering for device {} retained {:?}.",
            id,
            device_streams
                .iter()
                .map(|ds| ds.stream)
                .collect::<Vec<_>>()
        );
    }

    Ok(device_streams)
}

pub fn get_device_sample_rate(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<f64, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::DeviceSampleRate, devtype);
    let mut size = mem::size_of::<f64>();
    let mut rate: f64 = 0.0;
    let err = audio_object_get_property_data(id, &address, &mut size, &mut rate);
    if err == NO_ERR {
        Ok(rate)
    } else {
        Err(err)
    }
}

pub fn get_ranges_of_device_sample_rate(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<Vec<AudioValueRange>, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::DeviceSampleRates, devtype);

    let mut size: usize = 0;
    let err = audio_object_get_property_data_size(id, &address, &mut size);
    if err != NO_ERR {
        return Err(err);
    }

    let mut ranges: Vec<AudioValueRange> = allocate_array_by_size(size);
    let err = audio_object_get_property_data(id, &address, &mut size, ranges.as_mut_ptr());
    if err == NO_ERR {
        Ok(ranges)
    } else {
        Err(err)
    }
}

pub fn get_stream_latency(id: AudioStreamID) -> std::result::Result<u32, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(
        Property::StreamLatency,
        DeviceType::INPUT | DeviceType::OUTPUT,
    );
    let mut size = mem::size_of::<u32>();
    let mut latency: u32 = 0;
    let err = audio_object_get_property_data(id, &address, &mut size, &mut latency);
    if err == NO_ERR {
        Ok(latency)
    } else {
        Err(err)
    }
}

pub fn get_stream_virtual_format(
    id: AudioStreamID,
) -> std::result::Result<AudioStreamBasicDescription, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(
        Property::StreamVirtualFormat,
        DeviceType::INPUT | DeviceType::OUTPUT,
    );
    let mut size = mem::size_of::<AudioStreamBasicDescription>();
    let mut format = AudioStreamBasicDescription::default();
    let err = audio_object_get_property_data(id, &address, &mut size, &mut format);
    if err == NO_ERR {
        Ok(format)
    } else {
        Err(err)
    }
}

pub fn get_clock_domain(
    id: AudioStreamID,
    devtype: DeviceType,
) -> std::result::Result<u32, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);
    debug_assert_running_serially();

    let address = get_property_address(Property::ClockDomain, devtype);
    let mut size = mem::size_of::<u32>();
    let mut clock_domain: u32 = 0;
    let err = audio_object_get_property_data(id, &address, &mut size, &mut clock_domain);
    if err == NO_ERR {
        Ok(clock_domain)
    } else {
        Err(err)
    }
}

pub enum Property {
    DeviceBufferFrameSizeRange,
    DeviceIsAlive,
    DeviceLatency,
    DeviceManufacturer,
    DeviceName,
    DeviceSampleRate,
    DeviceSampleRates,
    DeviceSource,
    DeviceSourceName,
    DeviceStreams,
    DeviceUID,
    HardwareDefaultInputDevice,
    HardwareDefaultOutputDevice,
    HardwareDevices,
    ModelUID,
    StreamLatency,
    StreamTerminalType,
    StreamVirtualFormat,
    TransportType,
    ClockDomain,
}

impl From<Property> for AudioObjectPropertySelector {
    fn from(p: Property) -> Self {
        match p {
            Property::DeviceBufferFrameSizeRange => kAudioDevicePropertyBufferFrameSizeRange,
            Property::DeviceIsAlive => kAudioDevicePropertyDeviceIsAlive,
            Property::DeviceLatency => kAudioDevicePropertyLatency,
            Property::DeviceManufacturer => kAudioObjectPropertyManufacturer,
            Property::DeviceName => kAudioObjectPropertyName,
            Property::DeviceSampleRate => kAudioDevicePropertyNominalSampleRate,
            Property::DeviceSampleRates => kAudioDevicePropertyAvailableNominalSampleRates,
            Property::DeviceSource => kAudioDevicePropertyDataSource,
            Property::DeviceSourceName => kAudioDevicePropertyDataSourceNameForIDCFString,
            Property::DeviceStreams => kAudioDevicePropertyStreams,
            Property::DeviceUID => kAudioDevicePropertyDeviceUID,
            Property::HardwareDefaultInputDevice => kAudioHardwarePropertyDefaultInputDevice,
            Property::HardwareDefaultOutputDevice => kAudioHardwarePropertyDefaultOutputDevice,
            Property::HardwareDevices => kAudioHardwarePropertyDevices,
            Property::ModelUID => kAudioDevicePropertyModelUID,
            Property::StreamLatency => kAudioStreamPropertyLatency,
            Property::StreamTerminalType => kAudioStreamPropertyTerminalType,
            Property::StreamVirtualFormat => kAudioStreamPropertyVirtualFormat,
            Property::TransportType => kAudioDevicePropertyTransportType,
            Property::ClockDomain => kAudioDevicePropertyClockDomain,
        }
    }
}

pub fn get_property_address(property: Property, devtype: DeviceType) -> AudioObjectPropertyAddress {
    const GLOBAL: ffi::cubeb_device_type =
        ffi::CUBEB_DEVICE_TYPE_INPUT | ffi::CUBEB_DEVICE_TYPE_OUTPUT;
    let scope = match devtype.bits() {
        ffi::CUBEB_DEVICE_TYPE_INPUT => kAudioDevicePropertyScopeInput,
        ffi::CUBEB_DEVICE_TYPE_OUTPUT => kAudioDevicePropertyScopeOutput,
        GLOBAL => kAudioObjectPropertyScopeGlobal,
        _ => panic!("Invalid type"),
    };
    AudioObjectPropertyAddress {
        mSelector: property.into(),
        mScope: scope,
        mElement: kAudioObjectPropertyElementMaster,
    }
}
