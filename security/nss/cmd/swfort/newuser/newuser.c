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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef XP_UNIX
#include <unistd.h>
#endif
#include "cryptint.h"
#include "blapi.h"	/* program calls low level functions directly!*/
#include "pk11func.h"
#include "secmod.h"
/*#include "secmodi.h"*/
#include "cert.h"
#include "key.h"
#include "nss.h"
#include "swforti.h"
#include "secutil.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define MAX_PERSONALITIES 50
typedef struct {
    int index;
    CI_CERT_STR label;
    CERTCertificate *cert;
} certlist;

typedef struct {
   int card;
   int index;
   CI_CERT_STR label;
   certlist valid[MAX_PERSONALITIES];
   int count;
} Cert;


#define EMAIL_OID_LEN 9
#define EMAIL_OID 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01
unsigned char emailAVA[127] = {
	0x31, 6+EMAIL_OID_LEN, 	/* Set */
	0x30, 4+EMAIL_OID_LEN,	/* Sequence */
	0x06, EMAIL_OID_LEN, EMAIL_OID,
	0x13, 0, /* printable String */
};
#define EMAIL_DATA_START 8+EMAIL_OID_LEN

int emailOffset[] = { 1, 3, EMAIL_DATA_START-1 };
int offsetCount = sizeof(emailOffset)/sizeof(emailOffset[0]);

unsigned char hash[20] = { 'H', 'a', 's', 'h', ' ', 'F', 'a', 'i', 'l', 'e',
		  'd', ' ', '*', '*', '*', '*', '*', '*', '*', '*' };
unsigned char sig[40] = { 'H', 'a', 's', 'h', ' ', 'F', 'a', 'i', 'l', 'e',
		 'd', ' ', '*', '*', '*', '*', '*', '*', '*', '*',
		 '>', '>', '>', ' ', 'N', 'o', 't', ' ', 'S', 'i',
		 'g', 'n', 'd', ' ', '<', '<', '<', ' ', ' ', ' ' };


/*void *malloc(int); */

unsigned char *data_start(unsigned char *buf, int length, int *data_length) 
{
    unsigned char tag;
    int used_length= 0;

    tag = buf[used_length++];

    /* blow out when we come to the end */
    if (tag == 0) {
	return NULL;
    }

    *data_length = buf[used_length++];

    if (*data_length&0x80) {
	int  len_count = *data_length & 0x7f;

	*data_length = 0;

	while (len_count-- > 0) {
	    *data_length = (*data_length << 8) | buf[used_length++];
	} 
    }

    if (*data_length > (length-used_length) ) {
	*data_length = length-used_length;
	return NULL;
    }

    return (buf + used_length);	
}

unsigned char *
GetAbove(unsigned char *cert,int cert_length,int *above_len)
{
	unsigned char *buf = cert;
	int buf_length = cert_length;
	unsigned char *tmp;
	int len;

	*above_len = 0;

	/* optional serial number */
	if ((buf[0] & 0xa0) == 0xa0) {
	    tmp = data_start(buf,buf_length,&len);
	    if (tmp == NULL) return NULL;
	    buf_length -= (tmp-buf) + len;
	    buf = tmp + len;
	}
        /* serial number */
	tmp = data_start(buf,buf_length,&len);
	if (tmp == NULL) return NULL;
	buf_length -= (tmp-buf) + len;
	buf = tmp + len;
	/* skip the OID */
	tmp = data_start(buf,buf_length,&len);
	if (tmp == NULL) return NULL;
	buf_length -= (tmp-buf) + len;
	buf = tmp + len;
	/* issuer */
	tmp = data_start(buf,buf_length,&len);
	if (tmp == NULL) return NULL;
	buf_length -= (tmp-buf) + len;
	buf = tmp + len;
	/* skip the date */
	tmp = data_start(buf,buf_length,&len);
	if (tmp == NULL) return NULL;
	buf_length -= (tmp-buf) + len;
	buf = tmp + len;

	*above_len = buf - cert;
	return cert;
}

unsigned char *
GetSubject(unsigned char *cert,int cert_length,int *subj_len) {
	unsigned char *buf = cert;
	int buf_length = cert_length;
	unsigned char *tmp;
	int len;

	*subj_len = 0;

	/* optional serial number */
	if ((buf[0] & 0xa0) == 0xa0) {
	    tmp = data_start(buf,buf_length,&len);
	    if (tmp == NULL) return NULL;
	    buf_length -= (tmp-buf) + len;
	    buf = tmp + len;
	}
        /* serial number */
	tmp = data_start(buf,buf_length,&len);
	if (tmp == NULL) return NULL;
	buf_length -= (tmp-buf) + len;
	buf = tmp + len;
	/* skip the OID */
	tmp = data_start(buf,buf_length,&len);
	if (tmp == NULL) return NULL;
	buf_length -= (tmp-buf) + len;
	buf = tmp + len;
	/* issuer */
	tmp = data_start(buf,buf_length,&len);
	if (tmp == NULL) return NULL;
	buf_length -= (tmp-buf) + len;
	buf = tmp + len;
	/* skip the date */
	tmp = data_start(buf,buf_length,&len);
	if (tmp == NULL) return NULL;
	buf_length -= (tmp-buf) + len;
	buf = tmp + len;

	return data_start(buf,buf_length,subj_len);
}

unsigned char *
GetBelow(unsigned char *cert,int cert_length,int *below_len) {
	unsigned char *subj;
	int subj_len;
	unsigned char *below;

	*below_len = 0;

	subj = GetSubject(cert,cert_length,&subj_len);

	below = subj + subj_len;
	*below_len = cert_length - (below - cert);
	return below;
}

unsigned char *
GetSignature(unsigned char *sig,int sig_length,int *subj_len) {
	unsigned char *buf = sig;
	int buf_length = sig_length;
	unsigned char *tmp;
	int len;

	*subj_len = 0;

        /* signature oid */
	tmp = data_start(buf,buf_length,&len);
	if (tmp == NULL) return NULL;
	buf_length -= (tmp-buf) + len;
	buf = tmp + len;
	/* signature data */
	tmp = data_start(buf,buf_length,&len);
	if (tmp == NULL) return NULL;

	*subj_len = len -1;
	return tmp+1;
}

int DER_Sequence(unsigned char *buf, int length) {
    int next = 0;

    buf[next++] = 0x30;
    if (length < 0x80) {
	buf[next++] = length;
    } else {
	buf[next++] = 0x82;
	buf[next++] = (length >> 8) & 0xff;
	buf[next++] = length & 0xff;
    }
    return next;
}

static
int Cert_length(unsigned char *buf, int length) {
    unsigned char tag;
    int used_length= 0;
    int data_length;

    tag = buf[used_length++];

    /* blow out when we come to the end */
    if (tag == 0) {
        return 0;
    }

    data_length = buf[used_length++];

    if (data_length&0x80) {
        int  len_count = data_length & 0x7f;

        data_length = 0;

        while (len_count-- > 0) {
            data_length = (data_length << 8) | buf[used_length++];
        }
    }

    if (data_length > (length-used_length) ) {
        return length;
    }

    return (data_length + used_length);
}

int
InitCard(int card, char *inpass) {
    int cirv;
    char buf[50];
    char *pass;

    cirv = CI_Open( 0 /* flags */, card);
    if (cirv != CI_OK) return cirv;

    if (inpass == NULL) {
        sprintf(buf,"Enter PIN for card in socket %d: ",card);
        pass = SECU_GetPasswordString(NULL, buf);

        if (pass == NULL) {
	    CI_Close(CI_POWER_DOWN_FLAG,card);
	    return CI_FAIL;
        }
    } else pass=inpass;

    cirv = CI_CheckPIN(CI_USER_PIN,(unsigned char *)pass);
    if (cirv != CI_OK)  {
	CI_Close(CI_POWER_DOWN_FLAG,card);
    }
    return cirv;
}

int
isUser(CI_PERSON *person) {
    return 1;
}

int
isCA(CI_PERSON *person) {
    return 0;
}

int FoundCert(int card, char *name, Cert *cert) {
    CI_PERSON personalities[MAX_PERSONALITIES];
    CI_PERSON *person;
    int cirv;
    int i;
    int user_len = strlen(name);

    PORT_Memset(personalities, 0, sizeof(CI_PERSON)*MAX_PERSONALITIES);

    cirv = CI_GetPersonalityList(MAX_PERSONALITIES,personalities);
    if (cirv != CI_OK) return 0;


    cert->count = 1;
    cert->valid[0].index = 0;
    memcpy(cert->valid[0].label,"RRXX0000Root PAA Certificate        ",
		sizeof(cert->valid[0].label));
    cert->valid[0].cert = NULL;
    for (i=0; i < MAX_PERSONALITIES; i++) {
	person = &personalities[i];
	if ( (PORT_Memcmp(person->CertLabel,"RRXX",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"RTXX",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"LAXX",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"INKS",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"INKX",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"ONKS",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"ONKX",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"KEAK",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"3IKX",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"DSA1",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"DSAI",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"DSAO",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"3IXS",4) == 0) ||
	     (PORT_Memcmp(person->CertLabel,"3OXS",4) == 0) ){
	    int index;

	    cert->valid[cert->count].cert = NULL;
	    memcpy(cert->valid[cert->count].label,
				person->CertLabel,sizeof(person->CertLabel));
	    for (index = sizeof(person->CertLabel)-1;
		cert->valid[cert->count].label[index] == ' '; index--) {
		cert->valid[cert->count].label[index] = 0;
	    }
	    cert->valid[cert->count++].index = person->CertificateIndex;
	}
    }
    for (i=0; i < MAX_PERSONALITIES; i++) {
	person = &personalities[i];
	if (strncmp((char *)&person->CertLabel[8],name,user_len) == 0) {
	    cert->card = card;
	    cert->index = person->CertificateIndex;
	    memcpy(&cert->label,person->CertLabel,sizeof(person->CertLabel));
	    return 1;
	}
    }
    return 0;
}

void
Terminate(char *mess, int cirv, int card1, int card2)
{
    fprintf(stderr,"FAIL: %s error %d\n",mess,cirv);
    if (card1 != -1) CI_Close(CI_POWER_DOWN_FLAG,card1);
    if (card2 != -1) CI_Close(CI_POWER_DOWN_FLAG,card2);
    CI_Terminate();
    exit(1);
}

void
usage(char *prog)
{
    fprintf(stderr,"usage: %s [-e email][-t transport][-u userpin][-U userpass][-s ssopin][-S ssopass][-o outfile] common_name ca_label\n",prog);
    exit(1);
}

#define CERT_SIZE 2048	


/* version and oid */
unsigned char header[] = {
    /* Cert OID */
    0x02, 0x10,
     0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00,
    0x30, 0x0b, 0x06, 0x09,
    0x60, 0x86, 0x48, 0x01, 0x65, 0x02, 0x01, 0x01, 0x13 };

#define KEY_START 21
#define KMID_OFFSET 4
#define KEA_OFFSET 15
#define DSA_OFFSET 148
unsigned char key[] = {
   /* Sequence(Constructed): 293 bytes (0x125) */
   0x30, 0x82, 0x01, 0x25,
   /*Sequence(Constructed): 11 bytes (0xb) */
   0x30, 0x0b,
   /* ObjectId(Universal): 9 bytes (0x9) */
   0x06, 0x09,
   0x60, 0x86, 0x48, 0x01, 0x65, 0x02, 0x01, 0x01, 0x14,
   /* BitString(Universal): 276 bytes (0x114) */
   0x03, 0x82, 0x01, 0x14,
   0x00, 0x00, 0x01, 0xef, 0x04, 0x01, 0x00, 0x01,
   0x00, 0x00, 0x69, 0x60, 0x70, 0x00, 0x80, 0x02,
   0x2e, 0x46, 0xb9, 0xcb, 0x22, 0x72, 0x0b, 0x1c,
   0xe6, 0x25, 0x20, 0x16, 0x86, 0x05, 0x8e, 0x2b,
   0x98, 0xd1, 0x46, 0x3d, 0x00, 0xb8, 0x69, 0xe1,
   0x1a, 0x42, 0x7d, 0x7d, 0xb5, 0xbf, 0x9f, 0x26,
   0xd3, 0x2c, 0xb1, 0x73, 0x01, 0xb6, 0xb2, 0x6f,
   0x7b, 0xa5, 0x54, 0x85, 0x60, 0x77, 0x81, 0x8a,
   0x87, 0x86, 0xe0, 0x2d, 0xbf, 0xdb, 0x28, 0xe8,
   0xfa, 0x20, 0x35, 0xb4, 0xc0, 0x94, 0x10, 0x8e,
   0x1c, 0x58, 0xaa, 0x02, 0x60, 0x97, 0xf5, 0xb3,
   0x2f, 0xf8, 0x99, 0x29, 0x28, 0x73, 0x47, 0x36,
   0xdd, 0x1d, 0x78, 0x95, 0xeb, 0xb8, 0xec, 0x45,
   0x96, 0x69, 0x6f, 0x54, 0xc8, 0x1f, 0x2d, 0x3a,
   0xd9, 0x0e, 0x8e, 0xaa, 0x59, 0x11, 0x8c, 0x3b,
   0x8d, 0xa4, 0xed, 0xf2, 0x7d, 0xdc, 0x42, 0xaa,
   0xa4, 0xd2, 0x1c, 0xb9, 0x87, 0xd0, 0xd9, 0x3d,
   0x8e, 0x89, 0xbb, 0x06, 0x54, 0xcf, 0x32, 0x00,
   0x02, 0x00, 0x00, 0x80, 0x0b, 0x80, 0x6c, 0x0f,
   0x71, 0xd1, 0xa1, 0xa9, 0x26, 0xb4, 0xf1, 0xcd,
   0x6a, 0x7a, 0x09, 0xaa, 0x58, 0x28, 0xd7, 0x35,
   0x74, 0x8e, 0x7c, 0x83, 0xcb, 0xfe, 0x00, 0x3b,
   0x62, 0x00, 0xfb, 0x90, 0x37, 0xcd, 0x93, 0xcf,
   0xf3, 0xe4, 0x6d, 0x8d, 0xdd, 0xb8, 0x53, 0xe0,
   0x5c, 0xda, 0x1a, 0x7e, 0x56, 0x03, 0x95, 0x03,
   0x2f, 0x74, 0x86, 0xb1, 0xa0, 0xbb, 0x05, 0x91,
   0xe4, 0x76, 0x83, 0xe6, 0x62, 0xf9, 0x12, 0x64,
   0x5a, 0x62, 0xd8, 0x94, 0x04, 0x1f, 0x83, 0x02,
   0x2e, 0xc5, 0xa7, 0x17, 0x46, 0x46, 0x21, 0x96,
   0xc3, 0xa9, 0x8e, 0x92, 0x18, 0xd1, 0x52, 0x08,
   0x1d, 0xff, 0x8e, 0x24, 0xdb, 0x6c, 0xd8, 0xfe,
   0x80, 0x93, 0xe1, 0xa5, 0x4a, 0x0a, 0x37, 0x24,
   0x18, 0x07, 0xbe, 0x0f, 0xaf, 0x73, 0xea, 0x50,
   0x64, 0xa1, 0xb3, 0x77, 0xe5, 0x41, 0x02, 0x82,
   0x39, 0xb9, 0xe3, 0x94 
};

unsigned char valitity[] = {
   0x30, 0x1e,
   0x17, 0x0d,
   '2','0','0','0','0','1','0','1','0','0','0','0','Z',
   0x17, 0x0d,
   '2','0','0','5','1','2','0','1','0','0','0','0','Z'
};


unsigned char cnam_oid[] = { 0x06, 0x03, 0x55, 0x04, 0x03 };

unsigned char signature[] = {
    /* the OID */
    0x30, 0x0b, 0x06, 0x09,
    0x60, 0x86, 0x48, 0x01, 0x65, 0x02, 0x01, 0x01, 0x13,
    /* signature wrap */
    0x03, 0x29, 0x00,
    /* 40 byte dsa signature */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

unsigned char fortezza_oid [] = {
    0x60, 0x86, 0x48, 0x01, 0x65, 0x02, 0x01, 0x01, 0x13
};

unsigned char software_ou[] = {
    0x31, 26, 0x30, 24,
    0x06, 0x03, 0x55, 0x04, 0x0b,
    0x13, 17, 
      'S','o','f','t','w',
      'a','r','e',' ','F',
      'O','R','T','E','Z','Z','A'
};


char letterarray[] = {
	'a','b','c','d','e','f','g','h','i','j','k','l','m','n',
	'o','p','q','r','s','t','u','v','w','x','y','z' };

char constarray[] = {
	'b','c','d','f','g','h','j','k','l','m','n',
	'p','q','r','s','t','v','w','x','y','z' };

char vowelarray[] = {
	'a','e','i','o','u','y' };

char digitarray[] = {
	'0','1','2','3','4','5','6','7','8','9' };

unsigned long
getRandom(unsigned long max) {
	unsigned short data;
	unsigned long result;

	fort_GenerateRandom((unsigned char *)&data,sizeof(data));

	result = (unsigned long)data * max;
	result = result >> 16;
	return result;
}


char getLetter(void)
{
   return letterarray[getRandom(sizeof(letterarray))];
}
char getVowel(void)
{
   return vowelarray[getRandom(sizeof(vowelarray))];
}
char getDigit(void)
{
   return digitarray[getRandom(sizeof(digitarray))];
}

char getConst(void)
{
   return constarray[getRandom(sizeof(constarray))];
}

char *getPinPhrase(void)
{
    char * pass = PORT_ZAlloc(5);

    pass[0] = getDigit();
    pass[1] = getDigit();
    pass[2] = getDigit();
    pass[3] = getDigit();

    return pass;
}

char *getPassPhrase(void)
{
    char * pass = PORT_ZAlloc(13);

    pass[0] = getConst()+'A'-'a';
    pass[1] = getVowel();
    pass[2] = getConst();
    pass[3] = getVowel();
    pass[4] = getConst();
    pass[5] = getVowel();
    pass[6] = getConst();
    pass[7] = getDigit();
    pass[8] = getDigit();
    pass[9] = getDigit();
    pass[10] = getDigit();
    pass[11] = getLetter()+'A'-'a';

    return pass;
}

extern void
makeCertSlot(fortSlotEntry *   entry,
		int            index,
		char *         label,
		SECItem *      cert,
 		FORTSkipjackKeyPtr Ks, 
		unsigned char *xKEA, 
		unsigned char *xDSA, 
 		unsigned char *pubKey, 
		int            pubKeyLen, 
		unsigned char *p, 
		unsigned char *q, 
		unsigned char *g);

extern void
makeProtectedPhrase(FORTSWFile *     file, 
		fortProtectedPhrase *prot_phrase,
  	     	FORTSkipjackKeyPtr   Ks, 
		FORTSkipjackKeyPtr   Kinit, 
		char *               phrase);

extern void
fill_in(SECItem *item, unsigned char *data, int len);

char *userLabel = "INKS0002                            ";
int main(int argc, char **argv)
{
	char *progname = *argv++;
	char *commonName = NULL;
	char *caname = NULL;
	char *email = NULL;
	char *outname = NULL;
	char *cp;
	int arg_count = 0;
	Cert caCert;
        SECItem userCert;
	int cirv,i;
	int cards, start;
	unsigned char *subject;
	int            subject_len;
	int            signature_len = sizeof(signature);
	int            newSubject_len, newCertBody_len, len;
	int            cname1_len, cname_len, pstring_len;
	int            valitity_len = sizeof(valitity);
	unsigned char origCert[CERT_SIZE];
	unsigned char newSubject[CERT_SIZE];
	unsigned char newCertBody[CERT_SIZE];
	unsigned char newCert[CERT_SIZE];
	unsigned char pstring[CERT_SIZE];
	unsigned char cname1[CERT_SIZE];
	unsigned char cname[CERT_SIZE];
	CERTCertificate *myCACert = NULL;
	CERTCertificate *cert;
	CERTCertDBHandle *certhandle;
	SECStatus rv;
	unsigned char serial[16];
	SECKEYPublicKey *pubKey;
	DSAPrivateKey *keaPrivKey;
	DSAPrivateKey *dsaPrivKey;
	CI_RANDOM randomVal;
	PQGParams *params;
	int pca_index = -1;
	unsigned char *p,*q,*g;
	FORTSkipjackKey Ks;
	FORTSkipjackKey Kinit;
        FORTSWFile *file;
        FORTSignedSWFile *signed_file;
        FORTSignedSWFile *signed_file2;
	unsigned char random[20];
	unsigned char vers;
	unsigned char *data;
	char *transportPin=NULL;
	char *ssoMemPhrase=NULL;
	char *userMemPhrase=NULL;
	char *ssoPin=NULL;
	char *userPin=NULL;
	char *pass=NULL;
	SECItem *outItem;
	int email_len = 0;
	int emailAVA_len = 0;

   
    /* put better argument parsing here */
    while ((cp = *argv++) != NULL) {
	if (*cp == '-') {
	    while (*++cp) {
		switch (*cp) {
		/* verbose mode */
		case 'e':
		    email = *argv++;
		    break;
		/* explicitly set the target */
		case 'o':
		    outname = *argv++;
		    break;
		case 't':
		/* provide password on command line */
		    transportPin = *argv++;
		    break;
		case 'u':
		/* provide user password on command line */
		    userPin = *argv++;
		    break;
		case 'U':
		/* provide user password on command line */
		    userMemPhrase = *argv++;
		    break;
		case 's':
		/* provide user password on command line */
		    ssoPin = *argv++;
		    break;
		case 'S':
		/* provide user password on command line */
		    ssoMemPhrase = *argv++;
		    break;
		case 'p':
		/* provide card password on command line */
		    pass = *argv++;
		    break;
		case 'd':
		    transportPin="test1234567890";
		    ssoMemPhrase="sso1234567890";
		    userMemPhrase="user1234567890";
		    ssoPin="9999";
		    userPin="0000";
		    break;
		default:
		    usage(progname);
		    break;
		}
	    }
	} else switch (arg_count++) {
	    case 0:
		commonName = cp;
		break;
	    case 1:
		caname = cp;
		break;
	    default:
		usage(progname);
	} 
    }

    if (outname == NULL) outname = "swfort.sfi";
    if (caname == NULL) usage(progname);



	caCert.card = -1;
	memset(newCert,0,CERT_SIZE);

	if (commonName == NULL) usage(progname);
	

	cirv = CI_Initialize(&cards);

        start = 0;
	for (i=0; i < cards; i++) {
	    cirv = InitCard(i+1,pass);
	    if (cirv == CI_OK) {
	      if (FoundCert(i+1,caname,&caCert)) {
		break;
	      }
	    }
	}
	   
	if (caCert.card == -1) {
	   fprintf(stderr,
	"WARNING: Couldn't find Signing CA...new cert will not be signed\n");
	}


	/*
	 * initialize enough security to deal with certificates.
	 */
	NSS_NoDB_Init(NULL);
	certhandle = CERT_GetDefaultCertDB();
	if (certhandle == NULL) {
	    Terminate("Couldn't build temparary Cert Database", 
						1, -1, caCert.card);
	    exit(1);
	}

	CI_GenerateRandom(random);
	RNG_RandomUpdate(random,sizeof(random));
	CI_GenerateRandom(random);
	RNG_RandomUpdate(random,sizeof(random));


    if (transportPin == NULL) transportPin = getPassPhrase();
    if (ssoMemPhrase == NULL) ssoMemPhrase = getPassPhrase();
    if (userMemPhrase == NULL) userMemPhrase = getPassPhrase();
    if (ssoPin == NULL) ssoPin = getPinPhrase();
    if (userPin == NULL) userPin = getPinPhrase();



	/* now dump the certs into the temparary data base */
	for (i=0; i < caCert.count; i++) {
    	    SECItem derCert;

	    cirv = CI_Select(caCert.card);
	    if (cirv != CI_OK) {
		Terminate("Couldn't select on CA card",cirv, 
					-1, caCert.card);
	    }
	    cirv = CI_GetCertificate(caCert.valid[i].index,origCert);
            if (cirv != CI_OK) {
		continue;
	    }
	    derCert.data = origCert;
	    derCert.len = Cert_length(origCert, sizeof(origCert));
	    cert = 
	(CERTCertificate *)CERT_NewTempCertificate(certhandle,&derCert, NULL, 
							PR_FALSE, PR_TRUE);
	    caCert.valid[i].cert = cert;
	    if (cert == NULL) continue;
            if (caCert.valid[i].index == caCert.index) myCACert=cert;
	    if (caCert.valid[i].index == atoi((char *)&caCert.label[4]))
				 pca_index = i;
	}

	if (myCACert == NULL) {
	   Terminate("Couldn't find CA's Certificate", 1, -1, caCert.card);
	   exit(1);
	}

	
        /*
	 * OK now build the user cert.
	 */
	/* first get the serial number and KMID */
        cirv = CI_GenerateRandom(randomVal);
	memcpy(&header[2],randomVal,sizeof(serial));
	memcpy(serial,randomVal,sizeof(serial));
	memcpy(&key[KEY_START+KMID_OFFSET],randomVal+sizeof(serial),7);
							 /* KMID */

	/* now generate the keys */
	pubKey = CERT_ExtractPublicKey(myCACert);
	if (pubKey == NULL) {
	   Terminate("Couldn't extract CA's public key", 
						1, -1, caCert.card);
	   exit(1);
	}


	switch (pubKey->keyType) {
	case fortezzaKey:
	    params = (PQGParams *)&pubKey->u.fortezza.params;
	    break;
	case dsaKey:
	    params = (PQGParams *)&pubKey->u.dsa.params;
	    break;
	default:
	   Terminate("Certificate is not a fortezza or DSA Cert",
					1, -1, caCert.card);
	   exit(1);
	}

	rv = DSA_NewKey(params,&keaPrivKey);
	if (rv != SECSuccess) {
	   Terminate("Couldn't Generate KEA key", 
					PORT_GetError(), -1, caCert.card);
	   exit(1);
	}
	rv = DSA_NewKey(params,&dsaPrivKey);
	if (rv != SECSuccess) {
	   Terminate("Couldn't Generate DSA key", 
					PORT_GetError(), -1, caCert.card);
	   exit(1);
	}

	if (keaPrivKey->publicValue.len == 129) 
					keaPrivKey->publicValue.data++;
	if (dsaPrivKey->publicValue.len == 129) 
					dsaPrivKey->publicValue.data++;
	if (keaPrivKey->privateValue.len == 21) 
					keaPrivKey->privateValue.data++;
	if (dsaPrivKey->privateValue.len == 21) 
					dsaPrivKey->privateValue.data++;

	/* save the parameters */
	p = params->prime.data;
	if (params->prime.len == 129) p++;
	q = params->subPrime.data;
	if (params->subPrime.len == 21) q++;
	g = params->base.data;
	if (params->base.len == 129) g++;

	memcpy(&key[KEY_START+KEA_OFFSET],
			keaPrivKey->publicValue.data,
			keaPrivKey->publicValue.len);
	memcpy(&key[KEY_START+DSA_OFFSET],
			dsaPrivKey->publicValue.data,
			dsaPrivKey->publicValue.len);

	/* build the der subject */
	subject = data_start(myCACert->derSubject.data,myCACert->derSubject.len,
		&subject_len);

	/* build the new Common name AVA */
	len = DER_Sequence(pstring,strlen(commonName));
	memcpy(pstring+len,commonName,strlen(commonName));
					 len += strlen(commonName);
	pstring_len = len;
	pstring[0] = 0x13;

	len = DER_Sequence(cname1,sizeof(cnam_oid)+pstring_len);
	memcpy(cname1+len,cnam_oid,sizeof(cnam_oid)); len += sizeof(cnam_oid);
	memcpy(cname1+len,pstring,pstring_len); len += pstring_len;
	cname1_len = len;

	len = DER_Sequence(cname, cname1_len);
	memcpy(cname+len,cname1,cname1_len); len += cname1_len;
	cname_len = len;
	cname[0] = 0x31; /* make it a set rather than a sequence */

	if (email) {
	    email_len = strlen(email);
	    emailAVA_len = EMAIL_DATA_START + email_len;
	}

	/* now assemble it */
	len = DER_Sequence(newSubject,subject_len + sizeof(software_ou) +
				cname_len + emailAVA_len);
	memcpy(newSubject+len,subject,subject_len);

	for (i=0; i < subject_len; i++) {
	   if (memcmp(newSubject+len+i,cnam_oid,sizeof(cnam_oid)) == 0) {
		newSubject[i+len+4] = 0x0b; /* change CN to OU */
		break;
	   }
	}
	len += subject_len;
	memcpy(newSubject+len,software_ou,sizeof(software_ou)); 
						len += sizeof(software_ou);
	memcpy(newSubject+len,cname,cname_len); len += cname_len;
	newSubject_len = len;

	/*
	 * build the email AVA
	 */
	if (email) {
	    memcpy(&emailAVA[EMAIL_DATA_START],email,email_len);
	    for (i=0; i < offsetCount; i++) {
		emailAVA[emailOffset[i]] += email_len;
	    }
	    memcpy(newSubject+len,emailAVA,emailAVA_len);
	    newSubject_len += emailAVA_len;
	}


	/*
	 * Assemble the Cert
	 */

	len = DER_Sequence(newCertBody,sizeof(header)+newSubject_len+
	   valitity_len+myCACert->derSubject.len+sizeof(key));
	memcpy(newCertBody+len,header,sizeof(header));len += sizeof(header);
	memcpy(newCertBody+len,myCACert->derSubject.data,
		myCACert->derSubject.len);len += myCACert->derSubject.len;
	memcpy(newCertBody+len,valitity,valitity_len);len += valitity_len;
	memcpy(newCertBody+len,newSubject,newSubject_len);
						len += newSubject_len;
	memcpy(newCertBody+len,key,sizeof(key));len += sizeof(key);
	newCertBody_len = len;


	/*
	 * generate the hash
	 */
	cirv = CI_InitializeHash();
	if (cirv == CI_OK) {
	    int hash_left = newCertBody_len & 63;
	    int hash_len = newCertBody_len - hash_left;
	    cirv = CI_Hash(hash_len,newCertBody);
	    if (cirv == CI_OK) {
		cirv = CI_GetHash(hash_left,newCertBody+hash_len,hash);
	    }
	}

	/*
	 * now sign the hash
	 */
	if ((cirv == CI_OK) && (caCert.card != -1)) {
	    cirv = CI_Select(caCert.card);
	    if (cirv == CI_OK) {
		cirv = CI_SetPersonality(caCert.index);
		if (cirv == CI_OK) {
		    cirv = CI_Sign(hash,sig);
		}
	    }
	} else cirv = -1;

	if (cirv != CI_OK) {
	   memcpy(sig,hash,sizeof(hash));
	}

	/*
	 * load in new signature
	 */
	{
	    int sig_len;
	    unsigned char *sig_start = 
			GetSignature(signature,signature_len,&sig_len);
	    memcpy(sig_start,sig,sizeof(sig));
	}

	/*
	 * now do the final wrap
	 */
	len = DER_Sequence(newCert,newCertBody_len+signature_len);
	memcpy(newCert+len,newCertBody,newCertBody_len); len += newCertBody_len;
	memcpy(newCert+len, signature, signature_len); len +=signature_len;
	userCert.data = newCert;
	userCert.len = len;


	/* OK, we now have our cert, let's go build our software file */
	signed_file = PORT_ZNew(FORTSignedSWFile);
	file = &signed_file->file;

	signed_file->signatureWrap.signature.data  = PORT_ZAlloc(40);
	signed_file->signatureWrap.signature.len  = 40;
	signed_file->signatureWrap.signatureAlgorithm.algorithm.data  =
                                       fortezza_oid;
	signed_file->signatureWrap.signatureAlgorithm.algorithm.len = 
					sizeof(fortezza_oid);

        vers = 1;
	fill_in(&file->version,&vers,1);
	file->derIssuer.data = myCACert->derSubject.data;
	file->derIssuer.len = myCACert->derSubject.len;
	file->serialID.data = serial;
	file->serialID.len =sizeof(serial);
	/* generate out Ks value */
	fort_GenerateRandom(Ks,sizeof(Ks));
	makeProtectedPhrase(file,&file->initMemPhrase,Kinit,NULL,transportPin);
	makeProtectedPhrase(file,&file->ssoMemPhrase,Ks,Kinit,ssoMemPhrase);
	makeProtectedPhrase(file,&file->ssoPinPhrase,Ks,Kinit,ssoPin);
	makeProtectedPhrase(file,&file->userMemPhrase,Ks,Kinit,userMemPhrase);
	makeProtectedPhrase(file,&file->userPinPhrase,Ks,Kinit,userPin);
	file->wrappedRandomSeed.data = PORT_ZAlloc(12);
	file->wrappedRandomSeed.len = 12;
        cirv = fort_GenerateRandom(file->wrappedRandomSeed.data,10);
	if (cirv != CI_OK) {
	   Terminate("Couldn't get Random Seed", 
					cirv, -1, caCert.card);
	}
	fort_skipjackWrap(Ks,12,file->wrappedRandomSeed.data,
                                file->wrappedRandomSeed.data);
	file->slotEntries = PORT_ZAlloc(sizeof(fortSlotEntry *)*5);
	/* paa */
	file->slotEntries[0] = PORT_ZNew(fortSlotEntry);
	makeCertSlot(file->slotEntries[0],0,
				(char *)caCert.valid[0].label,
				&caCert.valid[0].cert->derCert,
						Ks,NULL,NULL,NULL,0,p,q,g);
	/* pca */
	file->slotEntries[1] = PORT_ZNew(fortSlotEntry);
	makeCertSlot(file->slotEntries[1],1,
				(char *)caCert.valid[pca_index].label,
				&caCert.valid[pca_index].cert->derCert,
						Ks,NULL,NULL,NULL,0,p,q,g);
	/* ca */
	file->slotEntries[2] = PORT_ZNew(fortSlotEntry);
	/* make sure the caCert lable points to our new pca slot location */
	caCert.label[4] = '0';
	caCert.label[5] = '0';
	caCert.label[6] = '0';
	caCert.label[7] = '1';
	makeCertSlot(file->slotEntries[2],2,(char *)caCert.label,
				&myCACert->derCert,Ks,NULL,NULL,NULL,0,p,q,g);
	/* user */
	file->slotEntries[3] = PORT_ZNew(fortSlotEntry);
	strncpy(&userLabel[8],commonName,sizeof(CI_PERSON)-8);
	makeCertSlot(file->slotEntries[3],3,userLabel,&userCert,Ks,
		keaPrivKey->privateValue.data,
		dsaPrivKey->privateValue.data,
		key, sizeof(key), p, q, g);
	file->slotEntries[4] = 0;

	/* encode the file so we can sign it */
	outItem = FORT_PutSWFile(signed_file);

	/* get the der encoded data to sign */
	signed_file2 = FORT_GetSWFile(outItem);

	/* now sign it */
	len = signed_file2->signatureWrap.data.len;
	data = signed_file2->signatureWrap.data.data;
	/*
	 * generate the hash
	 */
	cirv = CI_InitializeHash();
	if (cirv == CI_OK) {
	    int hash_left = len & 63;
	    int hash_len = len - hash_left;
	    cirv = CI_Hash(hash_len,data);
	    if (cirv == CI_OK) {
		cirv = CI_GetHash(hash_left,data+hash_len,hash);
	    }
	}

	/*
	 * now sign the hash
	 */
	if ((cirv == CI_OK) && (caCert.card != -1)) {
	    cirv = CI_Select(caCert.card);
	    if (cirv == CI_OK) {
		cirv = CI_SetPersonality(caCert.index);
		if (cirv == CI_OK) {
		    cirv = CI_Sign(hash,sig);
		}
	    }
	} else cirv = -1;

	if (cirv != CI_OK) {
	   memcpy(sig,hash,sizeof(hash));
	}
	memcpy( signed_file->signatureWrap.signature.data,sig,sizeof(sig));
	signed_file->signatureWrap.signature.len = sizeof(sig)*8; 


	/* encode it for the last time */
	outItem = FORT_PutSWFile(signed_file);


	/*
	 * write it out to the .sfi file
	 */
	{
	    int fd = open(outname,O_WRONLY|O_CREAT|O_BINARY,0777);

	    write(fd,outItem->data,outItem->len);
	    close(fd);
	}
	CI_Close(CI_POWER_DOWN_FLAG,caCert.card);
	CI_Terminate();

	printf("Wrote %s to file %s.\n",commonName,outname);
	printf("Initialization Memphrase: %s\n",transportPin);
	printf("SSO Memphrase: %s\n",ssoMemPhrase);
	printf("User Memphrase: %s\n",userMemPhrase);
	printf("SSO pin: %s\n",ssoPin);
	printf("User pin: %s\n",userPin);

	return 0;
}

