/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_INCLUDE_HARDWARE_HARDWARE_H
#define ANDROID_INCLUDE_HARDWARE_HARDWARE_H

#include <stdint.h>
#include <sys/cdefs.h>

#include <cutils/native_handle.h>

__BEGIN_DECLS

/*
 * Value for the hw_module_t.tag field
 */

#define MAKE_TAG_CONSTANT(A,B,C,D) (((A) << 24) | ((B) << 16) | ((C) << 8) | (D))

#define HARDWARE_MODULE_TAG MAKE_TAG_CONSTANT('H', 'W', 'M', 'T')
#define HARDWARE_DEVICE_TAG MAKE_TAG_CONSTANT('H', 'W', 'D', 'T')

struct hw_module_t;
struct hw_module_methods_t;
struct hw_device_t;

/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
typedef struct hw_module_t {
    /** tag must be initialized to HARDWARE_MODULE_TAG */
    uint32_t tag;

    /** major version number for the module */
    uint16_t version_major;

    /** minor version number of the module */
    uint16_t version_minor;

    /** Identifier of module */
    const char *id;

    /** Name of this module */
    const char *name;

    /** Author/owner/implementor of the module */
    const char *author;

    /** Modules methods */
    struct hw_module_methods_t* methods;

    /** module's dso */
    void* dso;

    /** padding to 128 bytes, reserved for future use */
    uint32_t reserved[32-7];

} hw_module_t;

typedef struct hw_module_methods_t {
    /** Open a specific device */
    int (*open)(const struct hw_module_t* module, const char* id,
            struct hw_device_t** device);

} hw_module_methods_t;

/**
 * Every device data structure must begin with hw_device_t
 * followed by module specific public methods and attributes.
 */
typedef struct hw_device_t {
    /** tag must be initialized to HARDWARE_DEVICE_TAG */
    uint32_t tag;

    /** version number for hw_device_t */
    uint32_t version;

    /** reference to the module this device belongs to */
    struct hw_module_t* module;

    /** padding reserved for future use */
    uint32_t reserved[12];

    /** Close this device */
    int (*close)(struct hw_device_t* device);

} hw_device_t;

/**
 * Name of the hal_module_info
 */
#define HAL_MODULE_INFO_SYM         HMI

/**
 * Name of the hal_module_info as a string
 */
#define HAL_MODULE_INFO_SYM_AS_STR  "HMI"

/**
 * Get the module info associated with a module by id.
 * @return: 0 == success, <0 == error and *pHmi == NULL
 */
int hw_get_module(const char *id, const struct hw_module_t **module);


/**
 * pixel format definitions
 */

enum {
    HAL_PIXEL_FORMAT_RGBA_8888    = 1,
    HAL_PIXEL_FORMAT_RGBX_8888    = 2,
    HAL_PIXEL_FORMAT_RGB_888      = 3,
    HAL_PIXEL_FORMAT_RGB_565      = 4,
    HAL_PIXEL_FORMAT_BGRA_8888    = 5,
    HAL_PIXEL_FORMAT_RGBA_5551    = 6,
    HAL_PIXEL_FORMAT_RGBA_4444    = 7,
    HAL_PIXEL_FORMAT_YCbCr_422_SP = 0x10,
    HAL_PIXEL_FORMAT_YCrCb_420_SP = 0x11,
    HAL_PIXEL_FORMAT_YCbCr_422_P  = 0x12,
    HAL_PIXEL_FORMAT_YCbCr_420_P  = 0x13,
    HAL_PIXEL_FORMAT_YCbCr_422_I  = 0x14,
    HAL_PIXEL_FORMAT_YCbCr_420_I  = 0x15,
    HAL_PIXEL_FORMAT_CbYCrY_422_I = 0x16,
    HAL_PIXEL_FORMAT_CbYCrY_420_I = 0x17,
    HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED = 0x20,
    HAL_PIXEL_FORMAT_YCbCr_420_SP       = 0x21,
    HAL_PIXEL_FORMAT_YCrCb_420_SP_TILED = 0x22,
    HAL_PIXEL_FORMAT_YCrCb_422_SP       = 0x23,
};


/**
 * Transformation definitions
 */

enum {
    /* flip source image horizontally */
    HAL_TRANSFORM_FLIP_H    = 0x01,
    /* flip source image vertically */
    HAL_TRANSFORM_FLIP_V    = 0x02,
    /* rotate source image 90 degrees */
    HAL_TRANSFORM_ROT_90    = 0x04,
    /* rotate source image 180 degrees */
    HAL_TRANSFORM_ROT_180   = 0x03,
    /* rotate source image 270 degrees */
    HAL_TRANSFORM_ROT_270   = 0x07,
};

__END_DECLS

#endif  /* ANDROID_INCLUDE_HARDWARE_HARDWARE_H */
