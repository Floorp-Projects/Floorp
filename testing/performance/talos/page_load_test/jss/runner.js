(function(){

	// Figure out if we need to stagger the tests, or not
	var doDelay = typeof window != "undefined" && window.location && window.location.search == "?delay";

	// Get the output pre (if it exists, and if we're embedded in a runner page)
	var pre = typeof document != "undefined" && document.getElementsByTagName &&
		document.getElementsByTagName("pre")[0];
	
	// The number of iterations to do
	// and the number of seconds to wait for a timeout
	var numTests = 100, timeout = !doDelay ? 20000 : 4500;

	var title, testName, summary = 0, queue = [];

	// Initialize a batch of tests
	//  name = The name of the test collection
	this.startTest = function(name){
		testName = name;

		if ( typeof onlyName == "undefined" )
			log([ "__start_report", testName ]);
	};

	// End the tests and finalize the report
	this.endTest = function(){
		// Save the summary output until all the test are complete
		queue.push(function(){
			if ( typeof onlyName == "undefined" ) {
				log([ "__summarystats", summary ]);
				log([ "__end_report" ]);

			// Log the time to the special Talos function, if it exists
			} else if ( typeof tpRecordTime != "undefined" )
				tpRecordTime( summary );

			// Otherwise, we're only interested in the results from a single function
			else
				log([ "__start_report" + summary + "__end_report" ]);

			// Force an application quit (for the Mozilla perf test runner)
			if ( typeof goQuitApplication != "undefined" )
				goQuitApplication();
		});

		// Start running the first test
		dequeue();
	};

	// Run a new test
	//  name = The unique name of the test
	//  num = The 'length' of the test (length of string, # of tests, etc.)
	//  fn = A function holding the test to run
	this.test = function(name, num, fn){
		// Done't run the test if we're limiting to just one
		if ( typeof onlyName == "undefined" || (name == onlyName && num == onlyNum) ) {
			// Don't execute the test immediately
			queue.push(function(){
				doTest( name, num, fn );
			});
		}
	};

	// The actual testing function (only to be called via the queue control)
	function doTest(name, num, fn){
		title = name;
		var times = [], start, diff, sum = 0, min = -1, max = -1,
			median, std, mean;
		
		if ( !fn ) {
			fn = num;
			num = '';
		}

		// run tests
		try {
			// We need to let the test time out
			var testStart = (new Date()).getTime();
			
			for ( var i = 0; i < numTests; i++ ) {
				start = (new Date()).getTime();
				fn();
				var cur = (new Date()).getTime();
				diff = cur - start;
				
				// Make Sum
				sum += diff;
				
				// Make Min
				if ( min == -1 || diff < min )
				  min = diff;
				  
				// Make Max
				if ( max == -1 || diff > max )
				  max = diff;
				  
				// For making Median and Variance
				times.push( diff );
					
				// Check to see if the test has run for too long
				if ( timeout > 0 && cur - testStart > timeout )
					break;
			}
		} catch( e ) {
			if ( typeof onlyName == "undefined" )
				return log( [ title, num, NaN, NaN, NaN, NaN, NaN ] );
			else
				return log( [ "__FAIL" + e + "__FAIL" ] );
		}

		// Make Mean
		mean = sum / times.length;
		
		// Keep a running summary going
		summary += mean;
		
		// Make Median
		times = times.sort(function(a,b){
			return a - b;
		});
		
		if ( times.length % 2 == 0 )
			median = (times[(times.length/2)-1] + times[times.length/2]) / 2;
		else
			median = times[(times.length/2)];
		
		// Make Variance
		var variance = 0;
		for ( var i = 0; i < times.length; i++ )
			variance += Math.pow(times[i] - mean, 2);
		variance /= times.length - 1;
		
		// Make Standard Deviation
		std = Math.sqrt( variance );

		if ( typeof onlyName == "undefined" )
			log( [ title, num, median, mean, min, max, std ] );

		// Execute the next test
		dequeue();
	};

	// Remove the next test from the queue and execute it
	function dequeue(){
		// If we're in a browser, and the user wants to delay the tests,
		// then we should throw it in a setTimeout
		if ( doDelay && typeof setTimeout != "undefined" )
			setTimeout(function(){
				queue.shift()();
			}, 13);

		// Otherwise execute the test immediately
		else
			queue.shift()();
	}

	// Log the results
	function log( arr ) {
		// If we're in an embedded runner, output to the output pre
		if ( pre )
			pre.innerHTML += arr.join(":") + "\n";

		// Otherwise, just document.write out the results
		else if ( typeof document != "undefined" && document.write && !doDelay )
			document.write( arr.join(":") + "\n" );

		// Otherwise, we're probably in a shell of some sort
		else if ( typeof window == "undefined" && typeof print != "undefined" )
			print( arr.join(":") );
	}

})();
