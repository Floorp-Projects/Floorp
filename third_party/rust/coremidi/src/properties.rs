use std::mem::MaybeUninit;

use core_foundation::{
    base::{CFGetRetainCount, CFIndex, CFTypeRef, OSStatus, TCFType},
    string::{CFString, CFStringRef},
};

use coremidi_sys::*;

use crate::{object::Object, result_from_status, unit_result_from_status};

pub trait PropertyGetter<T> {
    fn value_from(&self, object: &Object) -> Result<T, OSStatus>;
}

pub trait PropertySetter<T> {
    fn set_value(&self, object: &Object, value: T) -> Result<(), OSStatus>;
}

/// Because Property structs can be constructed from strings that have been
/// passed in from the user or are constants CFStringRefs from CoreMidi, we
/// need to abstract over how we store their keys.
enum PropertyKeyStorage {
    Owned(CFString),
    Constant(CFStringRef),
}

impl PropertyKeyStorage {
    /// Return a raw CFStringRef pointing to this property key
    fn as_string_ref(&self) -> CFStringRef {
        match self {
            PropertyKeyStorage::Owned(owned) => owned.as_concrete_TypeRef(),
            PropertyKeyStorage::Constant(constant) => *constant,
        }
    }

    /// For checking the retain count when debugging
    #[allow(dead_code)]
    fn retain_count(&self) -> CFIndex {
        match self {
            PropertyKeyStorage::Owned(owned) => owned.retain_count(),
            PropertyKeyStorage::Constant(constant) => unsafe {
                CFGetRetainCount(*constant as CFTypeRef)
            },
        }
    }
}

/// A MIDI object property which value is an String
///
pub struct StringProperty(PropertyKeyStorage);

impl StringProperty {
    pub fn new(name: &str) -> Self {
        StringProperty(PropertyKeyStorage::Owned(CFString::new(name)))
    }

    /// Note: Should only be used internally with predefined CoreMidi constants,
    /// since it does not bump the retain count of the CFStringRef.
    fn from_constant_string_ref(string_ref: CFStringRef) -> Self {
        StringProperty(PropertyKeyStorage::Constant(string_ref))
    }
}

impl<T> PropertyGetter<T> for StringProperty
where
    T: From<String>,
{
    fn value_from(&self, object: &Object) -> Result<T, OSStatus> {
        let property_key = self.0.as_string_ref();
        let mut string_ref = MaybeUninit::uninit();
        let status =
            unsafe { MIDIObjectGetStringProperty(object.0, property_key, string_ref.as_mut_ptr()) };
        result_from_status(status, || {
            let string_ref = unsafe { string_ref.assume_init() };
            if string_ref.is_null() {
                return "".to_string().into();
            };
            let cf_string: CFString = unsafe { TCFType::wrap_under_create_rule(string_ref) };
            cf_string.to_string().into()
        })
    }
}

impl<'a, T> PropertySetter<T> for StringProperty
where
    T: Into<String>,
{
    fn set_value(&self, object: &Object, value: T) -> Result<(), OSStatus> {
        let property_key = self.0.as_string_ref();
        let value: String = value.into();
        let string = CFString::new(&value);
        let string_ref = string.as_concrete_TypeRef();
        let status = unsafe { MIDIObjectSetStringProperty(object.0, property_key, string_ref) };
        unit_result_from_status(status)
    }
}

/// A MIDI object property which value is an Integer
///
pub struct IntegerProperty(PropertyKeyStorage);

impl IntegerProperty {
    pub fn new(name: &str) -> Self {
        IntegerProperty(PropertyKeyStorage::Owned(CFString::new(name)))
    }

    /// Note: Should only be used internally with predefined CoreMidi constants,
    /// since it does not bump the retain count of the CFStringRef.
    fn from_constant_string_ref(string_ref: CFStringRef) -> Self {
        IntegerProperty(PropertyKeyStorage::Constant(string_ref))
    }
}

impl<T> PropertyGetter<T> for IntegerProperty
where
    T: From<SInt32>,
{
    fn value_from(&self, object: &Object) -> Result<T, OSStatus> {
        let property_key = self.0.as_string_ref();
        let mut value = MaybeUninit::uninit();
        let status =
            unsafe { MIDIObjectGetIntegerProperty(object.0, property_key, value.as_mut_ptr()) };
        result_from_status(status, || {
            let value = unsafe { value.assume_init() };
            value.into()
        })
    }
}

impl<T> PropertySetter<T> for IntegerProperty
where
    T: Into<SInt32>,
{
    fn set_value(&self, object: &Object, value: T) -> Result<(), OSStatus> {
        let property_key = self.0.as_string_ref();
        let status = unsafe { MIDIObjectSetIntegerProperty(object.0, property_key, value.into()) };
        unit_result_from_status(status)
    }
}

/// A MIDI object property which value is a Boolean
///
pub struct BooleanProperty(IntegerProperty);

impl BooleanProperty {
    pub fn new(name: &str) -> Self {
        BooleanProperty(IntegerProperty::new(name))
    }

    /// Note: Should only be used internally with predefined CoreMidi constants,
    /// since it does not bump the retain count of the CFStringRef.
    fn from_constant_string_ref(string_ref: CFStringRef) -> Self {
        BooleanProperty(IntegerProperty::from_constant_string_ref(string_ref))
    }
}

impl<T> PropertyGetter<T> for BooleanProperty
where
    T: From<bool>,
{
    fn value_from(&self, object: &Object) -> Result<T, OSStatus> {
        self.0
            .value_from(object)
            .map(|value: SInt32| (value == 1).into())
    }
}

impl<T> PropertySetter<T> for BooleanProperty
where
    T: Into<bool>,
{
    fn set_value(&self, object: &Object, value: T) -> Result<(), OSStatus> {
        let value: SInt32 = if value.into() { 1 } else { 0 };
        self.0.set_value(object, value)
    }
}

/// The set of properties that might be available for MIDI objects.
///
pub struct Properties;

impl Properties {
    /// See [kMIDIPropertyName](https://developer.apple.com/reference/coremidi/kmidipropertyname)
    pub fn name() -> StringProperty {
        StringProperty::from_constant_string_ref(unsafe { kMIDIPropertyName })
    }

    /// See [kMIDIPropertyManufacturer](https://developer.apple.com/reference/coremidi/kmidipropertymanufacturer)
    pub fn manufacturer() -> StringProperty {
        StringProperty::from_constant_string_ref(unsafe { kMIDIPropertyManufacturer })
    }

    /// See [kMIDIPropertyModel](https://developer.apple.com/reference/coremidi/kmidipropertymodel)
    pub fn model() -> StringProperty {
        StringProperty::from_constant_string_ref(unsafe { kMIDIPropertyModel })
    }

    /// See [kMIDIPropertyUniqueID](https://developer.apple.com/reference/coremidi/kmidipropertyuniqueid)
    pub fn unique_id() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertyUniqueID })
    }

    /// See [kMIDIPropertyDeviceID](https://developer.apple.com/reference/coremidi/kmidipropertydeviceid)
    pub fn device_id() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertyDeviceID })
    }

    /// See [kMIDIPropertyReceiveChannels](https://developer.apple.com/reference/coremidi/kmidipropertyreceivechannels)
    pub fn receive_channels() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertyReceiveChannels })
    }

    /// See [kMIDIPropertyTransmitChannels](https://developer.apple.com/reference/coremidi/kmidipropertytransmitchannels)
    pub fn transmit_channels() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertyTransmitChannels })
    }

    /// See [kMIDIPropertyMaxSysExSpeed](https://developer.apple.com/reference/coremidi/kmidipropertymaxsysexspeed)
    pub fn max_sysex_speed() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertyMaxSysExSpeed })
    }

    /// See [kMIDIPropertyAdvanceScheduleTimeMuSec](https://developer.apple.com/reference/coremidi/kMIDIPropertyAdvanceScheduleTimeMuSec)
    pub fn advance_schedule_time_musec() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertyAdvanceScheduleTimeMuSec })
    }

    /// See [kMIDIPropertyIsEmbeddedEntity](https://developer.apple.com/reference/coremidi/kMIDIPropertyIsEmbeddedEntity)
    pub fn is_embedded_entity() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyIsEmbeddedEntity })
    }

    /// See [kMIDIPropertyIsBroadcast](https://developer.apple.com/reference/coremidi/kMIDIPropertyIsBroadcast)
    pub fn is_broadcast() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyIsBroadcast })
    }

    /// See [kMIDIPropertySingleRealtimeEntity](https://developer.apple.com/reference/coremidi/kMIDIPropertySingleRealtimeEntity)
    pub fn single_realtime_entity() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertySingleRealtimeEntity })
    }

    /// See [kMIDIPropertyConnectionUniqueID](https://developer.apple.com/reference/coremidi/kMIDIPropertyConnectionUniqueID)
    pub fn connection_unique_id() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertyConnectionUniqueID })
    }

    /// See [kMIDIPropertyOffline](https://developer.apple.com/reference/coremidi/kMIDIPropertyOffline)
    pub fn offline() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyOffline })
    }

    /// See [kMIDIPropertyPrivate](https://developer.apple.com/reference/coremidi/kMIDIPropertyPrivate)
    pub fn private() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyPrivate })
    }

    /// See [kMIDIPropertyDriverOwner](https://developer.apple.com/reference/coremidi/kMIDIPropertyDriverOwner)
    pub fn driver_owner() -> StringProperty {
        StringProperty::from_constant_string_ref(unsafe { kMIDIPropertyDriverOwner })
    }

    // /// See [kMIDIPropertyNameConfiguration](https://developer.apple.com/reference/coremidi/kMIDIPropertyNameConfiguration)
    // pub fn name_configuration() -> Property { unsafe { Property(kMIDIPropertyNameConfiguration) } }

    // /// See [kMIDIPropertyImage](https://developer.apple.com/reference/coremidi/kMIDIPropertyImage)
    // pub fn image() -> Property { unsafe { Property(kMIDIPropertyImage) } }

    /// See [kMIDIPropertyDriverVersion](https://developer.apple.com/reference/coremidi/kMIDIPropertyDriverVersion)
    pub fn driver_version() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertyDriverVersion })
    }

    /// See [kMIDIPropertySupportsGeneralMIDI](https://developer.apple.com/reference/coremidi/kMIDIPropertySupportsGeneralMIDI)
    pub fn supports_general_midi() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertySupportsGeneralMIDI })
    }

    /// See [kMIDIPropertySupportsMMC](https://developer.apple.com/reference/coremidi/kMIDIPropertySupportsMMC)
    pub fn supports_mmc() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertySupportsMMC })
    }

    /// See [kMIDIPropertyCanRoute](https://developer.apple.com/reference/coremidi/kMIDIPropertyCanRoute)
    pub fn can_route() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyCanRoute })
    }

    /// See [kMIDIPropertyReceivesClock](https://developer.apple.com/reference/coremidi/kMIDIPropertyReceivesClock)
    pub fn receives_clock() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyReceivesClock })
    }

    /// See [kMIDIPropertyReceivesMTC](https://developer.apple.com/reference/coremidi/kMIDIPropertyReceivesMTC)
    pub fn receives_mtc() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyReceivesMTC })
    }

    /// See [kMIDIPropertyReceivesNotes](https://developer.apple.com/reference/coremidi/kMIDIPropertyReceivesNotes)
    pub fn receives_notes() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyReceivesNotes })
    }

    /// See [kMIDIPropertyReceivesProgramChanges](https://developer.apple.com/reference/coremidi/kMIDIPropertyReceivesProgramChanges)
    pub fn receives_program_changes() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyReceivesProgramChanges })
    }

    /// See [kMIDIPropertyReceivesBankSelectMSB](https://developer.apple.com/reference/coremidi/kMIDIPropertyReceivesBankSelectMSB)
    pub fn receives_bank_select_msb() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyReceivesBankSelectMSB })
    }

    /// See [kMIDIPropertyReceivesBankSelectLSB](https://developer.apple.com/reference/coremidi/kMIDIPropertyReceivesBankSelectLSB)
    pub fn receives_bank_select_lsb() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyReceivesBankSelectLSB })
    }

    /// See [kMIDIPropertyTransmitsBankSelectMSB](https://developer.apple.com/reference/coremidi/kMIDIPropertyTransmitsBankSelectMSB)
    pub fn transmits_bank_select_msb() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyTransmitsBankSelectMSB })
    }

    /// See [kMIDIPropertyTransmitsBankSelectLSB](https://developer.apple.com/reference/coremidi/kMIDIPropertyTransmitsBankSelectLSB)
    pub fn transmits_bank_select_lsb() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyTransmitsBankSelectLSB })
    }

    /// See [kMIDIPropertyTransmitsClock](https://developer.apple.com/reference/coremidi/kMIDIPropertyTransmitsClock)
    pub fn transmits_clock() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyTransmitsClock })
    }

    /// See [kMIDIPropertyTransmitsMTC](https://developer.apple.com/reference/coremidi/kMIDIPropertyTransmitsMTC)
    pub fn transmits_mtc() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyTransmitsMTC })
    }

    /// See [kMIDIPropertyTransmitsNotes](https://developer.apple.com/reference/coremidi/kMIDIPropertyTransmitsNotes)
    pub fn transmits_notes() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyTransmitsNotes })
    }

    /// See [kMIDIPropertyTransmitsProgramChanges](https://developer.apple.com/reference/coremidi/kMIDIPropertyTransmitsProgramChanges)
    pub fn transmits_program_changes() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyTransmitsProgramChanges })
    }

    /// See [kMIDIPropertyPanDisruptsStereo](https://developer.apple.com/reference/coremidi/kMIDIPropertyPanDisruptsStereo)
    pub fn pan_disrupts_stereo() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyPanDisruptsStereo })
    }

    /// See [kMIDIPropertyIsSampler](https://developer.apple.com/reference/coremidi/kMIDIPropertyIsSampler)
    pub fn is_sampler() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyIsSampler })
    }

    /// See [kMIDIPropertyIsDrumMachine](https://developer.apple.com/reference/coremidi/kMIDIPropertyIsDrumMachine)
    pub fn is_drum_machine() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyIsDrumMachine })
    }

    /// See [kMIDIPropertyIsMixer](https://developer.apple.com/reference/coremidi/kMIDIPropertyIsMixer)
    pub fn is_mixer() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyIsMixer })
    }

    /// See [kMIDIPropertyIsEffectUnit](https://developer.apple.com/reference/coremidi/kMIDIPropertyIsEffectUnit)
    pub fn is_effect_unit() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertyIsEffectUnit })
    }

    /// See [kMIDIPropertyMaxReceiveChannels](https://developer.apple.com/reference/coremidi/kMIDIPropertyMaxReceiveChannels)
    pub fn max_receive_channels() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertyMaxReceiveChannels })
    }

    /// See [kMIDIPropertyMaxTransmitChannels](https://developer.apple.com/reference/coremidi/kMIDIPropertyMaxTransmitChannels)
    pub fn max_transmit_channels() -> IntegerProperty {
        IntegerProperty::from_constant_string_ref(unsafe { kMIDIPropertyMaxTransmitChannels })
    }

    /// See [kMIDIPropertyDriverDeviceEditorApp](https://developer.apple.com/reference/coremidi/kMIDIPropertyDriverDeviceEditorApp)
    pub fn driver_device_editor_app() -> StringProperty {
        StringProperty::from_constant_string_ref(unsafe { kMIDIPropertyDriverDeviceEditorApp })
    }

    /// See [kMIDIPropertySupportsShowControl](https://developer.apple.com/reference/coremidi/kMIDIPropertySupportsShowControl)
    pub fn supports_show_control() -> BooleanProperty {
        BooleanProperty::from_constant_string_ref(unsafe { kMIDIPropertySupportsShowControl })
    }

    /// See [kMIDIPropertyDisplayName](https://developer.apple.com/reference/coremidi/kMIDIPropertyDisplayName)
    pub fn display_name() -> StringProperty {
        StringProperty::from_constant_string_ref(unsafe { kMIDIPropertyDisplayName })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::{endpoints::destinations::VirtualDestination, Client};

    const NAME_ORIG: &str = "A";

    fn setup() -> (Client, VirtualDestination) {
        let client = Client::new("Test Client").unwrap();
        let dest = client.virtual_destination(NAME_ORIG, |_| ()).unwrap();
        (client, dest)
    }

    mod string {
        use super::*;

        const NAME_MODIFIED: &str = "B";

        // Test getting the original value of the "name" property
        fn check_get_original(property: &StringProperty, dest: &VirtualDestination) {
            let name: String = property.value_from(dest).unwrap();

            assert_eq!(name, NAME_ORIG);
        }

        // Test setting then getting the "name" property
        fn check_roundtrip(property: &StringProperty, dest: &VirtualDestination) {
            property.set_value(dest, NAME_MODIFIED).unwrap();
            let name: String = property.value_from(dest).unwrap();

            assert_eq!(name, NAME_MODIFIED);
        }

        #[test]
        fn test_from_constant() {
            let (_client, dest) = setup();
            let property = Properties::name();

            check_get_original(&property, &dest);
            check_roundtrip(&property, &dest);
        }

        #[test]
        fn test_new() {
            let (_client, dest) = setup();
            // "name" is the value of the CoreMidi constant kMIDIPropertyName
            let property = StringProperty::new("name");

            check_get_original(&property, &dest);
            check_roundtrip(&property, &dest);
        }
    }

    mod integer {
        use super::*;

        const ADVANCED_SCHEDULE_TIME: i32 = 44;

        #[test]
        fn test_not_set() {
            let (_client, dest) = setup();
            // Is not set by default for Virtual Destinations
            let property = Properties::advance_schedule_time_musec();

            let value: Result<i32, _> = property.value_from(&dest);

            assert!(value.is_err())
        }

        #[test]
        fn test_roundtrip() {
            let (_client, dest) = setup();
            let property = Properties::advance_schedule_time_musec();

            property.set_value(&dest, ADVANCED_SCHEDULE_TIME).unwrap();
            let num: i32 = property.value_from(&dest).unwrap();

            assert_eq!(num, ADVANCED_SCHEDULE_TIME);
        }
    }

    mod boolean {
        use super::*;

        #[test]
        fn test_not_set() {
            let (_client, dest) = setup();
            // Not set by default on Virtual Destinations
            let property = Properties::transmits_program_changes();

            let value: Result<bool, _> = property.value_from(&dest);

            assert!(value.is_err())
        }

        #[test]
        fn test_roundtrip() {
            let (_client, dest) = setup();
            let property = Properties::private();

            property.set_value(&dest, true).unwrap();
            let value: bool = property.value_from(&dest).unwrap();

            assert_eq!(value, true);
        }
    }
}
