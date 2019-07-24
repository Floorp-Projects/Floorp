package com.leanplum.models;

import android.support.annotation.NonNull;

import java.util.Date;

public class MessageArchiveData {
    @NonNull public String messageID;
    @NonNull public String messageBody;
    @NonNull public String recipientUserID;
    @NonNull public Date deliveryDateTime;

    public MessageArchiveData(String messageID, String messageBody, String recipientUserID, Date deliveryDateTime) {
        this.messageID = messageID;
        this.messageBody = messageBody;
        this.recipientUserID = recipientUserID;
        this.deliveryDateTime = deliveryDateTime;
    }
}
