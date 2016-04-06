/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This is a very trivial program I wrote for testing out a 
 * couple data-only Cryptoki modules for NSS.  It's not a "real"
 * test program that prints out nice "PASS" or "FAIL" messages;
 * it just makes calls and dumps data.
 */

#include "config.h"

#ifdef HAVE_NSPR_H
#include "nspr.h"
#else
#error "NSPR is required."
#endif

#ifdef WITH_NSS
#define FGMR 1
#include "ck.h"
#else
#include "pkcs11t.h"
#include "pkcs11.h"
#endif

/* The RSA versions are sloppier with namespaces */
#ifndef CK_TRUE
#define CK_TRUE TRUE
#endif

#ifndef CK_FALSE
#define CK_FALSE FALSE
#endif

int
rmain
(
  int argc,
  char *argv[]
);

int
main
(
  int argc,
  char *argv[]
)
{
  int rv = 0;

  PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 14);
  rv = rmain(argc, argv);
  PR_Cleanup();

  return rv;
}

static CK_ATTRIBUTE_TYPE all_known_attribute_types[] = {
  CKA_CLASS,
  CKA_TOKEN,
  CKA_PRIVATE,
  CKA_LABEL,
  CKA_APPLICATION,
  CKA_VALUE,
  CKA_CERTIFICATE_TYPE,
  CKA_ISSUER,
  CKA_SERIAL_NUMBER,
  CKA_KEY_TYPE,
  CKA_SUBJECT,
  CKA_ID,
  CKA_SENSITIVE,
  CKA_ENCRYPT,
  CKA_DECRYPT,
  CKA_WRAP,
  CKA_UNWRAP,
  CKA_SIGN,
  CKA_SIGN_RECOVER,
  CKA_VERIFY,
  CKA_VERIFY_RECOVER,
  CKA_DERIVE,
  CKA_START_DATE,
  CKA_END_DATE,
  CKA_MODULUS,
  CKA_MODULUS_BITS,
  CKA_PUBLIC_EXPONENT,
  CKA_PRIVATE_EXPONENT,
  CKA_PRIME_1,
  CKA_PRIME_2,
  CKA_EXPONENT_1,
  CKA_EXPONENT_2,
  CKA_COEFFICIENT,
  CKA_PRIME,
  CKA_SUBPRIME,
  CKA_BASE,
  CKA_VALUE_BITS,
  CKA_VALUE_LEN,
  CKA_EXTRACTABLE,
  CKA_LOCAL,
  CKA_NEVER_EXTRACTABLE,
  CKA_ALWAYS_SENSITIVE,
  CKA_MODIFIABLE,
#ifdef CKA_NETSCAPE
  CKA_NETSCAPE_URL,
  CKA_NETSCAPE_EMAIL,
  CKA_NETSCAPE_SMIME_INFO,
  CKA_NETSCAPE_SMIME_TIMESTAMP,
  CKA_NETSCAPE_PKCS8_SALT,
  CKA_NETSCAPE_PASSWORD_CHECK,
  CKA_NETSCAPE_EXPIRES,
#endif /* CKA_NETSCAPE */
#ifdef CKA_TRUST
  CKA_TRUST_DIGITAL_SIGNATURE,
  CKA_TRUST_NON_REPUDIATION,
  CKA_TRUST_KEY_ENCIPHERMENT,
  CKA_TRUST_DATA_ENCIPHERMENT,
  CKA_TRUST_KEY_AGREEMENT,
  CKA_TRUST_KEY_CERT_SIGN,
  CKA_TRUST_CRL_SIGN,
  CKA_TRUST_SERVER_AUTH,
  CKA_TRUST_CLIENT_AUTH,
  CKA_TRUST_CODE_SIGNING,
  CKA_TRUST_EMAIL_PROTECTION,
  CKA_TRUST_IPSEC_END_SYSTEM,
  CKA_TRUST_IPSEC_TUNNEL,
  CKA_TRUST_IPSEC_USER,
  CKA_TRUST_TIME_STAMPING,
#endif /* CKA_TRUST */
};

static number_of_all_known_attribute_types = 
  (sizeof(all_known_attribute_types)/sizeof(all_known_attribute_types[0]));

int
usage
(
  char *argv0
)
{
  PR_fprintf(PR_STDERR, "Usage: %s [-i {string|--}] <library>.so\n", argv0);
  return 1;
}

int
rmain
(
  int argc,
  char *argv[]
)
{
  char *argv0 = argv[0];
  PRLibrary *lib;
  CK_C_GetFunctionList gfl;
  CK_FUNCTION_LIST_PTR epv = (CK_FUNCTION_LIST_PTR)NULL;
  CK_RV ck_rv;
  CK_INFO info;
  CK_ULONG nSlots;
  CK_SLOT_ID *pSlots;
  CK_ULONG i;
  CK_C_INITIALIZE_ARGS ia, *iap;

  (void)memset(&ia, 0, sizeof(CK_C_INITIALIZE_ARGS));
  iap = (CK_C_INITIALIZE_ARGS *)NULL;
  while( argv++, --argc ) {
    if( '-' == argv[0][0] ) {
      switch( argv[0][1] ) {
      case 'i':
        iap = &ia;
        if( ((char *)NULL != argv[1]) && ('-' != argv[1][0]) ) {
#ifdef WITH_NSS
          ia.pConfig = argv[1];
          ia.ulConfigLen = strlen(argv[1]);
          argv++, --argc;
#else
          return usage(argv0);
#endif /* WITH_NSS */
        }
        break;
      case '-':
        argv++, --argc;
        goto endargs;
      default:
        return usage(argv0);
      }
    } else {
      break;
    }
  }
 endargs:;

  if( 1 != argc ) {
    return usage(argv0);
  }

  lib = PR_LoadLibrary(argv[0]);
  if( (PRLibrary *)NULL == lib ) {
    PR_fprintf(PR_STDERR, "Can't load %s: %ld, %ld\n", argv[1], PR_GetError(), PR_GetOSError());
    return 1;
  }

  gfl = (CK_C_GetFunctionList)PR_FindSymbol(lib, "C_GetFunctionList");
  if( (CK_C_GetFunctionList)NULL == gfl ) {
    PR_fprintf(PR_STDERR, "Can't find C_GetFunctionList in %s: %ld, %ld\n", argv[1], 
               PR_GetError(), PR_GetOSError());
    return 1;
  }

  ck_rv = (*gfl)(&epv);
  if( CKR_OK != ck_rv ) {
    PR_fprintf(PR_STDERR, "CK_GetFunctionList returned 0x%08x\n", ck_rv);
    return 1;
  }

  PR_fprintf(PR_STDOUT, "Module %s loaded, epv = 0x%08x.\n\n", argv[1], (CK_ULONG)epv);

  /* C_Initialize */
  ck_rv = epv->C_Initialize(iap);
  if( CKR_OK != ck_rv ) {
    PR_fprintf(PR_STDERR, "C_Initialize returned 0x%08x\n", ck_rv);
    return 1;
  }

  /* C_GetInfo */
  (void)memset(&info, 0, sizeof(CK_INFO));
  ck_rv = epv->C_GetInfo(&info);
  if( CKR_OK != ck_rv ) {
    PR_fprintf(PR_STDERR, "C_GetInfo returned 0x%08x\n", ck_rv);
    return 1;
  }

  PR_fprintf(PR_STDOUT, "Module Info:\n");
  PR_fprintf(PR_STDOUT, "    cryptokiVersion = %lu.%02lu\n", 
             (PRUint32)info.cryptokiVersion.major, (PRUint32)info.cryptokiVersion.minor);
  PR_fprintf(PR_STDOUT, "    manufacturerID = \"%.32s\"\n", info.manufacturerID);
  PR_fprintf(PR_STDOUT, "    flags = 0x%08lx\n", info.flags);
  PR_fprintf(PR_STDOUT, "    libraryDescription = \"%.32s\"\n", info.libraryDescription);
  PR_fprintf(PR_STDOUT, "    libraryVersion = %lu.%02lu\n", 
             (PRUint32)info.libraryVersion.major, (PRUint32)info.libraryVersion.minor);
  PR_fprintf(PR_STDOUT, "\n");

  /* C_GetSlotList */
  nSlots = 0;
  ck_rv = epv->C_GetSlotList(CK_FALSE, (CK_SLOT_ID_PTR)CK_NULL_PTR, &nSlots);
  switch( ck_rv ) {
  case CKR_BUFFER_TOO_SMALL:
  case CKR_OK:
    break;
  default:
    PR_fprintf(PR_STDERR, "C_GetSlotList(FALSE, NULL, ) returned 0x%08x\n", ck_rv);
    return 1;
  }

  PR_fprintf(PR_STDOUT, "There are %lu slots.\n", nSlots);

  pSlots = (CK_SLOT_ID_PTR)PR_Calloc(nSlots, sizeof(CK_SLOT_ID));
  if( (CK_SLOT_ID_PTR)NULL == pSlots ) {
    PR_fprintf(PR_STDERR, "[memory allocation of %lu bytes failed]\n", nSlots * sizeof(CK_SLOT_ID));
    return 1;
  }

  ck_rv = epv->C_GetSlotList(CK_FALSE, pSlots, &nSlots);
  if( CKR_OK != ck_rv ) {
    PR_fprintf(PR_STDERR, "C_GetSlotList(FALSE, , ) returned 0x%08x\n", ck_rv);
    return 1;
  }

  for( i = 0; i < nSlots; i++ ) {
    PR_fprintf(PR_STDOUT, "    [%lu]: CK_SLOT_ID = %lu\n", (i+1), pSlots[i]);
  }

  PR_fprintf(PR_STDOUT, "\n");

  /* C_GetSlotInfo */
  for( i = 0; i < nSlots; i++ ) {
    CK_SLOT_INFO sinfo;

    PR_fprintf(PR_STDOUT, "[%lu]: CK_SLOT_ID = %lu\n", (i+1), pSlots[i]);

    (void)memset(&sinfo, 0, sizeof(CK_SLOT_INFO));
    ck_rv = epv->C_GetSlotInfo(pSlots[i], &sinfo);
    if( CKR_OK != ck_rv ) {
      PR_fprintf(PR_STDERR, "C_GetSlotInfo(%lu, ) returned 0x%08x\n", pSlots[i], ck_rv);
      return 1;
    }

    PR_fprintf(PR_STDOUT, "    Slot Info:\n");
    PR_fprintf(PR_STDOUT, "        slotDescription = \"%.64s\"\n", sinfo.slotDescription);
    PR_fprintf(PR_STDOUT, "        manufacturerID = \"%.32s\"\n", sinfo.manufacturerID);
    PR_fprintf(PR_STDOUT, "        flags = 0x%08lx\n", sinfo.flags);
    PR_fprintf(PR_STDOUT, "            -> TOKEN PRESENT = %s\n", 
               sinfo.flags & CKF_TOKEN_PRESENT ? "TRUE" : "FALSE");
    PR_fprintf(PR_STDOUT, "            -> REMOVABLE DEVICE = %s\n",
               sinfo.flags & CKF_REMOVABLE_DEVICE ? "TRUE" : "FALSE");
    PR_fprintf(PR_STDOUT, "            -> HW SLOT = %s\n", 
               sinfo.flags & CKF_HW_SLOT ? "TRUE" : "FALSE");
    PR_fprintf(PR_STDOUT, "        hardwareVersion = %lu.%02lu\n", 
               (PRUint32)sinfo.hardwareVersion.major, (PRUint32)sinfo.hardwareVersion.minor);
    PR_fprintf(PR_STDOUT, "        firmwareVersion = %lu.%02lu\n",
               (PRUint32)sinfo.firmwareVersion.major, (PRUint32)sinfo.firmwareVersion.minor);

    if( sinfo.flags & CKF_TOKEN_PRESENT ) {
      CK_TOKEN_INFO tinfo;
      CK_MECHANISM_TYPE *pMechanismList;
      CK_ULONG nMechanisms = 0;
      CK_ULONG j;

      (void)memset(&tinfo, 0, sizeof(CK_TOKEN_INFO));
      ck_rv = epv->C_GetTokenInfo(pSlots[i], &tinfo);
      if( CKR_OK != ck_rv ) {
        PR_fprintf(PR_STDERR, "C_GetTokenInfo(%lu, ) returned 0x%08x\n", pSlots[i], ck_rv);
        return 1;
      }

      PR_fprintf(PR_STDOUT, "    Token Info:\n");
      PR_fprintf(PR_STDOUT, "        label = \"%.32s\"\n", tinfo.label);
      PR_fprintf(PR_STDOUT, "        manufacturerID = \"%.32s\"\n", tinfo.manufacturerID);
      PR_fprintf(PR_STDOUT, "        model = \"%.16s\"\n", tinfo.model);
      PR_fprintf(PR_STDOUT, "        serialNumber = \"%.16s\"\n", tinfo.serialNumber);
      PR_fprintf(PR_STDOUT, "        flags = 0x%08lx\n", tinfo.flags);
      PR_fprintf(PR_STDOUT, "            -> RNG = %s\n",
                 tinfo.flags & CKF_RNG ? "TRUE" : "FALSE");
      PR_fprintf(PR_STDOUT, "            -> WRITE PROTECTED = %s\n",
                 tinfo.flags & CKF_WRITE_PROTECTED ? "TRUE" : "FALSE");
      PR_fprintf(PR_STDOUT, "            -> LOGIN REQUIRED = %s\n",
                 tinfo.flags & CKF_LOGIN_REQUIRED ? "TRUE" : "FALSE");
      PR_fprintf(PR_STDOUT, "            -> USER PIN INITIALIZED = %s\n",
                 tinfo.flags & CKF_USER_PIN_INITIALIZED ? "TRUE" : "FALSE");
      PR_fprintf(PR_STDOUT, "            -> RESTORE KEY NOT NEEDED = %s\n",
                 tinfo.flags & CKF_RESTORE_KEY_NOT_NEEDED ? "TRUE" : "FALSE");
      PR_fprintf(PR_STDOUT, "            -> CLOCK ON TOKEN = %s\n",
                 tinfo.flags & CKF_CLOCK_ON_TOKEN ? "TRUE" : "FALSE");
#ifdef CKF_SUPPORTS_PARALLEL
      PR_fprintf(PR_STDOUT, "            -> SUPPORTS PARALLEL = %s\n",
                 tinfo.flags & CKF_SUPPORTS_PARALLEL ? "TRUE" : "FALSE");
#endif /* CKF_SUPPORTS_PARALLEL */
      PR_fprintf(PR_STDOUT, "            -> PROTECTED AUTHENTICATION PATH = %s\n",
                 tinfo.flags & CKF_PROTECTED_AUTHENTICATION_PATH ? "TRUE" : "FALSE");
      PR_fprintf(PR_STDOUT, "            -> DUAL_CRYPTO_OPERATIONS = %s\n", 
                 tinfo.flags & CKF_DUAL_CRYPTO_OPERATIONS ? "TRUE" : "FALSE");
      PR_fprintf(PR_STDOUT, "        ulMaxSessionCount = %lu\n", tinfo.ulMaxSessionCount);
      PR_fprintf(PR_STDOUT, "        ulSessionCount = %lu\n", tinfo.ulSessionCount);
      PR_fprintf(PR_STDOUT, "        ulMaxRwSessionCount = %lu\n", tinfo.ulMaxRwSessionCount);
      PR_fprintf(PR_STDOUT, "        ulRwSessionCount = %lu\n", tinfo.ulRwSessionCount);
      PR_fprintf(PR_STDOUT, "        ulMaxPinLen = %lu\n", tinfo.ulMaxPinLen);
      PR_fprintf(PR_STDOUT, "        ulMinPinLen = %lu\n", tinfo.ulMinPinLen);
      PR_fprintf(PR_STDOUT, "        ulTotalPublicMemory = %lu\n", tinfo.ulTotalPublicMemory);
      PR_fprintf(PR_STDOUT, "        ulFreePublicMemory = %lu\n", tinfo.ulFreePublicMemory);
      PR_fprintf(PR_STDOUT, "        ulTotalPrivateMemory = %lu\n", tinfo.ulTotalPrivateMemory);
      PR_fprintf(PR_STDOUT, "        ulFreePrivateMemory = %lu\n", tinfo.ulFreePrivateMemory);
      PR_fprintf(PR_STDOUT, "        hardwareVersion = %lu.%02lu\n", 
                 (PRUint32)tinfo.hardwareVersion.major, (PRUint32)tinfo.hardwareVersion.minor);
      PR_fprintf(PR_STDOUT, "        firmwareVersion = %lu.%02lu\n",
                 (PRUint32)tinfo.firmwareVersion.major, (PRUint32)tinfo.firmwareVersion.minor);
      PR_fprintf(PR_STDOUT, "        utcTime = \"%.16s\"\n", tinfo.utcTime);


      ck_rv = epv->C_GetMechanismList(pSlots[i], (CK_MECHANISM_TYPE_PTR)CK_NULL_PTR, &nMechanisms);
      switch( ck_rv ) {
      case CKR_BUFFER_TOO_SMALL:
      case CKR_OK:
        break;
      default:
        PR_fprintf(PR_STDERR, "C_GetMechanismList(%lu, NULL, ) returned 0x%08x\n", pSlots[i], ck_rv);
        return 1;
      }

      PR_fprintf(PR_STDOUT, "    %lu mechanisms:\n", nMechanisms);

      pMechanismList = (CK_MECHANISM_TYPE_PTR)PR_Calloc(nMechanisms, sizeof(CK_MECHANISM_TYPE));
      if( (CK_MECHANISM_TYPE_PTR)NULL == pMechanismList ) {
        PR_fprintf(PR_STDERR, "[memory allocation of %lu bytes failed]\n", 
                   nMechanisms * sizeof(CK_MECHANISM_TYPE));
        return 1;
      }

      ck_rv = epv->C_GetMechanismList(pSlots[i], pMechanismList, &nMechanisms);
      if( CKR_OK != ck_rv ) {
        PR_fprintf(PR_STDERR, "C_GetMechanismList(%lu, , ) returned 0x%08x\n", pSlots[i], ck_rv);
        return 1;
      }

      for( j = 0; j < nMechanisms; j++ ) {
        PR_fprintf(PR_STDOUT, "        {%lu}: CK_MECHANISM_TYPE = %lu\n", (j+1), pMechanismList[j]);
      }

      PR_fprintf(PR_STDOUT, "\n");

      for( j = 0; j < nMechanisms; j++ ) {
        CK_MECHANISM_INFO minfo;

        (void)memset(&minfo, 0, sizeof(CK_MECHANISM_INFO));
        ck_rv = epv->C_GetMechanismInfo(pSlots[i], pMechanismList[j], &minfo);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_GetMechanismInfo(%lu, %lu, ) returned 0x%08x\n", pSlots[i], 
                     pMechanismList[j]);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    [%lu]: CK_MECHANISM_TYPE = %lu\n", (j+1), pMechanismList[j]);
        PR_fprintf(PR_STDOUT, "    ulMinKeySize = %lu\n", minfo.ulMinKeySize);
        PR_fprintf(PR_STDOUT, "    ulMaxKeySize = %lu\n", minfo.ulMaxKeySize);
        PR_fprintf(PR_STDOUT, "    flags = 0x%08x\n", minfo.flags);
        PR_fprintf(PR_STDOUT, "        -> HW = %s\n", minfo.flags & CKF_HW ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> ENCRYPT = %s\n", minfo.flags & CKF_ENCRYPT ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> DECRYPT = %s\n", minfo.flags & CKF_DECRYPT ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> DIGEST = %s\n", minfo.flags & CKF_DIGEST ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> SIGN = %s\n", minfo.flags & CKF_SIGN ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> SIGN_RECOVER = %s\n", minfo.flags & CKF_SIGN_RECOVER ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> VERIFY = %s\n", minfo.flags & CKF_VERIFY ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> VERIFY_RECOVER = %s\n", minfo.flags & CKF_VERIFY_RECOVER ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> GENERATE = %s\n", minfo.flags & CKF_GENERATE ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> GENERATE_KEY_PAIR = %s\n", minfo.flags & CKF_GENERATE_KEY_PAIR ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> WRAP = %s\n", minfo.flags & CKF_WRAP ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> UNWRAP = %s\n", minfo.flags & CKF_UNWRAP ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> DERIVE = %s\n", minfo.flags & CKF_DERIVE ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "        -> EXTENSION = %s\n", minfo.flags & CKF_EXTENSION ? "TRUE" : "FALSE");

        PR_fprintf(PR_STDOUT, "\n");
      }

      if( tinfo.flags & CKF_LOGIN_REQUIRED ) {
        PR_fprintf(PR_STDERR, "*** LOGIN REQUIRED but not yet implemented ***\n");
        /* all the stuff about logging in as SO and setting the user pin if needed, etc. */
        return 2;
      }

      /* session to find objects */
      {
        CK_SESSION_HANDLE h = (CK_SESSION_HANDLE)0;
        CK_SESSION_INFO sinfo;
        CK_ATTRIBUTE_PTR pTemplate;
        CK_ULONG tnObjects = 0;

        ck_rv = epv->C_OpenSession(pSlots[i], CKF_SERIAL_SESSION, (CK_VOID_PTR)CK_NULL_PTR, (CK_NOTIFY)CK_NULL_PTR, &h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_OpenSession(%lu, CKF_SERIAL_SESSION, , ) returned 0x%08x\n", pSlots[i], ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Opened a session: handle = 0x%08x\n", h);

        (void)memset(&sinfo, 0, sizeof(CK_SESSION_INFO));
        ck_rv = epv->C_GetSessionInfo(h, &sinfo);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDOUT, "C_GetSessionInfo(%lu, ) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    SESSION INFO:\n");
        PR_fprintf(PR_STDOUT, "        slotID = %lu\n", sinfo.slotID);
        PR_fprintf(PR_STDOUT, "        state = %lu\n", sinfo.state);
        PR_fprintf(PR_STDOUT, "        flags = 0x%08x\n", sinfo.flags);
#ifdef CKF_EXCLUSIVE_SESSION
        PR_fprintf(PR_STDOUT, "            -> EXCLUSIVE SESSION = %s\n", sinfo.flags & CKF_EXCLUSIVE_SESSION ? "TRUE" : "FALSE");
#endif /* CKF_EXCLUSIVE_SESSION */
        PR_fprintf(PR_STDOUT, "            -> RW SESSION = %s\n", sinfo.flags & CKF_RW_SESSION ? "TRUE" : "FALSE");
        PR_fprintf(PR_STDOUT, "            -> SERIAL SESSION = %s\n", sinfo.flags & CKF_SERIAL_SESSION ? "TRUE" : "FALSE");
#ifdef CKF_INSERTION_CALLBACK
        PR_fprintf(PR_STDOUT, "            -> INSERTION CALLBACK = %s\n", sinfo.flags & CKF_INSERTION_CALLBACK ? "TRUE" : "FALSE");
#endif /* CKF_INSERTION_CALLBACK */
        PR_fprintf(PR_STDOUT, "        ulDeviceError = %lu\n", sinfo.ulDeviceError);
        PR_fprintf(PR_STDOUT, "\n");

        ck_rv = epv->C_FindObjectsInit(h, (CK_ATTRIBUTE_PTR)CK_NULL_PTR, 0);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDOUT, "C_FindObjectsInit(%lu, NULL_PTR, 0) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        pTemplate = (CK_ATTRIBUTE_PTR)PR_Calloc(number_of_all_known_attribute_types, sizeof(CK_ATTRIBUTE));
        if( (CK_ATTRIBUTE_PTR)NULL == pTemplate ) {
          PR_fprintf(PR_STDERR, "[memory allocation of %lu bytes failed]\n", 
                     number_of_all_known_attribute_types * sizeof(CK_ATTRIBUTE));
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    All objects:\n");

        while(1) {
          CK_OBJECT_HANDLE o = (CK_OBJECT_HANDLE)0;
          CK_ULONG nObjects = 0;
          CK_ULONG k;
          CK_ULONG nAttributes = 0;
          CK_ATTRIBUTE_PTR pT2;
          CK_ULONG l;

          ck_rv = epv->C_FindObjects(h, &o, 1, &nObjects);
          if( CKR_OK != ck_rv ) {
            PR_fprintf(PR_STDERR, "C_FindObjects(%lu, , 1, ) returned 0x%08x\n", h, ck_rv);
            return 1;
          }

          if( 0 == nObjects ) {
            PR_fprintf(PR_STDOUT, "\n");
            break;
          }

          tnObjects++;

          PR_fprintf(PR_STDOUT, "        OBJECT HANDLE %lu:\n", o);

          for( k = 0; k < number_of_all_known_attribute_types; k++ ) {
            pTemplate[k].type = all_known_attribute_types[k];
            pTemplate[k].pValue = (CK_VOID_PTR)CK_NULL_PTR;
            pTemplate[k].ulValueLen = 0;
          }

          ck_rv = epv->C_GetAttributeValue(h, o, pTemplate, number_of_all_known_attribute_types);
          switch( ck_rv ) {
          case CKR_OK:
          case CKR_ATTRIBUTE_SENSITIVE:
          case CKR_ATTRIBUTE_TYPE_INVALID:
          case CKR_BUFFER_TOO_SMALL:
            break;
          default:
            PR_fprintf(PR_STDERR, "C_GetAtributeValue(%lu, %lu, {all attribute types}, %lu) returned 0x%08x\n",
                       h, o, number_of_all_known_attribute_types, ck_rv);
            return 1;
          }

          for( k = 0; k < number_of_all_known_attribute_types; k++ ) {
            if( -1 != (CK_LONG)pTemplate[k].ulValueLen ) {
              nAttributes++;
            }
          }

          if( 1 ) {
            PR_fprintf(PR_STDOUT, "            %lu attributes:\n", nAttributes);
            for( k = 0; k < number_of_all_known_attribute_types; k++ ) {
              if( -1 != (CK_LONG)pTemplate[k].ulValueLen ) {
                PR_fprintf(PR_STDOUT, "                0x%08x (len = %lu)\n", pTemplate[k].type, 
                           pTemplate[k].ulValueLen);
              }
            }
            PR_fprintf(PR_STDOUT, "\n");
          }

          pT2 = (CK_ATTRIBUTE_PTR)PR_Calloc(nAttributes, sizeof(CK_ATTRIBUTE));
          if( (CK_ATTRIBUTE_PTR)NULL == pT2 ) {
            PR_fprintf(PR_STDERR, "[memory allocation of %lu bytes failed]\n", 
                       nAttributes * sizeof(CK_ATTRIBUTE));
            return 1;
          }

          for( l = 0, k = 0; k < number_of_all_known_attribute_types; k++ ) {
            if( -1 != (CK_LONG)pTemplate[k].ulValueLen ) {
              pT2[l].type = pTemplate[k].type;
              pT2[l].ulValueLen = pTemplate[k].ulValueLen;
              pT2[l].pValue = (CK_VOID_PTR)PR_Malloc(pT2[l].ulValueLen);
              if( (CK_VOID_PTR)NULL == pT2[l].pValue ) {
                PR_fprintf(PR_STDERR, "[memory allocation of %lu bytes failed]\n", pT2[l].ulValueLen);
                return 1;
              }
              l++;
            }
          }

          PR_ASSERT( l == nAttributes );

          ck_rv = epv->C_GetAttributeValue(h, o, pT2, nAttributes);
          switch( ck_rv ) {
          case CKR_OK:
          case CKR_ATTRIBUTE_SENSITIVE:
          case CKR_ATTRIBUTE_TYPE_INVALID:
          case CKR_BUFFER_TOO_SMALL:
            break;
          default:
            PR_fprintf(PR_STDERR, "C_GetAtributeValue(%lu, %lu, {existent attribute types}, %lu) returned 0x%08x\n",
                       h, o, nAttributes, ck_rv);
            return 1;
          }

          for( l = 0; l < nAttributes; l++ ) {
            PR_fprintf(PR_STDOUT, "            type = 0x%08x, len = %ld", pT2[l].type, (CK_LONG)pT2[l].ulValueLen);
            if( -1 == (CK_LONG)pT2[l].ulValueLen ) {
              ;
            } else {
              CK_ULONG m;

              if( pT2[l].ulValueLen <= 8 ) {
                PR_fprintf(PR_STDOUT, ", value = ");
              } else {
                PR_fprintf(PR_STDOUT, ", value = \n                ");
              }

              for( m = 0; (m < pT2[l].ulValueLen) && (m < 20); m++ ) {
                PR_fprintf(PR_STDOUT, "%02x", (CK_ULONG)(0xff & ((CK_CHAR_PTR)pT2[l].pValue)[m]));
              }

              PR_fprintf(PR_STDOUT, " ");

              for( m = 0; (m < pT2[l].ulValueLen) && (m < 20); m++ ) {
                CK_CHAR c = ((CK_CHAR_PTR)pT2[l].pValue)[m];
                if( (c < 0x20) || (c >= 0x7f) ) {
                  c = '.';
                }
                PR_fprintf(PR_STDOUT, "%c", c);
              }
            }

            PR_fprintf(PR_STDOUT, "\n");
          }
          
          PR_fprintf(PR_STDOUT, "\n");

          for( l = 0; l < nAttributes; l++ ) {
            PR_Free(pT2[l].pValue);
          }
          PR_Free(pT2);
        } /* while(1) */

        ck_rv = epv->C_FindObjectsFinal(h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsFinal(%lu) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    (%lu objects total)\n", tnObjects);

        ck_rv = epv->C_CloseSession(h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CloseSession(%lu) returned 0x%08x\n", h, ck_rv);
          return 1;
        }
      } /* session to find objects */

      /* session to create, find, and delete a couple session objects */
      {
        CK_SESSION_HANDLE h = (CK_SESSION_HANDLE)0;
        CK_ATTRIBUTE one[7], two[7], three[7], delta[1], mask[1];
        CK_OBJECT_CLASS cko_data = CKO_DATA;
        CK_BBOOL false = CK_FALSE, true = CK_TRUE;
        char *key = "TEST PROGRAM";
        CK_ULONG key_len = strlen(key);
        CK_OBJECT_HANDLE hOneIn = (CK_OBJECT_HANDLE)0, hTwoIn = (CK_OBJECT_HANDLE)0, 
          hThreeIn = (CK_OBJECT_HANDLE)0, hDeltaIn = (CK_OBJECT_HANDLE)0;
        CK_OBJECT_HANDLE found[10];
        CK_ULONG nFound;

        ck_rv = epv->C_OpenSession(pSlots[i], CKF_SERIAL_SESSION, (CK_VOID_PTR)CK_NULL_PTR, (CK_NOTIFY)CK_NULL_PTR, &h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_OpenSession(%lu, CKF_SERIAL_SESSION, , ) returned 0x%08x\n", pSlots[i], ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Opened a session: handle = 0x%08x\n", h);

        one[0].type = CKA_CLASS;
        one[0].pValue = &cko_data;
        one[0].ulValueLen = sizeof(CK_OBJECT_CLASS);
        one[1].type = CKA_TOKEN;
        one[1].pValue = &false;
        one[1].ulValueLen = sizeof(CK_BBOOL);
        one[2].type = CKA_PRIVATE;
        one[2].pValue = &false;
        one[2].ulValueLen = sizeof(CK_BBOOL);
        one[3].type = CKA_MODIFIABLE;
        one[3].pValue = &true;
        one[3].ulValueLen = sizeof(CK_BBOOL);
        one[4].type = CKA_LABEL;
        one[4].pValue = "Test data object one";
        one[4].ulValueLen = strlen(one[4].pValue);
        one[5].type = CKA_APPLICATION;
        one[5].pValue = key;
        one[5].ulValueLen = key_len;
        one[6].type = CKA_VALUE;
        one[6].pValue = "Object one";
        one[6].ulValueLen = strlen(one[6].pValue);

        two[0].type = CKA_CLASS;
        two[0].pValue = &cko_data;
        two[0].ulValueLen = sizeof(CK_OBJECT_CLASS);
        two[1].type = CKA_TOKEN;
        two[1].pValue = &false;
        two[1].ulValueLen = sizeof(CK_BBOOL);
        two[2].type = CKA_PRIVATE;
        two[2].pValue = &false;
        two[2].ulValueLen = sizeof(CK_BBOOL);
        two[3].type = CKA_MODIFIABLE;
        two[3].pValue = &true;
        two[3].ulValueLen = sizeof(CK_BBOOL);
        two[4].type = CKA_LABEL;
        two[4].pValue = "Test data object two";
        two[4].ulValueLen = strlen(two[4].pValue);
        two[5].type = CKA_APPLICATION;
        two[5].pValue = key;
        two[5].ulValueLen = key_len;
        two[6].type = CKA_VALUE;
        two[6].pValue = "Object two";
        two[6].ulValueLen = strlen(two[6].pValue);

        three[0].type = CKA_CLASS;
        three[0].pValue = &cko_data;
        three[0].ulValueLen = sizeof(CK_OBJECT_CLASS);
        three[1].type = CKA_TOKEN;
        three[1].pValue = &false;
        three[1].ulValueLen = sizeof(CK_BBOOL);
        three[2].type = CKA_PRIVATE;
        three[2].pValue = &false;
        three[2].ulValueLen = sizeof(CK_BBOOL);
        three[3].type = CKA_MODIFIABLE;
        three[3].pValue = &true;
        three[3].ulValueLen = sizeof(CK_BBOOL);
        three[4].type = CKA_LABEL;
        three[4].pValue = "Test data object three";
        three[4].ulValueLen = strlen(three[4].pValue);
        three[5].type = CKA_APPLICATION;
        three[5].pValue = key;
        three[5].ulValueLen = key_len;
        three[6].type = CKA_VALUE;
        three[6].pValue = "Object three";
        three[6].ulValueLen = strlen(three[6].pValue);

        ck_rv = epv->C_CreateObject(h, one, 7, &hOneIn);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CreateObject(%lu, one, 7, ) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Created object one: handle = %lu\n", hOneIn);

        ck_rv = epv->C_CreateObject(h, two, 7, &hTwoIn);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CreateObject(%lu, two, 7, ) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Created object two: handle = %lu\n", hTwoIn);

        ck_rv = epv->C_CreateObject(h, three, 7, &hThreeIn);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CreateObject(%lu, three, 7, ) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Created object three: handle = %lu\n", hThreeIn);

        delta[0].type = CKA_VALUE;
        delta[0].pValue = "Copied object";
        delta[0].ulValueLen = strlen(delta[0].pValue);

        ck_rv = epv->C_CopyObject(h, hThreeIn, delta, 1, &hDeltaIn);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CopyObject(%lu, %lu, delta, 1, ) returned 0x%08x\n", 
                     h, hThreeIn, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Copied object three: new handle = %lu\n", hDeltaIn);

        mask[0].type = CKA_APPLICATION;
        mask[0].pValue = key;
        mask[0].ulValueLen = key_len;

        ck_rv = epv->C_FindObjectsInit(h, mask, 1);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsInit(%lu, mask, 1) returned 0x%08x\n", 
                     h, ck_rv);
          return 1;
        }

        (void)memset(&found, 0, sizeof(found));
        nFound = 0;
        ck_rv = epv->C_FindObjects(h, found, 10, &nFound);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjects(%lu,, 10, ) returned 0x%08x\n", 
                     h, ck_rv);
          return 1;
        }

        if( 4 != nFound ) {
          PR_fprintf(PR_STDERR, "Found %lu objects, not 4.\n", nFound);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Found 4 objects: %lu, %lu, %lu, %lu\n", 
                   found[0], found[1], found[2], found[3]);

        ck_rv = epv->C_FindObjectsFinal(h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsFinal(%lu) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        ck_rv = epv->C_DestroyObject(h, hThreeIn);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_DestroyObject(%lu, %lu) returned 0x%08x\n", h, hThreeIn, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Destroyed object three (handle = %lu)\n", hThreeIn);

        delta[0].type = CKA_APPLICATION;
        delta[0].pValue = "Changed application";
        delta[0].ulValueLen = strlen(delta[0].pValue);

        ck_rv = epv->C_SetAttributeValue(h, hTwoIn, delta, 1);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_SetAttributeValue(%lu, %lu, delta, 1) returned 0x%08x\n", 
                     h, hTwoIn, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Changed object two (handle = %lu).\n", hTwoIn);

        /* Can another session find these session objects? */
        {
          CK_SESSION_HANDLE h2 = (CK_SESSION_HANDLE)0;

          ck_rv = epv->C_OpenSession(pSlots[i], CKF_SERIAL_SESSION, (CK_VOID_PTR)CK_NULL_PTR, (CK_NOTIFY)CK_NULL_PTR, &h2);
          if( CKR_OK != ck_rv ) {
            PR_fprintf(PR_STDERR, "C_OpenSession(%lu, CKF_SERIAL_SESSION, , ) returned 0x%08x\n", pSlots[i], ck_rv);
            return 1;
          }

          PR_fprintf(PR_STDOUT, "    Opened a second session: handle = 0x%08x\n", h2);

          /* mask is still the same */

          ck_rv = epv->C_FindObjectsInit(h2, mask, 1);
          if( CKR_OK != ck_rv ) {
            PR_fprintf(PR_STDERR, "C_FindObjectsInit(%lu, mask, 1) returned 0x%08x\n", 
                       h2, ck_rv);
            return 1;
          }

          (void)memset(&found, 0, sizeof(found));
          nFound = 0;
          ck_rv = epv->C_FindObjects(h2, found, 10, &nFound);
          if( CKR_OK != ck_rv ) {
            PR_fprintf(PR_STDERR, "C_FindObjects(%lu,, 10, ) returned 0x%08x\n", 
                       h2, ck_rv);
            return 1;
          }

          if( 2 != nFound ) {
            PR_fprintf(PR_STDERR, "Found %lu objects, not 2.\n", nFound);
            return 1;
          }

          PR_fprintf(PR_STDOUT, "    Found 2 objects: %lu, %lu\n", 
                     found[0], found[1]);

          ck_rv = epv->C_FindObjectsFinal(h2);
          if( CKR_OK != ck_rv ) {
            PR_fprintf(PR_STDERR, "C_FindObjectsFinal(%lu) returned 0x%08x\n", h2, ck_rv);
            return 1;
          }

          /* Leave the session hanging open, we'll CloseAllSessions later */
        } /* Can another session find these session objects? */

        ck_rv = epv->C_CloseAllSessions(pSlots[i]);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CloseAllSessions(%lu) returned 0x%08x\n", pSlots[i], ck_rv);
          return 1;
        }
      } /* session to create, find, and delete a couple session objects */

      /* Might be interesting to do a find here to verify that all session objects are gone. */

      if( tinfo.flags & CKF_WRITE_PROTECTED ) {
        PR_fprintf(PR_STDOUT, "Token is write protected, skipping token-object tests.\n");
      } else {
        CK_SESSION_HANDLE h = (CK_SESSION_HANDLE)0;
        CK_ATTRIBUTE tobj[7], tsobj[7], stobj[7], delta[1], mask[2];
        CK_OBJECT_CLASS cko_data = CKO_DATA;
        CK_BBOOL false = CK_FALSE, true = CK_TRUE;
        char *key = "TEST PROGRAM";
        CK_ULONG key_len = strlen(key);
        CK_OBJECT_HANDLE hTIn = (CK_OBJECT_HANDLE)0, hTSIn = (CK_OBJECT_HANDLE)0, 
          hSTIn = (CK_OBJECT_HANDLE)0, hDeltaIn = (CK_OBJECT_HANDLE)0;
        CK_OBJECT_HANDLE found[10];
        CK_ULONG nFound;

        ck_rv = epv->C_OpenSession(pSlots[i], CKF_SERIAL_SESSION, (CK_VOID_PTR)CK_NULL_PTR, (CK_NOTIFY)CK_NULL_PTR, &h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_OpenSession(%lu, CKF_SERIAL_SESSION, , ) returned 0x%08x\n", pSlots[i], ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Opened a session: handle = 0x%08x\n", h);

        tobj[0].type = CKA_CLASS;
        tobj[0].pValue = &cko_data;
        tobj[0].ulValueLen = sizeof(CK_OBJECT_CLASS);
        tobj[1].type = CKA_TOKEN;
        tobj[1].pValue = &true;
        tobj[1].ulValueLen = sizeof(CK_BBOOL);
        tobj[2].type = CKA_PRIVATE;
        tobj[2].pValue = &false;
        tobj[2].ulValueLen = sizeof(CK_BBOOL);
        tobj[3].type = CKA_MODIFIABLE;
        tobj[3].pValue = &true;
        tobj[3].ulValueLen = sizeof(CK_BBOOL);
        tobj[4].type = CKA_LABEL;
        tobj[4].pValue = "Test data object token";
        tobj[4].ulValueLen = strlen(tobj[4].pValue);
        tobj[5].type = CKA_APPLICATION;
        tobj[5].pValue = key;
        tobj[5].ulValueLen = key_len;
        tobj[6].type = CKA_VALUE;
        tobj[6].pValue = "Object token";
        tobj[6].ulValueLen = strlen(tobj[6].pValue);

        tsobj[0].type = CKA_CLASS;
        tsobj[0].pValue = &cko_data;
        tsobj[0].ulValueLen = sizeof(CK_OBJECT_CLASS);
        tsobj[1].type = CKA_TOKEN;
        tsobj[1].pValue = &true;
        tsobj[1].ulValueLen = sizeof(CK_BBOOL);
        tsobj[2].type = CKA_PRIVATE;
        tsobj[2].pValue = &false;
        tsobj[2].ulValueLen = sizeof(CK_BBOOL);
        tsobj[3].type = CKA_MODIFIABLE;
        tsobj[3].pValue = &true;
        tsobj[3].ulValueLen = sizeof(CK_BBOOL);
        tsobj[4].type = CKA_LABEL;
        tsobj[4].pValue = "Test data object token->session";
        tsobj[4].ulValueLen = strlen(tsobj[4].pValue);
        tsobj[5].type = CKA_APPLICATION;
        tsobj[5].pValue = key;
        tsobj[5].ulValueLen = key_len;
        tsobj[6].type = CKA_VALUE;
        tsobj[6].pValue = "Object token->session";
        tsobj[6].ulValueLen = strlen(tsobj[6].pValue);

        stobj[0].type = CKA_CLASS;
        stobj[0].pValue = &cko_data;
        stobj[0].ulValueLen = sizeof(CK_OBJECT_CLASS);
        stobj[1].type = CKA_TOKEN;
        stobj[1].pValue = &false;
        stobj[1].ulValueLen = sizeof(CK_BBOOL);
        stobj[2].type = CKA_PRIVATE;
        stobj[2].pValue = &false;
        stobj[2].ulValueLen = sizeof(CK_BBOOL);
        stobj[3].type = CKA_MODIFIABLE;
        stobj[3].pValue = &true;
        stobj[3].ulValueLen = sizeof(CK_BBOOL);
        stobj[4].type = CKA_LABEL;
        stobj[4].pValue = "Test data object session->token";
        stobj[4].ulValueLen = strlen(stobj[4].pValue);
        stobj[5].type = CKA_APPLICATION;
        stobj[5].pValue = key;
        stobj[5].ulValueLen = key_len;
        stobj[6].type = CKA_VALUE;
        stobj[6].pValue = "Object session->token";
        stobj[6].ulValueLen = strlen(stobj[6].pValue);

        ck_rv = epv->C_CreateObject(h, tobj, 7, &hTIn);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CreateObject(%lu, tobj, 7, ) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Created object token: handle = %lu\n", hTIn);

        ck_rv = epv->C_CreateObject(h, tsobj, 7, &hTSIn);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CreateObject(%lu, tobj, 7, ) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Created object token->session: handle = %lu\n", hTSIn);
        ck_rv = epv->C_CreateObject(h, stobj, 7, &hSTIn);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CreateObject(%lu, tobj, 7, ) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Created object session->token: handle = %lu\n", hSTIn);

        /* I've created two token objects and one session object; find the two */

        mask[0].type = CKA_APPLICATION;
        mask[0].pValue = key;
        mask[0].ulValueLen = key_len;
        mask[1].type = CKA_TOKEN;
        mask[1].pValue = &true;
        mask[1].ulValueLen = sizeof(CK_BBOOL);

        ck_rv = epv->C_FindObjectsInit(h, mask, 2);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsInit(%lu, mask, 2) returned 0x%08x\n", 
                     h, ck_rv);
          return 1;
        }

        (void)memset(&found, 0, sizeof(found));
        nFound = 0;
        ck_rv = epv->C_FindObjects(h, found, 10, &nFound);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjects(%lu,, 10, ) returned 0x%08x\n", 
                     h, ck_rv);
          return 1;
        }

        if( 2 != nFound ) {
          PR_fprintf(PR_STDERR, "Found %lu objects, not 2.\n", nFound);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Found 2 objects: %lu, %lu\n", 
                   found[0], found[1]);

        ck_rv = epv->C_FindObjectsFinal(h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsFinal(%lu) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        /* Convert a token to session object */

        delta[0].type = CKA_TOKEN;
        delta[0].pValue = &false;
        delta[0].ulValueLen = sizeof(CK_BBOOL);

        ck_rv = epv->C_SetAttributeValue(h, hTSIn, delta, 1);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_SetAttributeValue(%lu, %lu, delta, 1) returned 0x%08x\n", 
                     h, hTSIn, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Changed object from token to session (handle = %lu).\n", hTSIn);

        /* Now find again; there should be one */

        mask[0].type = CKA_APPLICATION;
        mask[0].pValue = key;
        mask[0].ulValueLen = key_len;
        mask[1].type = CKA_TOKEN;
        mask[1].pValue = &true;
        mask[1].ulValueLen = sizeof(CK_BBOOL);

        ck_rv = epv->C_FindObjectsInit(h, mask, 2);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsInit(%lu, mask, 2) returned 0x%08x\n", 
                     h, ck_rv);
          return 1;
        }

        (void)memset(&found, 0, sizeof(found));
        nFound = 0;
        ck_rv = epv->C_FindObjects(h, found, 10, &nFound);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjects(%lu,, 10, ) returned 0x%08x\n", 
                     h, ck_rv);
          return 1;
        }

        if( 1 != nFound ) {
          PR_fprintf(PR_STDERR, "Found %lu objects, not 1.\n", nFound);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Found 1 objects: %lu\n", 
                   found[0]);

        ck_rv = epv->C_FindObjectsFinal(h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsFinal(%lu) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        /* Convert a session to a token object */

        delta[0].type = CKA_TOKEN;
        delta[0].pValue = &true;
        delta[0].ulValueLen = sizeof(CK_BBOOL);

        ck_rv = epv->C_SetAttributeValue(h, hSTIn, delta, 1);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_SetAttributeValue(%lu, %lu, delta, 1) returned 0x%08x\n", 
                     h, hSTIn, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Changed object from session to token (handle = %lu).\n", hSTIn);

        /* Now find again; there should be two again */

        mask[0].type = CKA_APPLICATION;
        mask[0].pValue = key;
        mask[0].ulValueLen = key_len;
        mask[1].type = CKA_TOKEN;
        mask[1].pValue = &true;
        mask[1].ulValueLen = sizeof(CK_BBOOL);

        ck_rv = epv->C_FindObjectsInit(h, mask, 2);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsInit(%lu, mask, 2) returned 0x%08x\n", 
                     h, ck_rv);
          return 1;
        }

        (void)memset(&found, 0, sizeof(found));
        nFound = 0;
        ck_rv = epv->C_FindObjects(h, found, 10, &nFound);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjects(%lu,, 10, ) returned 0x%08x\n", 
                     h, ck_rv);
          return 1;
        }

        if( 2 != nFound ) {
          PR_fprintf(PR_STDERR, "Found %lu objects, not 2.\n", nFound);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Found 2 objects: %lu, %lu\n", 
                   found[0], found[1]);

        ck_rv = epv->C_FindObjectsFinal(h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsFinal(%lu) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        /* Delete the two (found) token objects to clean up */

        ck_rv = epv->C_DestroyObject(h, found[0]);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_DestroyObject(%lu, %lu) returned 0x%08x\n", h, found[0], ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Destroyed token object (handle = %lu)\n", found[0]);

        ck_rv = epv->C_DestroyObject(h, found[1]);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_DestroyObject(%lu, %lu) returned 0x%08x\n", h, found[1], ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Destroyed token object (handle = %lu)\n", found[1]);
        
        /* Close the session and all objects should be gone */

        ck_rv = epv->C_CloseSession(h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CloseSession(%lu) returned 0x%08x\n", h, ck_rv);
          return 1;
        }
      } /* if( tinfo.flags & CKF_WRITE_PROTECTED ) */

      if( tinfo.flags & CKF_WRITE_PROTECTED ) {
        PR_fprintf(PR_STDOUT, "Token is write protected, skipping leaving a record.\n");
      } else {
        CK_SESSION_HANDLE h = (CK_SESSION_HANDLE)0;
        CK_ATTRIBUTE record[7], mask[2];
        CK_OBJECT_CLASS cko_data = CKO_DATA;
        CK_BBOOL false = CK_FALSE, true = CK_TRUE;
        char *key = "TEST RECORD";
        CK_ULONG key_len = strlen(key);
        CK_OBJECT_HANDLE hin = (CK_OBJECT_HANDLE)0;
        char timebuffer[256];

        ck_rv = epv->C_OpenSession(pSlots[i], CKF_SERIAL_SESSION, (CK_VOID_PTR)CK_NULL_PTR, (CK_NOTIFY)CK_NULL_PTR, &h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_OpenSession(%lu, CKF_SERIAL_SESSION, , ) returned 0x%08x\n", pSlots[i], ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Opened a session: handle = 0x%08x\n", h);

        /* I can't believe how hard NSPR makes this operation */
        {
          time_t now = 0;
          struct tm *tm;
          time(&now);
          tm = localtime(&now);
          strftime(timebuffer, sizeof(timebuffer), "%Y-%m-%d %T %Z", tm);
        }

        record[0].type = CKA_CLASS;
        record[0].pValue = &cko_data;
        record[0].ulValueLen = sizeof(CK_OBJECT_CLASS);
        record[1].type = CKA_TOKEN;
        record[1].pValue = &true;
        record[1].ulValueLen = sizeof(CK_BBOOL);
        record[2].type = CKA_PRIVATE;
        record[2].pValue = &false;
        record[2].ulValueLen = sizeof(CK_BBOOL);
        record[3].type = CKA_MODIFIABLE;
        record[3].pValue = &true;
        record[3].ulValueLen = sizeof(CK_BBOOL);
        record[4].type = CKA_LABEL;
        record[4].pValue = "Test record";
        record[4].ulValueLen = strlen(record[4].pValue);
        record[5].type = CKA_APPLICATION;
        record[5].pValue = key;
        record[5].ulValueLen = key_len;
        record[6].type = CKA_VALUE;
        record[6].pValue = timebuffer;
        record[6].ulValueLen = strlen(timebuffer)+1;

        PR_fprintf(PR_STDOUT, "    Timestamping with \"%s\"\n", timebuffer);

        ck_rv = epv->C_CreateObject(h, record, 7, &hin);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_CreateObject(%lu, tobj, 7, ) returned 0x%08x\n", h, ck_rv);
          return 1;
        }

        PR_fprintf(PR_STDOUT, "    Created record object: handle = %lu\n", hin);
        
        PR_fprintf(PR_STDOUT, "   == All test timestamps ==\n");

        mask[0].type = CKA_CLASS;
        mask[0].pValue = &cko_data;
        mask[0].ulValueLen = sizeof(CK_OBJECT_CLASS);
        mask[1].type = CKA_APPLICATION;
        mask[1].pValue = key;
        mask[1].ulValueLen = key_len;

        ck_rv = epv->C_FindObjectsInit(h, mask, 2);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsInit(%lu, mask, 1) returned 0x%08x\n", 
                     h, ck_rv);
          return 1;
        }

        while( 1 ) {
          CK_OBJECT_HANDLE o = (CK_OBJECT_HANDLE)0;
          CK_ULONG nObjects = 0;
          CK_ATTRIBUTE value[1];
          char buffer[1024];

          ck_rv = epv->C_FindObjects(h, &o, 1, &nObjects);
          if( CKR_OK != ck_rv ) {
            PR_fprintf(PR_STDERR, "C_FindObjects(%lu, , 1, ) returned 0x%08x\n", h, ck_rv);
            return 1;
          }

          if( 0 == nObjects ) {
            PR_fprintf(PR_STDOUT, "\n");
            break;
          }

          value[0].type = CKA_VALUE;
          value[0].pValue = buffer;
          value[0].ulValueLen = sizeof(buffer);

          ck_rv = epv->C_GetAttributeValue(h, o, value, 1);
          switch( ck_rv ) {
          case CKR_OK:
            PR_fprintf(PR_STDOUT, "    %s\n", value[0].pValue);
            break;
          case CKR_ATTRIBUTE_SENSITIVE:
            PR_fprintf(PR_STDOUT, "    [Sensitive???]\n");
            break;
          case CKR_ATTRIBUTE_TYPE_INVALID:
            PR_fprintf(PR_STDOUT, "    [Invalid attribute???]\n");
            break;
          case CKR_BUFFER_TOO_SMALL:
            PR_fprintf(PR_STDOUT, "    (result > 1k (%lu))\n", value[0].ulValueLen);
            break;
          default:
            PR_fprintf(PR_STDERR, "C_GetAtributeValue(%lu, %lu, CKA_VALUE, 1) returned 0x%08x\n",
                       h, o);
            return 1;
          }
        } /* while */

        ck_rv = epv->C_FindObjectsFinal(h);
        if( CKR_OK != ck_rv ) {
          PR_fprintf(PR_STDERR, "C_FindObjectsFinal(%lu) returned 0x%08x\n", h, ck_rv);
          return 1;
        }
      } /* "leaving a record" else clause */

    }

    PR_fprintf(PR_STDOUT, "\n");
  }

  return 0;
}
