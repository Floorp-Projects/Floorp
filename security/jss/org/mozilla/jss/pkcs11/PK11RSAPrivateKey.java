package org.mozilla.jss.pkcs11;

import org.mozilla.jss.crypto.PrivateKey;
import org.mozilla.jss.util.Assert;
import java.math.BigInteger;

class PK11RSAPrivateKey
    extends PK11PrivKey implements java.security.interfaces.RSAPrivateKey
{

    private PK11RSAPrivateKey() { super(null); }

    protected PK11RSAPrivateKey(byte[] pointer) {
        super(pointer);
    }

    public PrivateKey.Type getType() {
        return PrivateKey.Type.RSA;
    }

    public BigInteger getModulus() {
        // !!!
        return null;
    }

    public BigInteger getPrivateExponent() {
        // !!!
        return null;
    }
}
