package com.leanplum.internal;

/** Records request call sequence of read/write operations to database. */
public interface RequestSequenceRecorder {
  /** Executes before database read in RequestOld. */
  void beforeRead();

  /** Executes after database read in RequestOld. */
  void afterRead();

  /** Executes before database write in RequestOld. */
  void beforeWrite();

  /** Executes after database write in RequestOld. */
  void afterWrite();
}
