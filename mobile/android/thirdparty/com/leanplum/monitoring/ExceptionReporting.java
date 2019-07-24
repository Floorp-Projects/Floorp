package com.leanplum.monitoring;

import android.content.Context;

public interface ExceptionReporting {
    void setContext(Context context);
    void reportException(Throwable t);
}
