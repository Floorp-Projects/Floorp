<?php
include_once("../core/config.php");
include_once("../core/class_rdf_parser.php");

define("EM_NS", "http://www.mozilla.org/2004/em-rdf#");
define("MF_RES", "urn:mozilla:install-manifest");
// ignoring iconURL,optionsURL,aboutURL and anything else not listed
$singleprops = array("id"=>1, "version"=>1, "creator"=>1, "homepageURL"=>1, "updateURL"=>1);
// ignoring File
$multiprops = array("contributor"=>1, "targetApplication"=>1, "requires"=>1);
// these properties are localizable
$l10nprops = array("name"=>1, "description"=>1);

function parse_install_manifest( $manifestdata ) {

$data = array();

$rdf=new Rdf_parser();
$rdf->rdf_parser_create( NULL );
$rdf->rdf_set_user_data( $data );
$rdf->rdf_set_statement_handler( "mf_statement_handler" );
$rdf->rdf_set_base("");

if ( ! $rdf->rdf_parse( $manifestdata, strlen($manifestdata), true ) ) {
	return null;
}

// now set the targetApplication data for real
$tarray = array();
if(is_array($data["manifest"]["targetApplication"])) {
	foreach($data["manifest"]["targetApplication"] as $ta) {
		$id = $data[$ta][EM_NS . "id"];
		$minVer = $data[$ta][EM_NS . "minVersion"];
		$maxVer = $data[$ta][EM_NS . "maxVersion"];
		$tarray[$id]["minVersion"] = $minVer;
		$tarray[$id]["maxVersion"] = $maxVer;
	}
}
$data["manifest"]["targetApplication"] = $tarray;
$rdf->rdf_parser_free();

return $data["manifest"];
}

function mf_statement_handler(
	&$data,
	$subject_type,
	$subject,
	$predicate,
	$ordinal,
	$object_type,
	$object,
	$xml_lang )
{
	global $singleprops, $multiprops, $l10nprops;

	// look for props on the install manifest itself
	if($subject == MF_RES) {
		// we're only really interested in EM props
		$l = strlen(EM_NS);
		if(strncmp($predicate,EM_NS,$l) == 0) {
			$prop = substr($predicate,$l,strlen($predicate)-$l);

			if($singleprops[$prop]) {
				$data["manifest"][$prop] = $object;
			}
			elseif($multiprops[$prop]) {
				$data["manifest"][$prop][] = $object;
			}
			elseif($l10nprops[$prop]) {
				// handling these separately
				// so we can handle multiple languages
				if($xml_lang) {
					$lang = $xml_lang;
				}
				else {
					// default to en-US
					$lang = "en-US";
				}
				$data["manifest"][$prop][$lang] = $object;
			}
        	}
	}
	else {
		// just save it, probably a targetApplication or something
		// shouldn't ever have multiple targets, doesn't matter
		$data[$subject][$predicate] = $object;
	}
}

?>
