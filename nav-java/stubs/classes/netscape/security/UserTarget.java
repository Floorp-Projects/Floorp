//
// UserTarget.java -- Copyright 1996, Netscape Communications Corp.
// Raman Tenneti
//


package netscape.security;

public
class UserTarget extends Target {

    public UserTarget() {
    }

    public UserTarget(String name, Principal prin,
		      int risk, String riskColor,
		      String description, String url) {
    }

    public UserTarget(String name, Principal prin,
		      int risk, String riskColor,
		      String description, String url, Target[] targetAry) {
    }

    public UserTarget(String name, Principal prin,
		      int risk, String riskColor,
		      String description, String detailDescription, String url) {
    }

    public UserTarget(String name, Principal prin,
		      int risk, String riskColor,
		      String description, String detailDescription, String url, 
		      Target[] targetAry) {
    }

    public Privilege enablePrivilege(Principal prin, Object data) {
        return null;
    }
}
