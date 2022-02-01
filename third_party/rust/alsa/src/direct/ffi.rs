
// Some definitions from the kernel headers

// const SNDRV_PCM_MMAP_OFFSET_DATA: c_uint = 0x00000000;
pub const SNDRV_PCM_MMAP_OFFSET_STATUS: libc::c_uint = 0x80000000;
pub const SNDRV_PCM_MMAP_OFFSET_CONTROL: libc::c_uint = 0x81000000;


pub const SNDRV_PCM_SYNC_PTR_HWSYNC: libc::c_uint = 1;
pub const SNDRV_PCM_SYNC_PTR_APPL: libc::c_uint = 2;
pub const SNDRV_PCM_SYNC_PTR_AVAIL_MIN: libc::c_uint = 4;

// #[repr(C)]
#[allow(non_camel_case_types)]
pub type snd_pcm_state_t = libc::c_int;

// #[repr(C)]
#[allow(non_camel_case_types)]
pub type snd_pcm_uframes_t = libc::c_ulong;

// I think?! Not sure how this will work with X32 ABI?!
#[allow(non_camel_case_types)]
pub type __kernel_off_t = libc::c_long;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct snd_pcm_mmap_status {
	pub state: snd_pcm_state_t,		/* RO: state - SNDRV_PCM_STATE_XXXX */
	pub pad1: libc::c_int,			/* Needed for 64 bit alignment */
	pub hw_ptr: snd_pcm_uframes_t,	/* RO: hw ptr (0...boundary-1) */
	pub tstamp: libc::timespec,		/* Timestamp */
	pub suspended_state: snd_pcm_state_t, /* RO: suspended stream state */
	pub audio_tstamp: libc::timespec,	/* from sample counter or wall clock */
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct snd_pcm_mmap_control {
	pub appl_ptr: snd_pcm_uframes_t,	/* RW: appl ptr (0...boundary-1) */
	pub avail_min: snd_pcm_uframes_t,	/* RW: min available frames for wakeup */
}

#[repr(C)]
#[derive(Debug)]
pub struct snd_pcm_channel_info {
	pub channel: libc::c_uint,
	pub offset: __kernel_off_t,		/* mmap offset */
	pub first: libc::c_uint,		/* offset to first sample in bits */
	pub step: libc::c_uint, 		/* samples distance in bits */
}

#[repr(C)]
#[derive(Copy, Clone)]
pub union snd_pcm_mmap_status_r {
	pub status: snd_pcm_mmap_status,
	pub reserved: [libc::c_uchar; 64],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub union snd_pcm_mmap_control_r {
	pub control: snd_pcm_mmap_control,
	pub reserved: [libc::c_uchar; 64],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct snd_pcm_sync_ptr {
	pub flags: libc::c_uint,
	pub s: snd_pcm_mmap_status_r,
	pub c: snd_pcm_mmap_control_r,
}

ioctl_read!(sndrv_pcm_ioctl_channel_info, b'A', 0x32, snd_pcm_channel_info);
ioctl_readwrite!(sndrv_pcm_ioctl_sync_ptr, b'A', 0x23, snd_pcm_sync_ptr);

pub fn pagesize() -> usize {
    unsafe { libc::sysconf(libc::_SC_PAGESIZE) as usize }
}
