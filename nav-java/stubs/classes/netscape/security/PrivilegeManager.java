//
// PrivilegeManager.java -- Copyright 1997, Netscape Communications Corp.
// Raman Tenneti
//

package netscape.security;

public final
class PrivilegeManager {
    public static final int PROPER_SUBSET = -1;
    public static final int EQUAL = 0;
    public static final int NO_SUBSET = 1;

    public PrivilegeManager() {
    }
    public void checkPrivilegeEnabled(Target target) {
    }
    public void checkPrivilegeEnabled(Target target, Object data) {
    }
    public static void checkPrivilegeEnabled(String targetStr) {
    }
    public static void enablePrivilege(String targetStr) {
    }
    public void enablePrivilege(Target target) {
    }
    public void enablePrivilege(Target target, Principal prefPrin) {
    }
    public void enablePrivilege(Target target, Principal prefPrin, Object data) {
    }
    public void revertPrivilege(Target target) {
    }
    public static void revertPrivilege(String targetStr) {
    }
    public void disablePrivilege(Target target) {
    }
    public static void disablePrivilege(String targetStr) {
    }
    public static void checkPrivilegeGranted(String targetStr) {
    }
    public void checkPrivilegeGranted(Target target) {
    }
    public void checkPrivilegeGranted(Target target, Object data) {
    }
    public void checkPrivilegeGranted(Target target, Principal prin, Object data) {
    }
    public boolean isCalledByPrincipal(Principal prin, int callerDepth) {
        return true;
    }
    public boolean isCalledByPrincipal(Principal prin) {
        return true;
    }
    public static Principal getSystemPrincipal() {
        return new Principal();
    }

    private static Principal[] createPrincipalArray() {
        Principal[] prinAry = new Principal[1];
        prinAry[0] =  new Principal();
        return prinAry;
    }

    private static PrivilegeManager mgr=null;
    public static PrivilegeManager getPrivilegeManager() {
        if (mgr == null)
            mgr = new PrivilegeManager();
        return mgr;
    }
    public static Principal[] getMyPrincipals() {
        return createPrincipalArray();
    }
    public Principal[] getClassPrincipals(Class cl) {
        return createPrincipalArray();
    }
    public boolean hasPrincipal(Class cl, Principal prin) {
        return true;
    }
    public int comparePrincipalArray(Principal[] p1, Principal[] p2) {
        return PROPER_SUBSET;
    }
    public boolean checkMatchPrincipal(Class cl, int callerDepth) {
        return true;
    }
    public boolean checkMatchPrincipal(Principal prin, int callerDepth) {
        return true;
    }
    public boolean checkMatchPrincipal(Class cl) {
        return true;
    }
    public boolean checkMatchPrincipalAlways() {
        return true;
    }
    public Principal[] getClassPrincipalsFromStack(int callerDepth) {
        return createPrincipalArray();
    }
    public PrivilegeTable getPrivilegeTableFromStack() {
        return new PrivilegeTable();
    }
}
