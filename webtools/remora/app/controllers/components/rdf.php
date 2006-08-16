<?php
require_once("includes/rdf_parser.php");
define('EM_NS', 'http://www.mozilla.org/2004/em-rdf#');
define('MF_RES', 'urn:mozilla:install-manifest');

class RdfComponent extends Object {
    /**
     * Parses install.rdf using Rdf_parser class
     * @param string $manifestData
     * @return array $data["manifest"]
     */
    function parseInstallManifest($manifestData) {
        $data = array();

        $rdf = new Rdf_parser();
        $rdf->rdf_parser_create(null);
        $rdf->rdf_set_user_data($data);
        $rdf->rdf_set_statement_handler(array('RdfComponent', 'mfStatementHandler'));
        $rdf->rdf_set_base("");

        if (!$rdf->rdf_parse($manifestData, strlen($manifestData), true)) {
            return xml_error_string(xml_get_error_code($rdf->rdf_parser["xml_parser"]));
        }

        // Set the targetApplication data
        $targetArray = array();
        if (@is_array($data["manifest"]["targetApplication"])) {
            foreach ($data["manifest"]["targetApplication"] as $targetApp) {
                $id = $data[$targetApp][EM_NS."id"];
                $targetArray[$id]["minVersion"] = $data[$targetApp][EM_NS."minVersion"];
                $targetArray[$id]["maxVersion"] = $data[$targetApp][EM_NS."maxVersion"];
            }
        }

        $data["manifest"]["targetApplication"] = $targetArray;

        $rdf->rdf_parser_free();

        return $data["manifest"];
    }

    /**
     * Parses install.rdf for our desired properties
     * @param array &$data
     * @param string $subjectType
     * @param string $subject
     * @param string $predicate
     * @param int $ordinal
     * @param string $objectType
     * @param string $object
     * @param string $xmlLang
     */
    function mfStatementHandler(&$data, $subjectType, $subject, $predicate,
                                $ordinal, $objectType, $object, $xmlLang) {     
        // single properties - ignoring: iconURL, optionsURL, aboutURL, and anything not listed
        $singleProps = array("id" => 1, "version" => 1, "creator" => 1, "homepageURL" => 1, "updateURL" => 1);
        // multiple properties - ignoring: File
        $multiProps = array("contributor" => 1, "targetApplication" => 1, "requires" => 1);
        // localizable properties
        $l10nProps = array("name" => 1, "description" => 1);

        // Look for properties on the install manifest itself
        if ($subject == MF_RES) {
            // we're only really interested in EM properties
            $length = strlen(EM_NS);
            if (strncmp($predicate, EM_NS, $length) == 0) {
                $prop = substr($predicate, $length, strlen($predicate)-$length);

                if (array_key_exists($prop, $singleProps) ) {
                    $data["manifest"][$prop] = $object;
                }
                elseif (array_key_exists($prop, $multiProps)) {
                    $data["manifest"][$prop][] = $object;
                }
                elseif (array_key_exists($prop, $l10nProps)) {
                    $lang = ($xmlLang) ? $xmlLang : "en-US";
                    $data["manifest"][$prop][$lang] = $object;
                }
            }
        }
        else {
            // save it anyway
            $data[$subject][$predicate] = $object;
        }
        return $data;
    }
}
