/*
Sort <SELECT> field script by Babvailiica
www.babailiica.com
version 1.3
*/

function selectAll(obj) {
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select")
		return;
	for (var i=0; i<obj.length; i++) {
		obj[i].selected = true;
	}
}

function selectNone(obj) { /* NEW added from version 1.1 */
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select")
		return;
	for (var i=0; i<obj.length; i++) {
		obj[i].selected = false;
	}
}

function swap(obj) { /*updated from version 1.3*/
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select" && obj.length < 2)
		return false;
	var first_element = false;
	var last_element = false;
	for (var i=0; i<obj.length; i++) {
		if (obj[i].selected) {
			if (first_element === false) {
				first_element = i;
			} else {
				last_element = i;
			}
		}
	}

	if (first_element === false || last_element === false)
		return false;

	var tmp = new Array((document.body.innerHTML ? obj[first_element].innerHTML : obj[first_element].text), obj[first_element].value, obj[first_element].style.color, obj[first_element].style.backgroundColor, obj[first_element].className, obj[first_element].id, obj[first_element].selected);
	if (document.body.innerHTML) obj[first_element].innerHTML = obj[last_element].innerHTML;
	else obj[first_element].text = obj[last_element].text;
	obj[first_element].value = obj[last_element].value;
	obj[first_element].style.color = obj[last_element].style.color;
	obj[first_element].style.backgroundColor = obj[last_element].style.backgroundColor;
	obj[first_element].className = obj[last_element].className;
	obj[first_element].id = obj[last_element].id;
	obj[first_element].selected = obj[last_element].selected;
	if (document.body.innerHTML) obj[last_element].innerHTML = tmp[0];
	else obj[last_element].text = tmp[0];
	obj[last_element].value = tmp[1];
	obj[last_element].style.color = tmp[2];
	obj[last_element].style.backgroundColor = tmp[3];
	obj[last_element].className = tmp[4];
	obj[last_element].id = tmp[5];
	obj[last_element].selected = tmp[6];
}

function addItem(obj, text, value, index, id, classname, color, bg, selected) { /* NEW added from version 1.1 updated from version 1.2*/
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select" || text == "")
		return;
	obj.length++;
	if (typeof index == "number" && index < obj.length-1) {
		var i = Number();
		for (i=obj.length-2; i>index-1; i--) {
			if (document.body.innerHTML) obj[i+1].innerHTML = obj[i].innerHTML;
			else obj[i+1].text = obj[i].text;
			obj[i+1].value = obj[i].value;
			obj[i+1].id = obj[i].id;
			obj[i+1].className = obj[i].className;
			obj[i+1].style.color = obj[i].style.color;
			obj[i+1].style.backgroundColor = obj[i].style.backgroundColor;
			obj[i+1].selected = obj[i].selected;
		}
	} else {
		index = obj.length - 1;
	}
	obj = obj[index];
	if (document.body.innerHTML) obj.innerHTML = text;
	else obj.text = text;
	obj.value = value;
	obj.id = id ? id : '';
	obj.className = classname ? classname : '';
	obj.style.color = color ? color : '';
	obj.style.backgroundColor = bg ? bg : '';
	obj.selected = selected
}

function removeSelectedFromList(obj, index) { /* NEW added from version 1.1 */
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select" || obj.length == 0)
		return;
	if (index === true) {
		for (index=obj.length-1; index>=0; index--) {
			if (obj[index].selected) {
				obj[index] = null;
			}
		}
	} else {
		obj[((typeof index != "number") || index > (obj.length - 1) || index < 0 ? obj.length - 1 : index)] = null;
	}
}

function mousewheel(obj) {
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select")
		return;
	if (obj.selectedIndex != -1) {
		if (event.wheelDelta > 0) {
			up(obj);
		} else {
			down(obj);
		}
		return false;
	}
}

function sort2d(arrayName, element, num, cs) {
	if (num) {
		for (var i=0; i<(arrayName.length-1); i++) {
			for (var j=i+1; j<arrayName.length; j++) {
				if (parseInt(arrayName[j][element],10) < parseInt(arrayName[i][element],10)) {
					var dummy = arrayName[i];
					arrayName[i] = arrayName[j];
					arrayName[j] = dummy;
				}
			}
		}
	} else {
		for (var i=0; i<(arrayName.length-1); i++) {
			for (var j=i+1; j<arrayName.length; j++) {
				if (cs) {
					if (arrayName[j][element].toLowerCase() < arrayName[i][element].toLowerCase()) {
						var dummy = arrayName[i];
						arrayName[i] = arrayName[j];
						arrayName[j] = dummy;
					}
				} else {
					if (arrayName[j][element] < arrayName[i][element]) {
						var dummy = arrayName[i];
						arrayName[i] = arrayName[j];
						arrayName[j] = dummy;
					}
				}
			}
		}
	}
}

/* sort the list!
by = 0 - order by text (default)
by = 1 - order by value
by = 2 - order by color
by = 3 - order by background color
by = 4 - order by class name
by = 5 - order by id
num = if true sorts numbers e.g. 2 before 10
cs = casesensitive e.g. a before Z*/
function listSort(obj, by, num, cs) { /*updated from version 1.2*/
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	by = (parseInt("0" + by) > 5) ? 0 : parseInt("0" + by);
	if (obj.tagName.toLowerCase() != "select" && obj.length < 2)
		return false;
	var elements = new Array();
	for (var i=0; i<obj.length; i++) {
		elements[elements.length] = new Array((document.body.innerHTML ? obj[i].innerHTML : obj[i].text), obj[i].value, (obj[i].currentStyle ? obj[i].currentStyle.color : obj[i].style.color), (obj[i].currentStyle ? obj[i].currentStyle.backgroundColor : obj[i].style.backgroundColor), obj[i].className, obj[i].id, obj[i].selected);
	}
	sort2d(elements, by, num, cs);
	for (i=0; i<obj.length; i++) {
		if (document.body.innerHTML) obj[i].innerHTML = elements[i][0];
		else obj[i].text = elements[i][0];
		obj[i].value = elements[i][1];
		obj[i].style.color = elements[i][2];
		obj[i].style.backgroundColor = elements[i][3];
		obj[i].className = elements[i][4];
		obj[i].id = elements[i][5];
		obj[i].selected = elements[i][6];
	}
}

function viceVersa(obj, onlyselected) { /*updated from version 1.3*/
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select" && obj.length < 2)
		return false;
	var elements = new Array();
	for (var i=obj.length-1; i>-1; i--) {
		if (obj[i].selected || !onlyselected) {
			elements[elements.length] = new Array((document.body.innerHTML ? obj[i].innerHTML : obj[i].text), obj[i].value, obj[i].style.color, obj[i].style.backgroundColor, obj[i].className, obj[i].id, obj[i].selected);
		}
	}
	var a = 0;
	for (i=0; i<obj.length; i++) {
		if (obj[i].selected || !onlyselected) {
			if (document.body.innerHTML) obj[i].innerHTML = elements[a][0];
			else obj[i].text = elements[a][0];
			obj[i].value = elements[a][1];
			obj[i].style.color = elements[a][2];
			obj[i].style.backgroundColor = elements[a][3];
			obj[i].className = elements[a][4];
			obj[i].id = elements[a][5];
			obj[i].selected = elements[a][6];
			a++;
		}
	}
}

function top(obj) { /*updated from version 1.2*/
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select" && obj.length < 2)
		return false;
	var elements = new Array();
	for (var i=0; i<obj.length; i++) {
		if (obj[i].selected) {
			elements[elements.length] = new Array((document.body.innerHTML ? obj[i].innerHTML : obj[i].text), obj[i].value, obj[i].style.color, obj[i].style.backgroundColor, obj[i].className, obj[i].id, obj[i].selected);
		}
	}
	for (i=0; i<obj.length; i++) {
		if (!obj[i].selected) {
			elements[elements.length] = new Array((document.body.innerHTML ? obj[i].innerHTML : obj[i].text), obj[i].value, obj[i].style.color, obj[i].style.backgroundColor, obj[i].className, obj[i].id, obj[i].selected);
		}
	}
	for (i=0; i<obj.length; i++) {
		if (document.body.innerHTML) obj[i].innerHTML = elements[i][0];
		else obj[i].text = elements[i][0];
		obj[i].value = elements[i][1];
		obj[i].style.color = elements[i][2];
		obj[i].style.backgroundColor = elements[i][3];
		obj[i].className = elements[i][4];
		obj[i].id = elements[i][5];
		obj[i].selected = elements[i][6];
	}
}

function bottom(obj) { /*updated from version 1.2*/
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select" && obj.length < 2)
		return false;
	var elements = new Array();
	for (var i=0; i<obj.length; i++) {
		if (!obj[i].selected) {
			elements[elements.length] = new Array((document.body.innerHTML ? obj[i].innerHTML : obj[i].text), obj[i].value, obj[i].style.color, obj[i].style.backgroundColor, obj[i].className, obj[i].id, obj[i].selected);
		}
	}
	for (i=0; i<obj.length; i++) {
		if (obj[i].selected) {
			elements[elements.length] = new Array((document.body.innerHTML ? obj[i].innerHTML : obj[i].text), obj[i].value, obj[i].style.color, obj[i].style.backgroundColor, obj[i].className, obj[i].id, obj[i].selected);
		}
	}
	for (i=obj.length-1; i>-1; i--) {
		if (document.body.innerHTML) obj[i].innerHTML = elements[i][0];
		else obj[i].text = elements[i][0];
		obj[i].value = elements[i][1];
		obj[i].style.color = elements[i][2];
		obj[i].style.backgroundColor = elements[i][3];
		obj[i].className = elements[i][4];
		obj[i].id = elements[i][5];
		obj[i].selected = elements[i][6];
	}
}

function up(obj) { /*updated from version 1.2*/
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select" && obj.length < 2)
		return false;
	var sel = new Array();
	for (var i=0; i<obj.length; i++) {
		if (obj[i].selected == true) {
			sel[sel.length] = i;
		}
	}
	for (i in sel) {
		if (sel[i] != 0 && obj[sel[i]-1] && !obj[sel[i]-1].selected) {
			var tmp = new Array((document.body.innerHTML ? obj[sel[i]-1].innerHTML : obj[sel[i]-1].text), obj[sel[i]-1].value, obj[sel[i]-1].id);
			if (document.body.innerHTML) obj[sel[i]-1].innerHTML = obj[sel[i]].innerHTML;
			else obj[sel[i]-1].text = obj[sel[i]].text;
			obj[sel[i]-1].value = obj[sel[i]].value;
			obj[sel[i]-1].style.color = obj[sel[i]].style.color;
			obj[sel[i]-1].style.backgroundColor = obj[sel[i]].style.backgroundColor;
			obj[sel[i]-1].className = obj[sel[i]].className;
			obj[sel[i]-1].id = obj[sel[i]].id;
			if (document.body.innerHTML) obj[sel[i]].innerHTML = tmp[0];
			else obj[sel[i]].text = tmp[0];
			obj[sel[i]].value = tmp[1];
			obj[sel[i]].style.color = tmp[2];
			obj[sel[i]].style.backgroundColor = tmp[3];
			obj[sel[i]].className = tmp[4];
			obj[sel[i]].id = tmp[5];
			obj[sel[i]-1].selected = true;
			obj[sel[i]].selected = false;
		}
	}
}

function down(obj) {
	obj = (typeof obj == "string") ? document.getElementById(obj) : obj;
	if (obj.tagName.toLowerCase() != "select" && obj.length < 2)
		return false;
	var sel = new Array();
	for (var i=obj.length-1; i>-1; i--) {
		if (obj[i].selected == true) {
			sel[sel.length] = i;
		}
	}
	for (i in sel) {
		if (sel[i] != obj.length-1 && obj[sel[i]+1] && !obj[sel[i]+1].selected) {
			var tmp = new Array((document.body.innerHTML ? obj[sel[i]+1].innerHTML : obj[sel[i]+1].text), obj[sel[i]+1].value, obj[sel[i]+1].id);
			if (document.body.innerHTML) obj[sel[i]+1].innerHTML = obj[sel[i]].innerHTML;
			else obj[sel[i]+1].text = obj[sel[i]].text;
			obj[sel[i]+1].value = obj[sel[i]].value;
			obj[sel[i]+1].style.color = obj[sel[i]].style.color;
			obj[sel[i]+1].style.backgroundColor = obj[sel[i]].style.backgroundColor;
			obj[sel[i]+1].className = obj[sel[i]].className;
			obj[sel[i]+1].id = obj[sel[i]].id;
			if (document.body.innerHTML) obj[sel[i]].innerHTML = tmp[0];
			else obj[sel[i]].text = tmp[0];
			obj[sel[i]].value = tmp[1];
			obj[sel[i]].style.color = tmp[2];
			obj[sel[i]].style.backgroundColor = tmp[3];
			obj[sel[i]].className = tmp[4];
			obj[sel[i]].id = tmp[5];
			obj[sel[i]+1].selected = true;
			obj[sel[i]].selected = false;
		}
	}
}

function inArray(v,a) {
	for (var i in a) {
		if (a[i] == v) {
			return true;
		}
	}
	return false;
}

function copyToList(from,to)
{
  fromList = document.getElementById(from);
  toList = document.getElementById(to);
  if (toList.options.length > 0 && toList.options[0].value == '')
  {
    toList.options.length = 0;
  }
  var sel = false;
  for (i=0;i<fromList.options.length;i++)
  {
    var current = fromList.options[i];
    if (current.selected)
    {
      sel = true;
      if (current.value == 'temp')
      {
        alert ('You cannot move this text!');
        return;
      }
      txt = current.text;
      val = current.value;
      toList.options[toList.length] = new Option(txt,val);
//      fromList.options[i] = null;
//      i--;
    }
  }
}


