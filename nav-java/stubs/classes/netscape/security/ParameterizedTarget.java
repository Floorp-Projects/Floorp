//
// ParameterizedTarget.java -- Copyright 1997, Netscape Communications Corp.
// Raman Tenneti 
//

package netscape.security;

public class ParameterizedTarget extends UserTarget {

    public ParameterizedTarget() {
    }

    public ParameterizedTarget(String name, Principal prin,
		      int risk, String riskColor,
		      String description, String url) {
    }

    public ParameterizedTarget(String name, Principal prin,
		      int risk, String riskColor,
		      String description, String detailDescription, String url) {
    }

    public String getDetailedInfo(Object data) {
	return "";
    }

    public Privilege enablePrivilege(Principal prin, Object data) {
        return Privilege.findPrivilege(Privilege.ALLOWED, Privilege.SESSION);
    }

    public Privilege checkPrivilegeEnabled(Principal prinAry[], Object data) {
        return Privilege.findPrivilege(Privilege.ALLOWED, Privilege.SESSION);
    }
}
