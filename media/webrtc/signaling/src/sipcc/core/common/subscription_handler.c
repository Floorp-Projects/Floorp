/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cc_types.h"
#include "phone_platform_constants.h"
#include "cc_constants.h"
#include "phone_debug.h"
#include "prot_configmgr.h"
#include "cc_blf.h"
#include "ccapi_snapshot.h"

#define SPEEDDIAL_START_BUTTON_NUMBER 2

static unsigned char transactionIds[MAX_REG_LINES];
static boolean displayBLFState = TRUE;
static cc_blf_state_t blfStates[MAX_REG_LINES];
static boolean isBLFHandlerRunning = FALSE;
static boolean isAvailable = FALSE;

#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

static void ccBLFHandlerInitialized();

/*
 *  Function:    sub_hndlr_isAlertingBLFState
 *
 *  Description: returns if the BLF state is "alerting"
 *
 *  Parameters:
 *    inst - line button number.
 *
 *  Returns:    TRUE/FALSE
 */
boolean sub_hndlr_isAlertingBLFState(int inst)
{
    static const char fname[] = "sub_hndlr_isAlertingBLFState";

    if ((displayBLFState == TRUE) && (blfStates[inst - 1] == CC_SIP_BLF_ALERTING)) {
        CCAPP_DEBUG(DEB_F_PREFIX"inst=%d, isAlerting=TRUE",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                    inst);

        return TRUE;
    }
    CCAPP_DEBUG(DEB_F_PREFIX"inst=%d, isAlerting=FALSE",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                inst);
    return FALSE;
}

/*
 *  Function:    sub_hndlr_isInUseBLFState
 *
 *  Description: returns if the BLF state is "in use"
 *
 *  Parameters:
 *    inst - line button number.
 *
 *  Returns:    TRUE/FALSE
 */
boolean sub_hndlr_isInUseBLFState(int inst)
{
    static const char fname[] = "sub_hndlr_isInUseBLFState";

    if ((displayBLFState == TRUE) && (blfStates[inst - 1] == CC_SIP_BLF_INUSE)) {
    CCAPP_DEBUG(DEB_F_PREFIX"inst=%d, isInUse=TRUE",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                inst);
        return TRUE;
    }
    CCAPP_DEBUG(DEB_F_PREFIX"inst=%d, isInUse=FALSE",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                inst);
    return FALSE;
}

/*
 *  Function:    sub_hndlr_isAvailable
 *
 *  Description: returns if the subscription handler is available.
 *
 *  Parameters: none.
 *
 *  Returns:    TRUE/FALSE
 */
boolean sub_hndlr_isAvailable()
{
    static const char fname[] = "sub_hndlr_isAvailable";

    CCAPP_DEBUG(DEB_F_PREFIX"isAvailable=%d",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                isAvailable);
    return isAvailable;
}

static unsigned short get_new_trans_id()
{
    static unsigned short curr_trans_id = 0;

    if (++curr_trans_id == 0) {
        curr_trans_id = 1;
    }

    return curr_trans_id;
}

/*
 *  Function:    sub_hndlr_start
 *
 *  Description: does blf subscriptions upon registration.
 *
 *  Parameters: none.
 *
 *  Returns:   void
 */
void sub_hndlr_start()
{
    static const char fname[] = "sub_hndlr_start";
    int i;
    cc_uint32_t lineFeature = 0;
    cc_uint32_t featureOptionMask = 0;
    char speedDialNumber[MAX_LINE_NAME_SIZE] = {0};
    char primaryLine[MAX_LINE_NAME_SIZE] = {0};
    int transId;

    CCAPP_DEBUG(DEB_F_PREFIX"entering",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
    /* let the system know that subscription handler is available. */
    isAvailable = TRUE;

    /* get primary DN */
    config_get_line_string(CFGID_LINE_NAME, primaryLine, 1, sizeof(primaryLine));

    /*
     * for speeddial/BLF buttons, make presence subscriptions.
     */
    for (i = SPEEDDIAL_START_BUTTON_NUMBER; i <= MAX_REG_LINES; i++) {
        // first line must always be a calling line.
        config_get_line_value(CFGID_LINE_FEATURE, &lineFeature, sizeof(lineFeature), i);


        CCAPP_DEBUG(DEB_F_PREFIX"inst=%d, lineFeature=%d",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                    i, lineFeature);
        switch (lineFeature) {
        case cfgLineFeatureSpeedDialBLF:
            config_get_line_string(CFGID_LINE_SPEEDDIAL_NUMBER, speedDialNumber, i, sizeof(speedDialNumber));
            if (speedDialNumber[0] == 0) {
                break;
            }
            config_get_line_value(CFGID_LINE_FEATURE, &featureOptionMask, sizeof(featureOptionMask), i);

            transId = get_new_trans_id();
            transactionIds[i - 1] = transId;
            CC_BLF_subscribe(transId,
                             INT_MAX,
                             primaryLine,
                             speedDialNumber,
                             i,
                             featureOptionMask );
            break;
        default:
            break;
        }

        //Initializes native BLF handler
        ccBLFHandlerInitialized();
    }
}

static void ccBLFHandlerInitialized()
{
    if (!isBLFHandlerRunning) {
        CC_BLF_init();
        isBLFHandlerRunning = TRUE;
    }
}

/*
 *  Function:    sub_hndlr_stop
 *
 *  Description: terminates blf subscriptions upon unregistration.
 *
 *  Parameters: none.
 *
 *  Returns:    void
 */
void sub_hndlr_stop()
{
    static const char fname[] = "sub_hndlr_stop";
    int i;

    CCAPP_DEBUG(DEB_F_PREFIX"entering",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
    isAvailable = FALSE;
    isBLFHandlerRunning = FALSE;

    // should clean up blf susbcription list.
    for (i = SPEEDDIAL_START_BUTTON_NUMBER; i <= MAX_REG_LINES; i++) {
        //first, reset the transaction ids
        transactionIds[i - 1] = 0;
        //reset blf states.
        blfStates[i - 1] = CC_SIP_BLF_UNKNOWN;
    }
    CC_BLF_unsubscribe_All();
}


/*
 *  Function:    hideBLFButtonsDisplay
 *
 *  Description: hides BLF states
 *
 *  Parameters: none.
 *
 *  Returns:    void
 */
static void hideBLFButtonsDisplay()
{
    static const char fname[] = "hideBLFButtonsDisplay";
    int i;
    cc_uint32_t lineFeature = 0;

    CCAPP_DEBUG(DEB_F_PREFIX"entering",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
    displayBLFState = FALSE;
    for (i = SPEEDDIAL_START_BUTTON_NUMBER; i <= MAX_REG_LINES; i++) {
        // first line must always be a calling line.
        config_get_line_value(CFGID_LINE_FEATURE, &lineFeature, sizeof(lineFeature), i);

        switch (lineFeature) {
        case cfgLineFeatureSpeedDialBLF:
            ccsnap_gen_blfFeatureEvent(CC_SIP_BLF_UNKNOWN, i);
            break;
        default:
            break;
        }
    }
}

/*
 *  Function:    unhideBLFButtonsDisplay
 *
 *  Description: unhides BLF states.
 *
 *  Parameters: none.
 *
 *  Returns:    void
 */
static void unhideBLFButtonsDisplay()
{
    static const char fname[] = "unhideBLFButtonsDisplay";
    int i;
    cc_uint32_t lineFeature = 0;
    char speedDialNumber[MAX_LINE_NAME_SIZE] = {0};

    CCAPP_DEBUG(DEB_F_PREFIX"entering",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));

    displayBLFState = TRUE;

    for (i = SPEEDDIAL_START_BUTTON_NUMBER; i <= MAX_REG_LINES; i++) {
        // first line must always be a calling line.
        config_get_line_value(CFGID_LINE_FEATURE, &lineFeature, sizeof(lineFeature), i);
        config_get_line_string(CFGID_LINE_SPEEDDIAL_NUMBER, speedDialNumber, i, sizeof(speedDialNumber));

        switch (lineFeature) {
        case cfgLineFeatureSpeedDialBLF:
            ccsnap_gen_blfFeatureEvent(blfStates[i - 1], i);
            break;
        default:
            break;
        }
    }
}

/*
 *  Function:    sub_hndlr_controlBLFButtons
 *
 *  Description: hides/unhides BLF states.
 *
 *  Parameters: none.
 *
 *  Returns:    void
 */
void sub_hndlr_controlBLFButtons(boolean state)
{
    static const char fname[] = "sub_hndlr_controlBLFButtons";

    if (state == TRUE) {
        CCAPP_DEBUG(DEB_F_PREFIX"going to hide",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
        hideBLFButtonsDisplay();
    } else {
        CCAPP_DEBUG(DEB_F_PREFIX"going to unhide",
                    DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname));
        unhideBLFButtonsDisplay();
    }
}

/*
 *  Function:    sub_hndlr_NotifyBLFStatus
 *
 *  Description: notifies the app of BLF state.
 *
 *  Parameters:
 *      requestId - requestId of the subscription
 *      status - BLF status
 *      appId -  button number of the BLF feature key.
 *
 *  Returns:    void
 */
void sub_hndlr_NotifyBLFStatus(int requestId, cc_blf_state_t status, int appId)
{
    static const char fname[] = "sub_hndlr_NotifyBLFStatus";
    cc_uint32_t lineFeature = 0;
    char speedDialNumber[MAX_LINE_NAME_SIZE] = {0};


    CCAPP_DEBUG(DEB_F_PREFIX"requestId=%d, status=%d, appId=%d",
                DEB_F_PREFIX_ARGS(SIP_CC_PROV, fname),
                requestId, status, appId);
    if (appId == 0) {
        // call list BLF.
    } else {
        config_get_line_value(CFGID_LINE_FEATURE, &lineFeature, sizeof(lineFeature), appId);
        config_get_line_string(CFGID_LINE_SPEEDDIAL_NUMBER, speedDialNumber, appId, sizeof(speedDialNumber));

        blfStates[appId - 1] = status;
        if (displayBLFState == FALSE) {
            return; // ignore the notify
        }
        if (lineFeature == cfgLineFeatureSpeedDialBLF) {
            ccsnap_gen_blfFeatureEvent(status, appId);
        }
    }
}

