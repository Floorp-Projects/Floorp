package org.mozilla.jss.pkcs11;

import org.mozilla.jss.crypto.PrivateKey;
import org.mozilla.jss.util.Assert;
import java.math.BigInteger;

class PK11DSAPrivateKey
    extends PK11PrivKey implements java.security.interfaces.DSAPrivateKey
{

    private PK11DSAPrivateKey() { super(null); }

    protected PK11DSAPrivateKey(byte[] pointer) {
        super(pointer);
    }

    public PrivateKey.Type getType() {
        return PrivateKey.Type.DSA;
    }

    public DSAParams getParams() {
        // !!!
    }

    public BigInteger getX() {
        // !!!
    }
}
