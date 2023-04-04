/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MAR_H__
#define MAR_H__

#include <assert.h>  // for C11 static_assert
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* We have a MAX_SIGNATURES limit so that an invalid MAR will never
 * waste too much of either updater's or signmar's time.
 * It is also used at various places internally and will affect memory usage.
 * If you want to increase this value above 9 then you need to adjust parsing
 * code in tool/mar.c.
 */
#define MAX_SIGNATURES 8
static_assert(MAX_SIGNATURES <= 9, "too many signatures");

struct ProductInformationBlock {
  const char* MARChannelID;
  const char* productVersion;
};

/**
 * The MAR item data structure.
 */
typedef struct MarItem_ {
  struct MarItem_* next; /* private field */
  uint32_t offset;       /* offset into archive */
  uint32_t length;       /* length of data in bytes */
  uint32_t flags;        /* contains file mode bits */
  char name[1];          /* file path */
} MarItem;

/**
 * File offset and length for tracking access of byte indexes
 */
typedef struct SeenIndex_ {
  struct SeenIndex_* next; /* private field */
  uint32_t offset;         /* offset into archive */
  uint32_t length;         /* length of the data in bytes */
} SeenIndex;

#define TABLESIZE 256

/**
 * Mozilla ARchive (MAR) file data structure
 */
struct MarFile_ {
  unsigned char* buffer;          /* file buffer containing the entire MAR */
  size_t data_len;                /* byte count of the data in the buffer */
  MarItem* item_table[TABLESIZE]; /* hash table of files in the archive */
  SeenIndex* index_list;          /* file indexes processed */
  int item_table_is_valid;        /* header and index validation flag */
};

typedef struct MarFile_ MarFile;

/**
 * Signature of callback function passed to mar_enum_items.
 * @param mar       The MAR file being visited.
 * @param item      The MAR item being visited.
 * @param data      The data parameter passed by the caller of mar_enum_items.
 * @return          A non-zero value to stop enumerating.
 */
typedef int (*MarItemCallback)(MarFile* mar, const MarItem* item, void* data);

enum MarReadResult_ {
  MAR_READ_SUCCESS,
  MAR_IO_ERROR,
  MAR_MEM_ERROR,
  MAR_FILE_TOO_BIG_ERROR,
};

typedef enum MarReadResult_ MarReadResult;

/**
 * Open a MAR file for reading.
 * @param path      Specifies the path to the MAR file to open.  This path must
 *                  be compatible with fopen.
 * @param out_mar   Out-parameter through which the created MarFile structure is
 *                  returned. Guaranteed to be a valid structure if
 *                  MAR_READ_SUCCESS is returned. Otherwise NULL will be
 *                  assigned.
 * @return          NULL if an error occurs.
 */
MarReadResult mar_open(const char* path, MarFile** out_mar);

#ifdef XP_WIN
MarReadResult mar_wopen(const wchar_t* path, MarFile** out_mar);
#endif

/**
 * Close a MAR file that was opened using mar_open.
 * @param mar       The MarFile object to close.
 */
void mar_close(MarFile* mar);

/**
 * Reads the specified amount of data from the buffer in MarFile that contains
 * the entirety of the MAR file data.
 * @param mar       The MAR file to read from.
 * @param dest      The buffer to read into.
 * @param position  The byte index to start reading from the MAR at.
 *                  On success, position will be incremented by size.
 * @param size      The number of bytes to read.
 * @return          0  If the specified amount of data was read.
 *                  -1 If the buffer MAR is not large enough to read the
 *                     specified amount of data at the specified position.
 */
int mar_read_buffer(MarFile* mar, void* dest, size_t* position, size_t size);

/**
 * Reads the specified amount of data from the buffer in MarFile that contains
 * the entirety of the MAR file data. If there isn't that much data remaining,
 * reads as much as possible.
 * @param mar       The MAR file to read from.
 * @param dest      The buffer to read into.
 * @param position  The byte index to start reading from the MAR at.
 *                  This function will increment position by the number of bytes
 *                  copied.
 * @param size      The maximum number of bytes to read.
 * @return          The number of bytes copied into dest.
 */
int mar_read_buffer_max(MarFile* mar, void* dest, size_t* position,
                        size_t size);

/**
 * Increments position by distance. Checks that the resulting position is still
 * within the bounds of the buffer. Much like fseek, this will allow position to
 * be successfully placed just after the end of the buffer.
 * @param mar       The MAR file to read from.
 * @param position  The byte index to start reading from the MAR at.
 *                  On success, position will be incremented by size.
 * @param distance  The number of bytes to move forward by.
 * @return          0  If position was successfully moved.
 *                  -1 If moving position by distance would move it outside the
 *                     bounds of the buffer.
 */
int mar_buffer_seek(MarFile* mar, size_t* position, size_t distance);

/**
 * Find an item in the MAR file by name.
 * @param mar       The MarFile object to query.
 * @param item      The name of the item to query.
 * @return          A const reference to a MAR item or NULL if not found.
 */
const MarItem* mar_find_item(MarFile* mar, const char* item);

/**
 * Enumerate all MAR items via callback function.
 * @param mar       The MAR file to enumerate.
 * @param callback  The function to call for each MAR item.
 * @param data      A caller specified value that is passed along to the
 *                  callback function.
 * @return          0 if the enumeration ran to completion.  Otherwise, any
 *                  non-zero return value from the callback is returned.
 */
int mar_enum_items(MarFile* mar, MarItemCallback callback, void* data);

/**
 * Read from MAR item at given offset up to bufsize bytes.
 * @param mar       The MAR file to read.
 * @param item      The MAR item to read.
 * @param offset    The byte offset relative to the start of the item.
 * @param buf       A pointer to a buffer to copy the data into.
 * @param bufsize   The length of the buffer to copy the data into.
 * @return          The number of bytes written or a negative value if an
 *                  error occurs.
 */
int mar_read(MarFile* mar, const MarItem* item, int offset, uint8_t* buf,
             int bufsize);

/**
 * Create a MAR file from a set of files.
 * @param dest      The path to the file to create.  This path must be
 *                  compatible with fopen.
 * @param numfiles  The number of files to store in the archive.
 * @param files     The list of null-terminated file paths.  Each file
 *                  path must be compatible with fopen.
 * @param infoBlock The information to store in the product information block.
 * @return          A non-zero value if an error occurs.
 */
int mar_create(const char* dest, int numfiles, char** files,
               struct ProductInformationBlock* infoBlock);

/**
 * Extract a MAR file to the current working directory.
 * @param path      The path to the MAR file to extract.  This path must be
 *                  compatible with fopen.
 * @return          A non-zero value if an error occurs.
 */
int mar_extract(const char* path);

#define MAR_MAX_CERT_SIZE (16 * 1024)  // Way larger than necessary

/* Read the entire file (not a MAR file) into a newly-allocated buffer.
 * This function does not write to stderr. Instead, the caller should
 * write whatever error messages it sees fit. The caller must free the returned
 * buffer using free().
 *
 * @param filePath The path to the file that should be read.
 * @param maxSize  The maximum valid file size.
 * @param data     On success, *data will point to a newly-allocated buffer
 *                 with the file's contents in it.
 * @param size     On success, *size will be the size of the created buffer.
 *
 * @return 0 on success, -1 on error
 */
int mar_read_entire_file(const char* filePath, uint32_t maxSize,
                         /*out*/ const uint8_t** data,
                         /*out*/ uint32_t* size);

/**
 * Verifies a MAR file by verifying each signature with the corresponding
 * certificate. That is, the first signature will be verified using the first
 * certificate given, the second signature will be verified using the second
 * certificate given, etc. The signature count must exactly match the number of
 * certificates given, and all signature verifications must succeed.
 * We do not check that the certificate was issued by any trusted authority.
 * We assume it to be self-signed.  We do not check whether the certificate
 * is valid for this usage.
 *
 * @param mar            The already opened MAR file.
 * @param certData       Pointer to the first element in an array of certificate
 *                       file data.
 * @param certDataSizes  Pointer to the first element in an array for size of
 *                       the cert data.
 * @param certCount      The number of elements in certData and certDataSizes
 * @return 0 on success
 *         a negative number if there was an error
 *         a positive number if the signature does not verify
 */
int mar_verify_signatures(MarFile* mar, const uint8_t* const* certData,
                          const uint32_t* certDataSizes, uint32_t certCount);

/**
 * Reads the product info block from the MAR file's additional block section.
 * The caller is responsible for freeing the fields in infoBlock
 * if the return is successful.
 *
 * @param infoBlock Out parameter for where to store the result to
 * @return 0 on success, -1 on failure
 */
int mar_read_product_info_block(MarFile* mar,
                                struct ProductInformationBlock* infoBlock);

#ifdef __cplusplus
}
#endif

#endif /* MAR_H__ */
