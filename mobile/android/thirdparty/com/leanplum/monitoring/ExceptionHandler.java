package com.leanplum.monitoring;

import android.content.Context;

import com.leanplum.Leanplum;
import com.leanplum.internal.Log;

public class ExceptionHandler {
  private static final String LEANPLUM_CRASH_REPORTER_CLASS =
          "com.leanplum.monitoring.internal.LeanplumExceptionReporter";
  private static final ExceptionHandler instance = new ExceptionHandler();
  public ExceptionReporting exceptionReporter = null;

  private ExceptionHandler() {}

  public static ExceptionHandler getInstance() {
    return instance;
  }

  public void setContext(Context context) {
    try {
      // Class.forName runs the static initializer in LeanplumExceptionReporter
      // which sets the exceptionReporter on the singleton
      Class.forName(LEANPLUM_CRASH_REPORTER_CLASS);
      if (exceptionReporter != null) {
        try {
          exceptionReporter.setContext(context);
        } catch (Throwable t) {
          Log.e("LeanplumExceptionHandler", t);
        }
      }
    } catch (ClassNotFoundException t) {
      Log.i("LeanplumExceptionHandler could not initialize Exception Reporting." +
              "This is expected if you have not included the leanplum-monitoring module");
    } catch (Throwable t) {
      Log.e("LeanplumExceptionHandler", t);
    }
  }

  public void reportException(Throwable exception) {
    if (exceptionReporter != null) {
      try {
        exceptionReporter.reportException(exception);
      } catch (Throwable t) {
        Log.e("LeanplumExceptionHandler", t);
      }
      Leanplum.countAggregator().incrementCount("report_exception");
    }

  }
}
