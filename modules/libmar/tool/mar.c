/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "mar.h"
#include "mar_cmdline.h"

#ifdef XP_WIN
#include <windows.h>
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

#if !defined(NO_SIGN_VERIFY) && (!defined(XP_WIN) || defined(MAR_NSS))
int NSSInitCryptoContext(const char *NSSConfigDir);
#endif

int mar_repackage_and_sign(const char *NSSConfigDir,
                           const char *certName, 
                           const char *src, 
                           const char * dest);

static void print_usage() {
  printf("usage:\n");
  printf("  mar [-H MARChannelID] [-V ProductVersion] [-C workingDir] "
         "{-c|-x|-t|-T} archive.mar [files...]\n");
#ifndef NO_SIGN_VERIFY
  printf("  mar [-C workingDir] -d NSSConfigDir -n certname -s "
         "archive.mar out_signed_archive.mar\n");
  printf("  mar [-C workingDir] -r "
         "signed_input_archive.mar output_archive.mar\n");
#if defined(XP_WIN) && !defined(MAR_NSS)
  printf("  mar [-C workingDir] -D DERFilePath -v signed_archive.mar\n");
#else 
  printf("  mar [-C workingDir] -d NSSConfigDir -n certname "
    "-v signed_archive.mar\n");
#endif
#endif
  printf("  mar [-H MARChannelID] [-V ProductVersion] [-C workingDir] "
         "-i unsigned_archive_to_refresh.mar\n");
}

static int mar_test_callback(MarFile *mar, 
                             const MarItem *item, 
                             void *unused) {
  printf("%u\t0%o\t%s\n", item->length, item->flags, item->name);
  return 0;
}

static int mar_test(const char *path) {
  MarFile *mar;

  mar = mar_open(path);
  if (!mar)
    return -1;

  printf("SIZE\tMODE\tNAME\n");
  mar_enum_items(mar, mar_test_callback, NULL);

  mar_close(mar);
  return 0;
}

int main(int argc, char **argv) {
  char *NSSConfigDir = NULL;
  char *certName = NULL;
  char *MARChannelID = MAR_CHANNEL_ID;
  char *productVersion = MOZ_APP_VERSION;
#if defined(XP_WIN) && !defined(MAR_NSS) && !defined(NO_SIGN_VERIFY)
  HANDLE certFile;
  DWORD fileSize;
  DWORD read;
  char *certBuffer;
  char *DERFilePath = NULL;
#endif

  if (argc < 3) {
    print_usage();
    return -1;
  }

  while (argc > 0) {
    if (argv[1][0] == '-' && (argv[1][1] == 'c' || 
        argv[1][1] == 't' || argv[1][1] == 'x' || 
        argv[1][1] == 'v' || argv[1][1] == 's' ||
        argv[1][1] == 'i' || argv[1][1] == 'T' ||
        argv[1][1] == 'r')) {
      break;
    /* -C workingdirectory */
    } else if (argv[1][0] == '-' && argv[1][1] == 'C') {
      chdir(argv[2]);
      argv += 2;
      argc -= 2;
    } 
#if defined(XP_WIN) && !defined(MAR_NSS) && !defined(NO_SIGN_VERIFY)
    /* -D DERFilePath */
    else if (argv[1][0] == '-' && argv[1][1] == 'D') {
      DERFilePath = argv[2];
      argv += 2;
      argc -= 2;
    }
#endif
    /* -d NSSConfigdir */
    else if (argv[1][0] == '-' && argv[1][1] == 'd') {
      NSSConfigDir = argv[2];
      argv += 2;
      argc -= 2;
     /* -n certName */
    } else if (argv[1][0] == '-' && argv[1][1] == 'n') {
      certName = argv[2];
      argv += 2;
      argc -= 2;
    /* MAR channel ID */
    } else if (argv[1][0] == '-' && argv[1][1] == 'H') {
      MARChannelID = argv[2];
      argv += 2;
      argc -= 2;
    /* Product Version */
    } else if (argv[1][0] == '-' && argv[1][1] == 'V') {
      productVersion = argv[2];
      argv += 2;
      argc -= 2;
    }
    else {
      print_usage();
      return -1;
    }
  }

  if (argv[1][0] != '-') {
    print_usage();
    return -1;
  }

  switch (argv[1][1]) {
  case 'c': {
    struct ProductInformationBlock infoBlock;
    infoBlock.MARChannelID = MARChannelID;
    infoBlock.productVersion = productVersion;
    return mar_create(argv[2], argc - 3, argv + 3, &infoBlock);
  }
  case 'i': {
    struct ProductInformationBlock infoBlock;
    infoBlock.MARChannelID = MARChannelID;
    infoBlock.productVersion = productVersion;
    return refresh_product_info_block(argv[2], &infoBlock);
  }
  case 'T': {
    int rv;
    struct ProductInformationBlock infoBlock;
    PRUint32 numSignatures, numAdditionalBlocks;
    int hasSignatureBlock, hasAdditionalBlock;
    if (!get_mar_file_info(argv[2], 
                           &hasSignatureBlock,
                           &numSignatures,
                           &hasAdditionalBlock, 
                           NULL, &numAdditionalBlocks)) {
      if (hasSignatureBlock) {
        printf("Signature block found with %d signature%s\n", 
               numSignatures, 
               numSignatures != 1 ? "s" : "");
      }
      if (hasAdditionalBlock) {
        printf("%d additional block%s found:\n", 
               numAdditionalBlocks,
               numAdditionalBlocks != 1 ? "s" : "");
      }

      rv = read_product_info_block(argv[2], &infoBlock);
      if (!rv) {
        printf("  - Product Information Block:\n");
        printf("    - MAR channel name: %s\n"
               "    - Product version: %s\n",
               infoBlock.MARChannelID,
               infoBlock.productVersion);
        free((void *)infoBlock.MARChannelID);
        free((void *)infoBlock.productVersion);
      }
     }
    printf("\n");
    /* The fall through from 'T' to 't' is intentional */
  }
  case 't':
    return mar_test(argv[2]);

  case 'x':
    return mar_extract(argv[2]);

#ifndef NO_SIGN_VERIFY
  case 'v':

#if defined(XP_WIN) && !defined(MAR_NSS)
    if (!DERFilePath) {
      print_usage();
      return -1;
    }
    /* If the mar program was built using CryptoAPI, then read in the buffer
       containing the cert from disk. */
    certFile = CreateFileA(DERFilePath, GENERIC_READ, 
                           FILE_SHARE_READ | 
                           FILE_SHARE_WRITE | 
                           FILE_SHARE_DELETE, 
                           NULL, 
                           OPEN_EXISTING, 
                           0, NULL);
    if (INVALID_HANDLE_VALUE == certFile) {
      return -1;
    }
    fileSize = GetFileSize(certFile, NULL);
    certBuffer = malloc(fileSize);
    if (!ReadFile(certFile, certBuffer, fileSize, &read, NULL) || 
        fileSize != read) {
      CloseHandle(certFile);
      free(certBuffer);
      return -1;
    }
    CloseHandle(certFile);

    if (mar_verify_signature(argv[2], certBuffer, fileSize, NULL)) {
      /* Determine if the source MAR file has the new fields for signing */
      int hasSignatureBlock;
      free(certBuffer);
      if (get_mar_file_info(argv[2], &hasSignatureBlock, 
                            NULL, NULL, NULL, NULL)) {
        fprintf(stderr, "ERROR: could not determine if MAR is old or new.\n");
      } else if (!hasSignatureBlock) {
        fprintf(stderr, "ERROR: The MAR file is in the old format so has"
                        " no signature to verify.\n");
      }
      return -1;
    }

    free(certBuffer);
    return 0;
#else
    if (!NSSConfigDir || !certName) {
      print_usage();
      return -1;
    }

    if (NSSInitCryptoContext(NSSConfigDir)) {
      fprintf(stderr, "ERROR: Could not initialize crypto library.\n");
      return -1;
    }

    return mar_verify_signature(argv[2], NULL, 0, 
                                certName);

#endif /* defined(XP_WIN) && !defined(MAR_NSS) */
  case 's':
    if (!NSSConfigDir || !certName || argc < 4) {
      print_usage();
      return -1;
    }
    return mar_repackage_and_sign(NSSConfigDir, certName, argv[2], argv[3]);

  case 'r':
    return strip_signature_block(argv[2], argv[3]);
#endif /* endif NO_SIGN_VERIFY disabled */

  default:
    print_usage();
    return -1;
  }

  return 0;
}
