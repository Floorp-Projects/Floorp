// This is a workaround for bug 465088, that the qcms assembly doesn't
// quite match the non-assembly output.

function check_qcms_has_assembly()
{
    // We have assembly code on x86 and x86_64 architectures.
    // Unfortunately, detecting that is a little complicated.

    if (navigator.platform == "MacIntel") {
        return true;
    }

    if (navigator.platform.indexOf("Win") == 0 || navigator.platform == "OS/2") {
        // Assume all Windows and OS/2 is x86 or x86_64.  We don't
        // expose any way for Web content to check.
        return true;
    }

    // On most Unix-like platforms, navigator.platform is basically
    // |uname -sm|.
    if (navigator.platform.match(/(i[3456]86|x86_64|amd64|i86)/)) {
        return true;
    }

    return false;
}

var qcms_has_assembly = check_qcms_has_assembly();
