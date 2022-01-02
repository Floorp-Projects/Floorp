/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MAR_CMDLINE_H__
#define MAR_CMDLINE_H__

/* We use NSPR here just to import the definition of uint32_t */

#ifdef __cplusplus
extern "C" {
#endif

struct ProductInformationBlock;

/**
 * Determines MAR file information.
 *
 * @param path                   The path of the MAR file to check.
 * @param hasSignatureBlock      Optional out parameter specifying if the MAR
 *                               file has a signature block or not.
 * @param numSignatures          Optional out parameter for storing the number
 *                               of signatures in the MAR file.
 * @param hasAdditionalBlocks    Optional out parameter specifying if the MAR
 *                               file has additional blocks or not.
 * @param offsetAdditionalBlocks Optional out parameter for the offset to the
 *                               first additional block. Value is only valid if
 *                               hasAdditionalBlocks is not equal to 0.
 * @param numAdditionalBlocks    Optional out parameter for the number of
 *                               additional blocks.  Value is only valid if
 *                               has_additional_blocks is not equal to 0.
 * @return 0 on success and non-zero on failure.
 */
int get_mar_file_info(const char* path, int* hasSignatureBlock,
                      uint32_t* numSignatures, int* hasAdditionalBlocks,
                      uint32_t* offsetAdditionalBlocks,
                      uint32_t* numAdditionalBlocks);

/**
 * Reads the product info block from the MAR file's additional block section.
 * The caller is responsible for freeing the fields in infoBlock
 * if the return is successful.
 *
 * @param infoBlock Out parameter for where to store the result to
 * @return 0 on success, -1 on failure
 */
int read_product_info_block(char* path,
                            struct ProductInformationBlock* infoBlock);

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
                               struct ProductInformationBlock* infoBlock);

/**
 * Writes out a copy of the MAR at src but with the signature block stripped.
 *
 * @param  src  The path of the source MAR file
 * @param  dest The path of the MAR file to write out that
                has no signature block
 * @return 0 on success
 *         -1 on error
*/
int strip_signature_block(const char* src, const char* dest);

/**
 * Extracts a signature from a MAR file, base64 encodes it, and writes it out
 *
 * @param  src       The path of the source MAR file
 * @param  sigIndex  The index of the signature to extract
 * @param  dest      The path of file to write the signature to
 * @return 0 on success
 *         -1 on error
 */
int extract_signature(const char* src, uint32_t sigIndex, const char* dest);

/**
 * Imports a base64 encoded signature into a MAR file
 *
 * @param  src           The path of the source MAR file
 * @param  sigIndex      The index of the signature to import
 * @param  base64SigFile A file which contains the signature to import
 * @param  dest          The path of the destination MAR file with replaced
 *                       signature
 * @return 0 on success
 *         -1 on error
 */
int import_signature(const char* src, uint32_t sigIndex,
                     const char* base64SigFile, const char* dest);

#ifdef __cplusplus
}
#endif

#endif /* MAR_CMDLINE_H__ */
