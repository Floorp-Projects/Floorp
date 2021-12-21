use coremidi_sys::MIDIPacketList;
use coremidi_sys::{MIDIPacket, MIDIPacketNext, MIDITimeStamp};

use std::fmt;
use std::ops::{Deref, DerefMut};
use std::slice;

pub type Timestamp = u64;

const MAX_PACKET_DATA_LENGTH: usize = 0xffffusize;

#[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
pub mod alignment {
    pub type Marker = [u32; 0]; // ensures 4-byte alignment (on ARM)
    pub const NEEDS_ALIGNMENT: bool = true;
}

#[cfg(not(any(target_arch = "arm", target_arch = "aarch64")))]
pub mod alignment {
    pub type Marker = [u8; 0]; // unaligned
    pub const NEEDS_ALIGNMENT: bool = false;
}

/// A collection of simultaneous MIDI events.
/// See [MIDIPacket](https://developer.apple.com/reference/coremidi/midipacket).
///
#[repr(C)]
pub struct Packet {
    // NOTE: At runtime this type must only be used behind immutable references
    //       that point to valid instances of MIDIPacket (mutable references would allow mem::swap).
    //       This type must NOT implement `Copy`!
    //       On ARM, this must be 4-byte aligned.
    inner: PacketInner,
    _alignment_marker: alignment::Marker,
}

#[repr(packed)]
struct PacketInner {
    timestamp: MIDITimeStamp,
    length: u16,
    data: [u8; 0], // zero-length, because we cannot make this type bigger without knowing how much data there actually is
}

impl Packet {
    /// Get the packet timestamp.
    ///
    pub fn timestamp(&self) -> Timestamp {
        self.inner.timestamp as Timestamp
    }

    /// Get the packet data. This method just gives raw MIDI bytes. You would need another
    /// library to decode them and work with higher level events.
    ///
    ///
    /// The following example:
    ///
    /// ```
    /// let packet_list = &coremidi::PacketBuffer::new(0, &[0x90, 0x40, 0x7f]);
    /// for packet in packet_list.iter() {
    ///   for byte in packet.data() {
    ///     print!(" {:x}", byte);
    ///   }
    /// }
    /// ```
    ///
    /// will print:
    ///
    /// ```text
    ///  90 40 7f
    /// ```
    pub fn data(&self) -> &[u8] {
        let data_ptr = self.inner.data.as_ptr();
        let data_len = self.inner.length as usize;
        unsafe { slice::from_raw_parts(data_ptr, data_len) }
    }
}

impl fmt::Debug for Packet {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let result = write!(
            f,
            "Packet(ptr={:x}, ts={:016x}, data=[",
            self as *const _ as usize,
            self.timestamp() as u64
        );
        let result = self
            .data()
            .iter()
            .enumerate()
            .fold(result, |prev_result, (i, b)| match prev_result {
                Err(err) => Err(err),
                Ok(()) => {
                    let sep = if i > 0 { ", " } else { "" };
                    write!(f, "{}{:02x}", sep, b)
                }
            });
        result.and_then(|_| write!(f, "])"))
    }
}

impl fmt::Display for Packet {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let result = write!(f, "{:016x}:", self.timestamp());
        self.data()
            .iter()
            .fold(result, |prev_result, b| match prev_result {
                Err(err) => Err(err),
                Ok(()) => write!(f, " {:02x}", b),
            })
    }
}

/// A [list of MIDI events](https://developer.apple.com/reference/coremidi/midipacketlist) being received from, or being sent to, one endpoint.
///
#[repr(C)]
pub struct PacketList {
    // NOTE: This type must only exist in the form of immutable references
    //       pointing to valid instances of MIDIPacketList.
    //       This type must NOT implement `Copy`!
    inner: PacketListInner,
    _do_not_construct: alignment::Marker,
}

#[repr(packed)]
struct PacketListInner {
    num_packets: u32,
    data: [MIDIPacket; 0],
}

impl PacketList {
    /// For internal usage only.
    /// Requires this instance to actually point to a valid MIDIPacketList
    pub(crate) unsafe fn as_ptr(&self) -> *mut MIDIPacketList {
        self as *const PacketList as *mut PacketList as *mut MIDIPacketList
    }
}

impl PacketList {
    /// Check if the packet list is empty.
    ///
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Get the number of packets in the list.
    ///
    pub fn len(&self) -> usize {
        self.inner.num_packets as usize
    }

    /// Get an iterator for the packets in the list.
    ///
    pub fn iter(&self) -> PacketListIterator {
        PacketListIterator {
            count: self.len(),
            packet_ptr: std::ptr::addr_of!(self.inner.data) as *const MIDIPacket,
            _phantom: ::std::marker::PhantomData::default(),
        }
    }
}

impl fmt::Debug for PacketList {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let result = write!(f, "PacketList(ptr={:x}, packets=[", unsafe {
            self.as_ptr() as usize
        });
        self.iter()
            .enumerate()
            .fold(result, |prev_result, (i, packet)| match prev_result {
                Err(err) => Err(err),
                Ok(()) => {
                    let sep = if i != 0 { ", " } else { "" };
                    write!(f, "{}{:?}", sep, packet)
                }
            })
            .and_then(|_| write!(f, "])"))
    }
}

impl fmt::Display for PacketList {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let num_packets = self.inner.num_packets;
        let result = write!(f, "PacketList(len={})", num_packets);
        self.iter()
            .fold(result, |prev_result, packet| match prev_result {
                Err(err) => Err(err),
                Ok(()) => write!(f, "\n  {}", packet),
            })
    }
}

pub struct PacketListIterator<'a> {
    count: usize,
    packet_ptr: *const MIDIPacket,
    _phantom: ::std::marker::PhantomData<&'a Packet>,
}

impl<'a> Iterator for PacketListIterator<'a> {
    type Item = &'a Packet;

    fn next(&mut self) -> Option<&'a Packet> {
        if self.count > 0 {
            let packet = unsafe { &*(self.packet_ptr as *const Packet) };
            self.count -= 1;
            self.packet_ptr = unsafe { MIDIPacketNext(self.packet_ptr) };
            Some(packet)
        } else {
            None
        }
    }
}

const PACKET_LIST_HEADER_SIZE: usize = 4; // MIDIPacketList::numPackets: UInt32
const PACKET_HEADER_SIZE: usize = 8 +      // MIDIPacket::timeStamp: MIDITimeStamp/UInt64
                                  2; // MIDIPacket::length: UInt16

const INLINE_PACKET_BUFFER_SIZE: usize = 28; // must be divisible by 4

enum PacketBufferStorage {
    /// Inline stores the data directy on the stack, if it is small enough.
    /// NOTE: using u32 ensures correct alignment (required on ARM)
    Inline([u32; INLINE_PACKET_BUFFER_SIZE / 4]),
    /// External is used whenever the size of the data exceeds INLINE_PACKET_BUFFER_SIZE.
    /// This means that the size of the contained vector is always greater than INLINE_PACKET_BUFFER_SIZE.
    External(Vec<u32>),
}

impl PacketBufferStorage {
    #[inline]
    pub fn with_capacity(capacity: usize) -> PacketBufferStorage {
        if capacity <= INLINE_PACKET_BUFFER_SIZE {
            PacketBufferStorage::Inline([0; INLINE_PACKET_BUFFER_SIZE / 4])
        } else {
            let u32_len = ((capacity - 1) / 4) + 1;
            let mut buffer = Vec::with_capacity(u32_len);
            unsafe {
                buffer.set_len(u32_len);
            }
            PacketBufferStorage::External(buffer)
        }
    }

    #[inline]
    fn capacity(&self) -> usize {
        match *self {
            PacketBufferStorage::Inline(ref inline) => inline.len() * 4,
            PacketBufferStorage::External(ref vec) => vec.len() * 4,
        }
    }

    #[inline]
    fn get_slice(&self) -> &[u8] {
        unsafe {
            match *self {
                PacketBufferStorage::Inline(ref inline) => {
                    slice::from_raw_parts(inline.as_ptr() as *const u8, inline.len() * 4)
                }
                PacketBufferStorage::External(ref vec) => {
                    slice::from_raw_parts(vec.as_ptr() as *const u8, vec.len() * 4)
                }
            }
        }
    }

    #[inline]
    fn get_slice_mut(&mut self) -> &mut [u8] {
        unsafe {
            match *self {
                PacketBufferStorage::Inline(ref mut inline) => {
                    slice::from_raw_parts_mut(inline.as_mut_ptr() as *mut u8, inline.len() * 4)
                }
                PacketBufferStorage::External(ref mut vec) => {
                    slice::from_raw_parts_mut(vec.as_mut_ptr() as *mut u8, vec.len() * 4)
                }
            }
        }
    }

    unsafe fn assign_packet(&mut self, packet_offset: usize, time: MIDITimeStamp, data: &[u8]) {
        assert!(data.len() <= MAX_PACKET_DATA_LENGTH, "packet data too long"); // cannot store longer size in u16

        if alignment::NEEDS_ALIGNMENT {
            debug_assert!(packet_offset & 0b11 == 0);
        }

        let slice = self.get_slice_mut();
        let ptr = slice[packet_offset..].as_mut_ptr() as *mut Packet;
        (*ptr).inner.timestamp = time;
        (*ptr).inner.length = data.len() as u16;
        let packet_data_start = packet_offset + PACKET_HEADER_SIZE;
        slice[packet_data_start..(packet_data_start + data.len())].copy_from_slice(data);
    }

    /// Requires that there is a valid Packet at `offset`, which has enough space for `data`
    unsafe fn extend_packet(&mut self, packet_offset: usize, data: &[u8]) {
        let slice = self.get_slice_mut();
        let ptr = slice[packet_offset..].as_mut_ptr() as *mut Packet;
        let packet_data_start = packet_offset + PACKET_HEADER_SIZE + (*ptr).inner.length as usize;
        (*ptr).inner.length += data.len() as u16;
        slice[packet_data_start..(packet_data_start + data.len())].copy_from_slice(data);
    }

    /// Call this only with larger length values (won't make the buffer smaller)
    unsafe fn ensure_capacity(&mut self, capacity: usize) {
        if capacity < INLINE_PACKET_BUFFER_SIZE || capacity < self.get_slice().len() {
            return;
        }

        let vec_capacity = ((capacity - 1) / 4) + 1;
        let vec: Option<Vec<u32>> = match *self {
            PacketBufferStorage::Inline(ref inline) => {
                let mut v = Vec::with_capacity(vec_capacity);
                v.extend_from_slice(inline);
                v.set_len(vec_capacity);
                Some(v)
            }
            PacketBufferStorage::External(ref mut vec) => {
                let current_len = vec.len();
                vec.reserve(vec_capacity - current_len);
                vec.set_len(vec_capacity);
                None
            }
        };

        // to prevent borrowcheck errors, this must come after the `match`
        if let Some(v) = vec {
            *self = PacketBufferStorage::External(v);
        }
    }
}

impl Deref for PacketBufferStorage {
    type Target = PacketList;

    #[inline]
    fn deref(&self) -> &PacketList {
        unsafe { &*(self.get_slice().as_ptr() as *const PacketList) }
    }
}

impl DerefMut for PacketBufferStorage {
    // NOTE: Mutable references `&mut PacketList` must not be exposed in the public API!
    //       The user could use mem::swap to modify the header without modifying the packets that follow.
    #[inline]
    fn deref_mut(&mut self) -> &mut PacketList {
        unsafe { &mut *(self.get_slice_mut().as_mut_ptr() as *mut PacketList) }
    }
}

/// A mutable `PacketList` builder.
///
/// A `PacketList` is an inmmutable reference to a [MIDIPacketList](https://developer.apple.com/reference/coremidi/midipacketlist) structure,
/// while a `PacketBuffer` is a mutable structure that allows to build a `PacketList` by adding packets.
/// It dereferences to a `PacketList`, so it can be used whenever a `PacketList` is needed.
///
pub struct PacketBuffer {
    storage: PacketBufferStorage,
    last_packet_offset: usize,
}

impl Deref for PacketBuffer {
    type Target = PacketList;

    #[inline]
    fn deref(&self) -> &PacketList {
        self.storage.deref()
    }
}

impl PacketBuffer {
    /// Create a `PacketBuffer` with a single packet containing the provided timestamp and data.
    ///
    /// According to the official documentation for CoreMIDI, the timestamp represents
    /// the time at which the events are to be played, where zero means "now".
    /// The timestamp applies to the first MIDI byte in the packet.
    ///
    /// Example on how to create a `PacketBuffer` with a single packet for a MIDI note on for C-5:
    ///
    /// ```
    /// let buffer = coremidi::PacketBuffer::new(0, &[0x90, 0x3c, 0x7f]);
    /// assert_eq!(buffer.len(), 1)
    /// ```
    pub fn new(time: MIDITimeStamp, data: &[u8]) -> PacketBuffer {
        let capacity = data.len() + PACKET_LIST_HEADER_SIZE + PACKET_HEADER_SIZE;
        let mut storage = PacketBufferStorage::with_capacity(capacity);
        storage.deref_mut().inner.num_packets = 1;
        let last_packet_offset = PACKET_LIST_HEADER_SIZE;
        unsafe {
            storage.assign_packet(last_packet_offset, time, data);
        }

        PacketBuffer {
            storage,
            last_packet_offset,
        }
    }

    /// Create an empty `PacketBuffer` with no packets.
    ///
    /// Example on how to create an empty `PacketBuffer`
    /// with a capacity for 128 bytes in total (including headers):
    ///
    /// ```
    /// let buffer = coremidi::PacketBuffer::with_capacity(128);
    /// assert_eq!(buffer.len(), 0);
    /// assert_eq!(buffer.capacity(), 128);
    /// ```
    pub fn with_capacity(capacity: usize) -> PacketBuffer {
        let capacity = std::cmp::max(capacity, INLINE_PACKET_BUFFER_SIZE);
        let mut storage = PacketBufferStorage::with_capacity(capacity);
        storage.deref_mut().inner.num_packets = 0;

        PacketBuffer {
            storage,
            last_packet_offset: PACKET_LIST_HEADER_SIZE,
        }
    }

    /// Get underlying buffer capacity in bytes
    pub fn capacity(&self) -> usize {
        self.storage.capacity()
    }

    /// Add a new event containing the provided timestamp and data.
    ///
    /// According to the official documentation for CoreMIDI, the timestamp represents
    /// the time at which the events are to be played, where zero means "now".
    /// The timestamp applies to the first MIDI byte in the packet.
    ///
    /// An event must not have a timestamp that is smaller than that of a previous event
    /// in the same `PacketList`
    ///
    /// Example:
    ///
    /// ```
    /// let mut chord = coremidi::PacketBuffer::new(0, &[0x90, 0x3c, 0x7f]);
    /// chord.push_data(0, &[0x90, 0x40, 0x7f]);
    /// assert_eq!(chord.len(), 1);
    /// let repr = format!("{}", &chord as &coremidi::PacketList);
    /// assert_eq!(repr, "PacketList(len=1)\n  0000000000000000: 90 3c 7f 90 40 7f");
    /// ```
    pub fn push_data(&mut self, time: MIDITimeStamp, data: &[u8]) -> &mut Self {
        let (can_merge, previous_data_len) = self.can_merge_into_last_packet(time, data);

        if can_merge {
            let new_packet_size = Self::packet_size(previous_data_len + data.len());
            unsafe {
                self.storage
                    .ensure_capacity(self.last_packet_offset + new_packet_size);
                self.storage.extend_packet(self.last_packet_offset, data);
            }
        } else {
            let packet_size = Self::packet_size(data.len());
            let next_offset = self.next_packet_offset();
            unsafe {
                self.storage.ensure_capacity(next_offset + packet_size);
                self.storage.assign_packet(next_offset, time, data);
            }
            self.packet_list_mut().num_packets += 1;
            self.last_packet_offset = next_offset;
        }

        self
    }

    /// Clears the buffer, removing all packets.
    /// Note that this method has no effect on the allocated capacity of the buffer.
    pub fn clear(&mut self) {
        self.packet_list_mut().num_packets = 0;
        self.last_packet_offset = PACKET_LIST_HEADER_SIZE;
    }

    /// Checks whether the given tiemstamped data can be merged into the previous packet
    fn can_merge_into_last_packet(&self, time: MIDITimeStamp, data: &[u8]) -> (bool, usize) {
        if self.packet_list_is_empty() {
            (false, 0)
        } else {
            let previous_packet = self.last_packet();
            let previous_packet_data = previous_packet.data();
            let previous_data_len = previous_packet_data.len();
            let can_merge = previous_packet.timestamp() == time
                && Self::not_sysex(data)
                && Self::has_status_byte(data)
                && Self::not_sysex(previous_packet_data)
                && Self::has_status_byte(previous_packet_data)
                && previous_data_len + data.len() < MAX_PACKET_DATA_LENGTH;

            (can_merge, previous_data_len)
        }
    }

    #[inline]
    fn last_packet(&self) -> &Packet {
        assert!(self.packet_list().num_packets > 0);
        let packets_slice = self.storage.get_slice();
        let packet_slot = &packets_slice[self.last_packet_offset..];
        unsafe { &*(packet_slot.as_ptr() as *const Packet) }
    }

    #[inline]
    fn next_packet_offset(&self) -> usize {
        if self.packet_list_is_empty() {
            self.last_packet_offset
        } else {
            let data_len = self.last_packet().inner.length as usize;
            let next_offset = self.last_packet_offset + Self::packet_size(data_len);
            if alignment::NEEDS_ALIGNMENT {
                (next_offset + 3) & !(3usize)
            } else {
                next_offset
            }
        }
    }

    #[inline]
    fn not_sysex(data: &[u8]) -> bool {
        data[0] != 0xF0
    }

    #[inline]
    fn has_status_byte(data: &[u8]) -> bool {
        data[0] & 0b10000000 != 0
    }

    #[inline]
    fn packet_size(data_len: usize) -> usize {
        PACKET_HEADER_SIZE + data_len
    }

    #[inline]
    fn packet_list(&self) -> &PacketListInner {
        &self.storage.deref().inner
    }

    #[inline]
    fn packet_list_is_empty(&self) -> bool {
        self.packet_list().num_packets == 0
    }

    #[inline]
    fn packet_list_mut(&mut self) -> &mut PacketListInner {
        &mut self.storage.deref_mut().inner
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use coremidi_sys::{MIDIPacketList, MIDITimeStamp};
    use std::mem;

    #[test]
    pub fn packet_struct_layout() {
        let expected_align = if super::alignment::NEEDS_ALIGNMENT {
            4
        } else {
            1
        };
        assert_eq!(expected_align, mem::align_of::<Packet>());
        assert_eq!(expected_align, mem::align_of::<PacketList>());

        let dummy_packet: Packet = unsafe { mem::zeroed() };
        let ptr = &dummy_packet as *const _ as *const u8;
        assert_eq!(
            PACKET_HEADER_SIZE,
            dummy_packet.inner.data.as_ptr() as usize - ptr as usize
        );

        let dummy_packet_list: PacketList = unsafe { mem::zeroed() };
        let ptr = &dummy_packet_list as *const _ as *const u8;
        assert_eq!(
            PACKET_LIST_HEADER_SIZE,
            std::ptr::addr_of!(dummy_packet_list.inner.data) as usize - ptr as usize
        );
    }

    #[test]
    pub fn single_packet_alloc_inline() {
        let packet_buf = PacketBuffer::new(42, &[0x90u8, 0x40, 0x7f]);
        if let PacketBufferStorage::External(_) = packet_buf.storage {
            panic!("A single 3-byte message must not be allocated externally")
        }
    }

    #[test]
    fn packet_buffer_deref() {
        let packet_buf = PacketBuffer::new(42, &[0x90u8, 0x40, 0x7f]);
        let packet_list: &PacketList = &packet_buf;
        assert_eq!(
            unsafe { packet_list.as_ptr() as *const MIDIPacketList },
            packet_buf.storage.get_slice().as_ptr() as *const _ as *const MIDIPacketList
        );
    }

    #[test]
    fn packet_list_length() {
        let mut packet_buf = PacketBuffer::new(42, &[0x90u8, 0x40, 0x7f]);
        packet_buf.push_data(43, &[0x91u8, 0x40, 0x7f]);
        packet_buf.push_data(44, &[0x80u8, 0x40, 0x7f]);
        packet_buf.push_data(45, &[0x81u8, 0x40, 0x7f]);
        assert_eq!(packet_buf.len(), 4);
    }

    #[test]
    fn packet_buffer_empty_with_capacity() {
        let packet_buf = PacketBuffer::with_capacity(128);
        assert_eq!(packet_buf.capacity(), 128);
        assert_eq!(packet_buf.len(), 0);
    }

    #[test]
    fn packet_buffer_with_capacity_zero() {
        let packet_buf = PacketBuffer::with_capacity(0);
        assert_eq!(packet_buf.capacity(), INLINE_PACKET_BUFFER_SIZE);
        assert_eq!(packet_buf.len(), 0);
    }

    #[test]
    fn packet_buffer_with_capacity() {
        let mut packet_buf = PacketBuffer::with_capacity(128);
        packet_buf.push_data(43, &[0x91u8, 0x40, 0x7f]);
        packet_buf.push_data(44, &[0x80u8, 0x40, 0x7f]);
        packet_buf.push_data(45, &[0x81u8, 0x40, 0x7f]);
        assert_eq!(packet_buf.capacity(), 128);
        assert_eq!(packet_buf.len(), 3);
    }

    #[test]
    fn packet_buffer_clear() {
        let mut packet_buf = PacketBuffer::new(42, &[0x90u8, 0x40, 0x7f]);
        packet_buf.push_data(43, &[0x91u8, 0x40, 0x7f]);
        packet_buf.push_data(44, &[0x80u8, 0x40, 0x7f]);
        packet_buf.push_data(45, &[0x81u8, 0x40, 0x7f]);
        assert_eq!(packet_buf.len(), 4);
        packet_buf.clear();
        assert_eq!(packet_buf.len(), 0);
    }

    #[test]
    fn compare_equal_timestamps() {
        // these messages should be merged into a single packet
        unsafe {
            compare_packet_list(vec![
                (42, vec![0x90, 0x40, 0x7f]),
                (42, vec![0x90, 0x41, 0x7f]),
                (42, vec![0x90, 0x42, 0x7f]),
            ])
        }
    }

    #[test]
    fn compare_unequal_timestamps() {
        unsafe {
            compare_packet_list(vec![
                (42, vec![0x90, 0x40, 0x7f]),
                (43, vec![0x90, 0x40, 0x7f]),
                (44, vec![0x90, 0x40, 0x7f]),
            ])
        }
    }

    #[test]
    fn compare_sysex() {
        // the sysex must not be merged with the surrounding packets
        unsafe {
            compare_packet_list(vec![
                (42, vec![0x90, 0x40, 0x7f]),
                (42, vec![0xF0, 0x01, 0x01, 0x01, 0x01, 0x01, 0xF7]), // sysex
                (42, vec![0x90, 0x41, 0x7f]),
            ])
        }
    }

    #[test]
    fn compare_sysex_split() {
        // the sysex must not be merged with the surrounding packets
        unsafe {
            compare_packet_list(vec![
                (42, vec![0x90, 0x40, 0x7f]),
                (42, vec![0xF0, 0x01, 0x01, 0x01, 0x01]), // sysex part 1
                (42, vec![0x01, 0xF7]),                   // sysex part 2
                (42, vec![0x90, 0x41, 0x7f]),
            ])
        }
    }

    #[test]
    fn compare_sysex_split2() {
        // the sysex must not be merged with the surrounding packets
        unsafe {
            compare_packet_list(vec![
                (42, vec![0x90, 0x40, 0x7f]),
                (42, vec![0xF0, 0x01, 0x01, 0x01, 0x01]), // sysex part 1
                (42, vec![0x01, 0x01, 0x01]),             // sysex part 2
                (42, vec![0x01, 0xF7]),                   // sysex part 3
                (42, vec![0x90, 0x41, 0x7f]),
            ])
        }
    }

    #[test]
    fn compare_sysex_malformed() {
        // the sysex must not be merged with the surrounding packets
        unsafe {
            compare_packet_list(vec![
                (42, vec![0x90, 0x40, 0x7f]),
                (42, vec![0xF0, 0x01, 0x01, 0x01, 0x01]), // sysex part 1
                (42, vec![0x01, 0x01, 0x01]),             // sysex part 2
                //(42, vec![0x01, 0xF7]), // sysex part 3 (missing)
                (42, vec![0x90, 0x41, 0x7f]),
            ])
        }
    }

    #[test]
    fn compare_sysex_long() {
        let mut sysex = vec![0xF0];
        sysex.resize(301, 0x01);
        sysex.push(0xF7);
        unsafe {
            compare_packet_list(vec![
                (42, vec![0x90, 0x40, 0x7f]),
                (43, vec![0x90, 0x41, 0x7f]),
                (43, sysex),
            ])
        }
    }

    /// Compares the results of building a PacketList using our PacketBuffer API
    /// and the native API (MIDIPacketListAdd, etc).
    unsafe fn compare_packet_list(packets: Vec<(MIDITimeStamp, Vec<u8>)>) {
        use coremidi_sys::{MIDIPacketListAdd, MIDIPacketListInit};

        // allocate a buffer on the stack for building the list using native methods
        const BUFFER_SIZE: usize = 65536; // maximum allowed size
        let buffer: &mut [u8] = &mut [0; BUFFER_SIZE];
        let pkt_list_ptr = buffer.as_mut_ptr() as *mut MIDIPacketList;

        // build the list
        let mut pkt_ptr = MIDIPacketListInit(pkt_list_ptr);
        for pkt in &packets {
            pkt_ptr = MIDIPacketListAdd(
                pkt_list_ptr,
                BUFFER_SIZE as u64,
                pkt_ptr,
                pkt.0,
                pkt.1.len() as u64,
                pkt.1.as_ptr(),
            );
            assert!(!pkt_ptr.is_null());
        }
        let list_native = &*(pkt_list_ptr as *const _ as *const PacketList);

        // build the PacketBuffer, containing the same packets
        let mut packet_buf = PacketBuffer::new(packets[0].0, &packets[0].1);
        for pkt in &packets[1..] {
            packet_buf.push_data(pkt.0, &pkt.1);
        }

        // print buffer contents for debugging purposes
        let packet_buf_slice = packet_buf.storage.get_slice();
        println!(
            "\nbuffer: {:?}",
            packet_buf_slice
                .iter()
                .map(|b| format!("{:02X}", b))
                .collect::<Vec<String>>()
                .join(" ")
        );
        println!(
            "\nnative: {:?}",
            buffer[0..packet_buf_slice.len()]
                .iter()
                .map(|b| format!("{:02X}", b))
                .collect::<Vec<String>>()
                .join(" ")
        );

        let list: &PacketList = &packet_buf;

        // check if the contents match
        assert_eq!(
            list_native.len(),
            list.len(),
            "PacketList lengths must match"
        );
        for (n, p) in list_native.iter().zip(list.iter()) {
            assert_eq!(n.data(), p.data());
        }
    }
}
