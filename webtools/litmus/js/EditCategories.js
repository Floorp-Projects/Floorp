var exists;
var allStretch;
	
//the main function, call to the effect object
function ec_init(){
    var divs = document.getElementsByClassName("collapsable");
    allStretch = new fx.MultiFadeSize(divs, {duration: 400});

    items = document.getElementsByClassName("display");
    for (i = 0; i < items.length; i++){
        var h3 = items[i];
        div = h3.nextSibling;
        h3.title = h3.className.replace("display ", "");

        if (window.location.href.indexOf(h3.title) < 0) {
            allStretch.hide(div, 'height');
            if (exists != true) exists = false;
        } else 
            exists = true;

        h3.onclick = function() {
            allStretch.showThisHideOpen(this.nextSibling, 100, 'height');
        }
    }

    if (exists == false) $('products').childNodes[2].fs.toggle('height');
}


