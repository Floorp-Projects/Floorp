//
// Principal.java -- Copyright 1997, Netscape Communications Corp.
// Raman Tenneti
//

package netscape.security;

import java.net.URL;
import java.net.MalformedURLException;

public final
class Principal {
    public static final int CODEBASE_EXACT=10;
    public static final int CODEBASE_REGEXP=11;
    public static final int CERT=12;
    public static final int CERT_FINGERPRINT=13;
    public static final int CERT_KEY=14;

    public Principal() {
    }
    public Principal(URL url) {
    }
    public Principal(int type, String value) {
    }
    public Principal(int type, byte[] value) {
    }
    public Principal(int type, byte[] value, Class cl) {
    }
    public boolean equals(Object prin) {
        return true;
    }
    public int hashCode() {
        return 0;
    }
    public boolean isCodebase() {
        return true;
    }
    public boolean isCodebaseExact() {
        return true;
    }
    public boolean isCodebaseRegexp() {
        return true;
    }
    public boolean isCert() {
        return true;
    }
    public boolean isCertFingerprint() {
        return true;
    }
    public String toString() {
        return "";
    }
    public String toVerboseString() {
        return "";
    }
    public String getVendor() {
        return "";
    }
    public String toVerboseHtml() {
        return "";
    }
    public String getFingerPrint() {
        return "";
    }
    public String getNickname() {
        return "";
    }
    public boolean isSystemPrincipal() {
        return true;
    }
}
