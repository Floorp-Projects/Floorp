# ashmem-rs

Provides a compatibility interface for Android 8.0's public ASharedMemory API.

Some variant of ashmem has existed in Android as a private API for some time.  Originally available only via [direct ioctl interface](https://elixir.bootlin.com/linux/v5.11.8/source/drivers/staging/android/uapi/ashmem.h), it later became callable (still private) via ashmem_create_region (et al.) in libcutils.

[ASharedMemory](https://developer.android.com/ndk/reference/group/memory) is the public API to ashmem made available in Android 8.0 (API level 26).
Builds targeting Android 10 (API 29) are no longer permitted to access ashmem via the ioctl interface.  This makes life for a portable program difficult - you can't reliably use the old or new interface during this transition period.

ashmem-rs provides a simple wrapper around the ASharedMemory API, directly calling the public API implementation via dynamically loaded libandroid where available, and falling back to the direct ioctl interface elsewhere.

References:
 - [ASharedMemory documentation](https://developer.android.com/ndk/reference/group/memory)
 - [Linux kernel ashmem.h definitions](https://elixir.bootlin.com/linux/v5.11.8/source/drivers/staging/android/uapi/ashmem.h)
