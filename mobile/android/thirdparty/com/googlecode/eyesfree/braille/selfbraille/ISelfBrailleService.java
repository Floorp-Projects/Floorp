/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package com.googlecode.eyesfree.braille.selfbraille;

/**
 * Interface for a client to control braille output for a part of the
 * accessibility node tree.
 */
public interface ISelfBrailleService extends android.os.IInterface {
    /** Local-side IPC implementation stub class. */
    public static abstract class Stub extends android.os.Binder implements
            com.googlecode.eyesfree.braille.selfbraille.ISelfBrailleService {
        private static final java.lang.String DESCRIPTOR = "com.googlecode.eyesfree.braille.selfbraille.ISelfBrailleService";

        /** Construct the stub at attach it to the interface. */
        public Stub() {
            this.attachInterface(this, DESCRIPTOR);
        }

        /**
         * Cast an IBinder object into an
         * com.googlecode.eyesfree.braille.selfbraille.ISelfBrailleService
         * interface, generating a proxy if needed.
         */
        public static com.googlecode.eyesfree.braille.selfbraille.ISelfBrailleService asInterface(
                android.os.IBinder obj) {
            if ((obj == null)) {
                return null;
            }
            android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (((iin != null) && (iin instanceof com.googlecode.eyesfree.braille.selfbraille.ISelfBrailleService))) {
                return ((com.googlecode.eyesfree.braille.selfbraille.ISelfBrailleService) iin);
            }
            return new com.googlecode.eyesfree.braille.selfbraille.ISelfBrailleService.Stub.Proxy(
                    obj);
        }

        @Override
        public android.os.IBinder asBinder() {
            return this;
        }

        @Override
        public boolean onTransact(int code, android.os.Parcel data,
                android.os.Parcel reply, int flags)
                throws android.os.RemoteException {
            switch (code) {
            case INTERFACE_TRANSACTION: {
                reply.writeString(DESCRIPTOR);
                return true;
            }
            case TRANSACTION_write: {
                data.enforceInterface(DESCRIPTOR);
                android.os.IBinder _arg0;
                _arg0 = data.readStrongBinder();
                com.googlecode.eyesfree.braille.selfbraille.WriteData _arg1;
                if ((0 != data.readInt())) {
                    _arg1 = com.googlecode.eyesfree.braille.selfbraille.WriteData.CREATOR
                            .createFromParcel(data);
                } else {
                    _arg1 = null;
                }
                this.write(_arg0, _arg1);
                reply.writeNoException();
                return true;
            }
            case TRANSACTION_disconnect: {
                data.enforceInterface(DESCRIPTOR);
                android.os.IBinder _arg0;
                _arg0 = data.readStrongBinder();
                this.disconnect(_arg0);
                return true;
            }
            }
            return super.onTransact(code, data, reply, flags);
        }

        private static class Proxy implements
                com.googlecode.eyesfree.braille.selfbraille.ISelfBrailleService {
            private android.os.IBinder mRemote;

            Proxy(android.os.IBinder remote) {
                mRemote = remote;
            }

            @Override
            public android.os.IBinder asBinder() {
                return mRemote;
            }

            public java.lang.String getInterfaceDescriptor() {
                return DESCRIPTOR;
            }

            @Override
            public void write(
                    android.os.IBinder clientToken,
                    com.googlecode.eyesfree.braille.selfbraille.WriteData writeData)
                    throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeStrongBinder(clientToken);
                    if ((writeData != null)) {
                        _data.writeInt(1);
                        writeData.writeToParcel(_data, 0);
                    } else {
                        _data.writeInt(0);
                    }
                    mRemote.transact(Stub.TRANSACTION_write, _data, _reply, 0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }

            @Override
            public void disconnect(android.os.IBinder clientToken)
                    throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeStrongBinder(clientToken);
                    mRemote.transact(Stub.TRANSACTION_disconnect, _data, null,
                            android.os.IBinder.FLAG_ONEWAY);
                } finally {
                    _data.recycle();
                }
            }
        }

        static final int TRANSACTION_write = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
        static final int TRANSACTION_disconnect = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
    }

    public void write(android.os.IBinder clientToken,
            com.googlecode.eyesfree.braille.selfbraille.WriteData writeData)
            throws android.os.RemoteException;

    public void disconnect(android.os.IBinder clientToken)
            throws android.os.RemoteException;
}
