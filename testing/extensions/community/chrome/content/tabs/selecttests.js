		var updateFunction;
		
		var sTestrunsWrapper; // an array of things that are kind of like testruns, but w/o important fields.
                              //returned by "test_runs_by_branch_product_name="
		
        var sTestrun;   // actual testrun
        
		function handleLoad() {
			if (window.arguments.length > 0)
				updateFunction = window.arguments[0];   // parent window passes in a function to update itself with data
				
			litmus.getTestruns(populateTestRuns);
		}
        function handleRunSelect() {
            var id = document.getElementById("qa-st-testrun").selectedItem.getAttribute("value");
            litmus.getTestrun(id, populateTestGroups);
        }
        function handleTestgroupSelect() {
            var id = document.getElementById("qa-st-testgroup").selectedItem.value;
            litmus.getTestgroup(id, populateSubgroups);
        }
        function handleSubgroupSelect() {
            var id = document.getElementById("qa-st-subgroup").selectedItem.value;
            updateCaller(id, 0);
        }
		function populateTestRuns(testrunsWrapper) {
            var menu = document.getElementById("qa-st-testrun");
			testrunsWrapper = qaTools.arrayify(testrunsWrapper);
			sTestrunsWrapper = testrunsWrapper;
			
            while (menu.firstChild) {  // clear menu
				menu.removeChild(menu.firstChild);
			}
			for (var i = 0; i < testrunsWrapper.length; i++) {
                if (testrunsWrapper[i].enabled == 0) continue;
                var item = menu.appendItem(testrunsWrapper[i].name, testrunsWrapper[i].test_run_id);
            }
			
			menu.selectedIndex = 0;
            handleRunSelect();
		}
        function populateTestGroups(testrun) {
            var menu = document.getElementById("qa-st-testgroup");
			while (menu.firstChild) {  // clear menu
				menu.removeChild(menu.firstChild);
			}
			var testgroups = qaTools.arrayify(testrun.testgroups);
            for (var i = 0; i < testgroups.length; i++) {
                if (testgroups[i].enabled == 0) continue;
				menu.appendItem(testgroups[i].name, testgroups[i].testgroup_id);
			}
            
            menu.selectedIndex = 0;
        }
        function populateSubgroups(testgroup) {
            var menu = document.getElementById("qa-st-subgroup");
			while (menu.firstChild) {  // clear menu
				menu.removeChild(menu.firstChild);
			}
			
			var subgroups = qaTools.arrayify(testgroup.subgroups);
            for (var i = 0; i < subgroups.length; i++) {
                if (subgroups[i].enabled == 0) continue;
				menu.appendItem(subgroups[i].name, subgroups[i].subgroup_id);
			}
			menu.selectedIndex = 0;
        }
		
		function OK() {
			
			return true;
		}
		function updateCaller(subgroupID, index) {
			litmus.writeStateToPref(subgroupID, index);
			updateFunction();
		}
		function Cancel() {
			return true;
		}
