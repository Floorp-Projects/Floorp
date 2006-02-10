package org.mozilla.jss.pkcs11;

import org.mozilla.jss.crypto.PrivateKey;
import org.mozilla.jss.crypto.TokenException;
import org.mozilla.jss.util.Assert;
import java.math.BigInteger;
// requires JAVA 1.5
//import java.security.interfaces.ECPrivateKey;

class PK11ECPrivateKey
//    extends PK11PrivKey implements ECPrivateKey
    extends PK11PrivKey 
{

    private PK11ECPrivateKey() { super(null); }

    protected PK11ECPrivateKey(byte[] pointer) {
        super(pointer);
    }

    public PrivateKey.Type getType() {
        return PrivateKey.Type.EC;
    }

    /**
     * If this fails, we just return null, since no exceptions are allowed.
     */
// requires JAVA 1.5
//    public ECParams getParams() {
//      try {
//        return getECParams();
//      } catch(TokenException te) {
//            return null;
//      }
//    }

    /**
     * Not implemented. NSS doesn't support extracting private key material
     * like this.
     */
// requires JAVA 1.5
//    public BigInteger getW() {
//        return null;
//    }
}
