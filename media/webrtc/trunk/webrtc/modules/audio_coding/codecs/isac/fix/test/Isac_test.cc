/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************
	Stand Alone test application for ISACFIX and ISAC LC

******************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "typedefs.h"

#include "isacfix.h"
ISACFIX_MainStruct *ISACfix_inst;

#define FS								16000


typedef struct {
	uint32_t arrival_time;            /* samples */
	uint32_t sample_count;            /* samples */
	uint16_t rtp_number;
} BottleNeckModel;

void get_arrival_time(int current_framesamples,   /* samples */
					  int packet_size,            /* bytes */
					  int bottleneck,             /* excluding headers; bits/s */
					  BottleNeckModel *BN_data)
{
	const int HeaderSize = 35; 
	int HeaderRate;

	HeaderRate = HeaderSize * 8 * FS / current_framesamples;     /* bits/s */

	/* everything in samples */
	BN_data->sample_count = BN_data->sample_count + current_framesamples;

	BN_data->arrival_time += ((packet_size + HeaderSize) * 8 * FS) / (bottleneck + HeaderRate);

	if (BN_data->arrival_time < BN_data->sample_count)
		BN_data->arrival_time = BN_data->sample_count;

	BN_data->rtp_number++;
}

/*
#ifdef __cplusplus
extern "C" {
#endif
*/
int main(int argc, char* argv[]){ 

	/* Parameters */
	FILE *pInFile, *pOutFile, *pChcFile; 
	int8_t inFile[40];
	int8_t outFile[40];
	int8_t chcFile[40];
	int8_t codec[10];
	int16_t bitrt, spType, size;
	uint16_t frameLen;
	int16_t sigOut[1000], sigIn[1000]; 
	uint16_t bitStream[500]; /* double to 32 kbps for 60 ms */

	int16_t chc, ok;
	int noOfCalls, cdlen;
	int16_t noOfLostFrames;
	int err, errtype;

	BottleNeckModel       BN_data;

	int totalbits =0;
	int totalsmpls =0;

	/*End Parameters*/

	
	if ((argc==6)||(argc==7) ){

		strcpy(codec,argv[5]);

		if(argc==7){
			if (!_stricmp("isac",codec)){
				bitrt = atoi(argv[6]);
				if ( (bitrt<10000)&&(bitrt>32000)){
					printf("Error: Supported bit rate in the range 10000-32000 bps!\n");
					exit(-1);
				}

			}else{
	      printf("Error: Codec not recognized. Check spelling!\n");
	      exit(-1);
			}

		} else { 
				printf("Error: Codec not recognized. Check spelling!\n");
				exit(-1);
		}
	} else {
		printf("Error: Wrong number of input parameter!\n\n");
		exit(-1);
	}
	
	frameLen = atoi(argv[4]);
	strcpy(chcFile,argv[3]);
	strcpy(outFile,argv[2]);
	strcpy(inFile,argv[1]);

	/*  Open file streams */
	if( (pInFile = fopen(inFile,"rb")) == NULL ) {
		printf( "Error: Did not find input file!\n" );
		exit(-1);
	}
	strcat(outFile,"_");
	strcat(outFile, argv[4]);
	strcat(outFile,"_");
	strcat(outFile, codec);
	
	if (argc==7){
		strcat(outFile,"_");
		strcat(outFile, argv[6]);
	}
	if (_stricmp("none", chcFile)){
		strcat(outFile,"_");
		strcat(outFile, "plc");
	}
	
	strcat(outFile, ".otp");
	
	if (_stricmp("none", chcFile)){
		if( (pChcFile = fopen(chcFile,"rb")) == NULL ) {
			printf( "Error: Did not find channel file!\n" );
			exit(-1);
		}
	}
    /******************************************************************/
	if (!_stricmp("isac", codec)){    /*    ISAC   */
		if ((frameLen!=480)&&(frameLen!=960)) {
			printf("Error: ISAC only supports 480 and 960 samples per frame (not %d)\n", frameLen);
			exit(-1);
		}
		if( (pOutFile = fopen(outFile,"wb")) == NULL ) {
			printf( "Could not open output file!\n" );
			exit(-1);
		}
		ok=WebRtcIsacfix_Create(&ISACfix_inst);
		if (ok!=0) {
			printf("Couldn't allocate memory for iSAC fix instance\n");
			exit(-1);
		}

		BN_data.arrival_time  = 0;
		BN_data.sample_count  = 0;
		BN_data.rtp_number    = 0;

		WebRtcIsacfix_EncoderInit(ISACfix_inst,1);
		WebRtcIsacfix_DecoderInit(ISACfix_inst);
		err = WebRtcIsacfix_Control(ISACfix_inst, bitrt, (frameLen>>4));
		if (err < 0) {
				/* exit if returned with error */
				errtype=WebRtcIsacfix_GetErrorCode(ISACfix_inst);
				printf("\n\n Error in initialization: %d.\n\n", errtype);
				exit(EXIT_FAILURE);
			}
		/* loop over frame */
		while (fread(sigIn,sizeof(int16_t),frameLen,pInFile) == frameLen) {
			
			noOfCalls=0;
			cdlen=0;
			while (cdlen<=0) {
				cdlen=WebRtcIsacfix_Encode(ISACfix_inst,&sigIn[noOfCalls*160],(int16_t*)bitStream);
				if(cdlen==-1){
					errtype=WebRtcIsacfix_GetErrorCode(ISACfix_inst);
					printf("\n\nError in encoder: %d.\n\n", errtype);
					exit(-1);
				}
				noOfCalls++;
			}
	
	
			if(_stricmp("none", chcFile)){
				if (fread(&chc,sizeof(int16_t),1,pChcFile)!=1) /* packet may be lost */
					break;
			} else {
				chc = 1; /* packets never lost */
			}
					
			/* simulate packet handling through NetEq and the modem */
			get_arrival_time(frameLen, cdlen, bitrt, &BN_data);
			
			if (chc){ /* decode */

				err = WebRtcIsacfix_UpdateBwEstimate1(ISACfix_inst,
								  bitStream,
								  cdlen,
								  BN_data.rtp_number,
								  BN_data.arrival_time);

				if (err < 0) {
					/* exit if returned with error */
					errtype=WebRtcIsacfix_GetErrorCode(ISACfix_inst);
					printf("\n\nError in decoder: %d.\n\n", errtype);
					exit(EXIT_FAILURE);
				}
				size = WebRtcIsacfix_Decode(ISACfix_inst, bitStream, cdlen, sigOut, &spType);
				if(size<=0){
					/* exit if returned with error */
					errtype=WebRtcIsacfix_GetErrorCode(ISACfix_inst);
					printf("\n\nError in decoder: %d.\n\n", errtype);
					exit(-1);
				}
			} else { /* PLC */
				if (frameLen == 480){
					noOfLostFrames = 1;
				} else{
					noOfLostFrames = 2;
				}
				size = WebRtcIsacfix_DecodePlc(ISACfix_inst, sigOut, noOfLostFrames );
				if(size<=0){
					errtype=WebRtcIsacfix_GetErrorCode(ISACfix_inst);
					printf("\n\nError in decoder: %d.\n\n", errtype);
					exit(-1);
				}
			}
				
			/* Write decoded speech to file */
			fwrite(sigOut,sizeof(short),size,pOutFile);

		totalbits += 8 * cdlen;
		totalsmpls += size;

		}
	/******************************************************************/
	}

//	printf("\n\ntotal bits				= %d bits", totalbits);
	printf("\nmeasured average bitrate		= %0.3f kbits/s", (double)totalbits * 16 / totalsmpls);
	printf("\n");


	fclose(pInFile);
	fclose(pOutFile);
	if (_stricmp("none", chcFile)){
		fclose(pChcFile);
	}

	if (!_stricmp("isac", codec)){
		WebRtcIsacfix_Free(ISACfix_inst);
	}
									 			
	return 0;

}
