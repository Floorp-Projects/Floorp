<?php

//sleep(1); # enable to see loading process

include_once 'JSON.php';

$json = new Services_JSON();

header('Content-Type: text/json');

if ($_REQUEST['action'] == 'getChildren') {
	$child1 = array(
		'title'=>'test',
		'isFolder'=>true,
		'widgetId'=>rand(10,10000),
	);
	$child2 = array(
		'title'=>'test2',
		'widgetId'=>rand(10,10000),
	);

	echo $json->encode(array($child1, $child2));

}

if ($_REQUEST['action'] == 'changeParent') {
	echo "true";
}

if ($_REQUEST['action'] == 'swapNodes') {
	echo "true";
}


if ($_REQUEST['action'] == 'removeNode') {
	echo "true";
}

if ($_REQUEST['action'] == 'createNode') {
	$data = $json->decode(@$_REQUEST['data']); // maybe no data at all
	$response = array(
		'title'=>$data->suggestedTitle,
		'widgetId'=>rand(10,10000),
	);
	echo $json->encode($response);
}

