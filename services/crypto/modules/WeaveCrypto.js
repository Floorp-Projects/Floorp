/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["WeaveCrypto"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/ctypes.jsm");

/**
 * Shortcuts for some algorithm SEC OIDs.  Full list available here:
 * http://lxr.mozilla.org/seamonkey/source/security/nss/lib/util/secoidt.h
 */
const DES_EDE3_CBC = 156;
const AES_128_CBC  = 184;
const AES_192_CBC  = 186;
const AES_256_CBC  = 188;

const ALGORITHM                 = AES_256_CBC;
const KEYSIZE_AES_256           = 32;
const KEY_DERIVATION_ITERATIONS = 4096;   // PKCS#5 recommends at least 1000.
const INITIAL_BUFFER_SIZE       = 1024;

this.WeaveCrypto = function WeaveCrypto() {
    this.init();
}

WeaveCrypto.prototype = {
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

    init : function() {
        try {
            // Preferences. Add observer so we get notified of changes.
            this.prefBranch = Services.prefs.getBranch("services.sync.log.");
            this.prefBranch.addObserver("cryptoDebug", this.observer, false);
            this.observer._self = this;
            try {
              this.debug = this.prefBranch.getBoolPref("cryptoDebug");
            } catch (x) {
              this.debug = false;
            }

            this.initNSS();
            this.initAlgorithmSettings();   // Depends on NSS.
            this.initIVSECItem();
            this.initSharedInts();
            this.initBuffers(INITIAL_BUFFER_SIZE);
        } catch (e) {
            this.log("init failed: " + e);
            throw e;
        }
    },

    // Avoid allocating new temporary ints on every run of _commonCrypt.
    _commonCryptSignedOutputSize:       null,
    _commonCryptSignedOutputSizeAddr:   null,
    _commonCryptUnsignedOutputSize:     null,
    _commonCryptUnsignedOutputSizeAddr: null,

    initSharedInts: function initSharedInts() {
        let signed   = new ctypes.int();
        let unsigned = new ctypes.unsigned_int();
        this._commonCryptSignedOutputSize       = signed;
        this._commonCryptUnsignedOutputSize     = unsigned;
        this._commonCryptSignedOutputSizeAddr   = signed.address();
        this._commonCryptUnsignedOutputSizeAddr = unsigned.address();
    },

    /**
     * Set a bunch of NSS values once, at init-time. These are:
     *   - .blockSize
     *   - .mechanism
     *   - .keygenMechanism
     *   - .padMechanism
     *   - .keySize
     *
     * See also the constant ALGORITHM.
     */
    initAlgorithmSettings: function() {
        this.mechanism = this.nss.PK11_AlgtagToMechanism(ALGORITHM);
        this.blockSize = this.nss.PK11_GetBlockSize(this.mechanism, null);
        this.ivLength  = this.nss.PK11_GetIVLength(this.mechanism);
        this.keySize   = KEYSIZE_AES_256;
        this.keygenMechanism = this.nss.CKM_AES_KEY_GEN;  // Always the same!

        // Determine which (padded) PKCS#11 mechanism to use.
        // E.g., AES_256_CBC --> CKM_AES_CBC --> CKM_AES_CBC_PAD
        this.padMechanism = this.nss.PK11_GetPadMechanism(this.mechanism);
        if (this.padMechanism == this.nss.CKM_INVALID_MECHANISM)
            throw Components.Exception("invalid algorithm (can't pad)", Cr.NS_ERROR_FAILURE);
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
        let path = ctypes.libraryName("nss3");

        // XXX really want to be able to pass specific dlopen flags here.
        var nsslib;
#ifdef MOZ_NATIVE_NSS
        // Search platform-dependent library paths for system NSS.
        this.log("Trying NSS library without path");
        nsslib = ctypes.open(path);
#else
        let file = Services.dirsvc.get("GreBinD", Ci.nsILocalFile);
        file.append(path);
        this.log("Trying NSS library with path " + file.path);
        nsslib = ctypes.open(file.path);
#endif

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
        // security/nss/lib/util/seccomon.h#83
        // typedef struct SECItemStr SECItem; --> SECItemStr defined right below it
        this.nss_t.SECItem = ctypes.StructType(
            "SECItem", [{ type: this.nss_t.SECItemType },
                        { data: ctypes.unsigned_char.ptr },
                        { len : ctypes.int }]);
        // security/nss/lib/util/secoidt.h#52
        // typedef struct SECAlgorithmIDStr --> def'n right below it
        this.nss_t.SECAlgorithmID = ctypes.StructType(
            "SECAlgorithmID", [{ algorithm:  this.nss_t.SECItem },
                               { parameters: this.nss_t.SECItem }]);


        // security/nss/lib/util/pkcs11t.h
        this.nss.CKK_RSA = 0x0;
        this.nss.CKM_RSA_PKCS_KEY_PAIR_GEN = 0x0000;
        this.nss.CKM_AES_KEY_GEN           = 0x1080; 
        this.nss.CKA_ENCRYPT = 0x104;
        this.nss.CKA_DECRYPT = 0x105;

        // security/nss/lib/softoken/secmodt.h
        this.nss.PK11_ATTR_SESSION   = 0x02;
        this.nss.PK11_ATTR_PUBLIC    = 0x08;
        this.nss.PK11_ATTR_SENSITIVE = 0x40;

        // security/nss/lib/util/secoidt.h
        this.nss.SEC_OID_PKCS5_PBKDF2         = 291;
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
        // security/nss/lib/util/secitem.h#49
        // extern SECItem *SECITEM_AllocItem(PRArenaPool *arena, SECItem *item, unsigned int len);
        this.nss.SECITEM_AllocItem = nsslib.declare("SECITEM_AllocItem",
                                                    ctypes.default_abi, this.nss_t.SECItem.ptr,
                                                    this.nss_t.PLArenaPool.ptr,     // Not used.
                                                    this.nss_t.SECItem.ptr, ctypes.unsigned_int);
        // security/nss/lib/util/secitem.h#274
        // extern void SECITEM_ZfreeItem(SECItem *zap, PRBool freeit);
        this.nss.SECITEM_ZfreeItem = nsslib.declare("SECITEM_ZfreeItem",
                                                    ctypes.default_abi, ctypes.void_t,
                                                    this.nss_t.SECItem.ptr, this.nss_t.PRBool);
        // security/nss/lib/util/secitem.h#114
        // extern void SECITEM_FreeItem(SECItem *zap, PRBool freeit);
        this.nss.SECITEM_FreeItem = nsslib.declare("SECITEM_FreeItem",
                                                   ctypes.default_abi, ctypes.void_t,
                                                   this.nss_t.SECItem.ptr, this.nss_t.PRBool);
        // security/nss/lib/util/secoid.h#103
        // extern void SECOID_DestroyAlgorithmID(SECAlgorithmID *aid, PRBool freeit);
        this.nss.SECOID_DestroyAlgorithmID = nsslib.declare("SECOID_DestroyAlgorithmID",
                                                            ctypes.default_abi, ctypes.void_t,
                                                            this.nss_t.SECAlgorithmID.ptr, this.nss_t.PRBool);
    },


    _sharedInputBuffer:      null,
    _sharedInputBufferInts:  null,
    _sharedInputBufferSize:  0,
    _sharedOutputBuffer:     null,
    _sharedOutputBufferSize: 0,
    _randomByteBuffer:       null,
    _randomByteBufferAddr:   null,
    _randomByteBufferSize:   0,

    _getInputBuffer: function _getInputBuffer(size) {
      if (size > this._sharedInputBufferSize) {
        let b = new ctypes.ArrayType(ctypes.unsigned_char, size)();
        this._sharedInputBuffer     = b;
        this._sharedInputBufferInts = ctypes.cast(b, ctypes.uint8_t.array(size));
        this._sharedInputBufferSize = size;
      }
      return this._sharedInputBuffer;
    },

    _getOutputBuffer: function _getOutputBuffer(size) {
      if (size > this._sharedOutputBufferSize) {
        let b = new ctypes.ArrayType(ctypes.unsigned_char, size)();
        this._sharedOutputBuffer     = b;
        this._sharedOutputBufferSize = size;
      }
      return this._sharedOutputBuffer;
    },

    _getRandomByteBuffer: function _getRandomByteBuffer(size) {
        if (size > this._randomByteBufferSize) {
          let b = new ctypes.ArrayType(ctypes.unsigned_char, size)();
          this._randomByteBuffer     = b;
          this._randomByteBufferAddr = b.address();
          this._randomByteBufferSize = size;
        }
        return this._randomByteBuffer;
    },

    initBuffers: function initBuffers(initialSize) {
        this._getInputBuffer(initialSize);
        this._getOutputBuffer(initialSize);

        this._getRandomByteBuffer(this.ivLength);
    },

    encrypt : function(clearTextUCS2, symmetricKey, iv) {
        this.log("encrypt() called");

        // js-ctypes autoconverts to a UTF8 buffer, but also includes a null
        // at the end which we don't want. Decrement length to skip it.
        let inputBuffer = new ctypes.ArrayType(ctypes.unsigned_char)(clearTextUCS2);
        let inputBufferSize = inputBuffer.length - 1;

        // When using CBC padding, the output size is the input size rounded
        // up to the nearest block. If the input size is exactly on a block
        // boundary, the output is 1 extra block long.
        let outputBufferSize = inputBufferSize + this.blockSize;
        let outputBuffer = this._getOutputBuffer(outputBufferSize);

        outputBuffer = this._commonCrypt(inputBuffer, inputBufferSize,
                                         outputBuffer, outputBufferSize,
                                         symmetricKey, iv, this.nss.CKA_ENCRYPT);

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
        //
        // Compress a JS string (2-byte chars) into a normal C string (1-byte chars).
        let len   = inputUCS2.length;
        let input = this._getInputBuffer(len);
        this.byteCompressInts(inputUCS2, this._sharedInputBufferInts, len);

        let outputBuffer = this._commonCrypt(input, len,
                                             this._getOutputBuffer(len), len,
                                             symmetricKey, iv, this.nss.CKA_DECRYPT);

        // outputBuffer contains UTF-8 data, let js-ctypes autoconvert that to a JS string.
        // XXX Bug 573842: wrap the string from ctypes to get a new string, so
        // we don't hit bug 573841.
        return "" + outputBuffer.readString() + "";
    },

    _commonCrypt : function (input, inputLength, output, outputLength, symmetricKey, iv, operation) {
        this.log("_commonCrypt() called");
        iv = atob(iv);

        // We never want an IV longer than the block size, which is 16 bytes
        // for AES. Neither do we want one smaller; throw in that case.
        if (iv.length < this.blockSize)
            throw "IV too short; must be " + this.blockSize + " bytes.";
        if (iv.length > this.blockSize)
            iv = iv.slice(0, this.blockSize);

        // We use a single IV SECItem for the sake of efficiency. Fill it here.
        this.byteCompressInts(iv, this._ivSECItemContents, iv.length);

        let ctx, symKey, ivParam;
        try {
            ivParam = this.nss.PK11_ParamFromIV(this.padMechanism, this._ivSECItem);
            if (ivParam.isNull())
                throw Components.Exception("can't convert IV to param", Cr.NS_ERROR_FAILURE);

            symKey = this.importSymKey(symmetricKey, operation);
            ctx = this.nss.PK11_CreateContextBySymKey(this.padMechanism, operation, symKey, ivParam);
            if (ctx.isNull())
                throw Components.Exception("couldn't create context for symkey", Cr.NS_ERROR_FAILURE);

            let maxOutputSize = outputLength;
            if (this.nss.PK11_CipherOp(ctx, output, this._commonCryptSignedOutputSize.address(), maxOutputSize, input, inputLength))
                throw Components.Exception("cipher operation failed", Cr.NS_ERROR_FAILURE);

            let actualOutputSize = this._commonCryptSignedOutputSize.value;
            let finalOutput = output.addressOfElement(actualOutputSize);
            maxOutputSize -= actualOutputSize;

            // PK11_DigestFinal sure sounds like the last step for *hashing*, but it
            // just seems to be an odd name -- NSS uses this to finish the current
            // cipher operation. You'd think it would be called PK11_CipherOpFinal...
            if (this.nss.PK11_DigestFinal(ctx, finalOutput, this._commonCryptUnsignedOutputSizeAddr, maxOutputSize))
                throw Components.Exception("cipher finalize failed", Cr.NS_ERROR_FAILURE);

            actualOutputSize += this._commonCryptUnsignedOutputSize.value;
            let newOutput = ctypes.cast(output, ctypes.unsigned_char.array(actualOutputSize));
            return newOutput;
        } catch (e) {
            this.log("_commonCrypt: failed: " + e);
            throw e;
        } finally {
            if (ctx && !ctx.isNull())
                this.nss.PK11_DestroyContext(ctx, true);
            if (ivParam && !ivParam.isNull())
                this.nss.SECITEM_FreeItem(ivParam, true);

            // Note that we do not free the IV SECItem; we reuse it.
            // Neither do we free the symKey, because that's memoized.
        }
    },


    generateRandomKey : function() {
        this.log("generateRandomKey() called");
        let slot, randKey, keydata;
        try {
            slot = this.nss.PK11_GetInternalSlot();
            if (slot.isNull())
                throw Components.Exception("couldn't get internal slot", Cr.NS_ERROR_FAILURE);

            randKey = this.nss.PK11_KeyGen(slot, this.keygenMechanism, null, this.keySize, null);
            if (randKey.isNull())
                throw Components.Exception("PK11_KeyGen failed.", Cr.NS_ERROR_FAILURE);

            // Slightly odd API, this call just prepares the key value for
            // extraction, we get the actual bits from the call to PK11_GetKeyData().
            if (this.nss.PK11_ExtractKeyValue(randKey))
                throw Components.Exception("PK11_ExtractKeyValue failed.", Cr.NS_ERROR_FAILURE);

            keydata = this.nss.PK11_GetKeyData(randKey);
            if (keydata.isNull())
                throw Components.Exception("PK11_GetKeyData failed.", Cr.NS_ERROR_FAILURE);

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

    generateRandomIV : function() this.generateRandomBytes(this.ivLength),

    generateRandomBytes : function(byteCount) {
        this.log("generateRandomBytes() called");

        // Temporary buffer to hold the generated data.
        let scratch = this._getRandomByteBuffer(byteCount);
        if (this.nss.PK11_GenerateRandom(scratch, byteCount))
            throw Components.Exception("PK11_GenrateRandom failed", Cr.NS_ERROR_FAILURE);

        return this.encodeBase64(this._randomByteBufferAddr, byteCount);
    },

    //
    // PK11SymKey memoization.
    //

    // Memoize the lookup of symmetric keys. We do this by using the base64
    // string itself as a key -- the overhead of SECItem creation during the
    // initial population is negligible, so that phase is not memoized.
    _encryptionSymKeyMemo: {},
    _decryptionSymKeyMemo: {},
    importSymKey: function importSymKey(encodedKeyString, operation) {
        let memo;

        // We use two separate memos for thoroughness: operation is an input to
        // key import.
        switch (operation) {
            case this.nss.CKA_ENCRYPT:
                memo = this._encryptionSymKeyMemo;
                break;
            case this.nss.CKA_DECRYPT:
                memo = this._decryptionSymKeyMemo;
                break;
            default:
                throw "Unsupported operation in importSymKey.";
        }

        if (encodedKeyString in memo)
            return memo[encodedKeyString];

        let keyItem, slot;
        try {
            keyItem = this.makeSECItem(encodedKeyString, true);
            slot    = this.nss.PK11_GetInternalKeySlot();
            if (slot.isNull())
                throw Components.Exception("can't get internal key slot",
                                           Cr.NS_ERROR_FAILURE);

            let symKey = this.nss.PK11_ImportSymKey(slot, this.padMechanism,
                                                    this.nss.PK11_OriginUnwrap,
                                                    operation, keyItem, null);
            if (!symKey || symKey.isNull())
                throw Components.Exception("symkey import failed",
                                           Cr.NS_ERROR_FAILURE);

            return memo[encodedKeyString] = symKey;
        } finally {
            if (slot && !slot.isNull())
                this.nss.PK11_FreeSlot(slot);
            this.freeSECItem(keyItem);
        }
    },


    //
    // Utility functions
    //

    /**
     * Compress a JS string into a C uint8 array. count is the number of
     * elements in the destination array. If the array is smaller than the
     * string, the string is effectively truncated. If the string is smaller
     * than the array, the array is not 0-padded.
     */
    byteCompressInts : function byteCompressInts (jsString, intArray, count) {
        let len = jsString.length;
        let end = Math.min(len, count);
        for (let i = 0; i < end; i++)
            intArray[i] = jsString.charCodeAt(i) & 0xFF;  // convert to bytes.
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

    expandData : function expandData(data, len) {
        // Byte-expand the buffer, so we can treat it as a UCS-2 string
        // consisting of u0000 - u00FF.
        let expanded = "";
        let intData = ctypes.cast(data, ctypes.uint8_t.array(len).ptr).contents;
        for (let i = 0; i < len; i++)
            expanded += String.fromCharCode(intData[i]);
      return expanded;
    },

    encodeBase64 : function (data, len) {
        return btoa(this.expandData(data, len));
    },

    // Returns a filled SECItem *, as returned by SECITEM_AllocItem.
    //
    // Note that this must be released with freeSECItem, which will also
    // deallocate the internal buffer.
    makeSECItem : function(input, isEncoded) {
        if (isEncoded)
            input = atob(input);

        let len = input.length;
        let item = this.nss.SECITEM_AllocItem(null, null, len);
        if (item.isNull())
            throw "SECITEM_AllocItem failed.";

        let ptr  = ctypes.cast(item.contents.data,
                               ctypes.unsigned_char.array(len).ptr);
        let dest = ctypes.cast(ptr.contents, ctypes.uint8_t.array(len));
        this.byteCompressInts(input, dest, len);
        return item;
    },

    freeSECItem : function(zap) {
        if (zap && !zap.isNull())
            this.nss.SECITEM_ZfreeItem(zap, true);
    },

    // We only ever handle one IV at a time, and they're always different.
    // Consequently, we maintain a single SECItem, and a handy pointer into its
    // contents to avoid repetitive and expensive casts.
    _ivSECItem: null,
    _ivSECItemContents: null,

    initIVSECItem: function initIVSECItem() {
        if (this._ivSECItem) {
            this._ivSECItemContents = null;
            this.freeSECItem(this._ivSECItem);
        }

        let item = this.nss.SECITEM_AllocItem(null, null, this.blockSize);
        if (item.isNull())
            throw "SECITEM_AllocItem failed.";

        let ptr = ctypes.cast(item.contents.data,
                              ctypes.unsigned_char.array(this.blockSize).ptr);
        let contents = ctypes.cast(ptr.contents,
                                   ctypes.uint8_t.array(this.blockSize));
        this._ivSECItem = item;
        this._ivSECItemContents = contents;
    },

    /**
     * Returns the expanded data string for the derived key.
     */
    deriveKeyFromPassphrase : function deriveKeyFromPassphrase(passphrase, salt, keyLength) {
        this.log("deriveKeyFromPassphrase() called.");
        let passItem = this.makeSECItem(passphrase, false);
        let saltItem = this.makeSECItem(salt, true);

        let pbeAlg    = ALGORITHM;
        let cipherAlg = ALGORITHM;   // Ignored by callee when pbeAlg != a pkcs5 mech.

        // Callee picks if SEC_OID_UNKNOWN, but only SHA1 is supported.
        let prfAlg    = this.nss.SEC_OID_HMAC_SHA1;

        keyLength  = keyLength || 0;    // 0 = Callee will pick.
        let iterations = KEY_DERIVATION_ITERATIONS;

        let algid, slot, symKey, keyData;
        try {
            algid = this.nss.PK11_CreatePBEV2AlgorithmID(pbeAlg, cipherAlg, prfAlg,
                                                         keyLength, iterations,
                                                         saltItem);
            if (algid.isNull())
                throw Components.Exception("PK11_CreatePBEV2AlgorithmID failed", Cr.NS_ERROR_FAILURE);

            slot = this.nss.PK11_GetInternalSlot();
            if (slot.isNull())
                throw Components.Exception("couldn't get internal slot", Cr.NS_ERROR_FAILURE);

            symKey = this.nss.PK11_PBEKeyGen(slot, algid, passItem, false, null);
            if (symKey.isNull())
                throw Components.Exception("PK11_PBEKeyGen failed", Cr.NS_ERROR_FAILURE);

            // Take the PK11SymKeyStr, returning the extracted key data.
            if (this.nss.PK11_ExtractKeyValue(symKey)) {
                throw this.makeException("PK11_ExtractKeyValue failed.", Cr.NS_ERROR_FAILURE);
            }

            keyData = this.nss.PK11_GetKeyData(symKey);

            if (keyData.isNull())
                throw Components.Exception("PK11_GetKeyData failed", Cr.NS_ERROR_FAILURE);

            // This copies the key contents into a JS string, so we don't leak.
            // The `finally` block below will clean up.
            return this.expandData(keyData.contents.data, keyData.contents.len);

        } catch (e) {
            this.log("deriveKeyFromPassphrase: failed: " + e);
            throw e;
        } finally {
            if (algid && !algid.isNull())
                this.nss.SECOID_DestroyAlgorithmID(algid, true);
            if (slot && !slot.isNull())
                this.nss.PK11_FreeSlot(slot);
            if (symKey && !symKey.isNull())
                this.nss.PK11_FreeSymKey(symKey);

            this.freeSECItem(passItem);
            this.freeSECItem(saltItem);
        }
    },
};
