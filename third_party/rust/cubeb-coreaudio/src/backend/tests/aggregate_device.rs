use super::utils::{
    test_get_all_devices, test_get_all_onwed_devices, test_get_default_device,
    test_get_drift_compensations, test_get_master_device, DeviceFilter, Scope,
};
use super::*;
use std::iter::zip;
use std::panic;

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
        run_serially_forward_panics(|| AggregateDevice::set_sub_devices(
            kAudioObjectUnknown,
            default_input,
            default_output
        ))
        .is_err()
    );
}

#[test]
#[should_panic]
fn test_aggregate_set_sub_devices_for_unknown_devices() {
    run_serially_forward_panics(|| {
        // If aggregate device id is kAudioObjectUnknown, we are unable to set device list.
        assert!(AggregateDevice::set_sub_devices(
            kAudioObjectUnknown,
            kAudioObjectUnknown,
            kAudioObjectUnknown
        )
        .is_err());
    });
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
        println!(
            "get_sub_devices({}={})",
            device,
            run_serially_forward_panics(|| get_device_uid(device))
        );
        let sub_devices =
            run_serially_forward_panics(|| AggregateDevice::get_sub_devices(device).unwrap());
        // TODO: If the device is a blank aggregate device, then the assertion fails!
        assert!(!sub_devices.is_empty());
    }
}

#[test]
#[should_panic]
fn test_aggregate_get_sub_devices_for_a_unknown_device() {
    run_serially_forward_panics(|| {
        let devices = AggregateDevice::get_sub_devices(kAudioObjectUnknown).unwrap();
        assert!(devices.is_empty());
    });
}

// AggregateDevice::set_master_device
// ------------------------------------
#[test]
#[should_panic]
fn test_aggregate_set_master_device_for_an_unknown_aggregate_device() {
    run_serially_forward_panics(|| {
        assert!(
            AggregateDevice::set_master_device(kAudioObjectUnknown, kAudioObjectUnknown).is_err()
        );
    });
}

// AggregateDevice::activate_clock_drift_compensation
// ------------------------------------
#[test]
#[should_panic]
fn test_aggregate_activate_clock_drift_compensation_for_an_unknown_aggregate_device() {
    run_serially_forward_panics(|| {
        assert!(AggregateDevice::activate_clock_drift_compensation(kAudioObjectUnknown).is_err());
    });
}

// AggregateDevice::destroy_device
// ------------------------------------
#[test]
#[should_panic]
fn test_aggregate_destroy_device_for_unknown_plugin_and_aggregate_devices() {
    run_serially_forward_panics(|| {
        assert!(AggregateDevice::destroy_device(kAudioObjectUnknown, kAudioObjectUnknown).is_err())
    });
}

#[test]
#[should_panic]
fn test_aggregate_destroy_aggregate_device_for_a_unknown_aggregate_device() {
    run_serially_forward_panics(|| {
        let plugin = AggregateDevice::get_system_plugin_id().unwrap();
        assert!(AggregateDevice::destroy_device(plugin, kAudioObjectUnknown).is_err());
    });
}

// AggregateDevice::create_blank_device_sync
// ------------------------------------
#[test]
fn test_aggregate_create_blank_device() {
    // TODO: Test this when there is no available devices.
    let plugin = run_serially(|| AggregateDevice::get_system_plugin_id()).unwrap();
    let device = run_serially(|| AggregateDevice::create_blank_device_sync(plugin)).unwrap();
    let devices = test_get_all_devices(DeviceFilter::IncludeAll);
    let device = devices.into_iter().find(|dev| dev == &device).unwrap();
    let uid = run_serially(|| get_device_global_uid(device).unwrap().into_string());
    assert!(uid.contains(PRIVATE_AGGREGATE_DEVICE_NAME));
    assert!(run_serially(|| AggregateDevice::destroy_device(plugin, device)).is_ok());
}

// AggregateDevice::get_sub_devices
// ------------------------------------
#[test]
#[should_panic]
fn test_aggregate_get_sub_devices_for_blank_aggregate_devices() {
    run_serially_forward_panics(|| {
        // TODO: Test this when there is no available devices.
        let plugin = AggregateDevice::get_system_plugin_id().unwrap();
        let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();
        // There is no sub device in a blank aggregate device!
        // AggregateDevice::get_sub_devices guarantees returning a non-empty devices vector, so
        // the following call will panic!
        let sub_devices = AggregateDevice::get_sub_devices(device).unwrap();
        assert!(sub_devices.is_empty());
        assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
    });
}

// AggregateDevice::set_sub_devices_sync
// ------------------------------------
#[test]
fn test_aggregate_set_sub_devices() {
    let input_device = test_get_default_device(Scope::Input);
    let output_device = test_get_default_device(Scope::Output);
    if input_device.is_none() || output_device.is_none() || input_device == output_device {
        println!("No input or output device to create an aggregate device.");
        return;
    }

    let input_device = input_device.unwrap();
    let output_device = output_device.unwrap();

    let plugin = run_serially(|| AggregateDevice::get_system_plugin_id()).unwrap();
    let device = run_serially(|| AggregateDevice::create_blank_device_sync(plugin)).unwrap();
    assert!(run_serially(|| AggregateDevice::set_sub_devices_sync(
        device,
        input_device,
        output_device
    ))
    .is_ok());

    let sub_devices = run_serially(|| AggregateDevice::get_sub_devices(device)).unwrap();
    let input_sub_devices =
        run_serially(|| AggregateDevice::get_sub_devices(input_device)).unwrap();
    let output_sub_devices =
        run_serially(|| AggregateDevice::get_sub_devices(output_device)).unwrap();

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

    let onwed_devices = run_serially(|| test_get_all_onwed_devices(device));
    let onwed_device_uids = run_serially(|| get_device_uids(&onwed_devices));
    let input_sub_device_uids = run_serially(|| get_device_uids(&input_sub_devices));
    let output_sub_device_uids = run_serially(|| get_device_uids(&output_sub_devices));
    for uid in &input_sub_device_uids {
        assert!(onwed_device_uids.contains(uid));
    }
    for uid in &output_sub_device_uids {
        assert!(onwed_device_uids.contains(uid));
    }

    assert!(run_serially(|| AggregateDevice::destroy_device(plugin, device)).is_ok());
}

#[test]
#[should_panic]
fn test_aggregate_set_sub_devices_for_unknown_input_devices() {
    let output_device = test_get_default_device(Scope::Output);
    if output_device.is_none() {
        panic!("Need a output device for the test!");
    }
    let output_device = output_device.unwrap();

    run_serially_forward_panics(|| {
        let plugin = AggregateDevice::get_system_plugin_id().unwrap();
        let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();

        assert!(
            AggregateDevice::set_sub_devices(device, kAudioObjectUnknown, output_device).is_err()
        );

        assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
    });
}

#[test]
#[should_panic]
fn test_aggregate_set_sub_devices_for_unknown_output_devices() {
    let input_device = test_get_default_device(Scope::Input);
    if input_device.is_none() {
        panic!("Need a input device for the test!");
    }
    let input_device = input_device.unwrap();

    run_serially_forward_panics(|| {
        let plugin = AggregateDevice::get_system_plugin_id().unwrap();
        let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();

        assert!(
            AggregateDevice::set_sub_devices(device, input_device, kAudioObjectUnknown).is_err()
        );

        assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
    });
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
fn test_aggregate_set_master_device() {
    let input_device = test_get_default_device(Scope::Input);
    let output_device = test_get_default_device(Scope::Output);
    if input_device.is_none() || output_device.is_none() || input_device == output_device {
        println!("No input or output device to create an aggregate device.");
        return;
    }

    let input_device = input_device.unwrap();
    let output_device = output_device.unwrap();

    let plugin = run_serially(|| AggregateDevice::get_system_plugin_id()).unwrap();
    let device = run_serially(|| AggregateDevice::create_blank_device_sync(plugin)).unwrap();
    assert!(run_serially(|| AggregateDevice::set_sub_devices_sync(
        device,
        input_device,
        output_device
    ))
    .is_ok());
    assert!(run_serially(|| AggregateDevice::set_master_device(device, output_device)).is_ok());

    let output_sub_devices =
        run_serially(|| AggregateDevice::get_sub_devices(output_device)).unwrap();
    let first_output_sub_device_uid = run_serially(|| get_device_uid(output_sub_devices[0]));

    // Check that the first sub device of the output device is set as master device.
    let master_device_uid = run_serially(|| test_get_master_device(device));
    assert_eq!(first_output_sub_device_uid, master_device_uid);

    assert!(run_serially(|| AggregateDevice::destroy_device(plugin, device)).is_ok());
}

#[test]
fn test_aggregate_set_master_device_for_a_blank_aggregate_device() {
    let output_device = test_get_default_device(Scope::Output);
    if output_device.is_none() {
        println!("No output device to test.");
        return;
    }

    let plugin = run_serially(|| AggregateDevice::get_system_plugin_id()).unwrap();
    let device = run_serially(|| AggregateDevice::create_blank_device_sync(plugin)).unwrap();
    assert!(
        run_serially(|| AggregateDevice::set_master_device(device, output_device.unwrap())).is_ok()
    );

    // TODO: it's really weird the aggregate device actually own nothing
    //       but its master device can be set successfully!
    // The sub devices of this blank aggregate device (by `AggregateDevice::get_sub_devices`)
    // and the own devices (by `test_get_all_onwed_devices`) is empty since the size returned
    // from `audio_object_get_property_data_size` is 0.
    // The CFStringRef of the master device returned from `test_get_master_device` is actually
    // non-null.

    assert!(run_serially(|| AggregateDevice::destroy_device(plugin, device)).is_ok());
}

fn get_device_uid(id: AudioObjectID) -> String {
    get_device_global_uid(id).map_or(String::new(), |uid| uid.into_string())
}

// AggregateDevice::activate_clock_drift_compensation
// ------------------------------------
#[test]
fn test_aggregate_activate_clock_drift_compensation() {
    let input_device = test_get_default_device(Scope::Input);
    let output_device = test_get_default_device(Scope::Output);
    if input_device.is_none() || output_device.is_none() || input_device == output_device {
        println!("No input or output device to create an aggregate device.");
        return;
    }

    let input_device = input_device.unwrap();
    let output_device = output_device.unwrap();

    let plugin = run_serially(|| AggregateDevice::get_system_plugin_id()).unwrap();
    let device = run_serially(|| AggregateDevice::create_blank_device_sync(plugin)).unwrap();
    assert!(run_serially(|| AggregateDevice::set_sub_devices_sync(
        device,
        input_device,
        output_device
    ))
    .is_ok());
    assert!(run_serially(|| AggregateDevice::set_master_device(device, output_device)).is_ok());
    assert!(run_serially(|| AggregateDevice::activate_clock_drift_compensation(device)).is_ok());

    // Check the compensations.
    let devices = run_serially(|| test_get_all_onwed_devices(device));
    let compensations = run_serially(|| get_drift_compensations(&devices));
    let master_device_uid = run_serially(|| test_get_master_device(device));
    assert!(!compensations.is_empty());
    assert_eq!(devices.len(), compensations.len());

    for (device, compensation) in zip(devices, compensations) {
        let uid = get_device_uid(device);
        assert_eq!(
            compensation,
            if uid == master_device_uid {
                0
            } else {
                DRIFT_COMPENSATION
            }
        );
    }

    assert!(run_serially(|| AggregateDevice::destroy_device(plugin, device)).is_ok());
}

#[test]
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

    let plugin = run_serially(|| AggregateDevice::get_system_plugin_id()).unwrap();
    let device = run_serially(|| AggregateDevice::create_blank_device_sync(plugin)).unwrap();
    assert!(run_serially(|| AggregateDevice::set_sub_devices_sync(
        device,
        input_device,
        output_device
    ))
    .is_ok());

    // The master device is by default the first sub device in the list.
    // This happens to be the first sub device of the input device, see implementation of
    // AggregateDevice::set_sub_devices.
    let first_input_sub_device_uid =
        run_serially(|| get_device_uid(AggregateDevice::get_sub_devices(input_device).unwrap()[0]));
    let first_sub_device_uid =
        run_serially(|| get_device_uid(AggregateDevice::get_sub_devices(device).unwrap()[0]));
    assert_eq!(first_input_sub_device_uid, first_sub_device_uid);
    let master_device_uid = run_serially(|| test_get_master_device(device));
    assert_eq!(first_sub_device_uid, master_device_uid);

    // Compensate the drift directly without setting master device.
    assert!(run_serially(|| AggregateDevice::activate_clock_drift_compensation(device)).is_ok());

    // Check the compensations.
    let devices = run_serially(|| test_get_all_onwed_devices(device));
    let compensations = run_serially(|| get_drift_compensations(&devices));
    assert!(!compensations.is_empty());
    assert_eq!(devices.len(), compensations.len());

    for (i, compensation) in compensations.iter().enumerate() {
        assert_eq!(*compensation, if i == 0 { 0 } else { DRIFT_COMPENSATION });
    }

    assert!(run_serially(|| AggregateDevice::destroy_device(plugin, device)).is_ok());
}

#[test]
#[should_panic]
fn test_aggregate_activate_clock_drift_compensation_for_a_blank_aggregate_device() {
    run_serially_forward_panics(|| {
        let plugin = AggregateDevice::get_system_plugin_id().unwrap();
        let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();

        let sub_devices = AggregateDevice::get_sub_devices(device).unwrap();
        assert!(sub_devices.is_empty());
        let onwed_devices = test_get_all_onwed_devices(device);
        assert!(onwed_devices.is_empty());

        // Get a panic since no sub devices to be set compensation.
        assert!(AggregateDevice::activate_clock_drift_compensation(device).is_err());

        assert!(AggregateDevice::destroy_device(plugin, device).is_ok());
    });
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
#[should_panic]
fn test_aggregate_destroy_aggregate_device_for_a_unknown_plugin_device() {
    run_serially_forward_panics(|| {
        let plugin = AggregateDevice::get_system_plugin_id().unwrap();
        let device = AggregateDevice::create_blank_device_sync(plugin).unwrap();
        assert!(AggregateDevice::destroy_device(kAudioObjectUnknown, device).is_err());
    });
}

// AggregateDevice::new
// ------------------------------------
#[test]
fn test_aggregate_new() {
    let input_device = test_get_default_device(Scope::Input);
    let output_device = test_get_default_device(Scope::Output);
    if input_device.is_none() || output_device.is_none() || input_device == output_device {
        println!("No input or output device to create an aggregate device.");
        return;
    }

    run_serially_forward_panics(|| {
        let input_device = input_device.unwrap();
        let output_device = output_device.unwrap();

        let aggr = AggregateDevice::new(input_device, output_device).unwrap();

        // Check main device
        let output_sub_devices = AggregateDevice::get_sub_devices(output_device).unwrap();
        let first_output_sub_device_uid = get_device_uid(output_sub_devices[0]);
        let master_device_uid = test_get_master_device(aggr.get_device_id());
        assert_eq!(first_output_sub_device_uid, master_device_uid);

        // Check drift compensation
        let devices = test_get_all_onwed_devices(aggr.get_device_id());
        let compensations = get_drift_compensations(&devices);
        assert!(!compensations.is_empty());
        assert_eq!(devices.len(), compensations.len());

        let device_uids = devices.iter().map(|&id| get_device_uid(id));
        for (uid, compensation) in zip(device_uids, compensations) {
            assert_eq!(
                compensation,
                if uid == master_device_uid {
                    0
                } else {
                    DRIFT_COMPENSATION
                },
                "Unexpected drift value for device with uid {}",
                uid
            );
        }
    });
}
