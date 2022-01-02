<?php

	$server = 'mysql.dromaeo.com';
	$user = 'dromaeo';
	$pass = 'dromaeo';

	require('JSON.php');

	$json = new Services_JSON();
        $sql = mysql_connect( $server, $user, $pass );

        mysql_select_db( 'dromaeo' );

	$id = str_replace(';', "", $_REQUEST['id']);

	if ( $id ) {
		$sets = array();
		$ids = split(",", $id);

		foreach ($ids as $i) {
			$query = mysql_query( "SELECT * FROM runs WHERE id=$i;" );
			$data = mysql_fetch_assoc($query);
	
			$query = mysql_query( "SELECT * FROM results WHERE run_id=$i;" );
			$results = array();
		
			while ( $row = mysql_fetch_assoc($query) ) {
				array_push($results, $row);
			}

			$data['results'] = $results;
			$data['ip'] = '';

			array_push($sets, $data);
		}

		echo $json->encode($sets);
	} else {
		$data = $json->decode(str_replace('\\"', '"', $_REQUEST['data']));

		if ( $data ) {
		mysql_query( sprintf("INSERT into runs VALUES(NULL,'%s','%s',NOW(),'%s');",
			$_SERVER['HTTP_USER_AGENT'], $_SERVER['REMOTE_ADDR'], str_replace(';', "", $_REQUEST['style'])) );

		$id = mysql_insert_id();

		if ( $id ) {

		foreach ($data as $row) {
			mysql_query( sprintf("INSERT into results VALUES(NULL,'%s','%s','%s','%s','%s','%s','%s','%s','%s','%s','%s');",
				$id, $row->collection, $row->version, $row->name, $row->scale, $row->median, $row->min, $row->max, $row->mean, $row->deviation, $row->runs) );
		}

		echo $id;
		}
		}
	}
?>
