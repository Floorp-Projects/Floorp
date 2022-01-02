use super::*;
use std::time::{SystemTime, UNIX_EPOCH};

pub const DRIFT_COMPENSATION: u32 = 1;

#[derive(Debug)]
pub struct AggregateDevice {
    plugin_id: AudioObjectID,
    device_id: AudioObjectID,
    // For log only
    input_id: AudioObjectID,
    output_id: AudioObjectID,
}

#[derive(Debug)]
pub enum Error {
    OS(OSStatus),
    Timeout(std::time::Duration),
    LessThan2Devices(usize),
}

impl From<OSStatus> for Error {
    fn from(status: OSStatus) -> Self {
        Error::OS(status)
    }
}

impl From<std::time::Duration> for Error {
    fn from(duration: std::time::Duration) -> Self {
        Error::Timeout(duration)
    }
}

impl From<usize> for Error {
    fn from(number: usize) -> Self {
        Error::LessThan2Devices(number)
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::OS(status) => write!(f, "OSStatus({})", status),
            Error::Timeout(duration) => write!(f, "Timeout({:?})", duration),
            Error::LessThan2Devices(number) => write!(f, "LessThan2Devices({} only)", number),
        }
    }
}

impl AggregateDevice {
    // Aggregate Device is a virtual audio interface which utilizes inputs and outputs
    // of one or more physical audio interfaces. It is possible to use the clock of
    // one of the devices as a master clock for all the combined devices and enable
    // drift compensation for the devices that are not designated clock master.
    //
    // Creating a new aggregate device programmatically requires [0][1]:
    // 1. Locate the base plug-in ("com.apple.audio.CoreAudio")
    // 2. Create a dictionary that describes the aggregate device
    //    (don't add sub-devices in that step, prone to fail [0])
    // 3. Ask the base plug-in to create the aggregate device (blank)
    // 4. Add the array of sub-devices.
    // 5. Set the master device (1st output device in our case)
    // 6. Enable drift compensation for the non-master devices
    //
    // [0] https://lists.apple.com/archives/coreaudio-api/2006/Apr/msg00092.html
    // [1] https://lists.apple.com/archives/coreaudio-api/2005/Jul/msg00150.html
    // [2] CoreAudio.framework/Headers/AudioHardware.h
    pub fn new(
        input_id: AudioObjectID,
        output_id: AudioObjectID,
    ) -> std::result::Result<Self, Error> {
        let plugin_id = Self::get_system_plugin_id()?;
        let device_id = Self::create_blank_device_sync(plugin_id)?;

        let mut cleanup = finally(|| {
            let r = Self::destroy_device(plugin_id, device_id);
            assert!(r.is_ok());
        });

        Self::set_sub_devices_sync(device_id, input_id, output_id)?;
        Self::set_master_device(device_id)?;
        Self::activate_clock_drift_compensation(device_id)?;
        Self::workaround_for_airpod(device_id, input_id, output_id)?;

        cleanup.dismiss();

        cubeb_log!(
            "Add devices input {} and output {} into an aggregate device {}",
            input_id,
            output_id,
            device_id
        );
        Ok(Self {
            plugin_id,
            device_id,
            input_id,
            output_id,
        })
    }

    pub fn get_device_id(&self) -> AudioObjectID {
        self.device_id
    }

    // The following APIs are set to `pub` for testing purpose.
    pub fn get_system_plugin_id() -> std::result::Result<AudioObjectID, Error> {
        let address = AudioObjectPropertyAddress {
            mSelector: kAudioHardwarePropertyPlugInForBundleID,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMaster,
        };

        let mut size: usize = 0;
        let status =
            audio_object_get_property_data_size(kAudioObjectSystemObject, &address, &mut size);
        if status != NO_ERR {
            return Err(Error::from(status));
        }
        assert_ne!(size, 0);

        let mut plugin_id = kAudioObjectUnknown;
        let mut in_bundle_ref = cfstringref_from_static_string("com.apple.audio.CoreAudio");
        let mut translation_value = AudioValueTranslation {
            mInputData: &mut in_bundle_ref as *mut CFStringRef as *mut c_void,
            mInputDataSize: mem::size_of::<CFStringRef>() as u32,
            mOutputData: &mut plugin_id as *mut AudioObjectID as *mut c_void,
            mOutputDataSize: mem::size_of::<AudioObjectID>() as u32,
        };
        assert_eq!(size, mem::size_of_val(&translation_value));

        let status = audio_object_get_property_data(
            kAudioObjectSystemObject,
            &address,
            &mut size,
            &mut translation_value,
        );
        unsafe {
            CFRelease(in_bundle_ref as *const c_void);
        }
        if status == NO_ERR {
            assert_ne!(plugin_id, kAudioObjectUnknown);
            Ok(plugin_id)
        } else {
            Err(Error::from(status))
        }
    }

    pub fn create_blank_device_sync(
        plugin_id: AudioObjectID,
    ) -> std::result::Result<AudioObjectID, Error> {
        let waiting_time = Duration::new(5, 0);

        let condvar_pair = Arc::new((Mutex::new(Vec::<AudioObjectID>::new()), Condvar::new()));
        let mut cloned_condvar_pair = condvar_pair.clone();
        let data_ptr = &mut cloned_condvar_pair as *mut Arc<(Mutex<Vec<AudioObjectID>>, Condvar)>;

        let address = get_property_address(
            Property::HardwareDevices,
            DeviceType::INPUT | DeviceType::OUTPUT,
        );

        let status = audio_object_add_property_listener(
            kAudioObjectSystemObject,
            &address,
            devices_changed_callback,
            data_ptr as *mut c_void,
        );
        assert_eq!(status, NO_ERR);

        let _teardown = finally(|| {
            let status = audio_object_remove_property_listener(
                kAudioObjectSystemObject,
                &address,
                devices_changed_callback,
                data_ptr as *mut c_void,
            );
            assert_eq!(status, NO_ERR);
        });

        let device = Self::create_blank_device(plugin_id)?;

        // Wait until the aggregate is created.
        let &(ref lock, ref cvar) = &*condvar_pair;
        let devices = lock.lock().unwrap();
        if !devices.contains(&device) {
            let (devs, timeout_res) = cvar.wait_timeout(devices, waiting_time).unwrap();
            if timeout_res.timed_out() {
                cubeb_log!(
                    "Time out for waiting the creation of aggregate device {}!",
                    device
                );
            }
            if !devs.contains(&device) {
                return Err(Error::from(waiting_time));
            }
        }

        extern "C" fn devices_changed_callback(
            id: AudioObjectID,
            _number_of_addresses: u32,
            _addresses: *const AudioObjectPropertyAddress,
            data: *mut c_void,
        ) -> OSStatus {
            assert_eq!(id, kAudioObjectSystemObject);
            let pair = unsafe { &mut *(data as *mut Arc<(Mutex<Vec<AudioObjectID>>, Condvar)>) };
            let &(ref lock, ref cvar) = &**pair;
            let mut devices = lock.lock().unwrap();
            *devices = audiounit_get_devices();
            cvar.notify_one();
            NO_ERR
        }

        Ok(device)
    }

    pub fn create_blank_device(
        plugin_id: AudioObjectID,
    ) -> std::result::Result<AudioObjectID, Error> {
        assert_ne!(plugin_id, kAudioObjectUnknown);

        let address = AudioObjectPropertyAddress {
            mSelector: kAudioPlugInCreateAggregateDevice,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMaster,
        };

        let mut size: usize = 0;
        let status = audio_object_get_property_data_size(plugin_id, &address, &mut size);
        if status != NO_ERR {
            return Err(Error::from(status));
        }
        assert_ne!(size, 0);

        let sys_time = SystemTime::now();
        let time_id = sys_time.duration_since(UNIX_EPOCH).unwrap().as_nanos();
        let device_name = format!("{}_{}", PRIVATE_AGGREGATE_DEVICE_NAME, time_id);
        let device_uid = format!("org.mozilla.{}", device_name);

        let mut device_id = kAudioObjectUnknown;
        let status = unsafe {
            let device_dict = CFMutableDictRef::default();

            // Set the name of the device.
            let device_name = cfstringref_from_string(&device_name);
            device_dict.add_value(
                cfstringref_from_static_string(AGGREGATE_DEVICE_NAME_KEY) as *const c_void,
                device_name as *const c_void,
            );
            CFRelease(device_name as *const c_void);

            // Set the uid of the device.
            let device_uid = cfstringref_from_string(&device_uid);
            device_dict.add_value(
                cfstringref_from_static_string(AGGREGATE_DEVICE_UID_KEY) as *const c_void,
                device_uid as *const c_void,
            );
            CFRelease(device_uid as *const c_void);

            // Make the device private to the process creating it.
            let private_value: i32 = 1;
            let device_private_key = CFNumberCreate(
                kCFAllocatorDefault,
                i64::from(kCFNumberIntType),
                &private_value as *const i32 as *const c_void,
            );
            device_dict.add_value(
                cfstringref_from_static_string(AGGREGATE_DEVICE_PRIVATE_KEY) as *const c_void,
                device_private_key as *const c_void,
            );
            CFRelease(device_private_key as *const c_void);

            // Set the device to a stacked aggregate (i.e. multi-output device).
            let stacked_value: i32 = 0; // 1 for normal aggregate device.
            let device_stacked_key = CFNumberCreate(
                kCFAllocatorDefault,
                i64::from(kCFNumberIntType),
                &stacked_value as *const i32 as *const c_void,
            );
            device_dict.add_value(
                cfstringref_from_static_string(AGGREGATE_DEVICE_STACKED_KEY) as *const c_void,
                device_stacked_key as *const c_void,
            );
            CFRelease(device_stacked_key as *const c_void);

            // This call will fire `audiounit_collection_changed_callback` indirectly!
            audio_object_get_property_data_with_qualifier(
                plugin_id,
                &address,
                mem::size_of_val(&device_dict),
                &device_dict,
                &mut size,
                &mut device_id,
            )
        };
        if status == NO_ERR {
            assert_ne!(device_id, kAudioObjectUnknown);
            Ok(device_id)
        } else {
            Err(Error::from(status))
        }
    }

    pub fn set_sub_devices_sync(
        device_id: AudioDeviceID,
        input_id: AudioDeviceID,
        output_id: AudioDeviceID,
    ) -> std::result::Result<(), Error> {
        let address = AudioObjectPropertyAddress {
            mSelector: kAudioAggregateDevicePropertyFullSubDeviceList,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMaster,
        };

        let waiting_time = Duration::new(5, 0);

        let condvar_pair = Arc::new((Mutex::new(AudioObjectID::default()), Condvar::new()));
        let mut cloned_condvar_pair = condvar_pair.clone();
        let data_ptr = &mut cloned_condvar_pair as *mut Arc<(Mutex<AudioObjectID>, Condvar)>;

        let status = audio_object_add_property_listener(
            device_id,
            &address,
            devices_changed_callback,
            data_ptr as *mut c_void,
        );
        if status != NO_ERR {
            return Err(Error::from(status));
        }

        let remove_listener = || -> OSStatus {
            audio_object_remove_property_listener(
                device_id,
                &address,
                devices_changed_callback,
                data_ptr as *mut c_void,
            )
        };

        Self::set_sub_devices(device_id, input_id, output_id)?;

        // Wait until the sub devices are added.
        let &(ref lock, ref cvar) = &*condvar_pair;
        let device = lock.lock().unwrap();
        if *device != device_id {
            let (dev, timeout_res) = cvar.wait_timeout(device, waiting_time).unwrap();
            if timeout_res.timed_out() {
                cubeb_log!(
                    "Time out for waiting for adding devices({}, {}) to aggregate device {}!",
                    input_id,
                    output_id,
                    device_id
                );
            }
            if *dev != device_id {
                let status = remove_listener();
                // If the error is kAudioHardwareBadObjectError, it implies `device_id` is somehow
                // dead, so its listener should receive nothing. It's ok to leave here.
                assert!(status == NO_ERR || status == (kAudioHardwareBadObjectError as OSStatus));
                // TODO: Destroy the aggregate device immediately if error is not
                // kAudioHardwareBadObjectError. Otherwise the `devices_changed_callback` is able
                // to touch the `cloned_condvar_pair` after it's freed.
                return Err(Error::from(waiting_time));
            }
        }

        extern "C" fn devices_changed_callback(
            id: AudioObjectID,
            _number_of_addresses: u32,
            _addresses: *const AudioObjectPropertyAddress,
            data: *mut c_void,
        ) -> OSStatus {
            let pair = unsafe { &mut *(data as *mut Arc<(Mutex<AudioObjectID>, Condvar)>) };
            let &(ref lock, ref cvar) = &**pair;
            let mut device = lock.lock().unwrap();
            *device = id;
            cvar.notify_one();
            NO_ERR
        }

        let status = remove_listener();
        assert_eq!(status, NO_ERR);
        Ok(())
    }

    pub fn set_sub_devices(
        device_id: AudioDeviceID,
        input_id: AudioDeviceID,
        output_id: AudioDeviceID,
    ) -> std::result::Result<(), Error> {
        assert_ne!(device_id, kAudioObjectUnknown);
        assert_ne!(input_id, kAudioObjectUnknown);
        assert_ne!(output_id, kAudioObjectUnknown);
        assert_ne!(input_id, output_id);

        let output_sub_devices = Self::get_sub_devices(output_id)?;
        let input_sub_devices = Self::get_sub_devices(input_id)?;

        unsafe {
            let sub_devices = CFArrayCreateMutable(ptr::null(), 0, &kCFTypeArrayCallBacks);
            // The order of the items in the array is significant and is used to determine the order of the streams
            // of the AudioAggregateDevice.
            for device in output_sub_devices {
                let uid = get_device_global_uid(device)?;
                CFArrayAppendValue(sub_devices, uid.get_raw() as *const c_void);
            }

            for device in input_sub_devices {
                let uid = get_device_global_uid(device)?;
                CFArrayAppendValue(sub_devices, uid.get_raw() as *const c_void);
            }

            let address = AudioObjectPropertyAddress {
                mSelector: kAudioAggregateDevicePropertyFullSubDeviceList,
                mScope: kAudioObjectPropertyScopeGlobal,
                mElement: kAudioObjectPropertyElementMaster,
            };

            let size = mem::size_of::<CFMutableArrayRef>();
            let status = audio_object_set_property_data(device_id, &address, size, &sub_devices);
            CFRelease(sub_devices as *const c_void);
            if status == NO_ERR {
                Ok(())
            } else {
                Err(Error::from(status))
            }
        }
    }

    pub fn get_sub_devices(
        device_id: AudioDeviceID,
    ) -> std::result::Result<Vec<AudioObjectID>, Error> {
        assert_ne!(device_id, kAudioObjectUnknown);

        let mut sub_devices = Vec::new();
        let address = AudioObjectPropertyAddress {
            mSelector: kAudioAggregateDevicePropertyActiveSubDeviceList,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMaster,
        };
        let mut size: usize = 0;
        let status = audio_object_get_property_data_size(device_id, &address, &mut size);

        if status == kAudioHardwareUnknownPropertyError as OSStatus {
            // Return a vector containing the device itself if the device has no sub devices.
            sub_devices.push(device_id);
            return Ok(sub_devices);
        } else if status != NO_ERR {
            return Err(Error::from(status));
        }

        assert_ne!(size, 0);

        let count = size / mem::size_of::<AudioObjectID>();
        sub_devices = allocate_array(count);
        let status = audio_object_get_property_data(
            device_id,
            &address,
            &mut size,
            sub_devices.as_mut_ptr(),
        );

        if status == NO_ERR {
            Ok(sub_devices)
        } else {
            Err(Error::from(status))
        }
    }

    pub fn set_master_device(device_id: AudioDeviceID) -> std::result::Result<(), Error> {
        assert_ne!(device_id, kAudioObjectUnknown);
        let address = AudioObjectPropertyAddress {
            mSelector: kAudioAggregateDevicePropertyMasterSubDevice,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMaster,
        };

        // Master become the 1st output sub device
        let output_device_id = audiounit_get_default_device_id(DeviceType::OUTPUT);
        assert_ne!(output_device_id, kAudioObjectUnknown);
        let output_sub_devices = Self::get_sub_devices(output_device_id)?;
        assert!(!output_sub_devices.is_empty());
        let master_sub_device_uid = get_device_global_uid(output_sub_devices[0]).unwrap();
        let master_sub_device = master_sub_device_uid.get_raw();
        let size = mem::size_of::<CFStringRef>();
        let status = audio_object_set_property_data(device_id, &address, size, &master_sub_device);
        if status == NO_ERR {
            Ok(())
        } else {
            Err(Error::from(status))
        }
    }

    pub fn activate_clock_drift_compensation(
        device_id: AudioObjectID,
    ) -> std::result::Result<(), Error> {
        assert_ne!(device_id, kAudioObjectUnknown);
        let address = AudioObjectPropertyAddress {
            mSelector: kAudioObjectPropertyOwnedObjects,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMaster,
        };

        let qualifier_data_size = mem::size_of::<AudioObjectID>();
        let class_id: AudioClassID = kAudioSubDeviceClassID;
        let qualifier_data = &class_id;

        let mut size: usize = 0;
        let status = audio_object_get_property_data_size_with_qualifier(
            device_id,
            &address,
            qualifier_data_size,
            qualifier_data,
            &mut size,
        );
        if status != NO_ERR {
            return Err(Error::from(status));
        }
        assert!(size > 0);
        let subdevices_num = size / mem::size_of::<AudioObjectID>();
        if subdevices_num < 2 {
            cubeb_log!(
                "Aggregate-device {} contains {} sub-devices only.\
                We should have at least one input and one output device.",
                device_id,
                subdevices_num
            );
            return Err(Error::LessThan2Devices(subdevices_num));
        }
        let mut sub_devices: Vec<AudioObjectID> = allocate_array(subdevices_num);
        let status = audio_object_get_property_data_with_qualifier(
            device_id,
            &address,
            qualifier_data_size,
            qualifier_data,
            &mut size,
            sub_devices.as_mut_ptr(),
        );
        if status != NO_ERR {
            return Err(Error::from(status));
        }

        let address = AudioObjectPropertyAddress {
            mSelector: kAudioSubDevicePropertyDriftCompensation,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMaster,
        };

        // Start from the second device since the first is the master clock
        for device in &sub_devices[1..] {
            let status = audio_object_set_property_data(
                *device,
                &address,
                mem::size_of::<u32>(),
                &DRIFT_COMPENSATION,
            );
            if status != NO_ERR {
                cubeb_log!(
                    "Failed to set drift compensation for {}. Ignore it.",
                    device
                );
            }
        }

        Ok(())
    }

    pub fn destroy_device(
        plugin_id: AudioObjectID,
        mut device_id: AudioDeviceID,
    ) -> std::result::Result<(), Error> {
        assert_ne!(plugin_id, kAudioObjectUnknown);
        assert_ne!(device_id, kAudioObjectUnknown);

        let address = AudioObjectPropertyAddress {
            mSelector: kAudioPlugInDestroyAggregateDevice,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMaster,
        };

        let mut size: usize = 0;
        let status = audio_object_get_property_data_size(plugin_id, &address, &mut size);
        if status != NO_ERR {
            return Err(Error::from(status));
        }
        assert!(size > 0);

        let status = audio_object_get_property_data(plugin_id, &address, &mut size, &mut device_id);
        if status == NO_ERR {
            Ok(())
        } else {
            Err(Error::from(status))
        }
    }

    pub fn workaround_for_airpod(
        device_id: AudioDeviceID,
        input_id: AudioDeviceID,
        output_id: AudioDeviceID,
    ) -> std::result::Result<(), Error> {
        assert_ne!(device_id, kAudioObjectUnknown);
        assert_ne!(input_id, kAudioObjectUnknown);
        assert_ne!(output_id, kAudioObjectUnknown);
        assert_ne!(input_id, output_id);

        let label = get_device_label(input_id, DeviceType::INPUT)?;
        let input_label = label.into_string();

        let label = get_device_label(output_id, DeviceType::OUTPUT)?;
        let output_label = label.into_string();

        if input_label.contains("AirPods") && output_label.contains("AirPods") {
            let input_rate =
                get_device_sample_rate(input_id, DeviceType::INPUT | DeviceType::OUTPUT)?;
            cubeb_log!(
                "The nominal rate of the input device {}: {}",
                input_id,
                input_rate
            );

            let output_rate =
                match get_device_sample_rate(output_id, DeviceType::INPUT | DeviceType::OUTPUT) {
                    Ok(rate) => format!("{}", rate),
                    Err(e) => format!("Error {}", e),
                };
            cubeb_log!(
                "The nominal rate of the output device {}: {}",
                output_id,
                output_rate
            );

            let addr = AudioObjectPropertyAddress {
                mSelector: kAudioDevicePropertyNominalSampleRate,
                mScope: kAudioObjectPropertyScopeGlobal,
                mElement: kAudioObjectPropertyElementMaster,
            };

            let status = audio_object_set_property_data(
                device_id,
                &addr,
                mem::size_of::<f64>(),
                &input_rate,
            );
            if status != NO_ERR {
                return Err(Error::from(status));
            }
        }

        Ok(())
    }
}

impl Default for AggregateDevice {
    fn default() -> Self {
        Self {
            plugin_id: kAudioObjectUnknown,
            device_id: kAudioObjectUnknown,
            input_id: kAudioObjectUnknown,
            output_id: kAudioObjectUnknown,
        }
    }
}

impl Drop for AggregateDevice {
    fn drop(&mut self) {
        if self.plugin_id != kAudioObjectUnknown && self.device_id != kAudioObjectUnknown {
            if let Err(r) = Self::destroy_device(self.plugin_id, self.device_id) {
                cubeb_log!(
                    "Failed to destroyed aggregate device {}. Error: {}",
                    self.device_id,
                    r
                );
            } else {
                cubeb_log!(
                    "Destroyed aggregate device {} (input {}, output {})",
                    self.device_id,
                    self.input_id,
                    self.output_id
                );
            }
        }
    }
}
