package org.mozilla.jss.pkcs11;

import org.mozilla.jss.crypto.PrivateKey;
import org.mozilla.jss.crypto.TokenException;
import org.mozilla.jss.util.Assert;
import java.math.BigInteger;
import java.security.interfaces.DSAParams;
import java.security.interfaces.DSAPrivateKey;

class PK11DSAPrivateKey
    extends PK11PrivKey implements DSAPrivateKey
{

    private PK11DSAPrivateKey() { super(null); }

    protected PK11DSAPrivateKey(byte[] pointer) {
        super(pointer);
    }

    public PrivateKey.Type getType() {
        return PrivateKey.Type.DSA;
    }

    /**
     * If this fails, we just return null, since no exceptions are allowed.
     */
    public DSAParams getParams() {
      try {
        return getDSAParams();
      } catch(TokenException te) {
            return null;
      }
    }

    /**
     * Not implemented. NSS doesn't support extracting private key material
     * like this.
     */
    public BigInteger getX() {
        return null;
    }
}
