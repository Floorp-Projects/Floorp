//
// PrivilegeTable.java -- Copyright 1996, Netscape Communications Corp.
//

package netscape.security;

import java.util.*;

public final
class PrivilegeTable {
    public PrivilegeTable() {
    }
    public int size() {
        return 0;
    }
    public boolean isEmpty() {
        return true;
    }
    public Enumeration keys() {
        return null;
    }
    public Enumeration elements() {
        return null;
    }
    public String toString() {
        return "";
    }
    public Privilege get(Object obj) {
        return Privilege.findPrivilege(Privilege.ALLOWED, Privilege.SESSION);
    }
    public Privilege get(Target t) {
        return Privilege.findPrivilege(Privilege.ALLOWED, Privilege.SESSION);
    }
    public synchronized Privilege put(Object key, Privilege priv) {
        return priv;
    }
    public synchronized Privilege put(Target key, Privilege priv) {
        return priv;
    }
    public synchronized Privilege remove(Object key) {
        return Privilege.findPrivilege(Privilege.ALLOWED, Privilege.SESSION);
    }
    public synchronized Privilege remove(Target key) {
        return Privilege.findPrivilege(Privilege.ALLOWED, Privilege.SESSION);
    }
    public synchronized void clear() {
    }
    public Object clone() {
	return new PrivilegeTable();
    }
}
