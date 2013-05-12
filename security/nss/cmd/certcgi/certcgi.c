/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Cert-O-Matic CGI */


#include "nspr.h"
#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"

#include "pk11func.h"
#include "cert.h"
#include "cryptohi.h"
#include "secoid.h"
#include "secder.h"
#include "genname.h"
#include "xconst.h"
#include "secutil.h"
#include "pk11pqg.h"
#include "certxutl.h"
#include "nss.h"


/* #define TEST           1 */
/* #define FILEOUT        1 */
/* #define OFFLINE        1 */
#define START_FIELDS   100
#define PREFIX_LEN     6
#define SERIAL_FILE    "../serial"
#define DB_DIRECTORY   ".."

static char *progName;

typedef struct PairStr Pair;

struct PairStr {
    char *name;
    char *data;
};


char prefix[PREFIX_LEN];


const SEC_ASN1Template CERTIA5TypeTemplate[] = {
    { SEC_ASN1_IA5_STRING }
};



SECKEYPrivateKey *privkeys[9] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL};


#ifdef notdef
const SEC_ASN1Template CERT_GeneralNameTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, SEC_AnyTemplate }
};
#endif


static void
error_out(char  *error_string)
{
    printf("Content-type: text/plain\n\n");
    printf("%s", error_string);
    fflush(stderr);
    fflush(stdout);
    exit(1);
}

static void
error_allocate(void)
{
    error_out("ERROR: Unable to allocate memory");
}


static char *
make_copy_string(char  *read_pos, 
		 int   length, 
		 char  sentinal_value)
    /* copys string from to a new string it creates and 
       returns a pointer to the new string */
{
    int                remaining = length;
    char               *write_pos;
    char               *new;

    new = write_pos = (char *) PORT_Alloc (length);
    if (new == NULL) {
	error_allocate();
    }
    while (*read_pos != sentinal_value) {
	if (remaining == 1) {
	    remaining += length;
	    length = length * 2;
	    new = PORT_Realloc(new,length);
	    if (new == NULL) {
		error_allocate();
	    }
	    write_pos = new + length - remaining;
	}
	*write_pos = *read_pos;
	++write_pos;
	++read_pos;
	remaining = remaining - 1;
    }
    *write_pos = '\0';
    return new;
}


static SECStatus
clean_input(Pair *data)
    /* converts the non-alphanumeric characters in a form post 
       from hex codes back to characters */
{
    int           length;
    int           hi_digit;
    int           low_digit;
    char          character;
    char          *begin_pos;
    char          *read_pos;
    char          *write_pos;
    PRBool        name = PR_TRUE;

    begin_pos = data->name;
    while (begin_pos != NULL) {
	length = strlen(begin_pos);
	read_pos = write_pos = begin_pos;
	while ((read_pos - begin_pos) < length) {
	    if (*read_pos == '+') {
		*read_pos = ' ';
	    }
	    if (*read_pos == '%') {
		hi_digit = *(read_pos + 1);
		low_digit = *(read_pos +2);
		read_pos += 3;
		if (isdigit(hi_digit)){
		    hi_digit = hi_digit - '0';
		} else {
		    hi_digit = toupper(hi_digit);
		    if (isxdigit(hi_digit)) {
			hi_digit = (hi_digit - 'A') + 10;
		    } else {
			error_out("ERROR: Form data incorrectly formated");
		    }
		}
		if (isdigit(low_digit)){
		    low_digit = low_digit - '0';
		} else {
		    low_digit = toupper(low_digit);
		    if ((low_digit >='A') && (low_digit <= 'F')) {
			low_digit = (low_digit - 'A') + 10;
		    } else {
			error_out("ERROR: Form data incorrectly formated");
		    }
		}
		character = (hi_digit << 4) | low_digit;
		if (character != 10) {
		    *write_pos = character;
		    ++write_pos;
		}
	    } else {
		*write_pos = *read_pos;
		++write_pos;
		++read_pos;
	    }
	}
	*write_pos = '\0';
	if (name == PR_TRUE) {
	    begin_pos = data->data;
	    name = PR_FALSE;
	} else {
	    data++;
	    begin_pos = data->name;
	    name = PR_TRUE;
	}
    }
    return SECSuccess;
}

static char *
make_name(char  *new_data)
    /* gets the next field name in the input string and returns
       a pointer to a string containing a copy of it */
{
    int         length = 20;
    char        *name;

    name = make_copy_string(new_data, length, '=');
    return name;
}
	
static char *
make_data(char  *new_data)
    /* gets the data for the next field in the input string 
       and returns a pointer to a string containing it */
{
    int         length = 100;
    char        *data;
    char        *read_pos;

    read_pos = new_data;
    while (*(read_pos - 1) != '=') {
	++read_pos;
    }
    data = make_copy_string(read_pos, length, '&');
    return data;
}


static Pair
make_pair(char  *new_data)
    /* makes a pair name/data pair from the input string */
{
    Pair        temp;

    temp.name = make_name(new_data);
    temp.data = make_data(new_data);
    return temp;
}



static Pair *
make_datastruct(char  *data, int len)
    /* parses the input from the form post into a data 
       structure of field name/data pairs */
{
    Pair              *datastruct;
    Pair              *current;
    char              *curr_pos;
    int               fields = START_FIELDS;
    int               remaining = START_FIELDS;

    curr_pos = data;
    datastruct = current = (Pair *) PORT_Alloc(fields * sizeof(Pair));
    if (datastruct == NULL) {
	error_allocate();
    }
    while (curr_pos - data < len) {
	if (remaining == 1) {
	    remaining += fields;
	    fields = fields * 2;
	    datastruct = (Pair *) PORT_Realloc
		(datastruct, fields * sizeof(Pair));
	    if (datastruct == NULL) {
		error_allocate();
	    }
	    current = datastruct + (fields - remaining);
	}
	*current = make_pair(curr_pos);
	while (*curr_pos != '&') {
	    ++curr_pos;
	}
	++curr_pos;
	++current;
	remaining = remaining - 1;
    }
    current->name = NULL;
    return datastruct;
}

static char *
return_name(Pair  *data_struct,
	    int   n)
    /* returns a pointer to the name of the nth 
       (starting from 0) item in the data structure */
{
    char          *name;

    if ((data_struct + n)->name != NULL) {
	name = (data_struct + n)->name;
	return name;
    } else {
	return NULL;
    }
}

static char *
return_data(Pair  *data_struct,int n)
    /* returns a pointer to the data of the nth (starting from 0) 
       itme in the data structure */
{
    char          *data;

    data = (data_struct + n)->data;
    return data;
}


static char *
add_prefix(char  *field_name)
{
    extern char  prefix[PREFIX_LEN];
    int          i = 0;
    char         *rv;
    char         *write;

    rv = write = PORT_Alloc(PORT_Strlen(prefix) + PORT_Strlen(field_name) + 1);
    for(i = 0; i < PORT_Strlen(prefix); i++) {
	*write = prefix[i];
	write++;
    }
    *write = '\0';
    rv = PORT_Strcat(rv,field_name);
    return rv;
}


static char *
find_field(Pair    *data, 
	   char    *field_name, 
	   PRBool  add_pre)
    /* returns a pointer to the data of the first pair 
       thats name matches the string it is passed */
{
    int            i = 0;
    char           *retrieved;
    int            found = 0;

    if (add_pre) {
	field_name = add_prefix(field_name);
    }
    while(return_name(data, i) != NULL) {
	if (PORT_Strcmp(return_name(data, i), field_name) == 0) {
	    retrieved = return_data(data, i);
	    found = 1;
	    break;
	}
	i++;
    }
    if (!found) {
	retrieved = NULL;
    }
    return retrieved;
}

static PRBool
find_field_bool(Pair    *data, 
		char    *fieldname, 
		PRBool  add_pre)
{
    char                *rv;

    rv = find_field(data, fieldname, add_pre);
	
    if  ((rv != NULL) && (PORT_Strcmp(rv, "true")) == 0) {
	return PR_TRUE;
    } else {
	return PR_FALSE;
    }
}

static char *
update_data_by_name(Pair  *data, 
		    char  *field_name,
                    char  *new_data)
    /* replaces the data in the data structure associated with 
       a name with new data, returns null if not found */
{
    int                   i = 0;
    int                   found = 0;
    int                   length = 100;
    char                  *new;

    while (return_name(data, i) != NULL) {
	if (PORT_Strcmp(return_name(data, i), field_name) == 0) {
	    new = make_copy_string( new_data, length, '\0');
	    PORT_Free(return_data(data, i));
	    found = 1;
	    (*(data + i)).data = new;
	    break;
	}
	i++;
    }
    if (!found) {
	new = NULL;
    }
    return new;
}

static char *
update_data_by_index(Pair  *data, 
		     int   n, 
		     char  *new_data)
    /* replaces the data of a particular index in the data structure */
{
    int                    length = 100;
    char                   *new;

    new = make_copy_string(new_data, length, '\0');
    PORT_Free(return_data(data, n));
    (*(data + n)).data = new;
    return new;
}


static Pair *
add_field(Pair   *data, 
	  char*  field_name, 
	  char*  field_data)
    /* adds a new name/data pair to the data structure */
{
    int          i = 0;
    int          j;
    int          name_length = 100;
    int          data_length = 100;

    while(return_name(data, i) != NULL) {
	i++;
    }
    j = START_FIELDS;
    while ( j < (i + 1) ) {
	j = j * 2;
    }
    if (j == (i + 1)) {
	data = (Pair *) PORT_Realloc(data, (j * 2) * sizeof(Pair));
	if (data == NULL) {
	    error_allocate();
	}
    }
    (*(data + i)).name = make_copy_string(field_name, name_length, '\0');
    (*(data + i)).data = make_copy_string(field_data, data_length, '\0');
    (data + i + 1)->name = NULL;
    return data;
}


static CERTCertificateRequest *
makeCertReq(Pair             *form_data,
	    int              which_priv_key)
    /* makes and encodes a certrequest */
{

    PK11SlotInfo             *slot;
    CERTCertificateRequest   *certReq = NULL;
    CERTSubjectPublicKeyInfo *spki;
    SECKEYPrivateKey         *privkey = NULL;
    SECKEYPublicKey          *pubkey = NULL;
    CERTName                 *name;
    char                     *key;
    extern SECKEYPrivateKey  *privkeys[9];
    int                      keySizeInBits;
    char                     *challenge = "foo";
    SECStatus                rv = SECSuccess;
    PQGParams                *pqgParams = NULL;
    PQGVerify                *pqgVfy = NULL;

    name = CERT_AsciiToName(find_field(form_data, "subject", PR_TRUE));
    if (name == NULL) {
	error_out("ERROR: Unable to create Subject Name");
    }
    key = find_field(form_data, "key", PR_TRUE);
    if (key == NULL) {
	switch (*find_field(form_data, "keysize", PR_TRUE)) {
	  case '0':
	    keySizeInBits = 2048;
	    break;
	  case '1':
	    keySizeInBits = 1024;
	    break;
	  case '2':
	    keySizeInBits = 512;
	    break;
	  default:
	    error_out("ERROR: Unsupported Key length selected");
	}
	if (find_field_bool(form_data, "keyType-dsa", PR_TRUE)) {
	    rv = PK11_PQG_ParamGen(keySizeInBits, &pqgParams, &pqgVfy);
	    if (rv != SECSuccess) {
		error_out("ERROR: Unable to generate PQG parameters");
	    }
	    slot = PK11_GetBestSlot(CKM_DSA_KEY_PAIR_GEN, NULL);
	    privkey = PK11_GenerateKeyPair(slot, CKM_DSA_KEY_PAIR_GEN, 
					   pqgParams,&pubkey, PR_FALSE, 
					   PR_TRUE, NULL);
	} else {
	    privkey = SECKEY_CreateRSAPrivateKey(keySizeInBits, &pubkey, NULL);
	}
	privkeys[which_priv_key] = privkey;
	spki = SECKEY_CreateSubjectPublicKeyInfo(pubkey);
    } else {
	spki = SECKEY_ConvertAndDecodePublicKeyAndChallenge(key, challenge, 
							    NULL);
	if (spki == NULL) {
	    error_out("ERROR: Unable to decode Public Key and Challenge String");
	}
    }
    certReq = CERT_CreateCertificateRequest(name, spki, NULL);
    if (certReq == NULL) {
	error_out("ERROR: Unable to create Certificate Request");
    }
    if (pubkey != NULL) {
	SECKEY_DestroyPublicKey(pubkey);
    }
    if (spki != NULL) {
	SECKEY_DestroySubjectPublicKeyInfo(spki);
    }
    if (pqgParams != NULL) {
	PK11_PQG_DestroyParams(pqgParams);
    }
    if (pqgVfy != NULL) {
	PK11_PQG_DestroyVerify(pqgVfy);
    }
    return certReq;
}



static CERTCertificate *
MakeV1Cert(CERTCertDBHandle        *handle, 
	   CERTCertificateRequest  *req,
	   char                    *issuerNameStr, 
	   PRBool                  selfsign, 
	   int                     serialNumber,
	   int                     warpmonths,
	   Pair                    *data)
{
    CERTCertificate                 *issuerCert = NULL;
    CERTValidity                    *validity;
    CERTCertificate                 *cert = NULL;
    PRExplodedTime                  printableTime;
    PRTime                          now, 
	                            after;
    SECStatus rv;
   
    

    if ( !selfsign ) {
	issuerCert = CERT_FindCertByNameString(handle, issuerNameStr);
	if (!issuerCert) {
	    error_out("ERROR: Could not find issuer's certificate");
	    return NULL;
	}
    }
    if (find_field_bool(data, "manValidity", PR_TRUE)) {
	rv = DER_AsciiToTime(&now, find_field(data, "notBefore", PR_TRUE));
    } else {
	now = PR_Now();
    }
    PR_ExplodeTime (now, PR_GMTParameters, &printableTime);
    if ( warpmonths ) {
	printableTime.tm_month += warpmonths;
	now = PR_ImplodeTime (&printableTime);
	PR_ExplodeTime (now, PR_GMTParameters, &printableTime);
    }
    if (find_field_bool(data, "manValidity", PR_TRUE)) {
	rv = DER_AsciiToTime(&after, find_field(data, "notAfter", PR_TRUE));
	PR_ExplodeTime (after, PR_GMTParameters, &printableTime);
    } else {
	printableTime.tm_month += 3;
	after = PR_ImplodeTime (&printableTime);
    }
    /* note that the time is now in micro-second unit */
    validity = CERT_CreateValidity (now, after);

    if ( selfsign ) {
	cert = CERT_CreateCertificate
	    (serialNumber,&(req->subject), validity, req);
    } else {
	cert = CERT_CreateCertificate
	    (serialNumber,&(issuerCert->subject), validity, req);
    }
    
    CERT_DestroyValidity(validity);
    if ( issuerCert ) {
	CERT_DestroyCertificate (issuerCert);
    }
    return(cert);
}

static int
get_serial_number(Pair  *data)
{
    int                 serial = 0;
    int                 error;
    char                *filename = SERIAL_FILE;
    char                *SN;
    FILE                *serialFile;


    if (find_field_bool(data, "serial-auto", PR_TRUE)) {
	serialFile = fopen(filename, "r");
	if (serialFile != NULL) {
	    fread(&serial, sizeof(int), 1, serialFile);
	    if (ferror(serialFile) != 0) {
		error_out("Error: Unable to read serial number file");
	    }
	    if (serial == 4294967295) {
		serial = 21;
	    }
	    fclose(serialFile);
	    ++serial;
	    serialFile = fopen(filename,"w");
	    if (serialFile == NULL) {
	        error_out("ERROR: Unable to open serial number file for writing");
	    }
	    fwrite(&serial, sizeof(int), 1, serialFile);
	    if (ferror(serialFile) != 0) {
		error_out("Error: Unable to write to serial number file");
	    }
	} else {
	    fclose(serialFile);
	    serialFile = fopen(filename,"w");
	    if (serialFile == NULL) {
		error_out("ERROR: Unable to open serial number file");
	    }
	    serial = 21;
	    fwrite(&serial, sizeof(int), 1, serialFile);
	    if (ferror(serialFile) != 0) {
		error_out("Error: Unable to write to serial number file");
	    }
	    error = ferror(serialFile);
	    if (error != 0) {
		error_out("ERROR: Unable to write to serial file");
	    }
	}
	fclose(serialFile);
    } else {
	SN = find_field(data, "serial_value", PR_TRUE);
	while (*SN != '\0') {
	    serial = serial * 16;
	    if ((*SN >= 'A') && (*SN <='F')) {
		serial += *SN - 'A' + 10; 
	    } else {
		if ((*SN >= 'a') && (*SN <='f')) {
		    serial += *SN - 'a' + 10;
		} else {
		    serial += *SN - '0';
		}
	    }
	    ++SN;
	}
    }
    return serial;
}
	


typedef SECStatus (* EXTEN_VALUE_ENCODER)
		(PLArenaPool *extHandle, void *value, SECItem *encodedValue);

static SECStatus 
EncodeAndAddExtensionValue(
	PLArenaPool          *arena,
	void                 *extHandle, 
	void                 *value, 
	PRBool 		     criticality,
	int 		     extenType, 
	EXTEN_VALUE_ENCODER  EncodeValueFn)
{
    SECItem                  encodedValue;
    SECStatus                rv;
	

    encodedValue.data = NULL;
    encodedValue.len = 0;
    rv = (*EncodeValueFn)(arena, value, &encodedValue);
    if (rv != SECSuccess) {
	error_out("ERROR: Unable to encode extension value");
    }
    rv = CERT_AddExtension
	(extHandle, extenType, &encodedValue, criticality, PR_TRUE);
    return (rv);
}



static SECStatus 
AddKeyUsage (void  *extHandle, 
	     Pair  *data)
{
    SECItem        bitStringValue;
    unsigned char  keyUsage = 0x0;

    if (find_field_bool(data,"keyUsage-digitalSignature", PR_TRUE)){
	keyUsage |= (0x80 >> 0);
    }
    if (find_field_bool(data,"keyUsage-nonRepudiation", PR_TRUE)){
	keyUsage |= (0x80 >> 1);
    }
    if (find_field_bool(data,"keyUsage-keyEncipherment", PR_TRUE)){
	keyUsage |= (0x80 >> 2);
    }
    if (find_field_bool(data,"keyUsage-dataEncipherment", PR_TRUE)){
	keyUsage |= (0x80 >> 3);
    }
    if (find_field_bool(data,"keyUsage-keyAgreement", PR_TRUE)){
	keyUsage |= (0x80 >> 4);
    }
    if (find_field_bool(data,"keyUsage-keyCertSign", PR_TRUE)) {
	keyUsage |= (0x80 >> 5);
    }
    if (find_field_bool(data,"keyUsage-cRLSign", PR_TRUE)) {
	keyUsage |= (0x80 >> 6);
    }

    bitStringValue.data = &keyUsage;
    bitStringValue.len = 1;

    return (CERT_EncodeAndAddBitStrExtension
	    (extHandle, SEC_OID_X509_KEY_USAGE, &bitStringValue,
	     (find_field_bool(data, "keyUsage-crit", PR_TRUE))));

}

static CERTOidSequence *
CreateOidSequence(void)
{
  CERTOidSequence *rv = (CERTOidSequence *)NULL;
  PLArenaPool *arena = (PLArenaPool *)NULL;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if( (PLArenaPool *)NULL == arena ) {
    goto loser;
  }

  rv = (CERTOidSequence *)PORT_ArenaZAlloc(arena, sizeof(CERTOidSequence));
  if( (CERTOidSequence *)NULL == rv ) {
    goto loser;
  }

  rv->oids = (SECItem **)PORT_ArenaZAlloc(arena, sizeof(SECItem *));
  if( (SECItem **)NULL == rv->oids ) {
    goto loser;
  }

  rv->arena = arena;
  return rv;

 loser:
  if( (PLArenaPool *)NULL != arena ) {
    PORT_FreeArena(arena, PR_FALSE);
  }

  return (CERTOidSequence *)NULL;
}

static SECStatus
AddOidToSequence(CERTOidSequence *os, SECOidTag oidTag)
{
  SECItem **oids;
  PRUint32 count = 0;
  SECOidData *od;

  od = SECOID_FindOIDByTag(oidTag);
  if( (SECOidData *)NULL == od ) {
    return SECFailure;
  }

  for( oids = os->oids; (SECItem *)NULL != *oids; oids++ ) {
    count++;
  }

  /* ArenaZRealloc */

  {
    PRUint32 i;

    oids = (SECItem **)PORT_ArenaZAlloc(os->arena, sizeof(SECItem *) * (count+2));
    if( (SECItem **)NULL == oids ) {
      return SECFailure;
    }
    
    for( i = 0; i < count; i++ ) {
      oids[i] = os->oids[i];
    }

    /* ArenaZFree(os->oids); */
  }

  os->oids = oids;
  os->oids[count] = &od->oid;

  return SECSuccess;
}

static SECItem *
EncodeOidSequence(CERTOidSequence *os)
{
  SECItem *rv;
  extern const SEC_ASN1Template CERT_OidSeqTemplate[];

  rv = (SECItem *)PORT_ArenaZAlloc(os->arena, sizeof(SECItem));
  if( (SECItem *)NULL == rv ) {
    goto loser;
  }

  if( !SEC_ASN1EncodeItem(os->arena, rv, os, CERT_OidSeqTemplate) ) {
    goto loser;
  }

  return rv;

 loser:
  return (SECItem *)NULL;
}

static SECStatus
AddExtKeyUsage(void *extHandle, Pair *data)
{
  SECStatus rv;
  CERTOidSequence *os;
  SECItem *value;
  PRBool crit;

  os = CreateOidSequence();
  if( (CERTOidSequence *)NULL == os ) {
    return SECFailure;
  }

  if( find_field_bool(data, "extKeyUsage-serverAuth", PR_TRUE) ) {
    rv = AddOidToSequence(os, SEC_OID_EXT_KEY_USAGE_SERVER_AUTH);
    if( SECSuccess != rv ) goto loser;
  }

  if( find_field_bool(data, "extKeyUsage-msTrustListSign", PR_TRUE) ) {
    rv = AddOidToSequence(os, SEC_OID_MS_EXT_KEY_USAGE_CTL_SIGNING);
    if( SECSuccess != rv ) goto loser;
  }

  if( find_field_bool(data, "extKeyUsage-clientAuth", PR_TRUE) ) {
    rv = AddOidToSequence(os, SEC_OID_EXT_KEY_USAGE_CLIENT_AUTH);
    if( SECSuccess != rv ) goto loser;
  }

  if( find_field_bool(data, "extKeyUsage-codeSign", PR_TRUE) ) {
    rv = AddOidToSequence(os, SEC_OID_EXT_KEY_USAGE_CODE_SIGN);
    if( SECSuccess != rv ) goto loser;
  }

  if( find_field_bool(data, "extKeyUsage-emailProtect", PR_TRUE) ) {
    rv = AddOidToSequence(os, SEC_OID_EXT_KEY_USAGE_EMAIL_PROTECT);
    if( SECSuccess != rv ) goto loser;
  }

  if( find_field_bool(data, "extKeyUsage-timeStamp", PR_TRUE) ) {
    rv = AddOidToSequence(os, SEC_OID_EXT_KEY_USAGE_TIME_STAMP);
    if( SECSuccess != rv ) goto loser;
  }

  if( find_field_bool(data, "extKeyUsage-ocspResponder", PR_TRUE) ) {
    rv = AddOidToSequence(os, SEC_OID_OCSP_RESPONDER);
    if( SECSuccess != rv ) goto loser;
  }

  if( find_field_bool(data, "extKeyUsage-NS-govtApproved", PR_TRUE) ) {
    rv = AddOidToSequence(os, SEC_OID_NS_KEY_USAGE_GOVT_APPROVED);
    if( SECSuccess != rv ) goto loser;
  }

  value = EncodeOidSequence(os);

  crit = find_field_bool(data, "extKeyUsage-crit", PR_TRUE);

  rv = CERT_AddExtension(extHandle, SEC_OID_X509_EXT_KEY_USAGE, value,
                         crit, PR_TRUE);
  /*FALLTHROUGH*/
 loser:
  CERT_DestroyOidSequence(os);
  return rv;
}

static SECStatus
AddSubKeyID(void             *extHandle, 
	    Pair             *data, 
	    CERTCertificate  *subjectCert)
{
    SECItem                  encodedValue;
    SECStatus                rv;
    char                     *read;
    char                     *write;
    char                     *first;
    char                     character;
    int                      high_digit = 0,
	                     low_digit = 0;
    int                      len;
    PRBool                   odd = PR_FALSE;


    encodedValue.data = NULL;
    encodedValue.len = 0;
    first = read = write = find_field(data,"subjectKeyIdentifier-text", 
				      PR_TRUE);
    len = PORT_Strlen(first);
    odd = ((len % 2) != 0 ) ? PR_TRUE : PR_FALSE;
    if (find_field_bool(data, "subjectKeyIdentifier-radio-hex", PR_TRUE)) {
	if (odd) {
	    error_out("ERROR: Improperly formated subject key identifier, hex values must be expressed as an octet string");
	}
	while (*read != '\0') {
	    if (!isxdigit(*read)) {
		error_out("ERROR: Improperly formated subject key identifier");
	    }
	    *read = toupper(*read);
	    if ((*read >= 'A') && (*read <= 'F')) {
		high_digit = *read - 'A' + 10;
	    }  else {
		high_digit = *read - '0';
	    }
	    ++read;
	    if (!isxdigit(*read)) {
		error_out("ERROR: Improperly formated subject key identifier");
	    }
	    *read = toupper(*read);
	    if ((*read >= 'A') && (*read <= 'F')) {
		low_digit = *(read) - 'A' + 10;
	    } else {
		low_digit = *(read) - '0';
	    }
	    character = (high_digit << 4) | low_digit;
	    *write = character;
	    ++write;
	    ++read;
	}
	*write = '\0';
	len = write - first;
    }
    subjectCert->subjectKeyID.data = (unsigned char *) find_field
	(data,"subjectKeyIdentifier-text", PR_TRUE);
    subjectCert->subjectKeyID.len = len;
    rv = CERT_EncodeSubjectKeyID
	(NULL, &subjectCert->subjectKeyID, &encodedValue);
    if (rv) {
	return (rv);
    }
    return (CERT_AddExtension(extHandle,  SEC_OID_X509_SUBJECT_KEY_ID, 
			      &encodedValue, PR_FALSE, PR_TRUE));
}


static SECStatus 
AddAuthKeyID (void              *extHandle,
	      Pair              *data, 
	      char              *issuerNameStr, 
	      CERTCertDBHandle  *handle)
{
    CERTAuthKeyID               *authKeyID = NULL;    
    PLArenaPool                 *arena = NULL;
    SECStatus                   rv = SECSuccess;
    CERTCertificate             *issuerCert = NULL;
    CERTGeneralName             *genNames;
    CERTName                    *directoryName = NULL;


    issuerCert = CERT_FindCertByNameString(handle, issuerNameStr);
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( !arena ) {
	error_allocate();
    }

    authKeyID = PORT_ArenaZAlloc (arena, sizeof (CERTAuthKeyID));
    if (authKeyID == NULL) {
	error_allocate();
    }
    if (find_field_bool(data, "authorityKeyIdentifier-radio-keyIdentifier", 
			PR_TRUE)) {
	authKeyID->keyID.data = PORT_ArenaAlloc (arena, PORT_Strlen 
				       ((char *)issuerCert->subjectKeyID.data));
	if (authKeyID->keyID.data == NULL) {
	    error_allocate();
	}
	PORT_Memcpy (authKeyID->keyID.data, issuerCert->subjectKeyID.data, 
		     authKeyID->keyID.len = 
		        PORT_Strlen((char *)issuerCert->subjectKeyID.data));
    } else {
	
	PORT_Assert (arena);
	genNames = (CERTGeneralName *) PORT_ArenaZAlloc (arena, (sizeof(CERTGeneralName)));
	if (genNames == NULL){
	    error_allocate();
	}
	genNames->l.next = genNames->l.prev = &(genNames->l);
	genNames->type = certDirectoryName;

	directoryName = CERT_AsciiToName(issuerCert->subjectName);
	if (!directoryName) {
	    error_out("ERROR: Unable to create Directory Name");
	}
	rv = CERT_CopyName (arena, &genNames->name.directoryName, 
			    directoryName);
        CERT_DestroyName (directoryName);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to copy Directory Name");
	}
	authKeyID->authCertIssuer = genNames;
	if (authKeyID->authCertIssuer == NULL && SECFailure == 
	                                            PORT_GetError ()) {
	    error_out("ERROR: Unable to get Issuer General Name for Authority Key ID Extension");
	}
	authKeyID->authCertSerialNumber = issuerCert->serialNumber;
    }
    rv = EncodeAndAddExtensionValue(arena, extHandle, authKeyID, PR_FALSE, 
				    SEC_OID_X509_AUTH_KEY_ID, 
				    (EXTEN_VALUE_ENCODER)
				    CERT_EncodeAuthKeyID);
    if (arena) {
	PORT_FreeArena (arena, PR_FALSE);
    }
    return (rv);
}


static SECStatus 
AddPrivKeyUsagePeriod(void             *extHandle, 
		      Pair             *data, 
		      CERTCertificate  *cert)
{
    char *notBeforeStr;
    char *notAfterStr;
    PLArenaPool *arena = NULL;
    SECStatus rv = SECSuccess;
    CERTPrivKeyUsagePeriod *pkup;


    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( !arena ) {
	error_allocate();
    }
    pkup = PORT_ArenaZNew (arena, CERTPrivKeyUsagePeriod);
    if (pkup == NULL) {
	error_allocate();
    }
    notBeforeStr = (char *) PORT_Alloc(16 );
    notAfterStr = (char *) PORT_Alloc(16 );
    *notBeforeStr = '\0';
    *notAfterStr = '\0';
    pkup->arena = arena;
    pkup->notBefore.len = 0;
    pkup->notBefore.data = NULL;
    pkup->notAfter.len = 0;
    pkup->notAfter.data = NULL;
    if (find_field_bool(data, "privKeyUsagePeriod-radio-notBefore", PR_TRUE) ||
	find_field_bool(data, "privKeyUsagePeriod-radio-both", PR_TRUE)) {
	pkup->notBefore.len = 15;
	pkup->notBefore.data = (unsigned char *)notBeforeStr;
	if (find_field_bool(data, "privKeyUsagePeriod-notBefore-radio-manual", 
			    PR_TRUE)) {
	    PORT_Strcat(notBeforeStr,find_field(data,
					   "privKeyUsagePeriod-notBefore-year",
					   PR_TRUE));
	    PORT_Strcat(notBeforeStr,find_field(data,
					   "privKeyUsagePeriod-notBefore-month",
					   PR_TRUE));
	    PORT_Strcat(notBeforeStr,find_field(data,
					   "privKeyUsagePeriod-notBefore-day",
					   PR_TRUE));
	    PORT_Strcat(notBeforeStr,find_field(data,
					   "privKeyUsagePeriod-notBefore-hour",
					   PR_TRUE));
	    PORT_Strcat(notBeforeStr,find_field(data,
					  "privKeyUsagePeriod-notBefore-minute",
					   PR_TRUE));
	    PORT_Strcat(notBeforeStr,find_field(data,
					  "privKeyUsagePeriod-notBefore-second",
					   PR_TRUE));
	    if ((*(notBeforeStr + 14) != '\0') ||
		(!isdigit(*(notBeforeStr + 13))) ||
		(*(notBeforeStr + 12) >= '5' && *(notBeforeStr + 12) <= '0') ||
		(!isdigit(*(notBeforeStr + 11))) ||
		(*(notBeforeStr + 10) >= '5' && *(notBeforeStr + 10) <= '0') ||
		(!isdigit(*(notBeforeStr + 9))) ||
		(*(notBeforeStr + 8) >= '2' && *(notBeforeStr + 8) <= '0') ||
		(!isdigit(*(notBeforeStr + 7))) ||
		(*(notBeforeStr + 6) >= '3' && *(notBeforeStr + 6) <= '0') ||
		(!isdigit(*(notBeforeStr + 5))) ||
		(*(notBeforeStr + 4) >= '1' && *(notBeforeStr + 4) <= '0') ||
		(!isdigit(*(notBeforeStr + 3))) ||
		(!isdigit(*(notBeforeStr + 2))) ||
		(!isdigit(*(notBeforeStr + 1))) ||
		(!isdigit(*(notBeforeStr + 0))) ||
		(*(notBeforeStr + 8) == '2' && *(notBeforeStr + 9) >= '4') ||
		(*(notBeforeStr + 6) == '3' && *(notBeforeStr + 7) >= '1') ||
		(*(notBeforeStr + 4) == '1' && *(notBeforeStr + 5) >= '2')) {
		error_out("ERROR: Improperly formated private key usage period");
	    }
	    *(notBeforeStr + 14) = 'Z';
	    *(notBeforeStr + 15) = '\0';
	} else {
	    if ((*(cert->validity.notBefore.data) > '5') || 
		((*(cert->validity.notBefore.data) == '5') &&
		 (*(cert->validity.notBefore.data + 1) != '0'))) {
		PORT_Strcat(notBeforeStr, "19");
	    } else {
		PORT_Strcat(notBeforeStr, "20");
	    }
	    PORT_Strcat(notBeforeStr, (char *)cert->validity.notBefore.data);
	}
    }
    if (find_field_bool(data, "privKeyUsagePeriod-radio-notAfter", PR_TRUE) ||
	find_field_bool(data, "privKeyUsagePeriod-radio-both", PR_TRUE)) {
	pkup->notAfter.len = 15;
	pkup->notAfter.data = (unsigned char *)notAfterStr;
	PORT_Strcat(notAfterStr,find_field(data,"privKeyUsagePeriod-notAfter-year", 
				      PR_TRUE));
	PORT_Strcat(notAfterStr,find_field(data,"privKeyUsagePeriod-notAfter-month",
				      PR_TRUE));
	PORT_Strcat(notAfterStr,find_field(data,"privKeyUsagePeriod-notAfter-day", 
				      PR_TRUE));
	PORT_Strcat(notAfterStr,find_field(data,"privKeyUsagePeriod-notAfter-hour", 
				      PR_TRUE));
	PORT_Strcat(notAfterStr,find_field(data,"privKeyUsagePeriod-notAfter-minute",
				      PR_TRUE));
	PORT_Strcat(notAfterStr,find_field(data,"privKeyUsagePeriod-notAfter-second",
				      PR_TRUE));
	if ((*(notAfterStr + 14) != '\0') ||
	    (!isdigit(*(notAfterStr + 13))) ||
	    (*(notAfterStr + 12) >= '5' && *(notAfterStr + 12) <= '0') ||
	    (!isdigit(*(notAfterStr + 11))) ||
	    (*(notAfterStr + 10) >= '5' && *(notAfterStr + 10) <= '0') ||
	    (!isdigit(*(notAfterStr + 9))) ||
	    (*(notAfterStr + 8) >= '2' && *(notAfterStr + 8) <= '0') ||
	    (!isdigit(*(notAfterStr + 7))) ||
	    (*(notAfterStr + 6) >= '3' && *(notAfterStr + 6) <= '0') ||
	    (!isdigit(*(notAfterStr + 5))) ||
	    (*(notAfterStr + 4) >= '1' && *(notAfterStr + 4) <= '0') ||
	    (!isdigit(*(notAfterStr + 3))) ||
	    (!isdigit(*(notAfterStr + 2))) ||
	    (!isdigit(*(notAfterStr + 1))) ||
	    (!isdigit(*(notAfterStr + 0))) ||
	    (*(notAfterStr + 8) == '2' && *(notAfterStr + 9) >= '4') ||
	    (*(notAfterStr + 6) == '3' && *(notAfterStr + 7) >= '1') ||
	    (*(notAfterStr + 4) == '1' && *(notAfterStr + 5) >= '2')) {
	    error_out("ERROR: Improperly formated private key usage period");
	}
	*(notAfterStr + 14) = 'Z';
	*(notAfterStr + 15) = '\0';
    }
	
    PORT_Assert (arena);

    rv = EncodeAndAddExtensionValue(arena, extHandle, pkup, 
				    find_field_bool(data,
						    "privKeyUsagePeriod-crit", 
						    PR_TRUE), 
				    SEC_OID_X509_PRIVATE_KEY_USAGE_PERIOD, 
				    (EXTEN_VALUE_ENCODER)
				    CERT_EncodePrivateKeyUsagePeriod);
    if (arena) {
	PORT_FreeArena (arena, PR_FALSE);
    }
    if (notBeforeStr != NULL) {
	PORT_Free(notBeforeStr);
    }
    if (notAfterStr != NULL) {
	PORT_Free(notAfterStr);
    }
    return (rv);
}    

static SECStatus 
AddBasicConstraint(void   *extHandle, 
		   Pair   *data)
{
    CERTBasicConstraints  basicConstraint;
    SECItem               encodedValue;
    SECStatus             rv;

    encodedValue.data = NULL;
    encodedValue.len = 0;
    basicConstraint.pathLenConstraint = CERT_UNLIMITED_PATH_CONSTRAINT;
    basicConstraint.isCA = (find_field_bool(data,"basicConstraints-cA-radio-CA",
					    PR_TRUE));
    if (find_field_bool(data,"basicConstraints-pathLengthConstraint", PR_TRUE)){
	basicConstraint.pathLenConstraint = atoi 
	    (find_field(data,"basicConstraints-pathLengthConstraint-text", 
			PR_TRUE));
    }
    
    rv = CERT_EncodeBasicConstraintValue (NULL, &basicConstraint, 
					  &encodedValue);
    if (rv)
	return (rv);
    rv = CERT_AddExtension(extHandle, SEC_OID_X509_BASIC_CONSTRAINTS, 
			   &encodedValue, 
			   (find_field_bool(data,"basicConstraints-crit", 
					    PR_TRUE)), PR_TRUE);

    PORT_Free (encodedValue.data);
    return (rv);
}



static SECStatus 
AddNscpCertType (void  *extHandle, 
		 Pair  *data)
{
    SECItem            bitStringValue;
    unsigned char      CertType = 0x0;

    if (find_field_bool(data,"netscape-cert-type-ssl-client", PR_TRUE)){
	CertType |= (0x80 >> 0);
    }
    if (find_field_bool(data,"netscape-cert-type-ssl-server", PR_TRUE)){
	CertType |= (0x80 >> 1);
    }
    if (find_field_bool(data,"netscape-cert-type-smime", PR_TRUE)){
	CertType |= (0x80 >> 2);
    }
    if (find_field_bool(data,"netscape-cert-type-object-signing", PR_TRUE)){
	CertType |= (0x80 >> 3);
    }
    if (find_field_bool(data,"netscape-cert-type-reserved", PR_TRUE)){
	CertType |= (0x80 >> 4);
    }
    if (find_field_bool(data,"netscape-cert-type-ssl-ca", PR_TRUE)) {
	CertType |= (0x80 >> 5);
    }
    if (find_field_bool(data,"netscape-cert-type-smime-ca", PR_TRUE)) {
	CertType |= (0x80 >> 6);
    }
    if (find_field_bool(data,"netscape-cert-type-object-signing-ca", PR_TRUE)) {
	CertType |= (0x80 >> 7);
    }

    bitStringValue.data = &CertType;
    bitStringValue.len = 1;

    return (CERT_EncodeAndAddBitStrExtension
	    (extHandle, SEC_OID_NS_CERT_EXT_CERT_TYPE, &bitStringValue,
	     (find_field_bool(data, "netscape-cert-type-crit", PR_TRUE))));
}


static SECStatus
add_IA5StringExtension(void    *extHandle, 
		       char    *string, 
		       PRBool  crit, 
		       int     idtag)
{
    SECItem                    encodedValue;
    SECStatus                  rv;

    encodedValue.data = NULL;
    encodedValue.len = 0;

    rv = CERT_EncodeIA5TypeExtension(NULL, string, &encodedValue);
    if (rv) {
	return (rv);
    }
    return (CERT_AddExtension(extHandle, idtag, &encodedValue, crit, PR_TRUE));
}

static SECItem *
string_to_oid(char  *string)
{
    int             i;
    int             length = 20;
    int             remaining;
    int             first_value;
    int             second_value;
    int             value;
    int             oidLength;
    unsigned char   *oidString;
    unsigned char   *write;
    unsigned char   *read;
    unsigned char   *temp;
    SECItem         *oid;
    
    
    remaining = length;
    i = 0;
    while (*string == ' ') {
	string++;
    }
    while (isdigit(*(string + i))) {
	i++;
    }
    if (*(string + i) == '.') {
	*(string + i) = '\0';
    } else {
	error_out("ERROR: Improperly formated OID");
    }
    first_value = atoi(string);
    if (first_value < 0 || first_value > 2) {
	error_out("ERROR: Improperly formated OID");
    }
    string += i + 1;
    i = 0;
    while (isdigit(*(string + i))) {
	i++;
    }
    if (*(string + i) == '.') {
	*(string + i) = '\0';
    } else {
	error_out("ERROR: Improperly formated OID");
    }
    second_value = atoi(string);
    if (second_value < 0 || second_value > 39) {
	error_out("ERROR: Improperly formated OID");
    }
    oidString = PORT_ZAlloc(2);
    *oidString = (first_value * 40) + second_value;
    *(oidString + 1) = '\0';
    oidLength = 1;
    string += i + 1;
    i = 0;
    temp = write = PORT_ZAlloc(length);
    while (*string != '\0') {
	value = 0;
	while(isdigit(*(string + i))) {
	    i++;
	}
	if (*(string + i) == '\0') {
	    value = atoi(string);
	    string += i;
	} else {
	    if (*(string + i) == '.') {
		*(string + i) = '\0';
		value = atoi(string);
		string += i + 1;
	    } else {
		*(string + i) = '\0';
		i++;
		value = atoi(string);
		while (*(string + i) == ' ')
		    i++;
		if (*(string + i) != '\0') {
		    error_out("ERROR: Improperly formated OID");
		}
	    }
	}
	i = 0;
	while (value != 0) {
	    if (remaining < 1) {
		remaining += length;
		length = length * 2;
		temp = PORT_Realloc(temp, length);
		write = temp + length - remaining;
	    }
	    *write = (value & 0x7f) | (0x80);
	    write++;
	    remaining--;
	    value = value >> 7;
	}
	*temp = *temp & (0x7f);
	oidLength += write - temp;
	oidString = PORT_Realloc(oidString, (oidLength + 1));
	read = write - 1;
	write = oidLength + oidString - 1;
	for (i = 0; i < (length - remaining); i++) {
	    *write = *read;
	    write--;
	    read++;
	}
	write = temp;
	remaining = length;
    }
    *(oidString + oidLength) = '\0';
    oid = (SECItem *) PORT_ZAlloc(sizeof(SECItem));
    oid->data = oidString;
    oid->len  = oidLength;
    PORT_Free(temp);
    return oid;
}

static SECItem *
string_to_ipaddress(char *string)
{
    int      i = 0;
    int      value;
    int      j = 0;
    SECItem  *ipaddress;
    

    while (*string == ' ') {
	string++;
    }
    ipaddress = (SECItem *) PORT_ZAlloc(sizeof(SECItem));
    ipaddress->data = PORT_ZAlloc(9);
    while (*string != '\0' && j < 8) {
	while (isdigit(*(string + i))) {
	    i++;
	}
	if (*(string + i) == '.') {
	    *(string + i) = '\0';
	    value = atoi(string);
	    string = string + i + 1;
	    i = 0;
	} else {
	    if (*(string + i) == '\0') {
		value = atoi(string);
		string = string + i;
		i = 0;
	    } else {
		*(string + i) = '\0';
		while (*(string + i) == ' ') {
		    i++;
		}
		if (*(string + i) == '\0') {
		    value = atoi(string);
		    string = string + i;
		    i = 0;
		} else {
		    error_out("ERROR: Improperly formated IP Address");
		}
	    }
	}
	if (value >= 0 && value < 256) {
	    *(ipaddress->data + j) = value;
	} else {
	    error_out("ERROR: Improperly formated IP Address");
	}
	j++;
    }
    *(ipaddress->data + j) = '\0';
    if (j != 4 && j != 8) {
	error_out("ERROR: Improperly formated IP Address");
    }
    ipaddress->len = j;
    return ipaddress;
}

static SECItem *
string_to_binary(char  *string)
{
    SECItem            *rv;
    int                high_digit;
    int                low_digit;

    rv = (SECItem *) PORT_ZAlloc(sizeof(SECItem));
    if (rv == NULL) {
	error_allocate();
    }
    rv->data = (unsigned char *) PORT_ZAlloc((PORT_Strlen(string))/3 + 2);
    while (!isxdigit(*string)) {
	string++;
    }
    rv->len = 0;
    while (*string != '\0') {
	if (isxdigit(*string)) {
	    if (*string >= '0' && *string <= '9') {
		high_digit = *string - '0';
	    } else {
		*string = toupper(*string);
		high_digit = *string - 'A' + 10;
	    }
	    string++;
	    if (*string >= '0' && *string <= '9') {
		low_digit = *string - '0';
	    } else {
		*string = toupper(*string);
		low_digit = *string - 'A' + 10;
	    }
	    (rv->len)++;
	} else {
	    if (*string == ':') {
		string++;
	    } else {
		if (*string == ' ') {
		    while (*string == ' ') {
			string++;
		    }
		}
		if (*string != '\0') {
		    error_out("ERROR: Improperly formated binary encoding");
		}
	    }
	} 
    }

    return rv;
}

static SECStatus
MakeGeneralName(char             *name, 
		CERTGeneralName  *genName,
		PLArenaPool      *arena)
{
    SECItem                      *oid;
    SECOidData                   *oidData;
    SECItem                      *ipaddress;
    SECItem                      *temp = NULL;
    int                          i;
    int                          nameType;
    PRBool                       binary = PR_FALSE;
    SECStatus                    rv = SECSuccess;
    PRBool                       nickname = PR_FALSE;

    PORT_Assert(genName);
    PORT_Assert(arena);
    nameType = *(name + PORT_Strlen(name) - 1) - '0';
    if (nameType == 0  && *(name +PORT_Strlen(name) - 2) == '1') {
	nickname = PR_TRUE;
	nameType = certOtherName;
    }
    if (nameType < 1 || nameType > 9) {
	error_out("ERROR: Unknown General Name Type");
    }
    *(name + PORT_Strlen(name) - 4) = '\0';
    genName->type = nameType;
    
    switch (genName->type) {
      case certURI:
      case certRFC822Name:
      case certDNSName: {
	  genName->name.other.data = (unsigned char *)name;
	  genName->name.other.len = PORT_Strlen(name);
	  break;
      }
      
      case certIPAddress: {
	  ipaddress = string_to_ipaddress(name);
	  genName->name.other.data = ipaddress->data;
	  genName->name.other.len = ipaddress->len;
	  break;
      }
      
      case certRegisterID: {
	  oid = string_to_oid(name);
	  genName->name.other.data = oid->data;
	  genName->name.other.len = oid->len;
	  break;
      }
      
      case certEDIPartyName:
      case certX400Address: {
	  
	  genName->name.other.data = PORT_ArenaAlloc (arena, 
						      PORT_Strlen (name) + 2);
	  if (genName->name.other.data == NULL) {
	      error_allocate();
	  }
	  
	  PORT_Memcpy (genName->name.other.data + 2, name, PORT_Strlen (name));
	  /* This may not be accurate for all cases.  
	     For now, use this tag type */
	  genName->name.other.data[0] = (char)(((genName->type - 1) & 
						0x1f)| 0x80);
	  genName->name.other.data[1] = (char)PORT_Strlen (name);
	  genName->name.other.len = PORT_Strlen (name) + 2;
	  break;
      }
      
      case certOtherName: {
	  i = 0;
	  if (!nickname) {
	      while (!isdigit(*(name + PORT_Strlen(name) - i))) {
		  i++;
	      }
	      if (*(name + PORT_Strlen(name) - i) == '1') {
		  binary = PR_TRUE;
	      } else {
		  binary = PR_FALSE;
	      }  
	      while (*(name + PORT_Strlen(name) - i) != '-') {
		  i++;
	      }
	      *(name + PORT_Strlen(name) - i - 1) = '\0';
	      i = 0;
	      while (*(name + i) != '-') {
		  i++;
	      }
	      *(name + i - 1) = '\0';
	      oid = string_to_oid(name + i + 2);
	  } else {
	      oidData = SECOID_FindOIDByTag(SEC_OID_NETSCAPE_NICKNAME);
	      oid = &oidData->oid;
	      while (*(name + PORT_Strlen(name) - i) != '-') {
		  i++;
	      }
	      *(name + PORT_Strlen(name) - i) = '\0';
	  }
	  genName->name.OthName.oid.data = oid->data;
	  genName->name.OthName.oid.len  = oid->len;
	  if (binary) {
	      temp = string_to_binary(name);
	      genName->name.OthName.name.data = temp->data;
	      genName->name.OthName.name.len = temp->len;
	  } else {
	      temp = (SECItem *) PORT_ZAlloc(sizeof(SECItem));
	      if (temp == NULL) {
		  error_allocate();
	      }
	      temp->data = (unsigned char *)name;
	      temp->len = PORT_Strlen(name);
	      SEC_ASN1EncodeItem (arena, &(genName->name.OthName.name), temp,
				  CERTIA5TypeTemplate);
	  }
	  PORT_Free(temp);
	  break;
      }
      
      case certDirectoryName: {
	  CERTName *directoryName = NULL;
	  
	  directoryName = CERT_AsciiToName (name);
	  if (!directoryName) {
	      error_out("ERROR: Improperly formated alternative name");
	      break;
	  }
	  rv = CERT_CopyName (arena, &genName->name.directoryName, 
			      directoryName);
	  CERT_DestroyName (directoryName);
	  
	  break;
      }
    }
    genName->l.next = &(genName->l);
    genName->l.prev = &(genName->l);
    return rv;
}


static CERTGeneralName *
MakeAltName(Pair             *data, 
	    char             *which, 
	    PLArenaPool      *arena)
{
    CERTGeneralName          *SubAltName;
    CERTGeneralName          *current;
    CERTGeneralName          *newname;
    char                     *name = NULL;
    SECStatus                rv = SECSuccess;
    int                      len;
    

    len = PORT_Strlen(which);
    name = find_field(data, which, PR_TRUE);
    SubAltName = current = (CERTGeneralName *) PORT_ZAlloc
	                                        (sizeof(CERTGeneralName));
    if (current == NULL) {
	error_allocate();
    }
    while (name != NULL) {

	rv = MakeGeneralName(name, current, arena);

	if (rv != SECSuccess) {
	    break;
	}
	if (*(which + len -1) < '9') {
	    *(which + len - 1) = *(which + len - 1) + 1;
	} else {
	    if (isdigit(*(which + len - 2) )) {
		*(which + len - 2) = *(which + len - 2) + 1;
		*(which + len - 1) = '0';
	    } else {
		*(which + len - 1) = '1';
		*(which + len) = '0';
		*(which + len + 1) = '\0';
		len++;
	    }
	}
	len = PORT_Strlen(which);
	name = find_field(data, which, PR_TRUE);
	if (name != NULL) {
	    newname = (CERTGeneralName *) PORT_ZAlloc(sizeof(CERTGeneralName));
	    if (newname == NULL) {
		error_allocate();
	    }
	    current->l.next = &(newname->l);
	    newname->l.prev = &(current->l);
	    current = newname;
            newname = NULL;
	} else {
	    current->l.next = &(SubAltName->l);
	    SubAltName->l.prev = &(current->l);
	}
    }
    if (rv == SECFailure) {
	return NULL;
    }
    return SubAltName;
}

static CERTNameConstraints *
MakeNameConstraints(Pair             *data, 
		    PLArenaPool      *arena)
{
    CERTNameConstraints      *NameConstraints;
    CERTNameConstraint       *current = NULL;
    CERTNameConstraint       *last_permited = NULL;
    CERTNameConstraint       *last_excluded = NULL;
    char                     *constraint = NULL;
    char                     *which;
    SECStatus                rv = SECSuccess;
    int                      len;
    int                      i;
    long                     max;
    long                     min;
    PRBool                   permited;
    

    NameConstraints = (CERTNameConstraints *) PORT_ZAlloc
	                            (sizeof(CERTNameConstraints));
    which = make_copy_string("NameConstraintSelect0", 25,'\0');
    len = PORT_Strlen(which);
    constraint = find_field(data, which, PR_TRUE);
    NameConstraints->permited = NameConstraints->excluded = NULL;
    while (constraint != NULL) {
	current = (CERTNameConstraint *) PORT_ZAlloc
	                       (sizeof(CERTNameConstraint));
	if (current == NULL) {
	    error_allocate();
	}
	i = 0;
	while (*(constraint + PORT_Strlen(constraint) - i) != '-') {
	    i++;
	}
        *(constraint + PORT_Strlen(constraint) - i - 1) = '\0'; 
	max = (long) atoi(constraint + PORT_Strlen(constraint) + 3);
	if (max > 0) {
	    (void) SEC_ASN1EncodeInteger(arena, &current->max, max);
	}
	i = 0;
	while (*(constraint + PORT_Strlen(constraint) - i) != '-') {
	    i++;
	}
        *(constraint + PORT_Strlen(constraint) - i - 1) = '\0';
	min = (long) atoi(constraint + PORT_Strlen(constraint) + 3);
	(void) SEC_ASN1EncodeInteger(arena, &current->min, min);
	while (*(constraint + PORT_Strlen(constraint) - i) != '-') {
	    i++;
	}
        *(constraint + PORT_Strlen(constraint) - i - 1) = '\0';
	if (*(constraint + PORT_Strlen(constraint) + 3) == 'p') {
	    permited = PR_TRUE;
	} else {
	    permited = PR_FALSE;
	}
	rv = MakeGeneralName(constraint, &(current->name), arena);

	if (rv != SECSuccess) {
	    break;
	}
	if (*(which + len - 1) < '9') {
	    *(which + len - 1) = *(which + len - 1) + 1;
	} else {
	    if (isdigit(*(which + len - 2) )) {
		*(which + len - 2) = *(which + len - 2) + 1;
		*(which + len - 1) = '0';
	    } else {
		*(which + len - 1) = '1';
		*(which + len) = '0';
		*(which + len + 1) = '\0';
		len++;
	    }
	}
	len = PORT_Strlen(which);
	if (permited) {
	    if (NameConstraints->permited == NULL) {
		NameConstraints->permited = last_permited = current;
	    }
	    last_permited->l.next = &(current->l);
	    current->l.prev = &(last_permited->l);
	    last_permited = current;
	} else {
	    if (NameConstraints->excluded == NULL) {
		NameConstraints->excluded = last_excluded = current;
	    }
	    last_excluded->l.next = &(current->l);
	    current->l.prev = &(last_excluded->l);
	    last_excluded = current;
	}
	constraint = find_field(data, which, PR_TRUE);
	if (constraint != NULL) {
	    current = (CERTNameConstraint *) PORT_ZAlloc(sizeof(CERTNameConstraint));
	    if (current == NULL) {
		error_allocate();
	    }
	}
    }
    if (NameConstraints->permited != NULL) {
	last_permited->l.next = &(NameConstraints->permited->l);
	NameConstraints->permited->l.prev = &(last_permited->l);
    }
    if (NameConstraints->excluded != NULL) {
	last_excluded->l.next = &(NameConstraints->excluded->l);
	NameConstraints->excluded->l.prev = &(last_excluded->l);
    }
    if (which != NULL) {
	PORT_Free(which);
    }
    if (rv == SECFailure) {
	return NULL;
    }
    return NameConstraints;
}



static SECStatus
AddAltName(void              *extHandle,
	   Pair              *data,
	   char              *issuerNameStr, 
	   CERTCertDBHandle  *handle,
	   int               type)
{
    PRBool             autoIssuer = PR_FALSE;
    PLArenaPool        *arena = NULL;
    CERTGeneralName    *genName = NULL;
    char               *which = NULL;
    char               *name = NULL;
    SECStatus          rv = SECSuccess;
    SECItem            *issuersAltName = NULL;
    CERTCertificate    *issuerCert = NULL;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	error_allocate();
    }
    if (type == 0) {
	which = make_copy_string("SubAltNameSelect0", 20,'\0');
	genName = MakeAltName(data, which, arena);
    } else {
	if (autoIssuer) {
	    autoIssuer = find_field_bool(data,"IssuerAltNameSourceRadio-auto",
					 PR_TRUE);
	    issuerCert = CERT_FindCertByNameString(handle, issuerNameStr);
	    rv = cert_FindExtension((*issuerCert).extensions, 
				    SEC_OID_X509_SUBJECT_ALT_NAME, 
				    issuersAltName);
	    if (issuersAltName == NULL) {
		name = PORT_Alloc(PORT_Strlen((*issuerCert).subjectName) + 4);
		PORT_Strcpy(name, (*issuerCert).subjectName);
		PORT_Strcat(name, " - 5");
	    }
	} else {
	    which = make_copy_string("IssuerAltNameSelect0", 20,'\0');
	    genName = MakeAltName(data, which, arena);
	}
    }
    if (type == 0) {
	EncodeAndAddExtensionValue(arena, extHandle, genName, 
				   find_field_bool(data, "SubAltName-crit", 
						   PR_TRUE), 
				   SEC_OID_X509_SUBJECT_ALT_NAME, 
				   (EXTEN_VALUE_ENCODER)
				   CERT_EncodeAltNameExtension);

    } else {
	if (autoIssuer && (name == NULL)) {
	    rv = CERT_AddExtension
		(extHandle, SEC_OID_X509_ISSUER_ALT_NAME, issuersAltName,
		 find_field_bool(data, "IssuerAltName-crit", PR_TRUE), PR_TRUE);
	} else {
	    EncodeAndAddExtensionValue(arena, extHandle, genName, 
				       find_field_bool(data, 
						       "IssuerAltName-crit", 
						       PR_TRUE), 
				       SEC_OID_X509_ISSUER_ALT_NAME, 
				       (EXTEN_VALUE_ENCODER)
				       CERT_EncodeAltNameExtension);
	}
    }
    if (which != NULL) {
	PORT_Free(which);
    }
    if (issuerCert != NULL) {
	CERT_DestroyCertificate(issuerCert);
    }
    return rv;
}


static SECStatus
AddNameConstraints(void  *extHandle,
		   Pair  *data)
{
    PLArenaPool         *arena = NULL;
    CERTNameConstraints *constraints = NULL;
    SECStatus           rv = SECSuccess;


    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
	error_allocate();
    }
    constraints = MakeNameConstraints(data, arena);
    if (constraints != NULL) {
	EncodeAndAddExtensionValue(arena, extHandle, constraints, PR_TRUE, 
				   SEC_OID_X509_NAME_CONSTRAINTS, 
				   (EXTEN_VALUE_ENCODER)
				   CERT_EncodeNameConstraintsExtension);
    }
    if (arena != NULL) {
	PORT_ArenaRelease (arena, NULL);
    }
    return rv;
}


static SECStatus
add_extensions(CERTCertificate   *subjectCert, 
	       Pair              *data, 
	       char              *issuerNameStr, 
	       CERTCertDBHandle  *handle)
{
    void                         *extHandle;
    SECStatus                    rv = SECSuccess;


    extHandle = CERT_StartCertExtensions (subjectCert);
    if (extHandle == NULL) {
	error_out("ERROR: Unable to get certificates extension handle");
    }
    if (find_field_bool(data, "keyUsage", PR_TRUE)) {
	rv = AddKeyUsage(extHandle, data);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Key Usage extension");
	}
    }

    if( find_field_bool(data, "extKeyUsage", PR_TRUE) ) {
      rv = AddExtKeyUsage(extHandle, data);
      if( SECSuccess != rv ) {
        error_out("ERROR: Unable to add Extended Key Usage extension");
      }
    }

    if (find_field_bool(data, "basicConstraints", PR_TRUE)) {
	rv = AddBasicConstraint(extHandle, data);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Basic Constraint extension");
	}
    }
    if (find_field_bool(data, "subjectKeyIdentifier", PR_TRUE)) {
	rv = AddSubKeyID(extHandle, data, subjectCert);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Subject Key Identifier Extension");
	}
    }
    if (find_field_bool(data, "authorityKeyIdentifier", PR_TRUE)) {
	rv = AddAuthKeyID (extHandle, data, issuerNameStr, handle);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Authority Key Identifier extension");
	}
    }
    if (find_field_bool(data, "privKeyUsagePeriod", PR_TRUE)) {
	rv = AddPrivKeyUsagePeriod (extHandle, data, subjectCert);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Private Key Usage Period extension");
	}
    }
    if (find_field_bool(data, "SubAltName", PR_TRUE)) {
	rv = AddAltName (extHandle, data, NULL, NULL, 0);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Subject Alternative Name extension");
	}
    }
    if (find_field_bool(data, "IssuerAltName", PR_TRUE)) {
	rv = AddAltName (extHandle, data, issuerNameStr, handle, 1);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Issuer Alternative Name Extension");
	}
    }
    if (find_field_bool(data, "NameConstraints", PR_TRUE)) {
	rv = AddNameConstraints(extHandle, data);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Name Constraints Extension");
	}
    }
    if (find_field_bool(data, "netscape-cert-type", PR_TRUE)) {
	rv = AddNscpCertType(extHandle, data);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Netscape Certificate Type Extension");
	}
    }
    if (find_field_bool(data, "netscape-base-url", PR_TRUE)) {
	rv = add_IA5StringExtension(extHandle, 
				    find_field(data, "netscape-base-url-text", 
					       PR_TRUE), 
				    find_field_bool(data, 
						    "netscape-base-url-crit", 
						    PR_TRUE),
				    SEC_OID_NS_CERT_EXT_BASE_URL);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Netscape Base URL Extension");
	}
    }
    if (find_field_bool(data, "netscape-revocation-url", PR_TRUE)) {
	rv = add_IA5StringExtension(extHandle, 
				    find_field(data, 
					       "netscape-revocation-url-text", 
					       PR_TRUE), 
				    find_field_bool
				       (data, "netscape-revocation-url-crit", 
					PR_TRUE),
				    SEC_OID_NS_CERT_EXT_REVOCATION_URL);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Netscape Revocation URL Extension");
	}
    }
    if (find_field_bool(data, "netscape-ca-revocation-url", PR_TRUE)) {
	rv = add_IA5StringExtension(extHandle, 
				    find_field(data, 
					      "netscape-ca-revocation-url-text",
					       PR_TRUE), 
				    find_field_bool
				        (data, "netscape-ca-revocation-url-crit"
					 , PR_TRUE),
				    SEC_OID_NS_CERT_EXT_CA_REVOCATION_URL);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Netscape CA Revocation URL Extension");
	}
    }
    if (find_field_bool(data, "netscape-cert-renewal-url", PR_TRUE)) {
	rv = add_IA5StringExtension(extHandle, 
				    find_field(data, 
					       "netscape-cert-renewal-url-text",
					       PR_TRUE), 
				    find_field_bool
				        (data, "netscape-cert-renewal-url-crit",
					 PR_TRUE),
				    SEC_OID_NS_CERT_EXT_CERT_RENEWAL_URL);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Netscape Certificate Renewal URL Extension");
	}
    }
    if (find_field_bool(data, "netscape-ca-policy-url", PR_TRUE)) {
	rv = add_IA5StringExtension(extHandle, 
				    find_field(data, 
					       "netscape-ca-policy-url-text", 
					       PR_TRUE), 
				    find_field_bool
				         (data, "netscape-ca-policy-url-crit", 
					  PR_TRUE),
				    SEC_OID_NS_CERT_EXT_CA_POLICY_URL);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Netscape CA Policy URL Extension");
	}
    }
    if (find_field_bool(data, "netscape-ssl-server-name", PR_TRUE)) {
	rv = add_IA5StringExtension(extHandle, 
				    find_field(data, 
					       "netscape-ssl-server-name-text", 
					       PR_TRUE), 
				    find_field_bool
				         (data, "netscape-ssl-server-name-crit",
					  PR_TRUE),
				    SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Netscape SSL Server Name Extension");
	}
    }
    if (find_field_bool(data, "netscape-comment", PR_TRUE)) {
	rv = add_IA5StringExtension(extHandle, 
				    find_field(data, "netscape-comment-text", 
					       PR_TRUE), 
				    find_field_bool(data, 
						    "netscape-comment-crit", 
						    PR_TRUE),
				    SEC_OID_NS_CERT_EXT_COMMENT);
	if (rv != SECSuccess) {
	    error_out("ERROR: Unable to add Netscape Comment Extension");
	}
    }
    CERT_FinishExtensions(extHandle);
    return (rv);
}



char *
return_dbpasswd(PK11SlotInfo *slot, PRBool retry, void *data)
{
    char *rv;

    /* don't clobber our poor smart card */
    if (retry == PR_TRUE) {
	return NULL;
    }
    rv = PORT_Alloc(4);
    PORT_Strcpy(rv, "foo");
    return rv;
}


SECKEYPrivateKey *
FindPrivateKeyFromNameStr(char              *name, 
			  CERTCertDBHandle  *certHandle)
{
    SECKEYPrivateKey                        *key;
    CERTCertificate                         *cert;
    CERTCertificate                         *p11Cert;


    /* We don't presently have a PK11 function to find a cert by 
    ** subject name.  
    ** We do have a function to find a cert in the internal slot's
    ** cert db by subject name, but it doesn't setup the slot info.
    ** So, this HACK works, but should be replaced as soon as we 
    ** have a function to search for certs accross slots by subject name.
    */
    cert = CERT_FindCertByNameString(certHandle, name);
    if (cert == NULL || cert->nickname == NULL) {
	error_out("ERROR: Unable to retrieve issuers certificate");
    }
    p11Cert = PK11_FindCertFromNickname(cert->nickname, NULL);
    if (p11Cert == NULL) {
	error_out("ERROR: Unable to retrieve issuers certificate");
    }
    key = PK11_FindKeyByAnyCert(p11Cert, NULL);
    return key;
}

static SECItem *
SignCert(CERTCertificate   *cert,
	 char              *issuerNameStr,
	 Pair              *data,
	 CERTCertDBHandle  *handle,
         int               which_key)
{
    SECItem                der;
    SECKEYPrivateKey       *caPrivateKey = NULL;
    SECStatus              rv;
    PLArenaPool            *arena;
    SECOidTag              algID;

    if (which_key == 0) {
	caPrivateKey = FindPrivateKeyFromNameStr(issuerNameStr, handle); 
    } else {
	caPrivateKey = privkeys[which_key - 1];
    }
    if (caPrivateKey == NULL) {
	error_out("ERROR: unable to retrieve issuers key");
    }
	
    arena = cert->arena;

    algID = SEC_GetSignatureAlgorithmOidTag(caPrivateKey->keyType,
					    SEC_OID_UNKNOWN);
    if (algID == SEC_OID_UNKNOWN) {
	error_out("ERROR: Unknown key type for issuer.");
	goto done;
    }

    rv = SECOID_SetAlgorithmID(arena, &cert->signature, algID, 0);
    if (rv != SECSuccess) {
	error_out("ERROR: Could not set signature algorithm id.");
    }

    if (find_field_bool(data,"ver-1", PR_TRUE)) {
	*(cert->version.data) = 0;
	cert->version.len = 1;
    } else {
	*(cert->version.data) = 2;
	cert->version.len = 1;
    }
    der.data = NULL;
    der.len = 0;
    (void) SEC_ASN1EncodeItem (arena, &der, cert, CERT_CertificateTemplate);
    if (der.data == NULL) {
	error_out("ERROR: Could not encode certificate.\n");
    }
    rv = SEC_DerSignData (arena, &(cert->derCert), der.data, der.len, caPrivateKey,
			  algID);
    if (rv != SECSuccess) {
	error_out("ERROR: Could not sign encoded certificate data.\n");
    }
done:
    SECKEY_DestroyPrivateKey(caPrivateKey);
    return &(cert->derCert);
}


int
main(int argc, char **argv)
{
    int                    length = 500;
    int                    remaining = 500;
    int                    n;
    int                    i;
    int                    serial;
    int                    chainLen;
    int                    which_key;
    char                   *pos;
#ifdef OFFLINE
    char                   *form_output = "key=MIIBPTCBpzCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA7SLqjWBL9Wl11Vlg%0AaMqZCvcQOL%2FnvSqYPPRP0XZy9SoAeyWzQnBOiCm2t8H5mK7r2jnKdAQOmfhjaJil%0A3hNVu3SekHOXF6Ze7bkWa6%2FSGVcY%2FojkydxFSgY43nd1iydzPQDp8WWLL%2BpVpt%2B%2B%0ATRhFtVXbF0fQI03j9h3BoTgP2lkCAwEAARYDZm9vMA0GCSqGSIb3DQEBBAUAA4GB%0AAJ8UfRKJ0GtG%2B%2BufCC6tAfTzKrq3CTBHnom55EyXcsAsv6WbDqI%2F0rLAPkn2Xo1r%0AnNhtMxIuj441blMt%2Fa3AGLOy5zmC7Qawt8IytvQikQ1XTpTBCXevytrmLjCmlURr%0ANJryTM48WaMQHiMiJpbXCqVJC1d%2FpEWBtqvALzZaOOIy&subject=CN%3D%22test%22%26serial-auto%3Dtrue%26serial_value%3D%26ver-1%3Dtrue%26ver-3%3Dfalse%26caChoiceradio-SignWithDefaultkey%3Dtrue%26caChoiceradio-SignWithRandomChain%3Dfalse%26autoCAs%3D%26caChoiceradio-SignWithSpecifiedChain%3Dfalse%26manCAs%3D%26%24";
#else
    char                   *form_output;
#endif
    char                   *issuerNameStr;
    char                   *certName;
    char                   *DBdir = DB_DIRECTORY;
    char                   *prefixs[10] = {"CA#1-", "CA#2-", "CA#3-", 
					   "CA#4-", "CA#5-", "CA#6-", 
					   "CA#7-", "CA#8-", "CA#9-", ""};
    Pair                   *form_data;
    CERTCertificate        *cert;
    CERTCertDBHandle       *handle;
    CERTCertificateRequest *certReq = NULL;
    int                    warpmonths = 0;
    SECItem                *certDER;
#ifdef FILEOUT
    FILE                   *outfile;
#endif
    SECStatus              status = SECSuccess;
    extern                 char prefix[PREFIX_LEN];
    SEC_PKCS7ContentInfo   *certChain;
    SECItem                *encodedCertChain;
    PRBool                 UChain = PR_FALSE;


    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];


#ifdef TEST
    sleep(20);
#endif
    SECU_ConfigDirectory(DBdir);

    PK11_SetPasswordFunc(return_dbpasswd);
    status = NSS_InitReadWrite(DBdir);
    if (status != SECSuccess) {
	SECU_PrintPRandOSError(progName);
	return -1;
    }
    handle = CERT_GetDefaultCertDB();

    prefix[0]= '\0';
#if !defined(OFFLINE)
    form_output = (char*) PORT_Alloc(length);
    if (form_output == NULL) {
	error_allocate();
    }
    pos = form_output;
    while (feof(stdin) == 0 ) {
	if (remaining <= 1) {
	    remaining += length;
	    length = length * 2;
	    form_output = PORT_Realloc(form_output, (length));
	    if (form_output == NULL) {
		error_allocate();
	    }
	    pos = form_output + length - remaining;
	}
	n = fread(pos, 1, (size_t) (remaining - 1), stdin);
	pos += n;
	remaining -= n;
    }
    *pos = '&';
    pos++;
    length = pos - form_output;
#else
    length = PORT_Strlen(form_output);
#endif
#ifdef FILEOUT
    printf("Content-type: text/plain\n\n");
    fwrite(form_output, 1, (size_t)length, stdout);
    printf("\n");
#endif
#ifdef FILEOUT
    fwrite(form_output, 1, (size_t)length, stdout);
    printf("\n");
    fflush(stdout);
#endif
    form_data = make_datastruct(form_output, length);
    status = clean_input(form_data);
#if !defined(OFFLINE)
    PORT_Free(form_output);
#endif
#ifdef FILEOUT
    i = 0;
    while(return_name(form_data, i) != NULL) {
        printf("%s",return_name(form_data,i));
        printf("=\n");
        printf("%s",return_data(form_data,i));
        printf("\n");
	i++;
    }
    printf("I got that done, woo hoo\n");
    fflush(stdout);
#endif
    issuerNameStr = PORT_Alloc(200);
    if (find_field_bool(form_data, "caChoiceradio-SignWithSpecifiedChain",
			PR_FALSE)) {
	UChain = PR_TRUE;
	chainLen = atoi(find_field(form_data, "manCAs", PR_FALSE));
	PORT_Strcpy(prefix, prefixs[0]);
	issuerNameStr = PORT_Strcpy(issuerNameStr,
			       "CN=Cert-O-Matic II, O=Cert-O-Matic II");
	if (chainLen == 0) {
	    UChain =  PR_FALSE;
	}
    } else {
	if (find_field_bool(form_data, "caChoiceradio-SignWithRandomChain", 
			    PR_FALSE)) {
	    PORT_Strcpy(prefix,prefixs[9]);
	    chainLen = atoi(find_field(form_data, "autoCAs", PR_FALSE));
	    if (chainLen < 1 || chainLen > 18) {
		issuerNameStr = PORT_Strcpy(issuerNameStr, 
				       "CN=CA18, O=Cert-O-Matic II");
	    }
	    issuerNameStr = PORT_Strcpy(issuerNameStr, "CN=CA");
	    issuerNameStr = PORT_Strcat(issuerNameStr, 
				   find_field(form_data,"autoCAs", PR_FALSE));
	    issuerNameStr = PORT_Strcat(issuerNameStr,", O=Cert-O-Matic II");
	} else {
	    issuerNameStr = PORT_Strcpy(issuerNameStr, 
				   "CN=Cert-O-Matic II, O=Cert-O-Matic II");
	}
	chainLen = 0;
    }

    i = -1;
    which_key = 0;
    do {
    	extern SECStatus cert_GetKeyID(CERTCertificate *cert);
	i++;
	if (i != 0 && UChain) {
	    PORT_Strcpy(prefix, prefixs[i]);
	}
	/*        find_field(form_data,"subject", PR_TRUE); */
	certReq = makeCertReq(form_data, which_key);
#ifdef OFFLINE
	serial = 900;
#else
	serial = get_serial_number(form_data);
#endif
	cert = MakeV1Cert(handle, certReq, issuerNameStr, PR_FALSE, 
			  serial, warpmonths, form_data);
	if (certReq != NULL) {
	    CERT_DestroyCertificateRequest(certReq);
	}
	if (find_field_bool(form_data,"ver-3", PR_TRUE)) {
	    status = add_extensions(cert, form_data, issuerNameStr, handle);
	    if (status != SECSuccess) {
		error_out("ERROR: Unable to add extensions");
	    }
	}
	status = cert_GetKeyID(cert);
	if (status == SECFailure) {
	    error_out("ERROR: Unable to get Key ID.");
	}
	certDER = SignCert(cert, issuerNameStr, form_data, handle, which_key);
	CERT_NewTempCertificate(handle, certDER, NULL, PR_FALSE, PR_TRUE);
	issuerNameStr = find_field(form_data, "subject", PR_TRUE);
	/*        SECITEM_FreeItem(certDER, PR_TRUE); */
	CERT_DestroyCertificate(cert);
	if (i == (chainLen - 1)) {
	    i = 8;
	}
	++which_key;
    } while (i < 9 && UChain);



#ifdef FILEOUT
    outfile = fopen("../certout", "wb");
#endif
    certName = find_field(form_data, "subject", PR_FALSE);
    cert = CERT_FindCertByNameString(handle, certName);
    certChain = SEC_PKCS7CreateCertsOnly (cert, PR_TRUE, handle);
    if (certChain == NULL) {
	error_out("ERROR: No certificates in cert chain");
    }
    encodedCertChain = SEC_PKCS7EncodeItem (NULL, NULL, certChain, NULL, NULL, 
					    NULL);
    if (encodedCertChain) {
#if !defined(FILEOUT)
	printf("Content-type: application/x-x509-user-cert\r\n");
	printf("Content-length: %d\r\n\r\n", encodedCertChain->len);
	fwrite (encodedCertChain->data, 1, encodedCertChain->len, stdout);
#else
	fwrite (encodedCertChain->data, 1, encodedCertChain->len, outfile);
#endif

    } else {
	error_out("Error: Unable to DER encode certificate");
    }
#ifdef FILEOUT
    printf("\nI got here!\n");
    fflush(outfile);
    fclose(outfile);
#endif
    fflush(stdout);
    if (NSS_Shutdown() != SECSuccess) {
        exit(1);
    }
    return 0;
}

