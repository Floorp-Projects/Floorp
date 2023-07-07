use crate::MTLStorageMode;

/// See <https://developer.apple.com/documentation/metal/mtlcountersamplebufferdescriptor>
pub enum MTLCounterSampleBufferDescriptor {}

foreign_obj_type! {
    type CType = MTLCounterSampleBufferDescriptor;
    pub struct CounterSampleBufferDescriptor;
}

impl CounterSampleBufferDescriptor {
    pub fn new() -> Self {
        let class = class!(MTLCounterSampleBufferDescriptor);
        unsafe { msg_send![class, new] }
    }
}

impl CounterSampleBufferDescriptorRef {
    pub fn counter_set(&self) -> &CounterSetRef {
        unsafe { msg_send![self, counterSet] }
    }

    pub fn set_counter_set(&self, counter_set: &CounterSetRef) {
        unsafe { msg_send![self, setCounterSet: counter_set] }
    }

    pub fn label(&self) -> &str {
        unsafe {
            let label = msg_send![self, label];
            crate::nsstring_as_str(label)
        }
    }

    pub fn set_label(&self, label: &str) {
        unsafe {
            let nslabel = crate::nsstring_from_str(label);
            let () = msg_send![self, setLabel: nslabel];
        }
    }

    pub fn sample_count(&self) -> u64 {
        unsafe { msg_send![self, sampleCount] }
    }

    pub fn set_sample_count(&self, sample_count: u64) {
        unsafe { msg_send![self, setSampleCount: sample_count] }
    }

    pub fn storage_mode(&self) -> MTLStorageMode {
        unsafe { msg_send![self, storageMode] }
    }

    pub fn set_storage_mode(&self, storage_mode: MTLStorageMode) {
        unsafe { msg_send![self, setStorageMode: storage_mode] }
    }
}

/// See <https://developer.apple.com/documentation/metal/mtlcountersamplebuffer>
pub enum MTLCounterSampleBuffer {}

foreign_obj_type! {
    type CType = MTLCounterSampleBuffer;
    pub struct CounterSampleBuffer;
}

/// See <https://developer.apple.com/documentation/metal/mtlcounter>
pub enum MTLCounter {}

foreign_obj_type! {
    type CType = MTLCounter;
    pub struct Counter;
}

impl CounterRef {}

/// See <https://developer.apple.com/documentation/metal/mtlcounterset>
pub enum MTLCounterSet {}

foreign_obj_type! {
    type CType = MTLCounterSet;
    pub struct CounterSet;
}

impl CounterSetRef {
    pub fn name(&self) -> &str {
        unsafe {
            let name = msg_send![self, name];
            crate::nsstring_as_str(name)
        }
    }
}

/// See <https://developer.apple.com/documentation/metal/mtlcommoncounterset>
pub enum MTLCommonCounterSet {}

/// See <https://developer.apple.com/documentation/metal/mtlcommoncounter>
pub enum MTLCommonCounter {}

foreign_obj_type! {
    type CType = MTLCommonCounter;
    pub struct CommonCounter;
}
