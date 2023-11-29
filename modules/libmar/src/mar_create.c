/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "mar_private.h"
#include "mar_cmdline.h"
#include "mar.h"

#ifdef XP_WIN
#  include <winsock2.h>
#else
#  include <netinet/in.h>
#  include <unistd.h>
#endif

struct MarItemStack {
  void* head;
  uint32_t size_used;
  uint32_t size_allocated;
  uint32_t last_offset;
};

/**
 * Push a new item onto the stack of items.  The stack is a single block
 * of memory.
 */
static int mar_push(struct MarItemStack* stack, uint32_t length, uint32_t flags,
                    const char* name) {
  int namelen;
  uint32_t n_offset, n_length, n_flags;
  uint32_t size;
  char* data;

  namelen = strlen(name);
  size = MAR_ITEM_SIZE(namelen);

  if (stack->size_allocated - stack->size_used < size) {
    /* increase size of stack */
    size_t size_needed = ROUND_UP(stack->size_used + size, BLOCKSIZE);
    stack->head = realloc(stack->head, size_needed);
    if (!stack->head) {
      return -1;
    }
    stack->size_allocated = size_needed;
  }

  data = (((char*)stack->head) + stack->size_used);

  n_offset = htonl(stack->last_offset);
  n_length = htonl(length);
  n_flags = htonl(flags);

  memcpy(data, &n_offset, sizeof(n_offset));
  data += sizeof(n_offset);

  memcpy(data, &n_length, sizeof(n_length));
  data += sizeof(n_length);

  memcpy(data, &n_flags, sizeof(n_flags));
  data += sizeof(n_flags);

  memcpy(data, name, namelen + 1);

  stack->size_used += size;
  stack->last_offset += length;
  return 0;
}

static int mar_concat_file(FILE* fp, const char* path) {
  FILE* in;
  char buf[BLOCKSIZE];
  size_t len;
  int rv = 0;

  in = fopen(path, "rb");
  if (!in) {
    fprintf(stderr, "ERROR: could not open file in mar_concat_file()\n");
    perror(path);
    return -1;
  }

  while ((len = fread(buf, 1, BLOCKSIZE, in)) > 0) {
    if (fwrite(buf, len, 1, fp) != 1) {
      rv = -1;
      break;
    }
  }

  fclose(in);
  return rv;
}

/**
 * Writes out the product information block to the specified file.
 *
 * @param fp           The opened MAR file being created.
 * @param stack        A pointer to the MAR item stack being used to create
 *                     the MAR
 * @param infoBlock    The product info block to store in the file.
 * @return 0 on success.
 */
static int mar_concat_product_info_block(
    FILE* fp, struct MarItemStack* stack,
    struct ProductInformationBlock* infoBlock) {
  char buf[PIB_MAX_MAR_CHANNEL_ID_SIZE + PIB_MAX_PRODUCT_VERSION_SIZE];
  uint32_t additionalBlockID = 1, infoBlockSize, unused;
  if (!fp || !infoBlock || !infoBlock->MARChannelID ||
      !infoBlock->productVersion) {
    return -1;
  }

  /* The MAR channel name must be < 64 bytes per the spec */
  if (strlen(infoBlock->MARChannelID) > PIB_MAX_MAR_CHANNEL_ID_SIZE) {
    return -1;
  }

  /* The product version must be < 32 bytes per the spec */
  if (strlen(infoBlock->productVersion) > PIB_MAX_PRODUCT_VERSION_SIZE) {
    return -1;
  }

  /* Although we don't need the product information block size to include the
     maximum MAR channel name and product version, we allocate the maximum
     amount to make it easier to modify the MAR file for repurposing MAR files
     to different MAR channels. + 2 is for the NULL terminators. */
  infoBlockSize = sizeof(infoBlockSize) + sizeof(additionalBlockID) +
                  PIB_MAX_MAR_CHANNEL_ID_SIZE + PIB_MAX_PRODUCT_VERSION_SIZE +
                  2;
  if (stack) {
    stack->last_offset += infoBlockSize;
  }

  /* Write out the product info block size */
  infoBlockSize = htonl(infoBlockSize);
  if (fwrite(&infoBlockSize, sizeof(infoBlockSize), 1, fp) != 1) {
    return -1;
  }
  infoBlockSize = ntohl(infoBlockSize);

  /* Write out the product info block ID */
  additionalBlockID = htonl(additionalBlockID);
  if (fwrite(&additionalBlockID, sizeof(additionalBlockID), 1, fp) != 1) {
    return -1;
  }
  additionalBlockID = ntohl(additionalBlockID);

  /* Write out the channel name and NULL terminator */
  if (fwrite(infoBlock->MARChannelID, strlen(infoBlock->MARChannelID) + 1, 1,
             fp) != 1) {
    return -1;
  }

  /* Write out the product version string and NULL terminator */
  if (fwrite(infoBlock->productVersion, strlen(infoBlock->productVersion) + 1,
             1, fp) != 1) {
    return -1;
  }

  /* Write out the rest of the block that is unused */
  unused = infoBlockSize - (sizeof(infoBlockSize) + sizeof(additionalBlockID) +
                            strlen(infoBlock->MARChannelID) +
                            strlen(infoBlock->productVersion) + 2);
  memset(buf, 0, sizeof(buf));
  if (fwrite(buf, unused, 1, fp) != 1) {
    return -1;
  }
  return 0;
}

/**
 * Refreshes the product information block with the new information.
 * The input MAR must not be signed or the function call will fail.
 *
 * @param path             The path to the MAR file whose product info block
 *                         should be refreshed.
 * @param infoBlock        Out parameter for where to store the result to
 * @return 0 on success, -1 on failure
 */
int refresh_product_info_block(const char* path,
                               struct ProductInformationBlock* infoBlock) {
  FILE* fp;
  int rv;
  uint32_t numSignatures, additionalBlockSize, additionalBlockID,
      offsetAdditionalBlocks, numAdditionalBlocks, i;
  int additionalBlocks, hasSignatureBlock;
  int64_t oldPos;

  rv = get_mar_file_info(path, &hasSignatureBlock, &numSignatures,
                         &additionalBlocks, &offsetAdditionalBlocks,
                         &numAdditionalBlocks);
  if (rv) {
    fprintf(stderr, "ERROR: Could not obtain MAR information.\n");
    return -1;
  }

  if (hasSignatureBlock && numSignatures) {
    fprintf(stderr, "ERROR: Cannot refresh a signed MAR\n");
    return -1;
  }

  fp = fopen(path, "r+b");
  if (!fp) {
    fprintf(stderr, "ERROR: could not open target file: %s\n", path);
    return -1;
  }

  if (fseeko(fp, offsetAdditionalBlocks, SEEK_SET)) {
    fprintf(stderr, "ERROR: could not seek to additional blocks\n");
    fclose(fp);
    return -1;
  }

  for (i = 0; i < numAdditionalBlocks; ++i) {
    /* Get the position of the start of this block */
    oldPos = ftello(fp);

    /* Read the additional block size */
    if (fread(&additionalBlockSize, sizeof(additionalBlockSize), 1, fp) != 1) {
      fclose(fp);
      return -1;
    }
    additionalBlockSize = ntohl(additionalBlockSize);

    /* Read the additional block ID */
    if (fread(&additionalBlockID, sizeof(additionalBlockID), 1, fp) != 1) {
      fclose(fp);
      return -1;
    }
    additionalBlockID = ntohl(additionalBlockID);

    if (PRODUCT_INFO_BLOCK_ID == additionalBlockID) {
      if (fseeko(fp, oldPos, SEEK_SET)) {
        fprintf(stderr, "Could not seek back to Product Information Block\n");
        fclose(fp);
        return -1;
      }

      if (mar_concat_product_info_block(fp, NULL, infoBlock)) {
        fprintf(stderr, "Could not concat Product Information Block\n");
        fclose(fp);
        return -1;
      }

      fclose(fp);
      return 0;
    }

    /* This is not the additional block you're looking for. Move along. */
    if (fseek(fp, additionalBlockSize, SEEK_CUR)) {
      fprintf(stderr, "ERROR: Could not seek past current block.\n");
      fclose(fp);
      return -1;
    }
  }

  /* If we had a product info block we would have already returned */
  fclose(fp);
  fprintf(stderr, "ERROR: Could not refresh because block does not exist\n");
  return -1;
}

/**
 * Create a MAR file from a set of files.
 * @param dest      The path to the file to create.  This path must be
 *                  compatible with fopen.
 * @param numfiles  The number of files to store in the archive.
 * @param files     The list of null-terminated file paths.  Each file
 *                  path must be compatible with fopen.
 * @param infoBlock The information to store in the product information block.
 * @return A non-zero value if an error occurs.
 */
int mar_create(const char* dest, int num_files, char** files,
               struct ProductInformationBlock* infoBlock) {
  struct MarItemStack stack;
  uint32_t offset_to_index = 0, size_of_index, numSignatures,
           numAdditionalSections;
  uint64_t sizeOfEntireMAR = 0;
  struct stat st;
  FILE* fp;
  int i, rv = -1;

  memset(&stack, 0, sizeof(stack));

  fp = fopen(dest, "wb");
  if (!fp) {
    fprintf(stderr, "ERROR: could not create target file: %s\n", dest);
    return -1;
  }

  if (fwrite(MAR_ID, MAR_ID_SIZE, 1, fp) != 1) {
    goto failure;
  }
  if (fwrite(&offset_to_index, sizeof(uint32_t), 1, fp) != 1) {
    goto failure;
  }

  stack.last_offset = MAR_ID_SIZE + sizeof(offset_to_index) +
                      sizeof(numSignatures) + sizeof(numAdditionalSections) +
                      sizeof(sizeOfEntireMAR);

  /* We will circle back on this at the end of the MAR creation to fill it */
  if (fwrite(&sizeOfEntireMAR, sizeof(sizeOfEntireMAR), 1, fp) != 1) {
    goto failure;
  }

  /* Write out the number of signatures, for now only at most 1 is supported */
  numSignatures = 0;
  if (fwrite(&numSignatures, sizeof(numSignatures), 1, fp) != 1) {
    goto failure;
  }

  /* Write out the number of additional sections, for now just 1
     for the product info block */
  numAdditionalSections = htonl(1);
  if (fwrite(&numAdditionalSections, sizeof(numAdditionalSections), 1, fp) !=
      1) {
    goto failure;
  }
  numAdditionalSections = ntohl(numAdditionalSections);

  if (mar_concat_product_info_block(fp, &stack, infoBlock)) {
    goto failure;
  }

  for (i = 0; i < num_files; ++i) {
    if (stat(files[i], &st)) {
      fprintf(stderr, "ERROR: file not found: %s\n", files[i]);
      goto failure;
    }

    if (mar_push(&stack, st.st_size, st.st_mode & 0777, files[i])) {
      goto failure;
    }

    /* concatenate input file to archive */
    if (mar_concat_file(fp, files[i])) {
      goto failure;
    }
  }

  /* write out the index (prefixed with length of index) */
  size_of_index = htonl(stack.size_used);
  if (fwrite(&size_of_index, sizeof(size_of_index), 1, fp) != 1) {
    goto failure;
  }
  if (fwrite(stack.head, stack.size_used, 1, fp) != 1) {
    goto failure;
  }

  /* To protect against invalid MAR files, we assumes that the MAR file
     size is less than or equal to MAX_SIZE_OF_MAR_FILE. */
  if (ftell(fp) > MAX_SIZE_OF_MAR_FILE) {
    goto failure;
  }

  /* write out offset to index file in network byte order */
  offset_to_index = htonl(stack.last_offset);
  if (fseek(fp, MAR_ID_SIZE, SEEK_SET)) {
    goto failure;
  }
  if (fwrite(&offset_to_index, sizeof(offset_to_index), 1, fp) != 1) {
    goto failure;
  }
  offset_to_index = ntohl(stack.last_offset);

  sizeOfEntireMAR =
      ((uint64_t)stack.last_offset) + stack.size_used + sizeof(size_of_index);
  sizeOfEntireMAR = HOST_TO_NETWORK64(sizeOfEntireMAR);
  if (fwrite(&sizeOfEntireMAR, sizeof(sizeOfEntireMAR), 1, fp) != 1) {
    goto failure;
  }
  sizeOfEntireMAR = NETWORK_TO_HOST64(sizeOfEntireMAR);

  rv = 0;
failure:
  if (stack.head) {
    free(stack.head);
  }
  fclose(fp);
  if (rv) {
    remove(dest);
  }
  return rv;
}
