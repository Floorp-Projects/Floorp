#[cfg(any(unix, doc))]
mod fd;
#[cfg(any(unix, doc))]
pub use fd::*;

#[cfg(any(windows, doc))]
mod handle;
#[cfg(any(windows, doc))]
pub use handle::*;

mod ptr;
pub use ptr::*;

pub use drm_fourcc::DrmModifier;
use std::ops::Range;

/// Representation of an os specific memory.
pub enum PlatformMemory {
    #[cfg(any(unix, doc))]
    /// Unix file descriptor.
    Fd(Fd),
    #[cfg(any(windows, doc))]
    /// Windows handle.
    Handle(Handle),
    /// Host pointer.
    Ptr(Ptr),
}
impl PlatformMemory {
    #[cfg(any(unix, doc))]
    pub fn fd(&self)->Option<&Fd> {
        match self {
            Self::Fd(fd)=>Some(fd),
            #[cfg(windows)]
            Self::Handle(_)=>None,
            Self::Ptr(_)=>None,
        }
    }
    #[cfg(any(windows, doc))]
    pub fn handle(&self)->Option<&Handle> {
        match self {
            Self::Handle(handle)=>Some(handle),
            #[cfg(unix)]
            Self::Fd(_)=>None,
            Self::Ptr(_)=>None,
        }
    }
    pub fn ptr(&self)->Option<&Ptr> {
        match self {
            Self::Ptr(ptr)=>Some(ptr),
            #[cfg(unix)]
            Self::Fd(_)=>None,
            #[cfg(windows)]
            Self::Handle(_)=>None
        }
    }
}

#[cfg(any(unix, doc))]
impl std::convert::TryInto<Fd> for PlatformMemory{
    type Error = &'static str;
    fn try_into(self) -> Result<Fd, Self::Error>{
        match self {
            Self::Fd(fd)=>Ok(fd),
            #[cfg(windows)]
            Self::Handle(_)=>Err("PlatformMemory does not contain an fd"),
            Self::Ptr(_)=>Err("PlatformMemory does not contain an fd"),
        }
    }
}

#[cfg(any(windows, doc))]
impl std::convert::TryInto<Handle> for PlatformMemory{
    type Error = &'static str;
    fn try_into(self) -> Result<Handle, Self::Error>{
        match self {
            Self::Handle(handle)=>Ok(handle),
            #[cfg(unix)]
            Self::Fd(_)=>Err("PlatformMemory does not contain an handle"),
            Self::Ptr(_)=>Err("PlatformMemory does not contain an handle"),
        }
    }
}


impl std::convert::TryInto<Ptr> for PlatformMemory{
    type Error = &'static str;
    fn try_into(self) -> Result<Ptr, Self::Error>{
        match self {
            Self::Ptr(ptr)=>Ok(ptr),
            #[cfg(unix)]
            Self::Fd(_)=>Err("PlatformMemory does not contain a ptr"),
            #[cfg(windows)]
            Self::Handle(_)=>Err("PlatformMemory does not contain a ptr")
        }
    }
}

#[cfg(any(unix,docs))]
impl From<Fd> for PlatformMemory {
    fn from(fd: Fd) -> Self {
        Self::Fd(fd)
    }
}

#[cfg(any(windows,doc))]
impl From<Handle> for PlatformMemory {
    fn from(handle: Handle) -> Self {
        Self::Handle(handle)
    }
}

impl From<Ptr> for PlatformMemory {
    fn from(ptr: Ptr) -> Self {
        Self::Ptr(ptr)
    }
}

/// Representation of os specific memory types.
pub enum PlatformMemoryType {
    #[cfg(any(unix, doc))]
    Fd,
    #[cfg(any(windows, doc))]
    Handle,
    Ptr,
}


bitflags::bitflags!(
    /// External memory type flags.
    #[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
    pub struct ExternalMemoryTypeFlags: u32 {
        #[cfg(any(unix,doc))]
        /// This is supported on Unix only.
        /// Specifies a POSIX file descriptor handle that has only limited valid usage outside of Vulkan and other compatible APIs.
        /// It must be compatible with the POSIX system calls dup, dup2, close, and the non-standard system call dup3.
        /// Additionally, it must be transportable over a socket using an SCM_RIGHTS control message.
        /// It owns a reference to the underlying memory resource represented by its memory object.
        const OPAQUE_FD = 0x00000001;
        #[cfg(any(windows,doc))]
        /// This is supported on Windows only.
        /// Specifies an NT handle that has only limited valid usage outside of Vulkan and other compatible APIs.
        /// It must be compatible with the functions DuplicateHandle, CloseHandle, CompareObjectHandles, GetHandleInformation, and SetHandleInformation.
        /// It owns a reference to the underlying memory resource represented by its memory object.
        const OPAQUE_WIN32 = 0x00000002;
        #[cfg(any(windows,doc))]
        /// This is supported on Windows only.
        /// Specifies a global share handle that has only limited valid usage outside of Vulkan and other compatible APIs.
        /// It is not compatible with any native APIs.
        /// It does not own a reference to the underlying memory resource represented by its memory object, and will therefore become invalid when all the memory objects with it are destroyed.
        const OPAQUE_WIN32_KMT = 0x00000004;
        #[cfg(any(windows,doc))]
        /// This is supported on Windows only.
        /// Specifies an NT handle returned by IDXGIResource1::CreateSharedHandle referring to a Direct3D 10 or 11 texture resource.
        /// It owns a reference to the memory used by the Direct3D resource.
        const D3D11_TEXTURE = 0x00000008;
        #[cfg(any(windows,doc))]
        /// This is supported on Windows only.
        /// Specifies a global share handle returned by IDXGIResource::GetSharedHandle referring to a Direct3D 10 or 11 texture resource.
        /// It does not own a reference to the underlying Direct3D resource, and will therefore become invalid when all the memory objects and Direct3D resources associated with it are destroyed.
        const D3D11_TEXTURE_KMT = 0x00000010;
        #[cfg(any(windows,doc))]
        /// This is supported on Windows only.
        /// Specifies an NT handle returned by ID3D12Device::CreateSharedHandle referring to a Direct3D 12 heap resource.
        /// It owns a reference to the resources used by the Direct3D heap.
        const D3D12_HEAP = 0x00000020;
        #[cfg(any(windows,doc))]
        /// This is supported on Windows only.
        /// Specifies an NT handle returned by ID3D12Device::CreateSharedHandle referring to a Direct3D 12 committed resource.
        /// It owns a reference to the memory used by the Direct3D resource.
        const D3D12_RESOURCE = 0x00000040;
        #[cfg(any(target_os = "linux",target_os = "android",doc))]
        /// This is supported on Linux or Android only.
        /// Is a file descriptor for a Linux dma_buf.
        /// It owns a reference to the underlying memory resource represented by its memory object.
        const DMA_BUF = 0x00000200;
        #[cfg(any(target_os = "android",doc))]
        /// This is supported on Android only.
        /// Specifies an AHardwareBuffer object defined by the Android NDK. See Android Hardware Buffers for more details of this handle type.
        const ANDROID_HARDWARE_BUFFER = 0x00000400;
        /// Specifies a host pointer returned by a host memory allocation command.
        /// It does not own a reference to the underlying memory resource, and will therefore become invalid if the host memory is freed.
        const HOST_ALLOCATION = 0x00000080;
        /// Specifies a host pointer to host mapped foreign memory.
        /// It does not own a reference to the underlying memory resource, and will therefore become invalid if the foreign memory is unmapped or otherwise becomes no longer available.
        const HOST_MAPPED_FOREIGN_MEMORY = 0x00000100;
    }
);

impl From<ExternalMemoryType> for ExternalMemoryTypeFlags {
    fn from(external_memory_type: ExternalMemoryType) -> Self {
        match external_memory_type {
            #[cfg(unix)]
            ExternalMemoryType::OpaqueFd => Self::OPAQUE_FD,
            #[cfg(windows)]
            ExternalMemoryType::OpaqueWin32 => Self::OPAQUE_WIN32,
            #[cfg(windows)]
            ExternalMemoryType::OpaqueWin32Kmt => Self::OPAQUE_WIN32_KMT,
            #[cfg(windows)]
            ExternalMemoryType::D3D11Texture => Self::D3D11_TEXTURE,
            #[cfg(windows)]
            ExternalMemoryType::D3D11TextureKmt => Self::D3D11_TEXTURE_KMT,
            #[cfg(windows)]
            ExternalMemoryType::D3D12Heap => Self::D3D12_HEAP,
            #[cfg(windows)]
            ExternalMemoryType::D3D12Resource => Self::D3D12_RESOURCE,
            #[cfg(any(target_os = "linux", target_os = "android", doc))]
            ExternalMemoryType::DmaBuf => Self::DMA_BUF,
            #[cfg(target_os = "android")]
            ExternalMemoryType::AndroidHardwareBuffer => Self::ANDROID_HARDWARE_BUFFER,
            ExternalMemoryType::HostAllocation => Self::HOST_ALLOCATION,
            ExternalMemoryType::HostMappedForeignMemory => Self::HOST_MAPPED_FOREIGN_MEMORY,
        }
    }
}

impl From<ExternalImageMemoryType> for ExternalMemoryTypeFlags {
    fn from(external_memory_type: ExternalImageMemoryType) -> Self {
        match external_memory_type {
            #[cfg(unix)]
            ExternalImageMemoryType::OpaqueFd => Self::OPAQUE_FD,
            #[cfg(windows)]
            ExternalImageMemoryType::OpaqueWin32 => Self::OPAQUE_WIN32,
            #[cfg(windows)]
            ExternalImageMemoryType::OpaqueWin32Kmt => Self::OPAQUE_WIN32_KMT,
            #[cfg(windows)]
            ExternalImageMemoryType::D3D11Texture => Self::D3D11_TEXTURE,
            #[cfg(windows)]
            ExternalImageMemoryType::D3D11TextureKmt => Self::D3D11_TEXTURE_KMT,
            #[cfg(windows)]
            ExternalImageMemoryType::D3D12Heap => Self::D3D12_HEAP,
            #[cfg(windows)]
            ExternalImageMemoryType::D3D12Resource => Self::D3D12_RESOURCE,
            #[cfg(any(target_os = "linux", target_os = "android", doc))]
            ExternalImageMemoryType::DmaBuf(_) => Self::DMA_BUF,
            #[cfg(target_os = "android")]
            ExternalImageMemoryType::AndroidHardwareBuffer => Self::ANDROID_HARDWARE_BUFFER,
            ExternalImageMemoryType::HostAllocation => Self::HOST_ALLOCATION,
            ExternalImageMemoryType::HostMappedForeignMemory => Self::HOST_MAPPED_FOREIGN_MEMORY,
        }
    }
}

/// External memory types.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum ExternalMemoryType {
    #[cfg(any(unix, doc))]
    /// This is supported on Unix only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_FD][ExternalMemoryTypeFlags::OPAQUE_FD].
    OpaqueFd,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_WIN32][ExternalMemoryTypeFlags::OPAQUE_WIN32].
    OpaqueWin32,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_WIN32_KMT][ExternalMemoryTypeFlags::OPAQUE_WIN32_KMT].
    OpaqueWin32Kmt,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D11_TEXTURE][ExternalMemoryTypeFlags::D3D11_TEXTURE].
    D3D11Texture,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D11_TEXTURE_KMT][ExternalMemoryTypeFlags::D3D11_TEXTURE_KMT].
    D3D11TextureKmt,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D12_HEAP][ExternalMemoryTypeFlags::D3D12_HEAP].
    D3D12Heap,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D12_RESOURCE][ExternalMemoryTypeFlags::D3D12_RESOURCE].
    D3D12Resource,
    #[cfg(any(target_os = "linux", target_os = "android", doc))]
    /// This is supported on Linux or Android only.
    /// Same as [ExternalMemoryTypeFlags::DMA_BUF][ExternalMemoryTypeFlags::DMA_BUF].
    DmaBuf,
    #[cfg(any(target_os = "android", doc))]
    /// This is supported on Android only.
    /// Same as [ExternalMemoryTypeFlags::ANDROID_HARDWARE_BUFFER][ExternalMemoryTypeFlags::ANDROID_HARDWARE_BUFFER].
    AndroidHardwareBuffer,
    /// Same as [ExternalMemoryTypeFlags::HOST_ALLOCATION][ExternalMemoryTypeFlags::HOST_ALLOCATION].
    HostAllocation,
    /// Same as [ExternalMemoryTypeFlags::HOST_MAPPED_FOREIGN_MEMORY][ExternalMemoryTypeFlags::HOST_MAPPED_FOREIGN_MEMORY].
    HostMappedForeignMemory,
}

impl Into<PlatformMemoryType> for ExternalMemoryType {
    fn into(self) -> PlatformMemoryType {
        match self {
            #[cfg(unix)]
            ExternalMemoryType::OpaqueFd => PlatformMemoryType::Fd,
            #[cfg(windows)]
            ExternalMemoryType::OpaqueWin32 => PlatformMemoryType::Handle,
            #[cfg(windows)]
            ExternalMemoryType::OpaqueWin32Kmt => PlatformMemoryType::Handle,
            #[cfg(windows)]
            ExternalMemoryType::D3D11Texture => PlatformMemoryType::Handle,
            #[cfg(windows)]
            ExternalMemoryType::D3D11TextureKmt => PlatformMemoryType::Handle,
            #[cfg(windows)]
            ExternalMemoryType::D3D12Heap => PlatformMemoryType::Handle,
            #[cfg(windows)]
            ExternalMemoryType::D3D12Resource => PlatformMemoryType::Handle,
            #[cfg(any(target_os = "linux", target_os = "android", doc))]
            ExternalMemoryType::DmaBuf => PlatformMemoryType::Fd,
            #[cfg(any(target_os = "android", doc))]
            ExternalMemoryType::AndroidHardwareBuffer => PlatformMemoryType::Fd,
            ExternalMemoryType::HostAllocation => PlatformMemoryType::Ptr,
            ExternalMemoryType::HostMappedForeignMemory => PlatformMemoryType::Ptr,
        }
    }
}

/// Representation of an external memory for buffer creation.
#[derive(Debug)]
pub enum ExternalBufferMemory {
    #[cfg(unix)]
    /// This is supported on Unix only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_FD][ExternalMemoryTypeFlags::OPAQUE_FD] while holding a [Fd][Fd].
    OpaqueFd(Fd),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_WIN32][ExternalMemoryTypeFlags::OPAQUE_WIN32] while holding a [Handle][Handle].
    OpaqueWin32(Handle),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_WIN32_KMT][ExternalMemoryTypeFlags::OPAQUE_WIN32_KMT] while holding a [Handle][Handle].
    OpaqueWin32Kmt(Handle),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D11_TEXTURE][ExternalMemoryTypeFlags::D3D11_TEXTURE] while holding a [Handle][Handle].
    D3D11Texture(Handle),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D11_TEXTURE_KMT][ExternalMemoryTypeFlags::D3D11_TEXTURE_KMT] while holding a [Handle][Handle].
    D3D11TextureKmt(Handle),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D12_HEAP][ExternalMemoryTypeFlags::D3D12_HEAP] while holding a [Handle][Handle].
    D3D12Heap(Handle),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D12_RESOURCE][ExternalMemoryTypeFlags::D3D12_RESOURCE] while holding a [Handle][Handle].
    D3D12Resource(Handle),
    #[cfg(any(target_os = "linux", target_os = "android"))]
    /// This is supported on Linux or Android only.
    /// Same as [ExternalMemoryTypeFlags::DMA_BUF][ExternalMemoryTypeFlags::DMA_BUF] while holding a [Fd][Fd].
    DmaBuf(Fd),
    #[cfg(any(target_os = "android"))]
    /// This is supported on Android only.
    /// Same as [ExternalMemoryTypeFlags::ANDROID_HARDWARE_BUFFER][ExternalMemoryTypeFlags::ANDROID_HARDWARE_BUFFER] while holding a [Fd][Fd].
    AndroidHardwareBuffer(Fd),
    /// Same as [ExternalMemoryTypeFlags::HOST_ALLOCATION][ExternalMemoryTypeFlags::HOST_ALLOCATION] while holding a [Ptr][Ptr].
    HostAllocation(Ptr),
    /// Same as [ExternalMemoryTypeFlags::HOST_MAPPED_FOREIGN_MEMORY][ExternalMemoryTypeFlags::HOST_MAPPED_FOREIGN_MEMORY] while holding a [Ptr][Ptr].
    HostMappedForeignMemory(Ptr),
}
impl ExternalBufferMemory {
    /// Get the [ExternalMemoryType][ExternalMemoryType] from this enum.
    pub fn external_memory_type(&self) -> ExternalMemoryType {
        match self {
            #[cfg(unix)]
            Self::OpaqueFd(_) => ExternalMemoryType::OpaqueFd,
            #[cfg(windows)]
            Self::OpaqueWin32(_) => ExternalMemoryType::OpaqueWin32,
            #[cfg(windows)]
            Self::OpaqueWin32Kmt(_) => ExternalMemoryType::OpaqueWin32Kmt,
            #[cfg(windows)]
            Self::D3D11Texture(_) => ExternalMemoryType::D3D11Texture,
            #[cfg(windows)]
            Self::D3D11TextureKmt(_) => ExternalMemoryType::D3D11TextureKmt,
            #[cfg(windows)]
            Self::D3D12Heap(_) => ExternalMemoryType::D3D12Heap,
            #[cfg(windows)]
            Self::D3D12Resource(_) => ExternalMemoryType::D3D12Resource,
            #[cfg(any(target_os = "linux", target_os = "android"))]
            Self::DmaBuf(_) => ExternalMemoryType::DmaBuf,
            #[cfg(target_os = "android")]
            Self::AndroidHardwareBuffer(_) => ExternalMemoryType::AndroidHardwareBuffer,
            Self::HostAllocation(_) => ExternalMemoryType::HostAllocation,
            Self::HostMappedForeignMemory(_) => ExternalMemoryType::HostMappedForeignMemory,
        }
    }

    /// Get the [PlatformMemoryType][PlatformMemoryType] from this enum.
    pub fn platform_memory_type(&self) -> PlatformMemoryType {
        match self {
            #[cfg(unix)]
            Self::OpaqueFd(_) => PlatformMemoryType::Fd,
            #[cfg(windows)]
            Self::OpaqueWin32(_) => PlatformMemoryType::Handle,
            #[cfg(windows)]
            Self::OpaqueWin32Kmt(_) => PlatformMemoryType::Handle,
            #[cfg(windows)]
            Self::D3D11Texture(_) => PlatformMemoryType::Handle,
            #[cfg(windows)]
            Self::D3D11TextureKmt(_) => PlatformMemoryType::Handle,
            #[cfg(windows)]
            Self::D3D12Heap(_) => PlatformMemoryType::Handle,
            #[cfg(windows)]
            Self::D3D12Resource(_) => PlatformMemoryType::Handle,
            #[cfg(any(target_os = "linux", target_os = "android"))]
            Self::DmaBuf(_) => PlatformMemoryType::Fd,
            #[cfg(target_os = "android")]
            Self::AndroidHardwareBuffer(_) => PlatformMemoryType::Fd,
            Self::HostAllocation(_) => PlatformMemoryType::Ptr,
            Self::HostMappedForeignMemory(_) => PlatformMemoryType::Ptr,
        }
    }

    #[cfg(unix)]
    /// Get the associated unix file descriptor as ([Fd][Fd]).
    pub fn fd(&self) -> Option<&Fd> {
        match self {
            Self::OpaqueFd(fd) => Some(fd),
            #[cfg(any(target_os = "linux", target_os = "android"))]
            Self::DmaBuf(fd) => Some(fd),
            #[cfg(target_os = "android")]
            Self::AndroidHardwareBuffer(fd) => Some(fd),
            _ => None,
        }
    }
    #[cfg(windows)]
    /// Get the associated windows handle as ([Handle][Handle]).
    pub fn handle(&self) -> Option<&Handle> {
        match self {
            Self::OpaqueWin32(handle) => Some(handle),
            Self::OpaqueWin32Kmt(handle) => Some(handle),
            Self::D3D11Texture(handle) => Some(handle),
            Self::D3D11TextureKmt(handle) => Some(handle),
            Self::D3D12Heap(handle) => Some(handle),
            Self::D3D12Resource(handle) => Some(handle),
            _ => None,
        }
    }

    /// Get the associated host pointer as ([Ptr][Ptr]).
    pub fn ptr(&self) -> Option<&Ptr> {
        match self {
            Self::HostAllocation(ptr) => Some(ptr),
            Self::HostMappedForeignMemory(ptr) => Some(ptr),
            // Without this on non unix or windows platform, this will trigger error for unreachable pattern
            #[allow(unreachable_patterns)]
            _ => None,
        }
    }
}

/// Representation of an external memory type for buffers.
pub type ExternalBufferMemoryType = ExternalMemoryType;


/// Representation of an external memory for image creation.
#[derive(Debug)]
pub enum ExternalImageMemory {
    #[cfg(unix)]
    /// This is supported on Unix only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_FD][ExternalMemoryTypeFlags::OPAQUE_FD] while holding a [Fd][Fd].
    OpaqueFd(Fd),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_WIN32][ExternalMemoryTypeFlags::OPAQUE_WIN32] while holding a [Handle][Handle].
    OpaqueWin32(Handle),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_WIN32_KMT][ExternalMemoryTypeFlags::OPAQUE_WIN32_KMT] while holding a [Handle][Handle].
    OpaqueWin32Kmt(Handle),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D11_TEXTURE][ExternalMemoryTypeFlags::D3D11_TEXTURE] while holding a [Handle][Handle].
    D3D11Texture(Handle),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D11_TEXTURE_KMT][ExternalMemoryTypeFlags::D3D11_TEXTURE_KMT] while holding a [Handle][Handle].
    D3D11TextureKmt(Handle),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D12_HEAP][ExternalMemoryTypeFlags::D3D12_HEAP] while holding a [Handle][Handle].
    D3D12Heap(Handle),
    #[cfg(windows)]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D12_RESOURCE][ExternalMemoryTypeFlags::D3D12_RESOURCE] while holding a [Handle][Handle].
    D3D12Resource(Handle),
    #[cfg(any(target_os = "linux", target_os = "android"))]
    /// This is supported on Linux or Android only.
    /// Same as [ExternalMemoryTypeFlags::DMA_BUF][ExternalMemoryTypeFlags::DMA_BUF] while holding a [Fd][Fd].
    DmaBuf(Fd, Option<DrmFormatImageProperties>),
    #[cfg(any(target_os = "android"))]
    /// This is supported on Android only.
    /// Same as [ExternalMemoryTypeFlags::ANDROID_HARDWARE_BUFFER][ExternalMemoryTypeFlags::ANDROID_HARDWARE_BUFFER] while holding a [Fd][Fd].
    AndroidHardwareBuffer(Fd),
    /// Same as [ExternalMemoryTypeFlags::HOST_ALLOCATION][ExternalMemoryTypeFlags::HOST_ALLOCATION] while holding a [Ptr][Ptr].
    HostAllocation(Ptr),
    /// Same as [ExternalMemoryTypeFlags::HOST_MAPPED_FOREIGN_MEMORY][ExternalMemoryTypeFlags::HOST_MAPPED_FOREIGN_MEMORY] while holding a [Ptr][Ptr].
    HostMappedForeignMemory(Ptr),
}
impl ExternalImageMemory {
    /// Get the [ExternalMemoryType][ExternalMemoryType] from this enum.
    pub fn external_memory_type(&self) -> ExternalMemoryType {
        match self {
            #[cfg(unix)]
            Self::OpaqueFd(_) => ExternalMemoryType::OpaqueFd,
            #[cfg(windows)]
            Self::OpaqueWin32(_) => ExternalMemoryType::OpaqueWin32,
            #[cfg(windows)]
            Self::OpaqueWin32Kmt(_) => ExternalMemoryType::OpaqueWin32Kmt,
            #[cfg(windows)]
            Self::D3D11Texture(_) => ExternalMemoryType::D3D11Texture,
            #[cfg(windows)]
            Self::D3D11TextureKmt(_) => ExternalMemoryType::D3D11TextureKmt,
            #[cfg(windows)]
            Self::D3D12Heap(_) => ExternalMemoryType::D3D12Heap,
            #[cfg(windows)]
            Self::D3D12Resource(_) => ExternalMemoryType::D3D12Resource,
            #[cfg(any(target_os = "linux", target_os = "android"))]
            Self::DmaBuf(_, _) => ExternalMemoryType::DmaBuf,
            #[cfg(target_os = "android")]
            Self::AndroidHardwareBuffer(_) => ExternalMemoryType::AndroidHardwareBuffer,
            Self::HostAllocation(_) => ExternalMemoryType::HostAllocation,
            Self::HostMappedForeignMemory(_) => ExternalMemoryType::HostMappedForeignMemory,
        }
    }

    /// Get the [PlatformMemoryType][PlatformMemoryType] from this enum.
    pub fn platform_memory_type(&self) -> PlatformMemoryType {
        match self {
            #[cfg(unix)]
            Self::OpaqueFd(_) => PlatformMemoryType::Fd,
            #[cfg(windows)]
            Self::OpaqueWin32(_) => PlatformMemoryType::Handle,
            #[cfg(windows)]
            Self::OpaqueWin32Kmt(_) => PlatformMemoryType::Handle,
            #[cfg(windows)]
            Self::D3D11Texture(_) => PlatformMemoryType::Handle,
            #[cfg(windows)]
            Self::D3D11TextureKmt(_) => PlatformMemoryType::Handle,
            #[cfg(windows)]
            Self::D3D12Heap(_) => PlatformMemoryType::Handle,
            #[cfg(windows)]
            Self::D3D12Resource(_) => PlatformMemoryType::Handle,
            #[cfg(any(target_os = "linux", target_os = "android"))]
            Self::DmaBuf(_, _) => PlatformMemoryType::Fd,
            #[cfg(target_os = "android")]
            Self::AndroidHardwareBuffer(_) => PlatformMemoryType::Fd,
            Self::HostAllocation(_) => PlatformMemoryType::Ptr,
            Self::HostMappedForeignMemory(_) => PlatformMemoryType::Ptr,
        }
    }

    #[cfg(unix)]
    /// Get the associated unix file descriptor as ([Fd][Fd]).
    pub fn fd(&self) -> Option<&Fd> {
        match self {
            Self::OpaqueFd(fd) => Some(fd),
            #[cfg(any(target_os = "linux", target_os = "android"))]
            Self::DmaBuf(fd, _drm_format_properties) => Some(fd),
            #[cfg(target_os = "android")]
            Self::AndroidHardwareBuffer(fd) => Some(fd),
            _ => None,
        }
    }

    #[cfg(windows)]
    /// Get the associated windows handle as ([Handle][Handle]).
    pub fn handle(&self) -> Option<&Handle> {
        match self {
            Self::OpaqueWin32(handle) => Some(handle),
            Self::OpaqueWin32Kmt(handle) => Some(handle),
            Self::D3D11Texture(handle) => Some(handle),
            Self::D3D11TextureKmt(handle) => Some(handle),
            Self::D3D12Heap(handle) => Some(handle),
            Self::D3D12Resource(handle) => Some(handle),
            _ => None,
        }
    }

    /// Get the associated host pointer as ([Ptr][Ptr]).
    pub fn ptr(&self) -> Option<&Ptr> {
        match self {
            Self::HostAllocation(ptr) => Some(ptr),
            Self::HostMappedForeignMemory(ptr) => Some(ptr),
            // Without this on non unix or windows platform, this will trigger error for unreachable pattern
            #[allow(unreachable_patterns)]
            _ => None,
        }
    }
}


/// Representation of an external memory type for images.
#[derive(Clone, Debug, PartialEq)]
pub enum ExternalImageMemoryType {
    #[cfg(any(unix, doc))]
    /// This is supported on Unix only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_FD][ExternalMemoryTypeFlags::OPAQUE_FD]
    OpaqueFd,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_WIN32][ExternalMemoryTypeFlags::OPAQUE_WIN32]
    OpaqueWin32,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::OPAQUE_WIN32_KMT][ExternalMemoryTypeFlags::OPAQUE_WIN32_KMT]
    OpaqueWin32Kmt,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D11_TEXTURE][ExternalMemoryTypeFlags::D3D11_TEXTURE]
    D3D11Texture,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D11_TEXTURE_KMT][ExternalMemoryTypeFlags::D3D11_TEXTURE_KMT]
    D3D11TextureKmt,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D12_HEAP][ExternalMemoryTypeFlags::D3D12_HEAP]
    D3D12Heap,
    #[cfg(any(windows, doc))]
    /// This is supported on Windows only.
    /// Same as [ExternalMemoryTypeFlags::D3D12_RESOURCE][ExternalMemoryTypeFlags::D3D12_RESOURCE]
    D3D12Resource,
    #[cfg(any(target_os = "linux", target_os = "android", doc))]
    /// This is supported on Linux or Android only.
    /// Same as [ExternalMemoryTypeFlags::DMA_BUF][ExternalMemoryTypeFlags::DMA_BUF]
    DmaBuf(Vec<DrmModifier>),
    #[cfg(any(target_os = "android", doc))]
    /// This is supported on Android only.
    /// Same as [ExternalMemoryTypeFlags::ANDROID_HARDWARE_BUFFER][ExternalMemoryTypeFlags::ANDROID_HARDWARE_BUFFER]
    AndroidHardwareBuffer,
    /// Same as [ExternalMemoryTypeFlags::HOST_ALLOCATION][ExternalMemoryTypeFlags::HOST_ALLOCATION]
    HostAllocation,
    /// Same as [ExternalMemoryTypeFlags::HOST_MAPPED_FOREIGN_MEMORY][ExternalMemoryTypeFlags::HOST_MAPPED_FOREIGN_MEMORY]
    HostMappedForeignMemory,
}
impl ExternalImageMemoryType {
    /// Get the [ExternalMemoryType][ExternalMemoryType] from this enum.
    pub fn external_memory_type(&self) -> ExternalMemoryType {
        match self {
            #[cfg(unix)]
            Self::OpaqueFd => ExternalMemoryType::OpaqueFd,
            #[cfg(windows)]
            Self::OpaqueWin32 => ExternalMemoryType::OpaqueWin32,
            #[cfg(windows)]
            Self::OpaqueWin32Kmt => ExternalMemoryType::OpaqueWin32Kmt,
            #[cfg(windows)]
            Self::D3D11Texture => ExternalMemoryType::D3D11Texture,
            #[cfg(windows)]
            Self::D3D11TextureKmt => ExternalMemoryType::D3D11TextureKmt,
            #[cfg(windows)]
            Self::D3D12Heap => ExternalMemoryType::D3D12Heap,
            #[cfg(windows)]
            Self::D3D12Resource => ExternalMemoryType::D3D12Resource,
            #[cfg(any(target_os = "linux", target_os = "android"))]
            Self::DmaBuf(_) => ExternalMemoryType::DmaBuf,
            #[cfg(target_os = "android")]
            Self::AndroidHardwareBuffer => ExternalMemoryType::AndroidHardwareBuffer,
            Self::HostAllocation => ExternalMemoryType::HostAllocation,
            Self::HostMappedForeignMemory => ExternalMemoryType::HostMappedForeignMemory,
        }
    }
}

type Offset = u64;

/// Footprint of a plane layout in memory.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
pub struct PlaneLayout {
    /// Byte slice occupied by the subresource.
    pub slice: Range<Offset>,
    /// Byte distance between rows.
    pub row_pitch: Offset,
    /// Byte distance between array layers.
    pub array_pitch: Offset,
    /// Byte distance between depth slices.
    pub depth_pitch: Offset,
}

/// Description of drm format properties used to create an image using drm format modifier.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct DrmFormatImageProperties {
    /// Drm format modifier
    pub drm_modifier: DrmModifier,
    /// Plane subresource layouts
    pub plane_layouts: Vec<PlaneLayout>,
}
