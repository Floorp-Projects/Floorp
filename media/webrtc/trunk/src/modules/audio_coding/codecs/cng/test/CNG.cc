/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * CNG.cpp : Defines the entry point for the console application.
 */

#include <stdlib.h>
#include <string.h>
#include "stdafx.h"
#include "webrtc_cng.h"
#include "webrtc_vad.h"

CNG_enc_inst *e_inst; 
CNG_dec_inst *d_inst;

VadInst *vinst;
//#define ASSIGN

short anaSpeech[WEBRTC_CNG_MAX_OUTSIZE_ORDER], genSpeech[WEBRTC_CNG_MAX_OUTSIZE_ORDER], state[WEBRTC_CNG_MAX_OUTSIZE_ORDER];
unsigned char SIDpkt[114];

int main(int argc, char* argv[])
{
    FILE * infile, *outfile, *statefile;
    short res=0,errtype;
    /*float time=0.0;*/
    
    WebRtcVad_Create(&vinst);
    WebRtcVad_Init(vinst);
    
    short size;
    int samps=0;

    if (argc < 5){
        printf("Usage:\n CNG.exe infile outfile samplingfreq(Hz) interval(ms) order\n\n");
        return(0);
    }

    infile=fopen(argv[1],"rb");
    if (infile==NULL){
        printf("file %s does not exist\n",argv[1]);
        return(0);
    }
    outfile=fopen(argv[2],"wb");
    statefile=fopen("CNGVAD.d","wb");
    if (outfile==NULL){
        printf("file %s could not be created\n",argv[2]);
        return(0);
    }

    unsigned int fs=16000;
    short frameLen=fs/50;

#ifndef ASSIGN
    res=WebRtcCng_CreateEnc(&e_inst);
    if (res < 0) {
        /* exit if returned with error */
        errtype=WebRtcCng_GetErrorCodeEnc(e_inst);
        fprintf(stderr,"\n\n Error in initialization: %d.\n\n", errtype);
        exit(EXIT_FAILURE);
    }
    res=WebRtcCng_CreateDec(&d_inst);
    if (res < 0) {
        /* exit if returned with error */
        errtype=WebRtcCng_GetErrorCodeDec(d_inst);
        fprintf(stderr,"\n\n Error in initialization: %d.\n\n", errtype);
        exit(EXIT_FAILURE);
    }

#else

    // Test the Assign-functions
    int Esize, Dsize;
    void *Eaddr, *Daddr;

    res=WebRtcCng_AssignSizeEnc(&Esize);
    res=WebRtcCng_AssignSizeDec(&Dsize);
    Eaddr=malloc(Esize);
    Daddr=malloc(Dsize);

    res=WebRtcCng_AssignEnc(&e_inst, Eaddr);
    if (res < 0) {
        /* exit if returned with error */
        errtype=WebRtcCng_GetErrorCodeEnc(e_inst);
        fprintf(stderr,"\n\n Error in initialization: %d.\n\n", errtype);
        exit(EXIT_FAILURE);
    }

    res=WebRtcCng_AssignDec(&d_inst, Daddr);
    if (res < 0) {
        /* exit if returned with error */
        errtype=WebRtcCng_GetErrorCodeDec(d_inst);
        fprintf(stderr,"\n\n Error in initialization: %d.\n\n", errtype);
        exit(EXIT_FAILURE);
    }

#endif

    res=WebRtcCng_InitEnc(e_inst,atoi(argv[3]),atoi(argv[4]),atoi(argv[5]));
    if (res < 0) {
        /* exit if returned with error */
        errtype=WebRtcCng_GetErrorCodeEnc(e_inst);
        fprintf(stderr,"\n\n Error in initialization: %d.\n\n", errtype);
        exit(EXIT_FAILURE);
    }

    res=WebRtcCng_InitDec(d_inst);
    if (res < 0) {
        /* exit if returned with error */
        errtype=WebRtcCng_GetErrorCodeDec(d_inst);
        fprintf(stderr,"\n\n Error in initialization: %d.\n\n", errtype);
        exit(EXIT_FAILURE);
    }

    static bool firstSilent=true;

    int numSamp=0;
    int speech=0;
    int silent=0;
    long cnt=0;

    while(fread(anaSpeech,2,frameLen,infile)==frameLen){

        cnt++;
        if (cnt==60){
            cnt=60;
        }
        /*  time+=(float)frameLen/fs;
        numSamp+=frameLen;
        float temp[640];
        for(unsigned int j=0;j<frameLen;j++)
        temp[j]=(float)anaSpeech[j]; */

        //        if(!WebRtcVad_Process(vinst, fs, anaSpeech, frameLen)){


        if(1){ // Do CNG coding of entire file

            //        if(!((anaSpeech[0]==0)&&(anaSpeech[1]==0)&&(anaSpeech[2]==0))){
            if(firstSilent){
                res = WebRtcCng_Encode(e_inst, anaSpeech, frameLen/2, SIDpkt,&size,1);
                if (res < 0) {
                    /* exit if returned with error */
                    errtype=WebRtcCng_GetErrorCodeEnc(e_inst);
                    fprintf(stderr,"\n\n Error in encoder: %d.\n\n", errtype);
                    exit(EXIT_FAILURE);
                }

                firstSilent=false;

                res=WebRtcCng_Encode(e_inst, &anaSpeech[frameLen/2], frameLen/2, SIDpkt,&size,1);
                if (res < 0) {
                    /* exit if returned with error */
                    errtype=WebRtcCng_GetErrorCodeEnc(e_inst);
                    fprintf(stderr,"\n\n Error in encoder: %d.\n\n", errtype);
                    exit(EXIT_FAILURE);
                }

            }
            else{
                res=WebRtcCng_Encode(e_inst, anaSpeech, frameLen/2, SIDpkt,&size,0);
                if (res < 0) {
                    /* exit if returned with error */
                    errtype=WebRtcCng_GetErrorCodeEnc(e_inst);
                    fprintf(stderr,"\n\n Error in encoder: %d.\n\n", errtype);
                    exit(EXIT_FAILURE);
                }
                res=WebRtcCng_Encode(e_inst, &anaSpeech[frameLen/2], frameLen/2, SIDpkt,&size,0);
                if (res < 0) {
                    /* exit if returned with error */
                    errtype=WebRtcCng_GetErrorCodeEnc(e_inst);
                    fprintf(stderr,"\n\n Error in encoder: %d.\n\n", errtype);
                    exit(EXIT_FAILURE);
                }
            }

            if(size>0){
                res=WebRtcCng_UpdateSid(d_inst,SIDpkt, size);
                if (res < 0) {
                    /* exit if returned with error */
                    errtype=WebRtcCng_GetErrorCodeDec(d_inst);
                    fprintf(stderr,"\n\n Error in decoder: %d.\n\n", errtype);
                    exit(EXIT_FAILURE);
                }
            }
            res=WebRtcCng_Generate(d_inst,genSpeech, frameLen,0);
                if (res < 0) {
                    /* exit if returned with error */
                    errtype=WebRtcCng_GetErrorCodeDec(d_inst);
                    fprintf(stderr,"\n\n Error in decoder: %d.\n\n", errtype);
                    exit(EXIT_FAILURE);
                }
            memcpy(state,anaSpeech,2*frameLen);
        }
        else{
            firstSilent=true;
            memcpy(genSpeech,anaSpeech,2*frameLen);

            memset(anaSpeech,0,frameLen*2);
            memset(state,0,frameLen*2);

        }
        if (fwrite(genSpeech, 2, frameLen,
                   outfile) != static_cast<size_t>(frameLen)) {
          return -1;
        }
        if (fwrite(state, 2, frameLen,
                   statefile) != static_cast<size_t>(frameLen)) {
          return -1;
        }
    }
    fclose(infile);
    fclose(outfile);
    fclose(statefile);
    return 0;
}
