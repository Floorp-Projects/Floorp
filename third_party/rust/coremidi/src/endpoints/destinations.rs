use coremidi_sys::{
    ItemCount, MIDIEndpointDispose, MIDIGetDestination, MIDIGetNumberOfDestinations,
};

use std::ops::Deref;

use crate::{callback::BoxedCallback, object::Object, packets::PacketList};

use super::Endpoint;

/// A [MIDI source](https://developer.apple.com/reference/coremidi/midiendpointref) owned by an entity.
///
/// A source can be created from an index like this:
///
/// ```rust,no_run
/// let source = coremidi::Destination::from_index(0).unwrap();
/// println!("The source at index 0 has display name '{}'", source.display_name().unwrap());
/// ```
///
#[derive(Debug)]
pub struct Destination {
    pub(crate) endpoint: Endpoint,
}

impl Destination {
    /// Create a destination endpoint from its index.
    /// See [MIDIGetDestination](https://developer.apple.com/reference/coremidi/1495108-midigetdestination)
    ///
    pub fn from_index(index: usize) -> Option<Destination> {
        let endpoint_ref = unsafe { MIDIGetDestination(index as ItemCount) };
        match endpoint_ref {
            0 => None,
            _ => Some(Destination {
                endpoint: Endpoint {
                    object: Object(endpoint_ref),
                },
            }),
        }
    }
}

impl Deref for Destination {
    type Target = Endpoint;

    fn deref(&self) -> &Endpoint {
        &self.endpoint
    }
}

/// Destination endpoints available in the system.
///
/// The number of destinations available in the system can be retrieved with:
///
/// ```
/// let number_of_destinations = coremidi::Destinations::count();
/// ```
///
/// The destinations in the system can be iterated as:
///
/// ```rust,no_run
/// for destination in coremidi::Destinations {
///   println!("{}", destination.display_name().unwrap());
/// }
/// ```
///
pub struct Destinations;

impl Destinations {
    /// Get the number of destinations available in the system for sending MIDI messages.
    /// See [MIDIGetNumberOfDestinations](https://developer.apple.com/reference/coremidi/1495309-midigetnumberofdestinations).
    ///
    pub fn count() -> usize {
        unsafe { MIDIGetNumberOfDestinations() as usize }
    }
}

impl IntoIterator for Destinations {
    type Item = Destination;
    type IntoIter = DestinationsIterator;

    fn into_iter(self) -> Self::IntoIter {
        DestinationsIterator {
            index: 0,
            count: Self::count(),
        }
    }
}

pub struct DestinationsIterator {
    index: usize,
    count: usize,
}

impl Iterator for DestinationsIterator {
    type Item = Destination;

    fn next(&mut self) -> Option<Destination> {
        if self.index < self.count {
            let destination = Destination::from_index(self.index);
            self.index += 1;
            destination
        } else {
            None
        }
    }
}

/// A [MIDI virtual destination](https://developer.apple.com/reference/coremidi/1495347-mididestinationcreate) owned by a client.
///
/// A virtual destination can be created like:
///
/// ```rust,no_run
/// let client = coremidi::Client::new("example-client").unwrap();
/// client.virtual_destination("example-destination", |packet_list| println!("{}", packet_list)).unwrap();
/// ```
///
#[derive(Debug)]
pub struct VirtualDestination {
    // Note: the order is important here, endpoint needs to be dropped first
    pub(crate) endpoint: Endpoint,
    pub(crate) callback: BoxedCallback<PacketList>,
}

impl VirtualDestination {}

impl Deref for VirtualDestination {
    type Target = Endpoint;

    fn deref(&self) -> &Endpoint {
        &self.endpoint
    }
}

impl Drop for VirtualDestination {
    fn drop(&mut self) {
        unsafe { MIDIEndpointDispose(self.endpoint.object.0) };
    }
}
