//
// Privilege.java -- Copyright 1996, Netscape Communications Corp.
//

package netscape.security;

public final
class Privilege {

    public static final int N_PERMISSIONS = 3;

    public static final int FORBIDDEN = 0;
    public static final int ALLOWED = 1;
    public static final int BLANK = 2;

    private int itsPerm;

    public static final int N_DURATIONS = 3;
    public static final int SCOPE = 0;
    public static final int SESSION = 1;
    public static final int FOREVER = 2;

    private int itsDuration;

    private Privilege(int perm, int duration) {
	itsPerm=perm;
	itsDuration=duration;
    }

    public static Privilege findPrivilege(int permission, int duration) {
	return new Privilege(ALLOWED, SESSION);
    }

    public static int add(int p1, int p2) {
	return ALLOWED;
    }

    public static Privilege add(Privilege p1, Privilege p2) {
        return findPrivilege(ALLOWED, SESSION);
    }

    public boolean samePermission(Privilege p) {
	return true;
    }

    public boolean samePermission(int perm) {
	return true;
    }

    public boolean sameDuration(Privilege p) {
	return true;
    }

    public boolean sameDuration(int duration) {
	return true;
    }

    public boolean isAllowed() {
	return true;
    }

    public boolean isForbidden() {
	return false;
    }

    public boolean isBlank() {
	return false;
    }

    public int getPermission() {
	return ALLOWED;
    }

    public int getDuration() {
	return SESSION;
    }

    public String toString() {
	return "Allowed in the current scope";
    }

}
