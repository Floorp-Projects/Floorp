//
// Target.java -- Copyright 1997, Netscape Communications Corp.
// Raman Tenneti
//

package netscape.security;

public
class Target {
    public Target() {
    }
    public Target(String name, Principal prin) {
    }
    public Target(String name) {
    }
    public Target(String name, Principal prin, Target[] targetAry) {
    }
    public Target(String name, Principal prin,
	       int risk, String riskColor,
	       String description, String url) {
    }
    public Target(String name, Principal prin,
	          int risk, String riskColor,
	          String description, String url, Target[] targetAry) {
    }
    public Target(String name, Principal prin,
	          int risk, String riskColor,
	          String description, String detailDescription, String url) {
    }
    public Target(String name, Principal prin,
	          int risk, String riskColor,
	          String description, String detailDescription, String url, 
	          Target[] targetAry) {
    }
    public final Target registerTarget() {
        return this;
    }
    public static Target findTarget(String name) {
        return new Target();
    }
    public static Target findTarget(String name, Principal prin) {
        return new Target();
    }
    public static Target findTarget(Target target) {
        return new Target();
    }
    public Privilege checkPrivilegeEnabled(Principal prinAry[], Object data) {
        return Privilege.findPrivilege(Privilege.ALLOWED, Privilege.SESSION);
    }
    public Privilege checkPrivilegeEnabled(Principal prinAry[]) {
        return Privilege.findPrivilege(Privilege.ALLOWED, Privilege.SESSION);
    }
    public Privilege checkPrivilegeEnabled(Principal p, Object data) {
        return Privilege.findPrivilege(Privilege.ALLOWED, Privilege.SESSION);
    }
    public Privilege enablePrivilege(Principal prin, Object data) {
        return Privilege.findPrivilege(Privilege.ALLOWED, Privilege.SESSION);
    }
    public static Target[] getAllRegisteredTargets() {
        return null;
    }
    public String getRisk() {
        return "";
    }
    public String getRiskColor() {
        return "";
    }
    public String getDescription() {
        return "";
    }
    public String getDetailDescription() {
        return "";
    }
    public static Target getTargetFromDescription(String desc) {
        return null;
    }
    public String getHelpUrl() {
        return "";
    }
    public String getDetailedInfo(Object data) {
        return "";
    }
    public final boolean equals(Object obj) {
        return false;
    }
    public int hashCode() {
        return 0;
    }
    public String toString() {
        return "";
    }
    public String getName() {
        return "";
    }
}
