/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "cert.h"
#include "pk11pub.h"
int NSSInitCryptoContext(const char *NSSConfigDir);
#endif

int mar_repackage_and_sign(const char *NSSConfigDir,
                           const char * const *certNames,
                           uint32_t certCount,
                           const char *src, 
                           const char * dest);

static void print_version() {
  printf("Version: %s\n", MOZ_APP_VERSION);
  printf("Default Channel ID: %s\n", MAR_CHANNEL_ID);
}

static void print_usage() {
  printf("usage:\n");
  printf("Create a MAR file:\n");
  printf("  mar [-H MARChannelID] [-V ProductVersion] [-C workingDir] "
         "-c archive.mar [files...]\n");

  printf("Extract a MAR file:\n");
  printf("  mar [-C workingDir] -x archive.mar\n");
#ifndef NO_SIGN_VERIFY
  printf("Sign a MAR file:\n");
  printf("  mar [-C workingDir] -d NSSConfigDir -n certname -s "
         "archive.mar out_signed_archive.mar\n");

  printf("Strip a MAR signature:\n");
  printf("  mar [-C workingDir] -r "
         "signed_input_archive.mar output_archive.mar\n");

  printf("Extract a MAR signature:\n");
  printf("  mar [-C workingDir] -n(i) -X "
         "signed_input_archive.mar base_64_encoded_signature_file\n");

  printf("Import a MAR signature:\n");
  printf("  mar [-C workingDir] -n(i) -I "
         "signed_input_archive.mar base_64_encoded_signature_file "
         "changed_signed_output.mar\n");
  printf("(i) is the index of the certificate to extract\n");
#if defined(XP_MACOSX) || (defined(XP_WIN) && !defined(MAR_NSS))
  printf("Verify a MAR file:\n");
  printf("  mar [-C workingDir] -D DERFilePath -v signed_archive.mar\n");
  printf("At most %d signature certificate DER files are specified by "
         "-D0 DERFilePath1 -D1 DERFilePath2, ...\n", MAX_SIGNATURES);
#else
  printf("Verify a MAR file:\n");
  printf("  mar [-C workingDir] -d NSSConfigDir -n certname "
         "-v signed_archive.mar\n");
  printf("At most %d signature certificate names are specified by "
         "-n0 certName -n1 certName2, ...\n", MAX_SIGNATURES);
#endif
  printf("At most %d verification certificate names are specified by "
         "-n0 certName -n1 certName2, ...\n", MAX_SIGNATURES);
#endif
  printf("Print information on a MAR file:\n");
  printf("  mar -t archive.mar\n");

  printf("Print detailed information on a MAR file including signatures:\n");
  printf("  mar -T archive.mar\n");

  printf("Refresh the product information block of a MAR file:\n");
  printf("  mar [-H MARChannelID] [-V ProductVersion] [-C workingDir] "
         "-i unsigned_archive_to_refresh.mar\n");

  printf("Print executable version:\n");
  printf("  mar --version\n");
  printf("This program does not handle unicode file paths properly\n");
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
  const char *certNames[MAX_SIGNATURES];
  char *MARChannelID = MAR_CHANNEL_ID;
  char *productVersion = MOZ_APP_VERSION;
  uint32_t k;
  int rv = -1;
  uint32_t certCount = 0;
  int32_t sigIndex = -1;

#if !defined(NO_SIGN_VERIFY)
  uint32_t fileSizes[MAX_SIGNATURES];
  const uint8_t* certBuffers[MAX_SIGNATURES];
  char* DERFilePaths[MAX_SIGNATURES];
#if (!defined(XP_WIN) && !defined(XP_MACOSX)) || defined(MAR_NSS)
  CERTCertificate* certs[MAX_SIGNATURES];
#endif
#endif

  memset((void*)certNames, 0, sizeof(certNames));
#if defined(XP_WIN) && !defined(MAR_NSS) && !defined(NO_SIGN_VERIFY)
  memset((void*)certBuffers, 0, sizeof(certBuffers));
#endif
#if !defined(NO_SIGN_VERIFY) && ((!defined(MAR_NSS) && defined(XP_WIN)) || \
                                 defined(XP_MACOSX))
  memset(DERFilePaths, 0, sizeof(DERFilePaths));
  memset(fileSizes, 0, sizeof(fileSizes));
#endif

  if (argc > 1 && 0 == strcmp(argv[1], "--version")) {
    print_version();
    return 0;
  }

  if (argc < 3) {
    print_usage();
    return -1;
  }

  while (argc > 0) {
    if (argv[1][0] == '-' && (argv[1][1] == 'c' || 
        argv[1][1] == 't' || argv[1][1] == 'x' || 
        argv[1][1] == 'v' || argv[1][1] == 's' ||
        argv[1][1] == 'i' || argv[1][1] == 'T' ||
        argv[1][1] == 'r' || argv[1][1] == 'X' ||
        argv[1][1] == 'I')) {
      break;
    /* -C workingdirectory */
    } else if (argv[1][0] == '-' && argv[1][1] == 'C') {
      if (chdir(argv[2]) != 0) {
        return -1;
      }
      argv += 2;
      argc -= 2;
    } 
#if !defined(NO_SIGN_VERIFY) && ((!defined(MAR_NSS) && defined(XP_WIN)) || \
                                 defined(XP_MACOSX))
    /* -D DERFilePath, also matches -D[index] DERFilePath
       We allow an index for verifying to be symmetric
       with the import and export command line arguments. */
    else if (argv[1][0] == '-' &&
             argv[1][1] == 'D' &&
             (argv[1][2] == (char)('0' + certCount) || argv[1][2] == '\0')) {
      if (certCount >= MAX_SIGNATURES) {
        print_usage();
        return -1;
      }
      DERFilePaths[certCount++] = argv[2];
      argv += 2;
      argc -= 2;
    }
#endif
    /* -d NSSConfigdir */
    else if (argv[1][0] == '-' && argv[1][1] == 'd') {
      NSSConfigDir = argv[2];
      argv += 2;
      argc -= 2;
     /* -n certName, also matches -n[index] certName
        We allow an index for verifying to be symmetric
        with the import and export command line arguments. */
    } else if (argv[1][0] == '-' &&
               argv[1][1] == 'n' &&
               (argv[1][2] == (char)('0' + certCount) ||
                argv[1][2] == '\0' ||
                !strcmp(argv[2], "-X") ||
                !strcmp(argv[2], "-I"))) {
      if (certCount >= MAX_SIGNATURES) {
        print_usage();
        return -1;
      }
      certNames[certCount++] = argv[2];
      if (strlen(argv[1]) > 2 &&
          (!strcmp(argv[2], "-X") || !strcmp(argv[2], "-I")) &&
          argv[1][2] >= '0' && argv[1][2] <= '9') {
        sigIndex = argv[1][2] - '0';
        argv++;
        argc--;
      } else {
        argv += 2;
        argc -= 2;
      }
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
    struct ProductInformationBlock infoBlock;
    uint32_t numSignatures, numAdditionalBlocks;
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

  /* Extract a MAR file */
  case 'x':
    return mar_extract(argv[2]);

#ifndef NO_SIGN_VERIFY
  /* Extract a MAR signature */
  case 'X':
    if (sigIndex == -1) {
      fprintf(stderr, "ERROR: Signature index was not passed.\n");
      return -1;
    }
    if (sigIndex >= MAX_SIGNATURES || sigIndex < -1) {
      fprintf(stderr, "ERROR: Signature index is out of range: %d.\n",
              sigIndex);
      return -1;
    }
    return extract_signature(argv[2], sigIndex, argv[3]);

  /* Import a MAR signature */
  case 'I':
    if (sigIndex == -1) {
      fprintf(stderr, "ERROR: signature index was not passed.\n");
      return -1;
    }
    if (sigIndex >= MAX_SIGNATURES || sigIndex < -1) {
      fprintf(stderr, "ERROR: Signature index is out of range: %d.\n",
              sigIndex);
      return -1;
    }
    if (argc < 5) {
      print_usage();
      return -1;
    }
    return import_signature(argv[2], sigIndex, argv[3], argv[4]);

  case 'v':
    if (certCount == 0) {
      print_usage();
      return -1;
    }

#if (!defined(XP_WIN) && !defined(XP_MACOSX)) || defined(MAR_NSS)
    if (!NSSConfigDir || certCount == 0) {
      print_usage();
      return -1;
    }

    if (NSSInitCryptoContext(NSSConfigDir)) {
      fprintf(stderr, "ERROR: Could not initialize crypto library.\n");
      return -1;
    }
#endif

    rv = 0;
    for (k = 0; k < certCount; ++k) {
#if (defined(XP_WIN) || defined(XP_MACOSX)) && !defined(MAR_NSS)
      rv = mar_read_entire_file(DERFilePaths[k], MAR_MAX_CERT_SIZE,
                                &certBuffers[k], &fileSizes[k]);
#else
      /* It is somewhat circuitous to look up a CERTCertificate and then pass
       * in its DER encoding just so we can later re-create that
       * CERTCertificate to extract the public key out of it. However, by doing
       * things this way, we maximize the reuse of the mar_verify_signatures
       * function and also we keep the control flow as similar as possible
       * between programs and operating systems, at least for the functions
       * that are critically important to security.
       */
      certs[k] = PK11_FindCertFromNickname(certNames[k], NULL);
      if (certs[k]) {
        certBuffers[k] = certs[k]->derCert.data;
        fileSizes[k] = certs[k]->derCert.len;
      } else {
        rv = -1;
      }
#endif
      if (rv) {
        fprintf(stderr, "ERROR: could not read file %s", DERFilePaths[k]);
        break;
      }
    }

    if (!rv) {
      MarFile *mar = mar_open(argv[2]);
      if (mar) {
        rv = mar_verify_signatures(mar, certBuffers, fileSizes, certCount);
        mar_close(mar);
      } else {
        fprintf(stderr, "ERROR: Could not open MAR file.\n");
        rv = -1;
      }
    }
    for (k = 0; k < certCount; ++k) {
#if (defined(XP_WIN) || defined(XP_MACOSX)) && !defined(MAR_NSS)
      free((void*)certBuffers[k]);
#else
      /* certBuffers[k] is owned by certs[k] so don't free it */
      CERT_DestroyCertificate(certs[k]);
#endif
    }
    if (rv) {
      /* Determine if the source MAR file has the new fields for signing */
      int hasSignatureBlock;
      if (get_mar_file_info(argv[2], &hasSignatureBlock, 
                            NULL, NULL, NULL, NULL)) {
        fprintf(stderr, "ERROR: could not determine if MAR is old or new.\n");
      } else if (!hasSignatureBlock) {
        fprintf(stderr, "ERROR: The MAR file is in the old format so has"
                        " no signature to verify.\n");
      }
      return -1;
    }
    return 0;

  case 's':
    if (!NSSConfigDir || certCount == 0 || argc < 4) {
      print_usage();
      return -1;
    }
    return mar_repackage_and_sign(NSSConfigDir, certNames, certCount,
                                  argv[2], argv[3]);

  case 'r':
    return strip_signature_block(argv[2], argv[3]);
#endif /* endif NO_SIGN_VERIFY disabled */

  default:
    print_usage();
    return -1;
  }
}
