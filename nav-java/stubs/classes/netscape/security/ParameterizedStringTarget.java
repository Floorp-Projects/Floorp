//
// ParameterizedStringTarget.java -- Copyright 1997, Netscape Communications Corp.
// Raman Tenneti
//

package netscape.security;

public
class ParameterizedStringTarget extends ParameterizedTarget {

    public ParameterizedStringTarget() {
    }

    public ParameterizedStringTarget(String name, Principal prin,
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
