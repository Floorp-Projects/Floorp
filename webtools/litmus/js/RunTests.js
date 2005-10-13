function showTEST(testid) {
  if(testid=="all") {
    divTOG('tmp_menua,tmp_menub',1,0);
    divTOG('tmp_tests,tmp_separ',1,1);
    divTOG('mf,ml',0,1);
  } else {
    divTOG('tmp_tests,tmp_separ,tmp_menua,tmp_menub',1,0);
    divTOG("t"+testid+",ma"+testid+",mb"+testid,0,1);
    for(var i=1; i<=tmp_pagetotal; i++) {
      document.getElementById("li"+i).style.backgroundColor='inherit';
      document.getElementById("a"+i).style.color='inherit';
    }
    document.getElementById("li"+testid).style.backgroundColor='#bbbbbb';
    document.getElementById("a"+testid).style.color='#660000';
  }
}

function divSWAP(divID) { 
  document.getElementById(divID).style.display=(document.getElementById(divID).style.display=='none')?'block':'none';
}

function divTOG(divNAME,divSET,divVIS) {
  if (divNAME.indexOf(',')) { 
    divNAME=divNAME.split(","); 
  } else { 
    divNAME=new Array(divNAME); 
  }
  for(var i=0; i<divNAME.length; i++) {
    if(divSET) {
      tmpNAME=(divNAME[i]=="tmp_tests")?tmp_tests:(divNAME[i]=="tmp_separ")?tmp_separ:(divNAME[i]=="tmp_menua")?tmp_menua:(divNAME[i]=="tmp_menub")?tmp_menub:'none';
      for(var j=0; j<tmpNAME.length; j++) { 
        divCHANGE(tmpNAME[j],divVIS);
      }
    } else {
      divCHANGE(divNAME[i],divVIS);
    }
  }
}

function divCHANGE(divNAME,divVIS) { 
  divVIS=divVIS?'block':'none';
  if(divNAME.indexOf(',')) {
    divNAME=divNAME.split(",");
  } else { 
    divNAME=new Array(divNAME);
  }
  for(var i=0; i<divNAME.length; i++) {
    try {
      document.getElementById(divNAME[i]).style.display=divVIS;
    } catch(e) {
      // alert(divNAME[i]+ ": no properties");	
    }
  }
}
