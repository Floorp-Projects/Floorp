use super::*;

pub fn get_device_uid(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<StringRef, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);

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

pub fn get_device_model_uid(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<StringRef, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);

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

pub fn get_device_streams(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<Vec<AudioStreamID>, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);

    let address = get_property_address(Property::DeviceStreams, devtype);

    let mut size: usize = 0;
    let err = audio_object_get_property_data_size(id, &address, &mut size);
    if err != NO_ERR {
        return Err(err);
    }

    let mut streams: Vec<AudioObjectID> = allocate_array_by_size(size);
    let err = audio_object_get_property_data(id, &address, &mut size, streams.as_mut_ptr());
    if err == NO_ERR {
        Ok(streams)
    } else {
        Err(err)
    }
}

pub fn get_device_sample_rate(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<f64, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);

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

pub fn get_stream_terminal_type(id: AudioStreamID) -> std::result::Result<u32, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);

    let address = get_property_address(
        Property::StreamTerminalType,
        DeviceType::INPUT | DeviceType::OUTPUT,
    );
    let mut size = mem::size_of::<u32>();
    let mut terminal_type: u32 = 0;
    let err = audio_object_get_property_data(id, &address, &mut size, &mut terminal_type);
    if err == NO_ERR {
        Ok(terminal_type)
    } else {
        Err(err)
    }
}

pub fn get_stream_virtual_format(
    id: AudioStreamID,
) -> std::result::Result<AudioStreamBasicDescription, OSStatus> {
    assert_ne!(id, kAudioObjectUnknown);

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
