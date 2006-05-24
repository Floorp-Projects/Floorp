<?php
/**
 *	Functions that take a db result and export it to CSV.
 *	Usage example:
 *	<code>
 *	if ($_GET['csv'])
 *	{
 *		$res=db_query("SELECT * FROM fic_courses");
 *		csv_send_csv($res);
 *		exit;
 *	}
 *	</code>
 *	@package libs
 *	@subpackage csv
 *	@author Richard Faaberg <faabergr@onid.orst.edu>
 *	@author Mike Morgan <mike.morgan@oregonstate.edu> 
 */

/**
 *	Use a resource or two dimensional array, then send the CSV results to user.
 *	@param mixed $res MySQL resource / result, or a two dimensional array
 *	@param string $name name of the export file
 *	@return bool true if file sent, false otherwise
 */
function csv_send_csv($res,$name=null)
{
	// set name of the export file
	$filename=(is_null($name))?'export-'.date('Y-m-d').'.csv':$name.'.csv';
	// check for valid resource
	if ( is_resource($res) )
	{
		$csv=csv_export_to_csv($res);
	}
	elseif( is_array($res) && !empty($res) )
	{
		foreach ($res as $row)
		{
			if ( !is_array($row) )
				;
			else
				$csv[] = csv_array_to_csv($row)."\n";
		}
	}
	
	if ( is_array($csv) )
	{
		// stream csv to user
		header("Content-type: application/x-csv");
		header('Content-disposition: inline; filename="'.$filename.'"');
		header('Cache-Control: private');
		header('Pragma: public'); 	
		foreach ($csv as $row)
		{
			echo $row;
		}
		return true;
	}

	return false;
}

/**
 *	Replace quotes inside of a field with double quotes, which is something CSV requires.
 *	@param string $string unquoted quotes
 *	@return string $string quoted quotes
 */
function csv_fix_quotes($string)
{
	return preg_replace('/"/','""',$string);
}

/**
 *	Replace line breaks with commas trailed by a space.
 *	@param string $string string containing line breaks
 *	@param string string without line breaks
 */
function csv_fix_line_breaks($string)
{
	return preg_replace('/(\n\r|\r)/','\n',$string);
}

/**
 * 	Replaces instances of double quotes in a string with a single quote.
 * 	@param string $string the string to perform the replacement on
 * 	@return string the string with "" replaced by "
 */
function csv_unfix_quotes($string)
{
	return preg_replace('/""/', '"', $string);
}

/**
 *	Place quotes outside of every field, which inherently solves space, line break issues.
 *	@param string $string 
 *	@return string $string with quotes around it
 */
function csv_add_quotes($string)
{
	return '"'.$string.'"';
}

/**
 *  Removes quotes from the beginning and the end of a string.
 *  @param string $string the string to remove the quotes from
 *  @return string the string, sans quotes at the beginning and end
 */
function csv_remove_quotes($string)
{
	$pattern = "/^\"(.*)\"$/";
	$replacement = "$1";
	return preg_replace($pattern, $replacement, $string);
}

/**
 *	Convert an array into a CSV string with quotes around each value.
 *	@param array $array
 *	@return string the values in $array surrounded by quotes and separated by commas
 */
function csv_array_to_csv($array)
{
	$csv_arr = array();
	foreach ($array as $value)
	{
		$csv_arr[]=csv_add_quotes(csv_fix_quotes(csv_fix_line_breaks($value)));
	}
	$csv_string=implode(',',$csv_arr);

	return $csv_string;
}

/**
 *	Convert a CSV string into an array.
 *	Please use sparingly - this creates temp files
 *	@param string $string the CSV string
 *	@return array the elements from the CSV string in an array
 */
function csv_csv_to_array($string)
{
	$return = array();
	$length = strlen($string);
	
	// create a temp file and write the string to it
	$tmpfname = tempnam('/tmp', 'csvlib');
	$fh = fopen($tmpfname, 'w');
	fwrite($fh, $string);
	fclose($fh);

	// open the file for csv parsing
	$csvh = fopen($tmpfname, 'r');
	while (($arraydata = fgetcsv($csvh, $length, ',')) !== false)
	{
		$return = array_merge($return, $arraydata);
	}

	fclose($csvh);
	unlink($tmpfname);

	return $return;
}

/**
 * Read a CSV file into a two dimensional array
 * It returns all the rows in the file, so if the first row are headers, you'd need to take care of that in the returned array
 * @param string $filepath the path to the csv file
 * @param string $delimiter delimiter, default to ','
 * @param string $enclosure enclosure character, default to '"'
 * @return &array the two dimensional array with the csv file content, or an empty if an error occured
 */
function &csv_csv_file_to_array($filepath, $delimiter=',', $enclosure='"')
{
	$return = array();
	
	if (!file_exists($filepath) || !is_readable($filepath))
		return $return;
	
	$fh =& fopen($filepath, 'r');
	$size = filesize($filepath)+1;

	while ($data =& fgetcsv($fh, $size, $delimiter, $enclosure))
	{
		$return[] = $data;
	}
	
	fclose($fh);
	
	return $return;
}
?>
