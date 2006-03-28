function toggle(show,hide,display) {
    document.getElementById(show).style.display = display;
    document.getElementById(hide).style.display = "none";
}

function hide(id) {
    document.getElementById(id).style.display = "none";
}

function show(id,display) {
    document.getElementById(id).style.display = display;
}
