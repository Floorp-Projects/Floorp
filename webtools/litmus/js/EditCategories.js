//the main function, call to the effect object
function ec_init(){
    var myDivs = document.getElementsByClassName("collapsable");
    var myLinks = document.getElementsByClassName('collapse-link');

    //then we create the effect.
    var myAccordion = new fx.Accordion(myLinks, myDivs, {opacity: true});
    myAccordion.fxa[0].toggle();
}


