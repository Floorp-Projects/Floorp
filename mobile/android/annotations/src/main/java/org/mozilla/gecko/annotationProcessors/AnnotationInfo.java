/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.annotationProcessors;

/**
 * Object holding annotation data. Used by GeneratableElementIterator.
 */
public class AnnotationInfo {
    public enum ExceptionMode {
        ABORT,
        NSRESULT,
        IGNORE;

        String nativeValue() {
            return "mozilla::jni::ExceptionMode::" + name();
        }
    }

    public enum CallingThread {
        GECKO,
        UI,
        ANY;

        String nativeValue() {
            return "mozilla::jni::CallingThread::" + name();
        }
    }

    public enum DispatchTarget {
        GECKO,
        GECKO_PRIORITY,
        PROXY,
        CURRENT;

        String nativeValue() {
            return "mozilla::jni::DispatchTarget::" + name();
        }
    }

    public final String wrapperName;
    public final ExceptionMode exceptionMode;
    public final CallingThread callingThread;
    public final DispatchTarget dispatchTarget;
    public final boolean noLiteral;

    public AnnotationInfo(String wrapperName, ExceptionMode exceptionMode,
                          CallingThread callingThread, DispatchTarget dispatchTarget,
                          boolean noLiteral) {

        this.wrapperName = wrapperName;
        this.exceptionMode = exceptionMode;
        this.callingThread = callingThread;
        this.dispatchTarget = dispatchTarget;
        this.noLiteral = noLiteral;
    }
}
