// Only display opts/plans valid for selected product(s)

//TODO: problems if product names, versions or plans include this sequence:
var SEPARATOR = '@%@';

function selectProduct(f) {

    // Netscape 4.04 and 4.05 also choke with an "undefined"
    // error.  if someone can figure out how to "define" the
    // whatever, we ll remove this hack.  in the mean time, we ll
    // assume that v4.00-4.03 also die, so we ll disable the neat
    // javascript stuff for Netscape 4.05 and earlier.

    var cnt = 0;
    var i;
    var j;

    if (!f) {
        return;
    }
    
    var products = f.product.options;
    
    for (i=0 ; i<products.length ; i++) {
        if (products[i].selected) {
            cnt++;
        }
    }
    var doall = (cnt == products.length || cnt == 0);

//alert('xxx doall: '+doall);

    fill_opts(products, doall, 'component', cpts);
    
    fill_opts(products, doall, 'product_version', vers);
    
    fill_opts(products, doall, 'plan', plans);

/*    
    // Product versions:
    
    var oproduct_versions = f.product_version.options;

    var vsel = new Object();
    for (i=0 ; i<oproduct_versions.length ; i++) {
        if (oproduct_versions[i].selected) {
            vsel[oproduct_versions[i].value] = 1;
        }
    }

    oproduct_versions.length = 0;

    for (v in vers) {
         if (typeof(vers[v]) == 'function') continue;
         doit = doall;
         for (i=0 ; !doit && i<products.length ; i++) {
             if (products[i].selected) {
                 p = products[i].value;
                 for (j in vers[v]) {
                     if (typeof(vers[v][j]) == 'function') continue;
                     p2 = vers[v][j];
                     if (p2 == p) {
                         doit = true;
                         break;
                     }
                 }
             }
         }
         if (doit) {
             var opt = new Option(v, v);
             oproduct_versions.add(opt);
             if (vsel[v]) {
                 opt.selected = true;
             }
         }
    }
*/
/*
	// Test plans:
	
	var oplans = f.plan.options;
	
    var psel = new Object();
    for (i=0 ; i<oplans.length ; i++) {
         if (oplans[i].selected) {
             psel[oplans[i].value] = 1;
         }
    }

    oplans.length = 0;

    for (v in plans) {
        if (typeof(plans[v]) == 'function') continue;
        doit = doall;
        for (i=0 ; !doit && i<products.length ; i++) {
            if (products[i].selected) {
                p = products[i].value;
                for (j in plans[v]) {
                    if (typeof(plans[v][j]) == 'function') continue;
                    p2 = plans[v][j];
                    if (p2 == p) {
                        doit = true;
                        break;
                    }
                }
            }
        }
        if (doit) {
            var opt = new Option(v, v);
            oplans.add(opt);
            if (psel[v]) {
                opt.selected = true;
            }
        }
    }
*/
}

function fill_opts(products, doall, name, myArray) {

    var opts = document.getElementById(name).options;
    
    var opts_st = document.getElementById(name + '_st');
    
    var opts_sel = opts_st.value.split(SEPARATOR);
    
    opts.length = 0;

    for (var c in myArray) {
        var cc = myArray[c];
        var doit = doall;
        for (i=0 ; !doit && i<products.length ; i++) {
            if (products[i].selected) {
                var p = products[i].value;
                for (var jj=0; jj<cc.length; jj++) {
                    var ww = cc[jj];
                    if (ww == p) {
                        doit = true;
                        break;
                    }
                }
            }
        }
        if (doit) {
            var opt = new Option(c, c);
            opts.add(opt);
              for(var i=0;i<opts_sel.length;i++) {
                if(c == opts_sel[i]) {
                  opt.selected = true;
                  break;
                }
              }
        }
    }
}

function ss(name) {

  var opts = document.getElementById(name).options;

  var val=new Array();
  for(var i=0; i<opts.length; i++) {
    var c = opts[i];
    if(c.selected) {
      val.push(c.value);
    }
  }
  var opts_st = document.getElementById(name + '_st');
  opts_st.value=val.join(SEPARATOR);
}
