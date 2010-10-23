/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Dolske <dolske@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const EXPORTED_SYMBOLS = ["WeaveCrypto"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/ctypes.jsm");

function WeaveCrypto() {
    this.init();
}

WeaveCrypto.prototype = {
    QueryInterface: XPCOMUtils.generateQI([Ci.IWeaveCrypto]),

    prefBranch : null,
    debug      : true,  // services.sync.log.cryptoDebug
    nss        : null,
    nss_t      : null,

    observer : {
        _self : null,

        QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver,
                                                Ci.nsISupportsWeakReference]),

        observe : function (subject, topic, data) {
            let self = this._self;
            self.log("Observed " + topic + " topic.");
            if (topic == "nsPref:changed") {
                self.debug = self.prefBranch.getBoolPref("cryptoDebug");
            }
        }
    },

    // This is its own method so that it can be overridden.
    // (Components.Exception isn't thread-safe for instance)
    makeException : function makeException(message, result) {
        return Components.Exception(message, result);
    },

    init : function() {
        try {
            // Preferences. Add observer so we get notified of changes.
            this.prefBranch = Services.prefs.getBranch("services.sync.log.");
            this.prefBranch.QueryInterface(Ci.nsIPrefBranch2);
            this.prefBranch.addObserver("cryptoDebug", this.observer, false);
            this.observer._self = this;
            this.debug = this.prefBranch.getBoolPref("cryptoDebug");

            this.initNSS();
        } catch (e) {
            this.log("init failed: " + e);
            throw e;
        }
    },

    log : function (message) {
        if (!this.debug)
            return;
        dump("WeaveCrypto: " + message + "\n");
        Services.console.logStringMessage("WeaveCrypto: " + message);
    },

    initNSS : function() {
        // We use NSS for the crypto ops, which needs to be initialized before
        // use. By convention, PSM is required to be the module that
        // initializes NSS. So, make sure PSM is initialized in order to
        // implicitly initialize NSS.
        Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

        // Open the NSS library.
        let nssfile = Services.dirsvc.get("GreD", Ci.nsILocalFile);
        let os = Services.appinfo.OS;
        switch (os) {
          case "WINNT":
          case "WINMO":
          case "WINCE":
            nssfile.append("nss3.dll");
            break;
          case "Darwin":
            nssfile.append("libnss3.dylib");
            break;
          case "Linux":
          case "SunOS":
          case "WebOS": // Palm Pre
            nssfile.append("libnss3.so");
            break;
          case "Android":
            // Android uses a $GREDIR/lib/ subdir.
            nssfile.append("lib");
            nssfile.append("libnss3.so");
            break;
          default:
            throw this.makeException("unsupported platform: " + os, Cr.NS_ERROR_UNEXPECTED);
        }
        this.log("Using NSS library " + nssfile.path);

        // XXX really want to be able to pass specific dlopen flags here.
        let nsslib = ctypes.open(nssfile.path);

        this.log("Initializing NSS types and function declarations...");

        this.nss = {};
        this.nss_t = {};

        // nsprpub/pr/include/prtypes.h#435
        // typedef PRIntn PRBool; --> int
        this.nss_t.PRBool = ctypes.int;
        // security/nss/lib/util/seccomon.h#91
        // typedef enum
        this.nss_t.SECStatus = ctypes.int;
        // security/nss/lib/softoken/secmodt.h#59
        // typedef struct PK11SlotInfoStr PK11SlotInfo; (defined in secmodti.h) 
        this.nss_t.PK11SlotInfo = ctypes.void_t;
        // security/nss/lib/util/pkcs11t.h
        this.nss_t.CK_MECHANISM_TYPE = ctypes.unsigned_long;
        this.nss_t.CK_ATTRIBUTE_TYPE = ctypes.unsigned_long;
        this.nss_t.CK_KEY_TYPE       = ctypes.unsigned_long;
        this.nss_t.CK_OBJECT_HANDLE  = ctypes.unsigned_long;
        // security/nss/lib/softoken/secmodt.h#359
        // typedef enum PK11Origin
        this.nss_t.PK11Origin = ctypes.int;
        // PK11Origin enum values...
        this.nss.PK11_OriginUnwrap = 4;
        // security/nss/lib/softoken/secmodt.h#61
        // typedef struct PK11SymKeyStr PK11SymKey; (defined in secmodti.h)
        this.nss_t.PK11SymKey = ctypes.void_t;
        // security/nss/lib/util/secoidt.h#454
        // typedef enum
        this.nss_t.SECOidTag = ctypes.int;
        // security/nss/lib/util/seccomon.h#64
        // typedef enum
        this.nss_t.SECItemType = ctypes.int;
        // SECItemType enum values...
        this.nss.SIBUFFER = 0;
        // security/nss/lib/softoken/secmodt.h#62 (defined in secmodti.h)
        // typedef struct PK11ContextStr PK11Context;
        this.nss_t.PK11Context = ctypes.void_t;
        // Needed for SECKEYPrivateKey struct def'n, but I don't think we need to actually access it.
        this.nss_t.PLArenaPool = ctypes.void_t;
        // security/nss/lib/cryptohi/keythi.h#45
        // typedef enum
        this.nss_t.KeyType = ctypes.int;
        // security/nss/lib/softoken/secmodt.h#201
        // typedef PRUint32 PK11AttrFlags;
        this.nss_t.PK11AttrFlags = ctypes.unsigned_int;
        // security/nss/lib/util/secoidt.h#454
        // typedef enum
        this.nss_t.SECOidTag = ctypes.int;
        // security/nss/lib/util/seccomon.h#83
        // typedef struct SECItemStr SECItem; --> SECItemStr defined right below it
        this.nss_t.SECItem = ctypes.StructType(
            "SECItem", [{ type: this.nss_t.SECItemType },
                        { data: ctypes.unsigned_char.ptr },
                        { len : ctypes.int }]);
        // security/nss/lib/softoken/secmodt.h#65
        // typedef struct PK11RSAGenParamsStr --> def'n on line 139
        this.nss_t.PK11RSAGenParams = ctypes.StructType(
            "PK11RSAGenParams", [{ keySizeInBits: ctypes.int },
                                 { pe : ctypes.unsigned_long }]);
        // security/nss/lib/cryptohi/keythi.h#233
        // typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey; --> def'n right above it
        this.nss_t.SECKEYPrivateKey = ctypes.StructType(
            "SECKEYPrivateKey", [{ arena:        this.nss_t.PLArenaPool.ptr  },
                                 { keyType:      this.nss_t.KeyType          },
                                 { pkcs11Slot:   this.nss_t.PK11SlotInfo.ptr },
                                 { pkcs11ID:     this.nss_t.CK_OBJECT_HANDLE },
                                 { pkcs11IsTemp: this.nss_t.PRBool           },
                                 { wincx:        ctypes.voidptr_t            },
                                 { staticflags:  ctypes.unsigned_int         }]);
        // security/nss/lib/cryptohi/keythi.h#78
        // typedef struct SECKEYRSAPublicKeyStr --> def'n right above it
        this.nss_t.SECKEYRSAPublicKey = ctypes.StructType(
            "SECKEYRSAPublicKey", [{ arena:          this.nss_t.PLArenaPool.ptr },
                                   { modulus:        this.nss_t.SECItem         },
                                   { publicExponent: this.nss_t.SECItem         }]);
        // security/nss/lib/cryptohi/keythi.h#189
        // typedef struct SECKEYPublicKeyStr SECKEYPublicKey; --> def'n right above it
        this.nss_t.SECKEYPublicKey = ctypes.StructType(
            "SECKEYPublicKey", [{ arena:      this.nss_t.PLArenaPool.ptr    },
                                { keyType:    this.nss_t.KeyType            },
                                { pkcs11Slot: this.nss_t.PK11SlotInfo.ptr   },
                                { pkcs11ID:   this.nss_t.CK_OBJECT_HANDLE   },
                                { rsa:        this.nss_t.SECKEYRSAPublicKey } ]);
                                // XXX: "rsa" et al into a union here!
                                // { dsa: SECKEYDSAPublicKey },
                                // { dh:  SECKEYDHPublicKey },
                                // { kea: SECKEYKEAPublicKey },
                                // { fortezza: SECKEYFortezzaPublicKey },
                                // { ec:  SECKEYECPublicKey } ]);
        // security/nss/lib/util/secoidt.h#52
        // typedef struct SECAlgorithmIDStr --> def'n right below it
        this.nss_t.SECAlgorithmID = ctypes.StructType(
            "SECAlgorithmID", [{ algorithm:  this.nss_t.SECItem },
                               { parameters: this.nss_t.SECItem }]);
        // security/nss/lib/certdb/certt.h#98
        // typedef struct CERTSubjectPublicKeyInfoStrA --> def'n on line 160
        this.nss_t.CERTSubjectPublicKeyInfo = ctypes.StructType(
            "CERTSubjectPublicKeyInfo", [{ arena:            this.nss_t.PLArenaPool.ptr },
                                         { algorithm:        this.nss_t.SECAlgorithmID  },
                                         { subjectPublicKey: this.nss_t.SECItem         }]);


        // security/nss/lib/util/pkcs11t.h
        this.nss.CKK_RSA = 0x0;
        this.nss.CKM_RSA_PKCS_KEY_PAIR_GEN = 0x0000;
        this.nss.CKM_AES_KEY_GEN           = 0x1080; 
        this.nss.CKA_ENCRYPT = 0x104;
        this.nss.CKA_DECRYPT = 0x105;
        this.nss.CKA_UNWRAP  = 0x107;

        // security/nss/lib/softoken/secmodt.h
        this.nss.PK11_ATTR_SESSION   = 0x02;
        this.nss.PK11_ATTR_PUBLIC    = 0x08;
        this.nss.PK11_ATTR_SENSITIVE = 0x40;

        // security/nss/lib/util/secoidt.h
        this.nss.SEC_OID_HMAC_SHA1            = 294;
        this.nss.SEC_OID_PKCS1_RSA_ENCRYPTION = 16;


        // security/nss/lib/pk11wrap/pk11pub.h#286
        // SECStatus PK11_GenerateRandom(unsigned char *data,int len);
        this.nss.PK11_GenerateRandom = nsslib.declare("PK11_GenerateRandom",
                                                      ctypes.default_abi, this.nss_t.SECStatus,
                                                      ctypes.unsigned_char.ptr, ctypes.int);
        // security/nss/lib/pk11wrap/pk11pub.h#74
        // PK11SlotInfo *PK11_GetInternalSlot(void);
        this.nss.PK11_GetInternalSlot = nsslib.declare("PK11_GetInternalSlot",
                                                       ctypes.default_abi, this.nss_t.PK11SlotInfo.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#73
        // PK11SlotInfo *PK11_GetInternalKeySlot(void);
        this.nss.PK11_GetInternalKeySlot = nsslib.declare("PK11_GetInternalKeySlot",
                                                          ctypes.default_abi, this.nss_t.PK11SlotInfo.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#328
        // PK11SymKey *PK11_KeyGen(PK11SlotInfo *slot,CK_MECHANISM_TYPE type, SECItem *param, int keySize,void *wincx);
        this.nss.PK11_KeyGen = nsslib.declare("PK11_KeyGen",
                                              ctypes.default_abi, this.nss_t.PK11SymKey.ptr,
                                              this.nss_t.PK11SlotInfo.ptr, this.nss_t.CK_MECHANISM_TYPE,
                                              this.nss_t.SECItem.ptr, ctypes.int, ctypes.voidptr_t);
        // security/nss/lib/pk11wrap/pk11pub.h#477
        // SECStatus PK11_ExtractKeyValue(PK11SymKey *symKey);
        this.nss.PK11_ExtractKeyValue = nsslib.declare("PK11_ExtractKeyValue",
                                                       ctypes.default_abi, this.nss_t.SECStatus,
                                                       this.nss_t.PK11SymKey.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#478
        // SECItem * PK11_GetKeyData(PK11SymKey *symKey);
        this.nss.PK11_GetKeyData = nsslib.declare("PK11_GetKeyData",
                                                  ctypes.default_abi, this.nss_t.SECItem.ptr,
                                                  this.nss_t.PK11SymKey.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#278
        // CK_MECHANISM_TYPE PK11_AlgtagToMechanism(SECOidTag algTag);
        this.nss.PK11_AlgtagToMechanism = nsslib.declare("PK11_AlgtagToMechanism",
                                                         ctypes.default_abi, this.nss_t.CK_MECHANISM_TYPE,
                                                         this.nss_t.SECOidTag);
        // security/nss/lib/pk11wrap/pk11pub.h#270
        // int PK11_GetIVLength(CK_MECHANISM_TYPE type);
        this.nss.PK11_GetIVLength = nsslib.declare("PK11_GetIVLength",
                                                   ctypes.default_abi, ctypes.int,
                                                   this.nss_t.CK_MECHANISM_TYPE);
        // security/nss/lib/pk11wrap/pk11pub.h#269
        // int PK11_GetBlockSize(CK_MECHANISM_TYPE type,SECItem *params);
        this.nss.PK11_GetBlockSize = nsslib.declare("PK11_GetBlockSize",
                                                    ctypes.default_abi, ctypes.int,
                                                    this.nss_t.CK_MECHANISM_TYPE, this.nss_t.SECItem.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#293
        // CK_MECHANISM_TYPE PK11_GetPadMechanism(CK_MECHANISM_TYPE);
        this.nss.PK11_GetPadMechanism = nsslib.declare("PK11_GetPadMechanism",
                                                       ctypes.default_abi, this.nss_t.CK_MECHANISM_TYPE,
                                                       this.nss_t.CK_MECHANISM_TYPE);
        // security/nss/lib/pk11wrap/pk11pub.h#271
        // SECItem *PK11_ParamFromIV(CK_MECHANISM_TYPE type,SECItem *iv);
        this.nss.PK11_ParamFromIV = nsslib.declare("PK11_ParamFromIV",
                                                   ctypes.default_abi, this.nss_t.SECItem.ptr,
                                                   this.nss_t.CK_MECHANISM_TYPE, this.nss_t.SECItem.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#301
        // PK11SymKey *PK11_ImportSymKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, PK11Origin origin,
        //                               CK_ATTRIBUTE_TYPE operation, SECItem *key, void *wincx);
        this.nss.PK11_ImportSymKey = nsslib.declare("PK11_ImportSymKey",
                                                    ctypes.default_abi, this.nss_t.PK11SymKey.ptr,
                                                    this.nss_t.PK11SlotInfo.ptr, this.nss_t.CK_MECHANISM_TYPE, this.nss_t.PK11Origin,
                                                    this.nss_t.CK_ATTRIBUTE_TYPE, this.nss_t.SECItem.ptr, ctypes.voidptr_t);
        // security/nss/lib/pk11wrap/pk11pub.h#672
        // PK11Context *PK11_CreateContextBySymKey(CK_MECHANISM_TYPE type, CK_ATTRIBUTE_TYPE operation,
        //                                         PK11SymKey *symKey, SECItem *param);
        this.nss.PK11_CreateContextBySymKey = nsslib.declare("PK11_CreateContextBySymKey",
                                                             ctypes.default_abi, this.nss_t.PK11Context.ptr,
                                                             this.nss_t.CK_MECHANISM_TYPE, this.nss_t.CK_ATTRIBUTE_TYPE,
                                                             this.nss_t.PK11SymKey.ptr, this.nss_t.SECItem.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#685
        // SECStatus PK11_CipherOp(PK11Context *context, unsigned char *out
        //                         int *outlen, int maxout, unsigned char *in, int inlen);
        this.nss.PK11_CipherOp = nsslib.declare("PK11_CipherOp",
                                                ctypes.default_abi, this.nss_t.SECStatus,
                                                this.nss_t.PK11Context.ptr, ctypes.unsigned_char.ptr,
                                                ctypes.int.ptr, ctypes.int, ctypes.unsigned_char.ptr, ctypes.int);
        // security/nss/lib/pk11wrap/pk11pub.h#688
        // SECStatus PK11_DigestFinal(PK11Context *context, unsigned char *data,
        //                            unsigned int *outLen, unsigned int length);
        this.nss.PK11_DigestFinal = nsslib.declare("PK11_DigestFinal",
                                                   ctypes.default_abi, this.nss_t.SECStatus,
                                                   this.nss_t.PK11Context.ptr, ctypes.unsigned_char.ptr,
                                                   ctypes.unsigned_int.ptr, ctypes.unsigned_int);
        // security/nss/lib/pk11wrap/pk11pub.h#507
        // SECKEYPrivateKey *PK11_GenerateKeyPairWithFlags(PK11SlotInfo *slot,
        //                                                 CK_MECHANISM_TYPE type, void *param, SECKEYPublicKey **pubk,
        //                                                 PK11AttrFlags attrFlags, void *wincx);
        this.nss.PK11_GenerateKeyPairWithFlags = nsslib.declare("PK11_GenerateKeyPairWithFlags",
                                                   ctypes.default_abi, this.nss_t.SECKEYPrivateKey.ptr,
                                                   this.nss_t.PK11SlotInfo.ptr, this.nss_t.CK_MECHANISM_TYPE, ctypes.voidptr_t,
                                                   this.nss_t.SECKEYPublicKey.ptr.ptr, this.nss_t.PK11AttrFlags, ctypes.voidptr_t);
        // security/nss/lib/pk11wrap/pk11pub.h#466
        // SECStatus PK11_SetPrivateKeyNickname(SECKEYPrivateKey *privKey, const char *nickname);
        this.nss.PK11_SetPrivateKeyNickname = nsslib.declare("PK11_SetPrivateKeyNickname",
                                                             ctypes.default_abi, this.nss_t.SECStatus,
                                                             this.nss_t.SECKEYPrivateKey.ptr, ctypes.char.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#731
        // SECAlgorithmID * PK11_CreatePBEV2AlgorithmID(SECOidTag pbeAlgTag, SECOidTag cipherAlgTag,
        //                                              SECOidTag prfAlgTag, int keyLength, int iteration,
        //                                              SECItem *salt);
        this.nss.PK11_CreatePBEV2AlgorithmID = nsslib.declare("PK11_CreatePBEV2AlgorithmID",
                                                              ctypes.default_abi, this.nss_t.SECAlgorithmID.ptr,
                                                              this.nss_t.SECOidTag, this.nss_t.SECOidTag, this.nss_t.SECOidTag, 
                                                              ctypes.int, ctypes.int, this.nss_t.SECItem.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#736
        // PK11SymKey * PK11_PBEKeyGen(PK11SlotInfo *slot, SECAlgorithmID *algid,  SECItem *pwitem, PRBool faulty3DES, void *wincx);
        this.nss.PK11_PBEKeyGen = nsslib.declare("PK11_PBEKeyGen",
                                                 ctypes.default_abi, this.nss_t.PK11SymKey.ptr,
                                                 this.nss_t.PK11SlotInfo.ptr, this.nss_t.SECAlgorithmID.ptr,
                                                 this.nss_t.SECItem.ptr, this.nss_t.PRBool, ctypes.voidptr_t);
        // security/nss/lib/pk11wrap/pk11pub.h#574
        // SECStatus PK11_WrapPrivKey(PK11SlotInfo *slot, PK11SymKey *wrappingKey,
        //                            SECKEYPrivateKey *privKey, CK_MECHANISM_TYPE wrapType,
        //                            SECItem *param, SECItem *wrappedKey, void *wincx);
        this.nss.PK11_WrapPrivKey = nsslib.declare("PK11_WrapPrivKey",
                                                   ctypes.default_abi, this.nss_t.SECStatus,
                                                   this.nss_t.PK11SlotInfo.ptr, this.nss_t.PK11SymKey.ptr,
                                                   this.nss_t.SECKEYPrivateKey.ptr, this.nss_t.CK_MECHANISM_TYPE,
                                                   this.nss_t.SECItem.ptr, this.nss_t.SECItem.ptr, ctypes.voidptr_t);
        // security/nss/lib/cryptohi/keyhi.h#159
        // SECItem* SECKEY_EncodeDERSubjectPublicKeyInfo(SECKEYPublicKey *pubk);
        this.nss.SECKEY_EncodeDERSubjectPublicKeyInfo = nsslib.declare("SECKEY_EncodeDERSubjectPublicKeyInfo",
                                                                       ctypes.default_abi, this.nss_t.SECItem.ptr,
                                                                       this.nss_t.SECKEYPublicKey.ptr);
        // security/nss/lib/cryptohi/keyhi.h#165
        // CERTSubjectPublicKeyInfo * SECKEY_DecodeDERSubjectPublicKeyInfo(SECItem *spkider);
        this.nss.SECKEY_DecodeDERSubjectPublicKeyInfo = nsslib.declare("SECKEY_DecodeDERSubjectPublicKeyInfo",
                                                                       ctypes.default_abi, this.nss_t.CERTSubjectPublicKeyInfo.ptr,
                                                                       this.nss_t.SECItem.ptr);
        // security/nss/lib/cryptohi/keyhi.h#179
        // SECKEYPublicKey * SECKEY_ExtractPublicKey(CERTSubjectPublicKeyInfo *);
        this.nss.SECKEY_ExtractPublicKey = nsslib.declare("SECKEY_ExtractPublicKey",
                                                          ctypes.default_abi, this.nss_t.SECKEYPublicKey.ptr,
                                                          this.nss_t.CERTSubjectPublicKeyInfo.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#377
        // SECStatus PK11_PubWrapSymKey(CK_MECHANISM_TYPE type, SECKEYPublicKey *pubKey,
        //                              PK11SymKey *symKey, SECItem *wrappedKey);
        this.nss.PK11_PubWrapSymKey = nsslib.declare("PK11_PubWrapSymKey",
                                                     ctypes.default_abi, this.nss_t.SECStatus,
                                                     this.nss_t.CK_MECHANISM_TYPE, this.nss_t.SECKEYPublicKey.ptr,
                                                     this.nss_t.PK11SymKey.ptr, this.nss_t.SECItem.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#568
        // SECKEYPrivateKey *PK11_UnwrapPrivKey(PK11SlotInfo *slot, 
        //                 PK11SymKey *wrappingKey, CK_MECHANISM_TYPE wrapType,
        //                 SECItem *param, SECItem *wrappedKey, SECItem *label, 
        //                 SECItem *publicValue, PRBool token, PRBool sensitive,
        //                 CK_KEY_TYPE keyType, CK_ATTRIBUTE_TYPE *usage, int usageCount,
        //                 void *wincx);
        this.nss.PK11_UnwrapPrivKey = nsslib.declare("PK11_UnwrapPrivKey",
                                                     ctypes.default_abi, this.nss_t.SECKEYPrivateKey.ptr,
                                                     this.nss_t.PK11SlotInfo.ptr, this.nss_t.PK11SymKey.ptr,
                                                     this.nss_t.CK_MECHANISM_TYPE, this.nss_t.SECItem.ptr,
                                                     this.nss_t.SECItem.ptr, this.nss_t.SECItem.ptr,
                                                     this.nss_t.SECItem.ptr, this.nss_t.PRBool,
                                                     this.nss_t.PRBool, this.nss_t.CK_KEY_TYPE,
                                                     this.nss_t.CK_ATTRIBUTE_TYPE.ptr, ctypes.int,
                                                     ctypes.voidptr_t);
        // security/nss/lib/pk11wrap/pk11pub.h#447
        // PK11SymKey *PK11_PubUnwrapSymKey(SECKEYPrivateKey *key, SECItem *wrapppedKey,
        //         CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, int keySize);
        this.nss.PK11_PubUnwrapSymKey = nsslib.declare("PK11_PubUnwrapSymKey",
                                                       ctypes.default_abi, this.nss_t.PK11SymKey.ptr,
                                                       this.nss_t.SECKEYPrivateKey.ptr, this.nss_t.SECItem.ptr,
                                                       this.nss_t.CK_MECHANISM_TYPE, this.nss_t.CK_ATTRIBUTE_TYPE, ctypes.int);
        // security/nss/lib/pk11wrap/pk11pub.h#675
        // void PK11_DestroyContext(PK11Context *context, PRBool freeit);
        this.nss.PK11_DestroyContext = nsslib.declare("PK11_DestroyContext",
                                                       ctypes.default_abi, ctypes.void_t,
                                                       this.nss_t.PK11Context.ptr, this.nss_t.PRBool);
        // security/nss/lib/pk11wrap/pk11pub.h#299
        // void PK11_FreeSymKey(PK11SymKey *key);
        this.nss.PK11_FreeSymKey = nsslib.declare("PK11_FreeSymKey",
                                                  ctypes.default_abi, ctypes.void_t,
                                                  this.nss_t.PK11SymKey.ptr);
        // security/nss/lib/pk11wrap/pk11pub.h#70
        // void PK11_FreeSlot(PK11SlotInfo *slot);
        this.nss.PK11_FreeSlot = nsslib.declare("PK11_FreeSlot",
                                                ctypes.default_abi, ctypes.void_t,
                                                this.nss_t.PK11SlotInfo.ptr);
        // security/nss/lib/util/secitem.h#114
        // extern void SECITEM_FreeItem(SECItem *zap, PRBool freeit);
        this.nss.SECITEM_FreeItem = nsslib.declare("SECITEM_FreeItem",
                                                   ctypes.default_abi, ctypes.void_t,
                                                   this.nss_t.SECItem.ptr, this.nss_t.PRBool);
        // security/nss/lib/cryptohi/keyhi.h#193
        // extern void SECKEY_DestroyPublicKey(SECKEYPublicKey *key);
        this.nss.SECKEY_DestroyPublicKey = nsslib.declare("SECKEY_DestroyPublicKey",
                                                          ctypes.default_abi, ctypes.void_t,
                                                          this.nss_t.SECKEYPublicKey.ptr);
        // security/nss/lib/cryptohi/keyhi.h#186
        // extern void SECKEY_DestroyPrivateKey(SECKEYPrivateKey *key);
        this.nss.SECKEY_DestroyPrivateKey = nsslib.declare("SECKEY_DestroyPrivateKey",
                                                           ctypes.default_abi, ctypes.void_t,
                                                           this.nss_t.SECKEYPrivateKey.ptr);
        // security/nss/lib/util/secoid.h#103
        // extern void SECOID_DestroyAlgorithmID(SECAlgorithmID *aid, PRBool freeit);
        this.nss.SECOID_DestroyAlgorithmID = nsslib.declare("SECOID_DestroyAlgorithmID",
                                                            ctypes.default_abi, ctypes.void_t,
                                                            this.nss_t.SECAlgorithmID.ptr, this.nss_t.PRBool);
        // security/nss/lib/cryptohi/keyhi.h#58
        // extern void SECKEY_DestroySubjectPublicKeyInfo(CERTSubjectPublicKeyInfo *spki);
        this.nss.SECKEY_DestroySubjectPublicKeyInfo = nsslib.declare("SECKEY_DestroySubjectPublicKeyInfo",
                                                                     ctypes.default_abi, ctypes.void_t,
                                                                     this.nss_t.CERTSubjectPublicKeyInfo.ptr);
    },


    //
    // IWeaveCrypto interfaces
    //


    algorithm : Ci.IWeaveCrypto.AES_256_CBC,

    keypairBits : 2048,

    encrypt : function(clearTextUCS2, symmetricKey, iv) {
        this.log("encrypt() called");

        // js-ctypes autoconverts to a UTF8 buffer, but also includes a null
        // at the end which we don't want. Cast to make the length 1 byte shorter.
        let inputBuffer = new ctypes.ArrayType(ctypes.unsigned_char)(clearTextUCS2);
        inputBuffer = ctypes.cast(inputBuffer, ctypes.unsigned_char.array(inputBuffer.length - 1));

        // When using CBC padding, the output size is the input size rounded
        // up to the nearest block. If the input size is exactly on a block
        // boundary, the output is 1 extra block long.
        let mech = this.nss.PK11_AlgtagToMechanism(this.algorithm);
        let blockSize = this.nss.PK11_GetBlockSize(mech, null);
        let outputBufferSize = inputBuffer.length + blockSize;
        let outputBuffer = new ctypes.ArrayType(ctypes.unsigned_char, outputBufferSize)();

        outputBuffer = this._commonCrypt(inputBuffer, outputBuffer, symmetricKey, iv, this.nss.CKA_ENCRYPT);

        return this.encodeBase64(outputBuffer.address(), outputBuffer.length);
    },


    decrypt : function(cipherText, symmetricKey, iv) {
        this.log("decrypt() called");

        let inputUCS2 = "";
        if (cipherText.length)
            inputUCS2 = atob(cipherText);

        // We can't have js-ctypes create the buffer directly from the string
        // (as in encrypt()), because we do _not_ want it to do UTF8
        // conversion... We've got random binary data in the input's low byte.
        let input = new ctypes.ArrayType(ctypes.unsigned_char, inputUCS2.length)();
        this.byteCompress(inputUCS2, input);

        let outputBuffer = new ctypes.ArrayType(ctypes.unsigned_char, input.length)();

        outputBuffer = this._commonCrypt(input, outputBuffer, symmetricKey, iv, this.nss.CKA_DECRYPT);

        // outputBuffer contains UTF-8 data, let js-ctypes autoconvert that to a JS string.
        // XXX Bug 573842: wrap the string from ctypes to get a new string, so
        // we don't hit bug 573841.
        return "" + outputBuffer.readString() + "";
    },


    _commonCrypt : function (input, output, symmetricKey, iv, operation) {
        this.log("_commonCrypt() called");
        // Get rid of the base64 encoding and convert to SECItems.
        let keyItem = this.makeSECItem(symmetricKey, true);
        let ivItem  = this.makeSECItem(iv, true);

        // Determine which (padded) PKCS#11 mechanism to use.
        // EG: AES_128_CBC --> CKM_AES_CBC --> CKM_AES_CBC_PAD
        let mechanism = this.nss.PK11_AlgtagToMechanism(this.algorithm);
        mechanism = this.nss.PK11_GetPadMechanism(mechanism);
        if (mechanism == this.nss.CKM_INVALID_MECHANISM)
            throw this.makeException("invalid algorithm (can't pad)", Cr.NS_ERROR_FAILURE);

        let ctx, symKey, slot, ivParam;
        try {
            ivParam = this.nss.PK11_ParamFromIV(mechanism, ivItem.address());
            if (ivParam.isNull())
                throw this.makeException("can't convert IV to param", Cr.NS_ERROR_FAILURE);

            slot = this.nss.PK11_GetInternalKeySlot();
            if (slot.isNull())
                throw this.makeException("can't get internal key slot", Cr.NS_ERROR_FAILURE);

            symKey = this.nss.PK11_ImportSymKey(slot, mechanism, this.nss.PK11_OriginUnwrap, operation, keyItem.address(), null);
            if (symKey.isNull())
                throw this.makeException("symkey import failed", Cr.NS_ERROR_FAILURE);

            ctx = this.nss.PK11_CreateContextBySymKey(mechanism, operation, symKey, ivParam);
            if (ctx.isNull())
                throw this.makeException("couldn't create context for symkey", Cr.NS_ERROR_FAILURE);

            let maxOutputSize = output.length;
            let tmpOutputSize = new ctypes.int(); // Note 1: NSS uses a signed int here...

            if (this.nss.PK11_CipherOp(ctx, output, tmpOutputSize.address(), maxOutputSize, input, input.length))
                throw this.makeException("cipher operation failed", Cr.NS_ERROR_FAILURE);

            let actualOutputSize = tmpOutputSize.value;
            let finalOutput = output.addressOfElement(actualOutputSize);
            maxOutputSize -= actualOutputSize;

            // PK11_DigestFinal sure sounds like the last step for *hashing*, but it
            // just seems to be an odd name -- NSS uses this to finish the current
            // cipher operation. You'd think it would be called PK11_CipherOpFinal...
            let tmpOutputSize2 = new ctypes.unsigned_int(); // Note 2: ...but an unsigned here!
            if (this.nss.PK11_DigestFinal(ctx, finalOutput, tmpOutputSize2.address(), maxOutputSize))
                throw this.makeException("cipher finalize failed", Cr.NS_ERROR_FAILURE);

            actualOutputSize += tmpOutputSize2.value;
            let newOutput = ctypes.cast(output, ctypes.unsigned_char.array(actualOutputSize));
            return newOutput;
        } catch (e) {
            this.log("_commonCrypt: failed: " + e);
            throw e;
        } finally {
            if (ctx && !ctx.isNull())
                this.nss.PK11_DestroyContext(ctx, true);
            if (symKey && !symKey.isNull())
                this.nss.PK11_FreeSymKey(symKey);
            if (slot && !slot.isNull())
                this.nss.PK11_FreeSlot(slot);
            if (ivParam && !ivParam.isNull())
                this.nss.SECITEM_FreeItem(ivParam, true);
        }
    },


    generateKeypair : function(passphrase, salt, iv, out_encodedPublicKey, out_wrappedPrivateKey) {
        this.log("generateKeypair() called.");

        let pubKey, privKey, slot;
        try {
            // Attributes for the private key. We're just going to wrap and extract the
            // value, so they're not critical. The _PUBLIC attribute just indicates the
            // object can be accessed without being logged into the token.
            let attrFlags = (this.nss.PK11_ATTR_SESSION | this.nss.PK11_ATTR_PUBLIC | this.nss.PK11_ATTR_SENSITIVE);

            pubKey  = new this.nss_t.SECKEYPublicKey.ptr();

            let rsaParams = new this.nss_t.PK11RSAGenParams();
            rsaParams.keySizeInBits = this.keypairBits; // 1024, 2048, etc.
            rsaParams.pe = 65537;                       // public exponent.

            slot = this.nss.PK11_GetInternalSlot();
            if (slot.isNull())
                throw this.makeException("couldn't get internal slot", Cr.NS_ERROR_FAILURE);

            // Generate the keypair.
            privKey = this.nss.PK11_GenerateKeyPairWithFlags(slot,
                                                             this.nss.CKM_RSA_PKCS_KEY_PAIR_GEN,
                                                             rsaParams.address(),
                                                             pubKey.address(),
                                                             attrFlags, null);
            if (privKey.isNull())
                throw this.makeException("keypair generation failed", Cr.NS_ERROR_FAILURE);
            
            let s = this.nss.PK11_SetPrivateKeyNickname(privKey, "Weave User PrivKey");
            if (s)
                throw this.makeException("key nickname failed", Cr.NS_ERROR_FAILURE);

            let wrappedPrivateKey = this._wrapPrivateKey(privKey, passphrase, salt, iv);
            out_wrappedPrivateKey.value = wrappedPrivateKey; // outparam

            let derKey = this.nss.SECKEY_EncodeDERSubjectPublicKeyInfo(pubKey);
            if (derKey.isNull())
              throw this.makeException("SECKEY_EncodeDERSubjectPublicKeyInfo failed", Cr.NS_ERROR_FAILURE);

            let encodedPublicKey = this.encodeBase64(derKey.contents.data, derKey.contents.len);
            out_encodedPublicKey.value = encodedPublicKey; // outparam
        } catch (e) {
            this.log("generateKeypair: failed: " + e);
            throw e;
        } finally {
            if (pubKey && !pubKey.isNull())
                this.nss.SECKEY_DestroyPublicKey(pubKey);
            if (privKey && !privKey.isNull())
                this.nss.SECKEY_DestroyPrivateKey(privKey);
            if (slot && !slot.isNull())
                this.nss.PK11_FreeSlot(slot);
        }
    },


    generateRandomKey : function() {
        this.log("generateRandomKey() called");
        let encodedKey, keygenMech, keySize;

        // Doesn't NSS have a lookup function to do this?
        switch(this.algorithm) {
          case Ci.IWeaveCrypto.AES_128_CBC:
            keygenMech = this.nss.CKM_AES_KEY_GEN;
            keySize = 16;
            break;

          case Ci.IWeaveCrypto.AES_192_CBC:
            keygenMech = this.nss.CKM_AES_KEY_GEN;
            keySize = 24;
            break;

          case Ci.IWeaveCrypto.AES_256_CBC:
            keygenMech = this.nss.CKM_AES_KEY_GEN;
            keySize = 32;
            break;

          default:
            throw this.makeException("unknown algorithm", Cr.NS_ERROR_FAILURE);
        }

        let slot, randKey, keydata;
        try {
            slot = this.nss.PK11_GetInternalSlot();
            if (slot.isNull())
                throw this.makeException("couldn't get internal slot", Cr.NS_ERROR_FAILURE);

            randKey = this.nss.PK11_KeyGen(slot, keygenMech, null, keySize, null);
            if (randKey.isNull())
                throw this.makeException("PK11_KeyGen failed.", Cr.NS_ERROR_FAILURE);

            // Slightly odd API, this call just prepares the key value for
            // extraction, we get the actual bits from the call to PK11_GetKeyData().
            if (this.nss.PK11_ExtractKeyValue(randKey))
                throw this.makeException("PK11_ExtractKeyValue failed.", Cr.NS_ERROR_FAILURE);

            keydata = this.nss.PK11_GetKeyData(randKey);
            if (keydata.isNull())
                throw this.makeException("PK11_GetKeyData failed.", Cr.NS_ERROR_FAILURE);

            return this.encodeBase64(keydata.contents.data, keydata.contents.len);
        } catch (e) {
            this.log("generateRandomKey: failed: " + e);
            throw e;
        } finally {
            if (randKey && !randKey.isNull())
                this.nss.PK11_FreeSymKey(randKey);
            if (slot && !slot.isNull())
                this.nss.PK11_FreeSlot(slot);
        }
    },


    generateRandomIV : function() {
        this.log("generateRandomIV() called");

        let mech = this.nss.PK11_AlgtagToMechanism(this.algorithm);
        let size = this.nss.PK11_GetIVLength(mech);

        return this.generateRandomBytes(size);
    },


    generateRandomBytes : function(byteCount) {
        this.log("generateRandomBytes() called");

        // Temporary buffer to hold the generated data.
        let scratch = new ctypes.ArrayType(ctypes.unsigned_char, byteCount)();
        if (this.nss.PK11_GenerateRandom(scratch, byteCount))
            throw this.makeException("PK11_GenrateRandom failed", Cr.NS_ERROR_FAILURE);

        return this.encodeBase64(scratch.address(), scratch.length);
    },


    wrapSymmetricKey : function(symmetricKey, encodedPublicKey) {
        this.log("wrapSymmetricKey() called");

        // Step 1. Get rid of the base64 encoding on the inputs.

        let pubKeyData = this.makeSECItem(encodedPublicKey, true);
        let symKeyData = this.makeSECItem(symmetricKey, true);

        // This buffer is much larger than needed, but that's ok.
        let keyData = new ctypes.ArrayType(ctypes.unsigned_char, 4096)();
        let wrappedKey = new this.nss_t.SECItem(this.nss.SIBUFFER, keyData, keyData.length);

        // Step 2. Put the symmetric key bits into a P11 key object.
        let slot, symKey, pubKeyInfo, pubKey;
        try {
            slot = this.nss.PK11_GetInternalSlot();
            if (slot.isNull())
                throw this.makeException("couldn't get internal slot", Cr.NS_ERROR_FAILURE);

            // ImportSymKey wants a mechanism, from which it derives the key type.
            let keyMech = this.nss.PK11_AlgtagToMechanism(this.algorithm);

            // This imports a key with the usage set for encryption, but that doesn't
            // really matter because we're just going to wrap it up and not use it.
            symKey = this.nss.PK11_ImportSymKey(slot, keyMech, this.nss.PK11_OriginUnwrap, this.nss.CKA_ENCRYPT, symKeyData.address(), null);
            if (symKey.isNull())
                throw this.makeException("symkey import failed", Cr.NS_ERROR_FAILURE);

            // Step 3. Put the public key bits into a P11 key object.

            // Can't just do this directly, it's expecting a minimal ASN1 blob
            // pubKey = SECKEY_ImportDERPublicKey(&pubKeyData, CKK_RSA);
            pubKeyInfo = this.nss.SECKEY_DecodeDERSubjectPublicKeyInfo(pubKeyData.address());
            if (pubKeyInfo.isNull())
                throw this.makeException("SECKEY_DecodeDERSubjectPublicKeyInfo failed", Cr.NS_ERROR_FAILURE);

            pubKey = this.nss.SECKEY_ExtractPublicKey(pubKeyInfo);
            if (pubKey.isNull())
                throw this.makeException("SECKEY_ExtractPublicKey failed", Cr.NS_ERROR_FAILURE);

            // Step 4. Wrap the symmetric key with the public key.

            let wrapMech = this.nss.PK11_AlgtagToMechanism(this.nss.SEC_OID_PKCS1_RSA_ENCRYPTION);

            let s = this.nss.PK11_PubWrapSymKey(wrapMech, pubKey, symKey, wrappedKey.address());
            if (s)
                throw this.makeException("PK11_PubWrapSymKey failed", Cr.NS_ERROR_FAILURE);

            // Step 5. Base64 encode the wrapped key, cleanup, and return to caller.
            return this.encodeBase64(wrappedKey.data, wrappedKey.len);
        } catch (e) {
            this.log("wrapSymmetricKey: failed: " + e);
            throw e;
        } finally {
            if (pubKey && !pubKey.isNull())
                this.nss.SECKEY_DestroyPublicKey(pubKey);
            if (pubKeyInfo && !pubKeyInfo.isNull())
                this.nss.SECKEY_DestroySubjectPublicKeyInfo(pubKeyInfo);
            if (symKey && !symKey.isNull())
                this.nss.PK11_FreeSymKey(symKey);
            if (slot && !slot.isNull())
                this.nss.PK11_FreeSlot(slot);
        }
    },


    unwrapSymmetricKey : function(wrappedSymmetricKey, wrappedPrivateKey, passphrase, salt, iv) {
        this.log("unwrapSymmetricKey() called");
        let privKeyUsageLength = 1;
        let privKeyUsage = new ctypes.ArrayType(this.nss_t.CK_ATTRIBUTE_TYPE, privKeyUsageLength)();
        privKeyUsage[0] = this.nss.CKA_UNWRAP;

        // Step 1. Get rid of the base64 encoding on the inputs.
        let wrappedPrivKey = this.makeSECItem(wrappedPrivateKey, true);
        let wrappedSymKey  = this.makeSECItem(wrappedSymmetricKey, true);

        let ivParam, slot, pbeKey, symKey, privKey, symKeyData;
        try {
            // Step 2. Convert the passphrase to a symmetric key and get the IV in the proper form.
            pbeKey = this._deriveKeyFromPassphrase(passphrase, salt);
            let ivItem = this.makeSECItem(iv, true);

            // AES_128_CBC --> CKM_AES_CBC --> CKM_AES_CBC_PAD
            let wrapMech = this.nss.PK11_AlgtagToMechanism(this.algorithm);
            wrapMech = this.nss.PK11_GetPadMechanism(wrapMech);
            if (wrapMech == this.nss.CKM_INVALID_MECHANISM)
                throw this.makeException("unwrapSymKey: unknown key mech", Cr.NS_ERROR_FAILURE);

            ivParam = this.nss.PK11_ParamFromIV(wrapMech, ivItem.address());
            if (ivParam.isNull())
                throw this.makeException("unwrapSymKey: PK11_ParamFromIV failed", Cr.NS_ERROR_FAILURE);

            // Step 3. Unwrap the private key with the key from the passphrase.
            slot = this.nss.PK11_GetInternalSlot();
            if (slot.isNull())
                throw this.makeException("couldn't get internal slot", Cr.NS_ERROR_FAILURE);

            // Normally, one wants to associate a private key with a public key.
            // P11_UnwrapPrivKey() passes its keyID arg to PK11_MakeIDFromPubKey(),
            // which hashes the public key to create an ID (or, for small inputs,
            // assumes it's already hashed and does nothing).
            // We don't really care about this, because our unwrapped private key will
            // just live long enough to unwrap the bulk data key. So, we'll just jam in
            // a random value... We have an IV handy, so that will suffice.
            let keyID = ivItem.address();

            privKey = this.nss.PK11_UnwrapPrivKey(slot,
                                                  pbeKey, wrapMech, ivParam, wrappedPrivKey.address(),
                                                  null,   // label
                                                  keyID,
                                                  false, // isPerm (token object)
                                                  true,  // isSensitive
                                                  this.nss.CKK_RSA,
                                                  privKeyUsage.addressOfElement(0), privKeyUsageLength,
                                                  null);  // wincx
            if (privKey.isNull())
                throw this.makeException("PK11_UnwrapPrivKey failed", Cr.NS_ERROR_FAILURE);

            // Step 4. Unwrap the symmetric key with the user's private key.

            // XXX also have PK11_PubUnwrapSymKeyWithFlags() if more control is needed.
            // (last arg is keySize, 0 seems to work)
            symKey = this.nss.PK11_PubUnwrapSymKey(privKey, wrappedSymKey.address(), wrapMech,
                                                   this.nss.CKA_DECRYPT, 0);
            if (symKey.isNull())
                throw this.makeException("PK11_PubUnwrapSymKey failed", Cr.NS_ERROR_FAILURE);

            // Step 5. Base64 encode the unwrapped key, cleanup, and return to caller.
            if (this.nss.PK11_ExtractKeyValue(symKey))
                throw this.makeException("PK11_ExtractKeyValue failed.", Cr.NS_ERROR_FAILURE);

            symKeyData = this.nss.PK11_GetKeyData(symKey);
            if (symKeyData.isNull())
                throw this.makeException("PK11_GetKeyData failed.", Cr.NS_ERROR_FAILURE);

            return this.encodeBase64(symKeyData.contents.data, symKeyData.contents.len);
        } catch (e) {
            this.log("unwrapSymmetricKey: failed: " + e);
            throw e;
        } finally {
            if (privKey && !privKey.isNull())
                this.nss.SECKEY_DestroyPrivateKey(privKey);
            if (symKey && !symKey.isNull())
                this.nss.PK11_FreeSymKey(symKey);
            if (pbeKey && !pbeKey.isNull())
                this.nss.PK11_FreeSymKey(pbeKey);
            if (slot && !slot.isNull())
                this.nss.PK11_FreeSlot(slot);
            if (ivParam && !ivParam.isNull())
                this.nss.SECITEM_FreeItem(ivParam, true);
        }
    },


    rewrapPrivateKey : function(wrappedPrivateKey, oldPassphrase, salt, iv, newPassphrase) {
        this.log("rewrapPrivateKey() called");
        let privKeyUsageLength = 1;
        let privKeyUsage = new ctypes.ArrayType(this.nss_t.CK_ATTRIBUTE_TYPE, privKeyUsageLength)();
        privKeyUsage[0] = this.nss.CKA_UNWRAP;

        // Step 1. Get rid of the base64 encoding on the inputs.
        let wrappedPrivKey = this.makeSECItem(wrappedPrivateKey, true);

        let pbeKey, ivParam, slot, privKey;
        try {
            // Step 2. Convert the passphrase to a symmetric key and get the IV in the proper form.
            let pbeKey = this._deriveKeyFromPassphrase(oldPassphrase, salt);
            let ivItem = this.makeSECItem(iv, true);

            // AES_128_CBC --> CKM_AES_CBC --> CKM_AES_CBC_PAD
            let wrapMech = this.nss.PK11_AlgtagToMechanism(this.algorithm);
            wrapMech = this.nss.PK11_GetPadMechanism(wrapMech);
            if (wrapMech == this.nss.CKM_INVALID_MECHANISM)
                throw this.makeException("rewrapSymKey: unknown key mech", Cr.NS_ERROR_FAILURE);

            ivParam = this.nss.PK11_ParamFromIV(wrapMech, ivItem.address());
            if (ivParam.isNull())
                throw this.makeException("rewrapSymKey: PK11_ParamFromIV failed", Cr.NS_ERROR_FAILURE);

            // Step 3. Unwrap the private key with the key from the passphrase.
            slot = this.nss.PK11_GetInternalSlot();
            if (slot.isNull())
                throw this.makeException("couldn't get internal slot", Cr.NS_ERROR_FAILURE);

            let keyID = ivItem.address();

            privKey = this.nss.PK11_UnwrapPrivKey(slot,
                                                  pbeKey, wrapMech, ivParam, wrappedPrivKey.address(),
                                                  null,   // label
                                                  keyID,
                                                  false, // isPerm (token object)
                                                  true,  // isSensitive
                                                  this.nss.CKK_RSA,
                                                  privKeyUsage.addressOfElement(0), privKeyUsageLength,
                                                  null);  // wincx
            if (privKey.isNull())
                throw this.makeException("PK11_UnwrapPrivKey failed", Cr.NS_ERROR_FAILURE);

            // Step 4. Rewrap the private key with the new passphrase.
            return this._wrapPrivateKey(privKey, newPassphrase, salt, iv);
        } catch (e) {
            this.log("rewrapPrivateKey: failed: " + e);
            throw e;
        } finally {
            if (privKey && !privKey.isNull())
                this.nss.SECKEY_DestroyPrivateKey(privKey);
            if (slot && !slot.isNull())
                this.nss.PK11_FreeSlot(slot);
            if (ivParam && !ivParam.isNull())
                this.nss.SECITEM_FreeItem(ivParam, true);
            if (pbeKey && !pbeKey.isNull())
                this.nss.PK11_FreeSymKey(pbeKey);
        }
    },


    verifyPassphrase : function(wrappedPrivateKey, passphrase, salt, iv) {
        this.log("verifyPassphrase() called");
        let privKeyUsageLength = 1;
        let privKeyUsage = new ctypes.ArrayType(this.nss_t.CK_ATTRIBUTE_TYPE, privKeyUsageLength)();
        privKeyUsage[0] = this.nss.CKA_UNWRAP;

        // Step 1. Get rid of the base64 encoding on the inputs.
        let wrappedPrivKey = this.makeSECItem(wrappedPrivateKey, true);

        let pbeKey, ivParam, slot, privKey;
        try {
            // Step 2. Convert the passphrase to a symmetric key and get the IV in the proper form.
            pbeKey = this._deriveKeyFromPassphrase(passphrase, salt);
            let ivItem = this.makeSECItem(iv, true);

            // AES_128_CBC --> CKM_AES_CBC --> CKM_AES_CBC_PAD
            let wrapMech = this.nss.PK11_AlgtagToMechanism(this.algorithm);
            wrapMech = this.nss.PK11_GetPadMechanism(wrapMech);
            if (wrapMech == this.nss.CKM_INVALID_MECHANISM)
                throw this.makeException("rewrapSymKey: unknown key mech", Cr.NS_ERROR_FAILURE);

            ivParam = this.nss.PK11_ParamFromIV(wrapMech, ivItem.address());
            if (ivParam.isNull())
                throw this.makeException("rewrapSymKey: PK11_ParamFromIV failed", Cr.NS_ERROR_FAILURE);

            // Step 3. Unwrap the private key with the key from the passphrase.
            slot = this.nss.PK11_GetInternalSlot();
            if (slot.isNull())
                throw this.makeException("couldn't get internal slot", Cr.NS_ERROR_FAILURE);

            let keyID = ivItem.address();

            privKey = this.nss.PK11_UnwrapPrivKey(slot,
                                                  pbeKey, wrapMech, ivParam, wrappedPrivKey.address(),
                                                  null,   // label
                                                  keyID,
                                                  false, // isPerm (token object)
                                                  true,  // isSensitive
                                                  this.nss.CKK_RSA,
                                                  privKeyUsage.addressOfElement(0), privKeyUsageLength,
                                                  null);  // wincx
            return (!privKey.isNull());
        } catch (e) {
            this.log("verifyPassphrase: failed: " + e);
            throw e;
        } finally {
            if (privKey && !privKey.isNull())
                this.nss.SECKEY_DestroyPrivateKey(privKey);
            if (slot && !slot.isNull())
                this.nss.PK11_FreeSlot(slot);
            if (ivParam && !ivParam.isNull())
                this.nss.SECITEM_FreeItem(ivParam, true);
            if (pbeKey && !pbeKey.isNull())
                this.nss.PK11_FreeSymKey(pbeKey);
        }
    },


    //
    // Utility functions
    //


    // Compress a JS string (2-byte chars) into a normal C string (1-byte chars)
    // EG, for "ABC",  0x0041, 0x0042, 0x0043 --> 0x41, 0x42, 0x43
    byteCompress : function (jsString, charArray) {
        let intArray = ctypes.cast(charArray, ctypes.uint8_t.array(charArray.length));
        for (let i = 0; i < jsString.length; i++)
            intArray[i] = jsString.charCodeAt(i) % 256; // convert to bytes
    },

    // Expand a normal C string (1-byte chars) into a JS string (2-byte chars)
    // EG, for "ABC",  0x41, 0x42, 0x43 --> 0x0041, 0x0042, 0x0043
    byteExpand : function (charArray) {
        let expanded = "";
        let len = charArray.length;
        let intData = ctypes.cast(charArray, ctypes.uint8_t.array(len));
        for (let i = 0; i < len; i++)
            expanded += String.fromCharCode(intData[i]);
        return expanded;
    },

    encodeBase64 : function (data, len) {
        // Byte-expand the buffer, so we can treat it as a UCS-2 string
        // consisting of u0000 - u00FF.
        let expanded = "";
        let intData = ctypes.cast(data, ctypes.uint8_t.array(len).ptr).contents;
        for (let i = 0; i < len; i++)
            expanded += String.fromCharCode(intData[i]);
        return btoa(expanded);
    },


    makeSECItem : function(input, isEncoded) {
        if (isEncoded)
            input = atob(input);
        let outputData = new ctypes.ArrayType(ctypes.unsigned_char, input.length)();
        this.byteCompress(input, outputData);

        return new this.nss_t.SECItem(this.nss.SIBUFFER, outputData, outputData.length);
    },

    _deriveKeyFromPassphrase : function (passphrase, salt) {
        this.log("_deriveKeyFromPassphrase() called.");
        let passItem = this.makeSECItem(passphrase, false);
        let saltItem = this.makeSECItem(salt, true);

        // http://mxr.mozilla.org/seamonkey/source/security/nss/lib/pk11wrap/pk11pbe.c#1261

        // Bug 436577 prevents us from just using SEC_OID_PKCS5_PBKDF2 here
        let pbeAlg = this.algorithm;
        let cipherAlg = this.algorithm; // ignored by callee when pbeAlg != a pkcs5 mech.
        let prfAlg = this.nss.SEC_OID_HMAC_SHA1; // callee picks if SEC_OID_UNKNOWN, but only SHA1 is supported

        let keyLength  = 0;    // Callee will pick.
        let iterations = 4096; // PKCS#5 recommends at least 1000.

        let algid, slot, symKey;
        try {
            algid = this.nss.PK11_CreatePBEV2AlgorithmID(pbeAlg, cipherAlg, prfAlg,
                                                        keyLength, iterations, saltItem.address());
            if (algid.isNull())
                throw this.makeException("PK11_CreatePBEV2AlgorithmID failed", Cr.NS_ERROR_FAILURE);

            slot = this.nss.PK11_GetInternalSlot();
            if (slot.isNull())
                throw this.makeException("couldn't get internal slot", Cr.NS_ERROR_FAILURE);

            symKey = this.nss.PK11_PBEKeyGen(slot, algid, passItem.address(), false, null);
            if (symKey.isNull())
                throw this.makeException("PK11_PBEKeyGen failed", Cr.NS_ERROR_FAILURE);
        } catch (e) {
            this.log("_deriveKeyFromPassphrase: failed: " + e);
            throw e;
        } finally {
            if (algid && !algid.isNull())
                this.nss.SECOID_DestroyAlgorithmID(algid, true);
            if (slot && !slot.isNull())
                this.nss.PK11_FreeSlot(slot);
        }

        return symKey;
    },


    _wrapPrivateKey : function(privKey, passphrase, salt, iv) {
        this.log("_wrapPrivateKey() called.");
        let ivParam, pbeKey, wrappedKey;
        try {
            // Convert our passphrase to a symkey and get the IV in the form we want.
            pbeKey = this._deriveKeyFromPassphrase(passphrase, salt);

            let ivItem = this.makeSECItem(iv, true);

            // AES_128_CBC --> CKM_AES_CBC --> CKM_AES_CBC_PAD
            let wrapMech = this.nss.PK11_AlgtagToMechanism(this.algorithm);
            wrapMech = this.nss.PK11_GetPadMechanism(wrapMech);
            if (wrapMech == this.nss.CKM_INVALID_MECHANISM)
                throw this.makeException("wrapPrivKey: unknown key mech", Cr.NS_ERROR_FAILURE);

            let ivParam = this.nss.PK11_ParamFromIV(wrapMech, ivItem.address());
            if (ivParam.isNull())
                throw this.makeException("wrapPrivKey: PK11_ParamFromIV failed", Cr.NS_ERROR_FAILURE);

            // Use a buffer to hold the wrapped key. NSS says about 1200 bytes for
            // a 2048-bit RSA key, so a 4096 byte buffer should be plenty.
            let keyData = new ctypes.ArrayType(ctypes.unsigned_char, 4096)();
            wrappedKey = new this.nss_t.SECItem(this.nss.SIBUFFER, keyData, keyData.length);

            let s = this.nss.PK11_WrapPrivKey(privKey.contents.pkcs11Slot,
                                              pbeKey, privKey,
                                              wrapMech, ivParam,
                                              wrappedKey.address(), null);
            if (s)
                throw this.makeException("wrapPrivKey: PK11_WrapPrivKey failed", Cr.NS_ERROR_FAILURE);

            return this.encodeBase64(wrappedKey.data, wrappedKey.len);
        } catch (e) {
            this.log("_wrapPrivateKey: failed: " + e);
            throw e;
        } finally {
            if (ivParam && !ivParam.isNull())
                this.nss.SECITEM_FreeItem(ivParam, true);
            if (pbeKey && !pbeKey.isNull())
                this.nss.PK11_FreeSymKey(pbeKey);
        }
    }
};
