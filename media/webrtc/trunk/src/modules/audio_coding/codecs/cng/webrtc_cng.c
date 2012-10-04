/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <string.h>
#include <stdlib.h>

#include "webrtc_cng.h"
#include "signal_processing_library.h"
#include "cng_helpfuns.h"
#include "stdio.h"


typedef struct WebRtcCngDecInst_t_ {

    WebRtc_UWord32 dec_seed;
    WebRtc_Word32 dec_target_energy;
    WebRtc_Word32 dec_used_energy;
    WebRtc_Word16 dec_target_reflCoefs[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 dec_used_reflCoefs[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 dec_filtstate[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 dec_filtstateLow[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 dec_Efiltstate[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 dec_EfiltstateLow[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 dec_order;
    WebRtc_Word16 dec_target_scale_factor; /*Q29*/
    WebRtc_Word16 dec_used_scale_factor;  /*Q29*/
    WebRtc_Word16 target_scale_factor; /* Q13 */
    WebRtc_Word16 errorcode;
    WebRtc_Word16 initflag; 

} WebRtcCngDecInst_t;


typedef struct WebRtcCngEncInst_t_ {

    WebRtc_Word16 enc_nrOfCoefs;
    WebRtc_Word16 enc_sampfreq;
    WebRtc_Word16 enc_interval;
    WebRtc_Word16 enc_msSinceSID;
    WebRtc_Word32 enc_Energy;
    WebRtc_Word16 enc_reflCoefs[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word32 enc_corrVector[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 enc_filtstate[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 enc_filtstateLow[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_UWord32 enc_seed;    
    WebRtc_Word16 errorcode;
    WebRtc_Word16 initflag;

} WebRtcCngEncInst_t;

const WebRtc_Word32 WebRtcCng_kDbov[94]={
    1081109975,  858756178,  682134279,  541838517,  430397633,  341876992,
    271562548,  215709799,  171344384,  136103682,  108110997,   85875618,
    68213428,   54183852,   43039763,   34187699,   27156255,   21570980,
    17134438,   13610368,   10811100,    8587562,    6821343,    5418385,
    4303976,    3418770,    2715625,    2157098,    1713444,    1361037,
    1081110,     858756,     682134,     541839,     430398,     341877,
    271563,     215710,     171344,     136104,     108111,      85876,
    68213,      54184,      43040,      34188,      27156,      21571,
    17134,      13610,      10811,       8588,       6821,       5418,
    4304,       3419,       2716,       2157,       1713,       1361,
    1081,        859,        682,        542,        430,        342,
    272,        216,        171,        136,        108,         86, 
    68,         54,         43,         34,         27,         22, 
    17,         14,         11,          9,          7,          5, 
    4,          3,          3,          2,          2,           1, 
    1,          1,          1,          1
};
const WebRtc_Word16 WebRtcCng_kCorrWindow[WEBRTC_CNG_MAX_LPC_ORDER] = {
    32702, 32636, 32570, 32505, 32439, 32374, 
    32309, 32244, 32179, 32114, 32049, 31985
}; 

/****************************************************************************
 * WebRtcCng_Version(...)
 *
 * These functions returns the version name (string must be at least
 * 500 characters long)
 *
 * Output:
 *      - version       : Pointer to character string
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */

WebRtc_Word16 WebRtcCng_Version(char *version)
{
    strcpy((char*)version,(const char*)"1.2.0\n");
    return(0);
}


/****************************************************************************
 * WebRtcCng_AssignSizeEnc/Dec(...)
 *
 * These functions get the size needed for storing the instance for encoder
 * and decoder, respectively
 *
 * Input/Output:
 *      - sizeinbytes   : Pointer to integer where the size is returned
 *
 * Return value         :  0
 */

WebRtc_Word16 WebRtcCng_AssignSizeEnc(int *sizeinbytes)
{
    *sizeinbytes=sizeof(WebRtcCngEncInst_t)*2/sizeof(WebRtc_Word16);
    return(0);
}

WebRtc_Word16 WebRtcCng_AssignSizeDec(int *sizeinbytes)
{
    *sizeinbytes=sizeof(WebRtcCngDecInst_t)*2/sizeof(WebRtc_Word16);
    return(0);
}


/****************************************************************************
 * WebRtcCng_AssignEnc/Dec(...)
 *
 * These functions Assignes memory for the instances.
 *
 * Input:
 *        - CNG_inst_Addr :  Adress to where to assign memory
 * Output:
 *        - inst          :  Pointer to the instance that should be created
 *
 * Return value           :  0 - Ok
 *                          -1 - Error
 */

WebRtc_Word16 WebRtcCng_AssignEnc(CNG_enc_inst **inst, void *CNG_inst_Addr)
{
    if (CNG_inst_Addr!=NULL) {
        *inst = (CNG_enc_inst*)CNG_inst_Addr;
        (*(WebRtcCngEncInst_t**) inst)->errorcode = 0;
        (*(WebRtcCngEncInst_t**) inst)->initflag = 0;
        return(0);
    } else {
        /* The memory could not be allocated */
        return(-1);
    }
}

WebRtc_Word16 WebRtcCng_AssignDec(CNG_dec_inst **inst, void *CNG_inst_Addr)
{
    if (CNG_inst_Addr!=NULL) {
        *inst = (CNG_dec_inst*)CNG_inst_Addr;
        (*(WebRtcCngDecInst_t**) inst)->errorcode = 0;
        (*(WebRtcCngDecInst_t**) inst)->initflag = 0;
        return(0);
    } else {
        /* The memory could not be allocated */
        return(-1);
    }
}


/****************************************************************************
 * WebRtcCng_CreateEnc/Dec(...)
 *
 * These functions create an instance to the specified structure
 *
 * Input:
 *      - XXX_inst      : Pointer to created instance that should be created
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */

WebRtc_Word16 WebRtcCng_CreateEnc(CNG_enc_inst **cng_inst)
{
    *cng_inst=(CNG_enc_inst*)malloc(sizeof(WebRtcCngEncInst_t));
    if(*cng_inst!=NULL) {
        (*(WebRtcCngEncInst_t**) cng_inst)->errorcode = 0;
        (*(WebRtcCngEncInst_t**) cng_inst)->initflag = 0;
        return(0);
    }
    else {
        /* The memory could not be allocated */
        return(-1);
    }
}

WebRtc_Word16 WebRtcCng_CreateDec(CNG_dec_inst **cng_inst)
{
    *cng_inst=(CNG_dec_inst*)malloc(sizeof(WebRtcCngDecInst_t));
    if(*cng_inst!=NULL) {
        (*(WebRtcCngDecInst_t**) cng_inst)->errorcode = 0;
        (*(WebRtcCngDecInst_t**) cng_inst)->initflag = 0;
        return(0);
    }
    else {
        /* The memory could not be allocated */
        return(-1);
    }
}


/****************************************************************************
 * WebRtcCng_InitEnc/Dec(...)
 *
 * This function initializes a instance
 *
 * Input:
 *    - cng_inst      : Instance that should be initialized
 *
 *    - fs            : 8000 for narrowband and 16000 for wideband
 *    - interval      : generate SID data every interval ms
 *    - quality       : TBD
 *
 * Output:
 *    - cng_inst      : Initialized instance
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */


WebRtc_Word16 WebRtcCng_InitEnc(CNG_enc_inst *cng_inst,
                                WebRtc_Word16 fs,
                                WebRtc_Word16 interval,
                                WebRtc_Word16 quality)
{
    int i;

    WebRtcCngEncInst_t* inst=(WebRtcCngEncInst_t*)cng_inst;

    memset(inst, 0, sizeof(WebRtcCngEncInst_t));

     /* Check LPC order */

    if (quality>WEBRTC_CNG_MAX_LPC_ORDER) {
        inst->errorcode = CNG_DISALLOWED_LPC_ORDER;
        return (-1);
    }

    if (fs<=0) {
        inst->errorcode = CNG_DISALLOWED_SAMPLING_FREQUENCY;
        return (-1);
    }

    inst->enc_sampfreq=fs;
    inst->enc_interval=interval;
    inst->enc_nrOfCoefs=quality;
    inst->enc_msSinceSID=0;
    inst->enc_seed=7777; /*For debugging only*/
    inst->enc_Energy=0;
    for(i=0;i<(WEBRTC_CNG_MAX_LPC_ORDER+1);i++){
        inst->enc_reflCoefs[i]=0;
        inst->enc_corrVector[i]=0;
    }
    inst->initflag=1;

    return(0);
}

WebRtc_Word16 WebRtcCng_InitDec(CNG_dec_inst *cng_inst)
{
    int i;

    WebRtcCngDecInst_t* inst=(WebRtcCngDecInst_t*)cng_inst;

    memset(inst, 0, sizeof(WebRtcCngDecInst_t));
    inst->dec_seed=7777; /*For debugging only*/
    inst->dec_order=5;
    inst->dec_target_scale_factor=0;
    inst->dec_used_scale_factor=0;
    for(i=0;i<(WEBRTC_CNG_MAX_LPC_ORDER+1);i++){
        inst->dec_filtstate[i]=0;
        inst->dec_target_reflCoefs[i]=0;
        inst->dec_used_reflCoefs[i]=0;
    }
    inst->dec_target_reflCoefs[0]=0;
    inst->dec_used_reflCoefs[0]=0;
    inst ->dec_used_energy=0;
    inst->initflag=1;

    return(0);
}

/****************************************************************************
 * WebRtcCng_FreeEnc/Dec(...)
 *
 * These functions frees the dynamic memory of a specified instance
 *
 * Input:
 *    - cng_inst      : Pointer to created instance that should be freed
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */


WebRtc_Word16 WebRtcCng_FreeEnc(CNG_enc_inst *cng_inst)
{
    free(cng_inst);
    return(0);
}

WebRtc_Word16 WebRtcCng_FreeDec(CNG_dec_inst *cng_inst)
{
    free(cng_inst);
    return(0);
}



/****************************************************************************
 * WebRtcCng_Encode(...)
 *
 * These functions analyzes background noise
 *
 * Input:
 *    - cng_inst      : Pointer to created instance
 *    - speech        : Signal (noise) to be analyzed
 *    - nrOfSamples   : Size of speech vector
 *    - bytesOut      : Nr of bytes to transmit, might be 0
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */
WebRtc_Word16 WebRtcCng_Encode(CNG_enc_inst *cng_inst, 
                               WebRtc_Word16 *speech,
                               WebRtc_Word16 nrOfSamples,
                               WebRtc_UWord8* SIDdata,
                               WebRtc_Word16* bytesOut,
                               WebRtc_Word16 forceSID)
{
    WebRtcCngEncInst_t* inst=(WebRtcCngEncInst_t*)cng_inst;

    WebRtc_Word16 arCoefs[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word32 corrVector[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 refCs[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 hanningW[WEBRTC_CNG_MAX_OUTSIZE_ORDER];
    WebRtc_Word16 ReflBeta=19661; /*0.6 in q15*/
    WebRtc_Word16 ReflBetaComp=13107; /*0.4 in q15*/ 
    WebRtc_Word32 outEnergy;
    int outShifts;
    int i, stab;
    int acorrScale;
    int index;
    WebRtc_Word16 ind,factor;
    WebRtc_Word32 *bptr, blo, bhi;
    WebRtc_Word16 negate;
    const WebRtc_Word16 *aptr;

    WebRtc_Word16 speechBuf[WEBRTC_CNG_MAX_OUTSIZE_ORDER];


    /* check if encoder initiated */    
    if (inst->initflag != 1) {
        inst->errorcode = CNG_ENCODER_NOT_INITIATED;
        return (-1);
    }


    /* check framesize */    
    if (nrOfSamples>WEBRTC_CNG_MAX_OUTSIZE_ORDER) {
        inst->errorcode = CNG_DISALLOWED_FRAME_SIZE;
        return (-1);
    }


    for(i=0;i<nrOfSamples;i++){
        speechBuf[i]=speech[i];
    }

    factor=nrOfSamples;

    /* Calculate energy and a coefficients */
    outEnergy =WebRtcSpl_Energy(speechBuf, nrOfSamples, &outShifts);
    while(outShifts>0){
        if(outShifts>5){ /*We can only do 5 shifts without destroying accuracy in division factor*/
            outEnergy<<=(outShifts-5);
            outShifts=5;
        }
        else{
            factor/=2;
            outShifts--;
        }
    }
    outEnergy=WebRtcSpl_DivW32W16(outEnergy,factor);

    if (outEnergy > 1){
        /* Create Hanning Window */
        WebRtcSpl_GetHanningWindow(hanningW, nrOfSamples/2);
        for( i=0;i<(nrOfSamples/2);i++ )
            hanningW[nrOfSamples-i-1]=hanningW[i];

        WebRtcSpl_ElementwiseVectorMult(speechBuf, hanningW, speechBuf, nrOfSamples, 14);

        WebRtcSpl_AutoCorrelation( speechBuf, nrOfSamples, inst->enc_nrOfCoefs, corrVector, &acorrScale );

        if( *corrVector==0 )
            *corrVector = WEBRTC_SPL_WORD16_MAX;

        /* Adds the bandwidth expansion */
        aptr = WebRtcCng_kCorrWindow;
        bptr = corrVector;

        // (zzz) lpc16_1 = 17+1+820+2+2 = 842 (ordo2=700) 
        for( ind=0; ind<inst->enc_nrOfCoefs; ind++ )
        {
            // The below code multiplies the 16 b corrWindow values (Q15) with
            // the 32 b corrvector (Q0) and shifts the result down 15 steps.
                 
            negate = *bptr<0;
            if( negate )
                *bptr = -*bptr;

            blo = (WebRtc_Word32)*aptr * (*bptr & 0xffff);
            bhi = ((blo >> 16) & 0xffff) + ((WebRtc_Word32)(*aptr++) * ((*bptr >> 16) & 0xffff));
            blo = (blo & 0xffff) | ((bhi & 0xffff) << 16);

            *bptr = (( (bhi>>16) & 0x7fff) << 17) | ((WebRtc_UWord32)blo >> 15);
            if( negate )
                *bptr = -*bptr;
            bptr++;
        }

        // end of bandwidth expansion

        stab=WebRtcSpl_LevinsonDurbin(corrVector, arCoefs, refCs, inst->enc_nrOfCoefs);
        
        if(!stab){
            // disregard from this frame
            *bytesOut=0;
            return(0);
        }

    }
    else {
        for(i=0;i<inst->enc_nrOfCoefs; i++)
            refCs[i]=0;
    }

    if(forceSID){
        /*Read instantaneous values instead of averaged*/
        for(i=0;i<inst->enc_nrOfCoefs;i++)
            inst->enc_reflCoefs[i]=refCs[i];
        inst->enc_Energy=outEnergy;
    }
    else{
        /*Average history with new values*/
        for(i=0;i<(inst->enc_nrOfCoefs);i++){
            inst->enc_reflCoefs[i]=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(inst->enc_reflCoefs[i],ReflBeta,15);
            inst->enc_reflCoefs[i]+=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(refCs[i],ReflBetaComp,15);
        }
        inst->enc_Energy=(outEnergy>>2)+(inst->enc_Energy>>1)+(inst->enc_Energy>>2);
    }


    if(inst->enc_Energy<1){
        inst->enc_Energy=1;
    }

    if((inst->enc_msSinceSID>(inst->enc_interval-1))||forceSID){

        /* Search for best dbov value */
        index=0;
        for(i=1;i<93;i++){
            /* Always round downwards */
            if((inst->enc_Energy-WebRtcCng_kDbov[i])>0){
                index=i;
                break;
            }
        }
        if((i==93)&&(index==0))
            index=94;
        SIDdata[0]=index;


        /* Quantize coefs with tweak for WebRtc implementation of RFC3389 */
        if(inst->enc_nrOfCoefs==WEBRTC_CNG_MAX_LPC_ORDER){ 
            for(i=0;i<inst->enc_nrOfCoefs;i++){
                SIDdata[i+1]=((inst->enc_reflCoefs[i]+128)>>8); /* Q15 to Q7*/ /* +127 */
            }
        }else{
            for(i=0;i<inst->enc_nrOfCoefs;i++){
                SIDdata[i+1]=(127+((inst->enc_reflCoefs[i]+128)>>8)); /* Q15 to Q7*/ /* +127 */
            }
        }

        inst->enc_msSinceSID=0;
        *bytesOut=inst->enc_nrOfCoefs+1;

        inst->enc_msSinceSID+=(1000*nrOfSamples)/inst->enc_sampfreq;
        return(inst->enc_nrOfCoefs+1);
    }else{
        inst->enc_msSinceSID+=(1000*nrOfSamples)/inst->enc_sampfreq;
        *bytesOut=0;
    return(0);
    }
}


/****************************************************************************
 * WebRtcCng_UpdateSid(...)
 *
 * These functions updates the CN state, when a new SID packet arrives
 *
 * Input:
 *    - cng_inst      : Pointer to created instance that should be freed
 *    - SID           : SID packet, all headers removed
 *    - length        : Length in bytes of SID packet
 *
 * Return value       :  0 - Ok
 *                      -1 - Error
 */

WebRtc_Word16 WebRtcCng_UpdateSid(CNG_dec_inst *cng_inst,
                                  WebRtc_UWord8 *SID,
                                  WebRtc_Word16 length)
{

    WebRtcCngDecInst_t* inst=(WebRtcCngDecInst_t*)cng_inst;
    WebRtc_Word16 refCs[WEBRTC_CNG_MAX_LPC_ORDER];
    WebRtc_Word32 targetEnergy;
    int i;

    if (inst->initflag != 1) {
        inst->errorcode = CNG_DECODER_NOT_INITIATED;
        return (-1);
    }

    /*Throw away reflection coefficients of higher order than we can handle*/
    if(length> (WEBRTC_CNG_MAX_LPC_ORDER+1))
        length=WEBRTC_CNG_MAX_LPC_ORDER+1;

    inst->dec_order=length-1;

    if(SID[0]>93)
        SID[0]=93;
    targetEnergy=WebRtcCng_kDbov[SID[0]];
    /* Take down target energy to 75% */
    targetEnergy=targetEnergy>>1;
    targetEnergy+=targetEnergy>>2;

    inst->dec_target_energy=targetEnergy;

    /* Reconstruct coeffs with tweak for WebRtc implementation of RFC3389 */
    if(inst->dec_order==WEBRTC_CNG_MAX_LPC_ORDER){ 
        for(i=0;i<(inst->dec_order);i++){
            refCs[i]=SID[i+1]<<8; /* Q7 to Q15*/
            inst->dec_target_reflCoefs[i]=refCs[i];
        }
    }else{
        for(i=0;i<(inst->dec_order);i++){
            refCs[i]=(SID[i+1]-127)<<8; /* Q7 to Q15*/
            inst->dec_target_reflCoefs[i]=refCs[i];
        }
    }
    
    for(i=(inst->dec_order);i<WEBRTC_CNG_MAX_LPC_ORDER;i++){
            refCs[i]=0; 
            inst->dec_target_reflCoefs[i]=refCs[i];
        }

    return(0);
}


/****************************************************************************
 * WebRtcCng_Generate(...)
 *
 * These functions generates CN data when needed
 *
 * Input:
 *    - cng_inst      : Pointer to created instance that should be freed
 *    - outData       : pointer to area to write CN data
 *    - nrOfSamples   : How much data to generate
 *
 * Return value        :  0 - Ok
 *                       -1 - Error
 */
WebRtc_Word16 WebRtcCng_Generate(CNG_dec_inst *cng_inst,
                                 WebRtc_Word16 *outData,
                                 WebRtc_Word16 nrOfSamples,
                                 WebRtc_Word16 new_period)
{
    WebRtcCngDecInst_t* inst=(WebRtcCngDecInst_t*)cng_inst;
    
    int i;
    WebRtc_Word16 excitation[WEBRTC_CNG_MAX_OUTSIZE_ORDER];
    WebRtc_Word16 low[WEBRTC_CNG_MAX_OUTSIZE_ORDER];
    WebRtc_Word16 lpPoly[WEBRTC_CNG_MAX_LPC_ORDER+1];
    WebRtc_Word16 ReflBetaStd=26214; /*0.8 in q15*/
    WebRtc_Word16 ReflBetaCompStd=6553; /*0.2in q15*/
    WebRtc_Word16 ReflBetaNewP=19661; /*0.6 in q15*/
    WebRtc_Word16 ReflBetaCompNewP=13107; /*0.4 in q15*/
    WebRtc_Word16 Beta,BetaC, tmp1, tmp2, tmp3;
    WebRtc_Word32 targetEnergy;
    WebRtc_Word16 En;
    WebRtc_Word16 temp16;

    if (nrOfSamples>WEBRTC_CNG_MAX_OUTSIZE_ORDER) {
        inst->errorcode = CNG_DISALLOWED_FRAME_SIZE;
        return (-1);
    }


    if (new_period) {
        inst->dec_used_scale_factor=inst->dec_target_scale_factor;
        Beta=ReflBetaNewP;
        BetaC=ReflBetaCompNewP;
    } else {
        Beta=ReflBetaStd;
        BetaC=ReflBetaCompStd;
    }

    /*Here we use a 0.5 weighting, should possibly be modified to 0.6*/
    tmp1=inst->dec_used_scale_factor<<2; /* Q13->Q15 */
    tmp2=inst->dec_target_scale_factor<<2; /* Q13->Q15 */
    tmp3=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp1,Beta,15);
    tmp3+=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp2,BetaC,15);
    inst->dec_used_scale_factor=tmp3>>2; /* Q15->Q13 */

    inst->dec_used_energy=inst->dec_used_energy>>1;
    inst->dec_used_energy+=inst->dec_target_energy>>1;

    
    /* Do the same for the reflection coeffs */
    for (i=0;i<WEBRTC_CNG_MAX_LPC_ORDER;i++) {
        inst->dec_used_reflCoefs[i]=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(inst->dec_used_reflCoefs[i],Beta,15);
        inst->dec_used_reflCoefs[i]+=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(inst->dec_target_reflCoefs[i],BetaC,15);        
    }

    /* Compute the polynomial coefficients            */
    WebRtcCng_K2a16(inst->dec_used_reflCoefs, WEBRTC_CNG_MAX_LPC_ORDER, lpPoly);

    /***/ 

    targetEnergy=inst->dec_used_energy;

    // Calculate scaling factor based on filter energy
    En=8192; //1.0 in Q13
    for (i=0; i<(WEBRTC_CNG_MAX_LPC_ORDER); i++) {

        // Floating point value for reference 
        // E*=1.0-((float)inst->dec_used_reflCoefs[i]/32768.0)*((float)inst->dec_used_reflCoefs[i]/32768.0);

        // Same in fixed point
        temp16=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(inst->dec_used_reflCoefs[i],inst->dec_used_reflCoefs[i],15); // K(i).^2 in Q15
        temp16=0x7fff - temp16; // 1 - K(i).^2 in Q15
        En=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(En,temp16,15);

    }

    //float scaling= sqrt(E*inst->dec_target_energy/((1<<24)));

    //Calculate sqrt(En*target_energy/exctiation energy)

    targetEnergy=WebRtcSpl_Sqrt(inst->dec_used_energy);

    En=(WebRtc_Word16)WebRtcSpl_Sqrt(En)<<6; //We are missing a factor sqrt(2) here
    En=(En*3)>>1; //1.5 estimates sqrt(2)

    inst->dec_used_scale_factor=(WebRtc_Word16)((En*targetEnergy)>>12);


    /***/

    /*Generate excitation*/
    /*Excitation energy per sample is 2.^24 - Q13 N(0,1) */
    for(i=0;i<nrOfSamples;i++){
        excitation[i]=WebRtcSpl_RandN(&inst->dec_seed)>>1;
    }

    /*Scale to correct energy*/
    WebRtcSpl_ScaleVector(excitation, excitation, inst->dec_used_scale_factor, nrOfSamples, 13);

    WebRtcSpl_FilterAR(
        lpPoly,    /* Coefficients in Q12 */
        WEBRTC_CNG_MAX_LPC_ORDER+1, 
        excitation,            /* Speech samples */
        nrOfSamples, 
        inst->dec_filtstate,        /* State preservation */
        WEBRTC_CNG_MAX_LPC_ORDER, 
        inst->dec_filtstateLow,        /* State preservation */
        WEBRTC_CNG_MAX_LPC_ORDER, 
        outData,    /* Filtered speech samples */
        low,
        nrOfSamples
    );

    return(0);

}



/****************************************************************************
 * WebRtcCng_GetErrorCodeEnc/Dec(...)
 *
 * This functions can be used to check the error code of a CNG instance. When
 * a function returns -1 a error code will be set for that instance. The 
 * function below extract the code of the last error that occured in the 
 * specified instance.
 *
 * Input:
 *    - CNG_inst    : CNG enc/dec instance
 *
 * Return value     : Error code
 */

WebRtc_Word16 WebRtcCng_GetErrorCodeEnc(CNG_enc_inst *cng_inst)
{

    /* typecast pointer to real structure */
    WebRtcCngEncInst_t* inst=(WebRtcCngEncInst_t*)cng_inst;

    return inst->errorcode;
}

WebRtc_Word16 WebRtcCng_GetErrorCodeDec(CNG_dec_inst *cng_inst)
{

    /* typecast pointer to real structure */
    WebRtcCngDecInst_t* inst=(WebRtcCngDecInst_t*)cng_inst;

    return inst->errorcode;
}
