use super::utils::{
    test_get_all_devices, test_get_all_onwed_devices, test_get_default_device,
    test_get_drift_compensations, test_get_master_device, DeviceFilter, Scope,
};
use super::*;

// AggregateDevice::set_sub_devices
// ------------------------------------
#[test]
#[should_panic]
fn test_aggregate_set_sub_devices_for_an_unknown_aggregate_device() {
    // If aggregate device id is kAudioObjectUnknown, we are unable to set device list.
    let default_input = test_get_default_device(Scope::Input);
    let default_output = test_get_default_device(Scope::Output);
    if default_input.is_none() || default_output.is_none() {
        panic!("No input or output device.");
    }

    let default_input = default_input.unwrap();
    let default_output = default_output.unwrap();
    assert!(
        AggregateDevice::set_sub_devices(kAudioObjectUnknown, default_input, default_output)
            .is_err()
    );
}

#[test]
#[should_panic]
fn test_aggregate_set_sub_devices_for_unknown_devices() {
    // If aggregate device id is kAudioObjectUnknown, we are unable to set device list.
    assert!(AggregateDevice::set_sub_devices(
        kAudioObjectUnknown,
        kAudioObjectUnknown,
        kAudioObjectUnknown
    )
    .is_err());
}

// AggregateDevice::get_sub_devices
// ------------------------------------
// You can check this by creating an aggregate device in `Audio MIDI Setup`
// application and print out the sub devices of them!
#[test]
fn test_aggregate_get_sub_devices() {
    let devices = test_get_all_devices(DeviceFilter::ExcludeCubebAggregateAndVPIO);
    for device in devices {
        // `AggregateDevice::get_sub_devices(device)` will return a single-element vector
        // containing `device` itself if it's not an aggregate device. This test assumes devices
        // is not an empty aggregate device (Test will panic when calling get_sub_devices with
        // an empty aggregate device).
        let sub_devices = AggregateDevice::get_sub_devices(device).unwrap();
        // TODO: If the device is a blank aggregate device, then the assertion fails!
        assert!(!sub_devices.is_empty());
    }
}

#[test]
#[should_panic]
fn test_aggregate_get_sub_devices_for_a_unknown_device() {
    let devices = AggregateDevice::get_sub_devices(kAudioObjectUnknown).unwrap();
    assert!(devices.is_empty());
}

// AggregateDevice::set_master_device
// ------------------------------------
#[test]
#[should_panic]
fn test_aggregate_set_master_device_for_an_unknown_aggregate_device() {
    assert!(AggregateDevice::set_master_device(kAudioObjectUnknown, kAudioObjectUnknown).is_err());
}

// AggregateDevice::activate_clock_drift_compensation
// ------------------------------------
#[test]
#[should_panic]
fn test_aggregate_activate_clock_drift_compensation_for_an_unknown_aggregate_device() {
    assert!(AggregateDevice::activate_clock_drift_compensation(kAudioObjectUnknown).is_err());
}

// AggregateDevice::destroy_device
// ------------------------------------
#[test]
#[should_panic]
fn test_aggregate_destroy_device_for_unknown_plugin_and_aggregate_devices() {
    assert!(AggregateDevice::destroy_device(kAudioObjectUnknown, kAudioObjectUnknown).is_err())
}

#[test]
#[should_panic]
fn test_aggregate_destroy_aggregate_device_for_a_unknown_aggregate_device() {
    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    assert!(AggregateDevice::destroy_device(plugin, kAudioObjectUnknown).is_err());
}

// Default Ignored Tests
// ================================================================================================
// The following tests that calls `AggregateDevice::create_blank_device` are marked `ignore` by
// default since the device-collection-changed callbacks will be fired upon
// `AggregateDevice::create_blank_device` is called (it will plug a new device in system!).
// Some tests rely on the device-collection-changed callbacks in a certain way. The callbacks
// fired from a unexpected `AggregateDevice::create_blank_device` will break those tests.

// AggregateDevice::create_blank_device_sync
// ------------------------------------
#[test]
#[ignore]
fn test_aggregate_create_blank_device() {
    // TODO: Test this when there is no available devices.
    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();
    let devices = test_get_all_devices(DeviceFilter::IncludeAll);
    let device = devices.into_iter().find(|dev| dev == &device).unwrap();
    let uid = get_device_global_uid(device).unwrap().into_string();
    assert!(uid.contains(PRIVATE_AGGREGATE_DEVICE_NAME));
    assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
}

// AggregateDevice::get_sub_devices
// ------------------------------------
#[test]
#[ignore]
#[should_panic]
fn test_aggregate_get_sub_devices_for_blank_aggregate_devices() {
    // TODO: Test this when there is no available devices.
    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();
    // There is no sub device in a blank aggregate device!
    // AggregateDevice::get_sub_devices guarantees returning a non-empty devices vector, so
    // the following call will panic!
    let sub_devices = AggregateDevice::get_sub_devices(device).unwrap();
    assert!(sub_devices.is_empty());
    assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
}

// AggregateDevice::set_sub_devices_sync
// ------------------------------------
#[test]
#[ignore]
fn test_aggregate_set_sub_devices() {
    let input_device = test_get_default_device(Scope::Input);
    let output_device = test_get_default_device(Scope::Output);
    if input_device.is_none() || output_device.is_none() || input_device == output_device {
        println!("No input or output device to create an aggregate device.");
        return;
    }

    let input_device = input_device.unwrap();
    let output_device = output_device.unwrap();

    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();
    assert!(AggregateDevice::set_sub_devices_sync(device, input_device, output_device).is_ok());

    let sub_devices = AggregateDevice::get_sub_devices(device).unwrap();
    let input_sub_devices = AggregateDevice::get_sub_devices(input_device).unwrap();
    let output_sub_devices = AggregateDevice::get_sub_devices(output_device).unwrap();

    // TODO: There may be overlapping devices between input_sub_devices and output_sub_devices,
    //       but now AggregateDevice::set_sub_devices will add them directly.
    assert_eq!(
        sub_devices.len(),
        input_sub_devices.len() + output_sub_devices.len()
    );
    for dev in &input_sub_devices {
        assert!(sub_devices.contains(dev));
    }
    for dev in &output_sub_devices {
        assert!(sub_devices.contains(dev));
    }

    let onwed_devices = test_get_all_onwed_devices(device);
    let onwed_device_uids = get_device_uids(&onwed_devices);
    let input_sub_device_uids = get_device_uids(&input_sub_devices);
    let output_sub_device_uids = get_device_uids(&output_sub_devices);
    for uid in &input_sub_device_uids {
        assert!(onwed_device_uids.contains(uid));
    }
    for uid in &output_sub_device_uids {
        assert!(onwed_device_uids.contains(uid));
    }

    assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
}

#[test]
#[ignore]
#[should_panic]
fn test_aggregate_set_sub_devices_for_unknown_input_devices() {
    let output_device = test_get_default_device(Scope::Output);
    if output_device.is_none() {
        panic!("Need a output device for the test!");
    }
    let output_device = output_device.unwrap();

    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();

    assert!(AggregateDevice::set_sub_devices(device, kAudioObjectUnknown, output_device).is_err());

    assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
}

#[test]
#[ignore]
#[should_panic]
fn test_aggregate_set_sub_devices_for_unknown_output_devices() {
    let input_device = test_get_default_device(Scope::Input);
    if input_device.is_none() {
        panic!("Need a input device for the test!");
    }
    let input_device = input_device.unwrap();

    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();

    assert!(AggregateDevice::set_sub_devices(device, input_device, kAudioObjectUnknown).is_err());

    assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
}

fn get_device_uids(devices: &Vec<AudioObjectID>) -> Vec<String> {
    devices
        .iter()
        .map(|device| get_device_global_uid(*device).unwrap().into_string())
        .collect()
}

// AggregateDevice::set_master_device
// ------------------------------------
#[test]
#[ignore]
fn test_aggregate_set_master_device() {
    let input_device = test_get_default_device(Scope::Input);
    let output_device = test_get_default_device(Scope::Output);
    if input_device.is_none() || output_device.is_none() || input_device == output_device {
        println!("No input or output device to create an aggregate device.");
        return;
    }

    let input_device = input_device.unwrap();
    let output_device = output_device.unwrap();

    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();
    assert!(AggregateDevice::set_sub_devices_sync(device, input_device, output_device).is_ok());
    assert!(AggregateDevice::set_master_device(device, output_device).is_ok());

    // Check if master is set to the first sub device of the default output device.
    let first_output_sub_device_uid =
        get_device_uid(AggregateDevice::get_sub_devices(device).unwrap()[0]);
    let master_device_uid = test_get_master_device(device);
    assert_eq!(first_output_sub_device_uid, master_device_uid);

    assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
}

#[test]
#[ignore]
fn test_aggregate_set_master_device_for_a_blank_aggregate_device() {
    let output_device = test_get_default_device(Scope::Output);
    if output_device.is_none() {
        println!("No output device to test.");
        return;
    }

    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();
    assert!(AggregateDevice::set_master_device(device, output_device.unwrap()).is_ok());

    // TODO: it's really weird the aggregate device actually own nothing
    //       but its master device can be set successfully!
    // The sub devices of this blank aggregate device (by `AggregateDevice::get_sub_devices`)
    // and the own devices (by `test_get_all_onwed_devices`) is empty since the size returned
    // from `audio_object_get_property_data_size` is 0.
    // The CFStringRef of the master device returned from `test_get_master_device` is actually
    // non-null.

    assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
}

fn get_device_uid(id: AudioObjectID) -> String {
    get_device_global_uid(id).unwrap().into_string()
}

// AggregateDevice::activate_clock_drift_compensation
// ------------------------------------
#[test]
#[ignore]
fn test_aggregate_activate_clock_drift_compensation() {
    let input_device = test_get_default_device(Scope::Input);
    let output_device = test_get_default_device(Scope::Output);
    if input_device.is_none() || output_device.is_none() || input_device == output_device {
        println!("No input or output device to create an aggregate device.");
        return;
    }

    let input_device = input_device.unwrap();
    let output_device = output_device.unwrap();

    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();
    assert!(AggregateDevice::set_sub_devices_sync(device, input_device, output_device).is_ok());
    assert!(AggregateDevice::set_master_device(device, output_device).is_ok());
    assert!(AggregateDevice::activate_clock_drift_compensation(device).is_ok());

    // Check the compensations.
    let devices = test_get_all_onwed_devices(device);
    let compensations = get_drift_compensations(&devices);
    assert!(!compensations.is_empty());
    assert_eq!(devices.len(), compensations.len());

    for (i, compensation) in compensations.iter().enumerate() {
        assert_eq!(*compensation, if i == 0 { 0 } else { DRIFT_COMPENSATION });
    }

    assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
}

#[test]
#[ignore]
fn test_aggregate_activate_clock_drift_compensation_for_an_aggregate_device_without_master_device()
{
    let input_device = test_get_default_device(Scope::Input);
    let output_device = test_get_default_device(Scope::Output);
    if input_device.is_none() || output_device.is_none() || input_device == output_device {
        println!("No input or output device to create an aggregate device.");
        return;
    }

    let input_device = input_device.unwrap();
    let output_device = output_device.unwrap();

    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();
    assert!(AggregateDevice::set_sub_devices_sync(device, input_device, output_device).is_ok());

    // TODO: Is the master device the first output sub device by default if we
    //       don't set that ? Is it because we add the output sub device list
    //       before the input's one ? (See implementation of
    //       AggregateDevice::set_sub_devices).
    let first_output_sub_device_uid =
        get_device_uid(AggregateDevice::get_sub_devices(output_device).unwrap()[0]);
    let master_device_uid = test_get_master_device(device);
    assert_eq!(first_output_sub_device_uid, master_device_uid);

    // Compensate the drift directly without setting master device.
    assert!(AggregateDevice::activate_clock_drift_compensation(device).is_ok());

    // Check the compensations.
    let devices = test_get_all_onwed_devices(device);
    let compensations = get_drift_compensations(&devices);
    assert!(!compensations.is_empty());
    assert_eq!(devices.len(), compensations.len());

    for (i, compensation) in compensations.iter().enumerate() {
        assert_eq!(*compensation, if i == 0 { 0 } else { DRIFT_COMPENSATION });
    }

    assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
}

#[test]
#[should_panic]
#[ignore]
fn test_aggregate_activate_clock_drift_compensation_for_a_blank_aggregate_device() {
    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();

    let sub_devices = AggregateDevice::get_sub_devices(device).unwrap();
    assert!(sub_devices.is_empty());
    let onwed_devices = test_get_all_onwed_devices(device);
    assert!(onwed_devices.is_empty());

    // Get a panic since no sub devices to be set compensation.
    assert!(AggregateDevice::activate_clock_drift_compensation(device).is_err());

    assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
}

fn get_drift_compensations(devices: &Vec<AudioObjectID>) -> Vec<u32> {
    assert!(!devices.is_empty());
    let mut compensations = Vec::new();
    for device in devices {
        let compensation = test_get_drift_compensations(*device).unwrap();
        compensations.push(compensation);
    }

    compensations
}

// AggregateDevice::destroy_device
// ------------------------------------
#[test]
#[ignore]
#[should_panic]
fn test_aggregate_destroy_aggregate_device_for_a_unknown_plugin_device() {
    let plugin = AggregateDevice::get_system_plugin_id().unwrap();
    let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();
    assert!(AggregateDevice::destroy_device(kAudioObjectUnknown, device).is_err());
}
