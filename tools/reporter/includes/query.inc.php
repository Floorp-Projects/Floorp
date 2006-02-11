<?php
require_once($config['base_path'].'/includes/contrib/adodb/adodb.inc.php');

class query
{
        // approved "selectable" and "queryable" fields.  In sort order
    var $approved_selects = array('count' /*special */,
                                  'report_file_date',
                                  'host_hostname',
                                  'report_url',
                                  'report_behind_login',
                                  'report_useragent',
                                  'report_problem_type',
                                  'report_platform',
                                  'report_oscpu',
                                  'report_gecko',
                                  'report_product',
                                  'report_language',
                                  'report_id',
                                  'report_buildconfig',
                                  'report_description',
/*                                'report_email',
                                  'report_ip',
*/                                'report_host_id',
                                  'host_id',
    );
    var $approved_wheres = array('count',
                                 'host_hostname',
                                 'report_id',
                                 'report_url',
                                 'report_problem_type',
                                 'report_description',
                                 'report_behind_login',
                                 'report_useragent',
                                 'report_platform',
                                 'report_oscpu',
                                 'report_gecko',
                                 'report_language',
                                 'report_gecko',
                                 'report_buildconfig',
                                 'report_product',
                                 'report_file_date'
    );

    var $orderbyChecked = false;

    function query(){}

    function getQueryInputs(){
        global $config;

        $artificialReportID = false;

        /*******************
         * ASCDESC
         *******************/
        if (isset($_GET['ascdesc']) && (strtolower($_GET['ascdesc']) == 'asc' || strtolower($_GET['ascdesc']) == 'asc')){
            $ascdesc = $_GET['ascdesc'];
        } else {
            $ascdesc = 'desc';
        }

        /*******************
         * SHOW (really "Limit")
         *******************/
        if (!isset($_GET['show']) ||
             $_GET['show'] == null ||
             $_GET['show'] > $config['max_show'])
        {
            $show = $config['show'];
        } else {
            $show = $_GET['show'];
        }

        /*******************
         * PAGE (really other part of "Limit"
         *******************/
        if (!isset($_GET['page']) ||
            $_GET['page'] == null)
        {
            $page = 1;
        } else {
            $page = $_GET['page'];
        }

        /*******************
         * Count
         *******************/
        $count = null;
        if (isset($_GET['count'])){
            $count = 'report_id'; // XX limitation for now
        }
        // if nothing... it's nothing

        /*******************
         * SELECT
         *******************/
        /*
           If user defines what to select and were not counting, just
           use their input.
        */
        $selected = array();
        if (isset($_GET['selected']) && !isset($_GET['count'])){
            $selected = array();
            foreach($_GET['selected'] as $selectedChild){
                if(in_array(strtolower($selectedChild), $this->approved_selects)){
                    $selected[] = array('field' => $selectedChild,
                                        'title' => $config['fields'][$selectedChild]);
                }
            }
        } else {
            // Otherwise, we do it for  them
            $selected[] = array('field' => 'host_hostname',
                                'title' => $config['fields']['host_hostname']);

            // Showing date is good just about always, except when using a count
            if(!isset($_GET['count'])){
                $selected[] = array('field' => 'report_file_date',
                                    'title' => $config['fields']['report_file_date']);
            }
        }
        if(!isset($_GET['count']) &&
           $this->_searchQueryInput($selected, 'report_id') === false){
            $artificialReportID = true;
            $selected[] = array('field' => 'report_id',
                                'title' => $config['fields']['report_id']);
        }

      /*******************
       * ORDER BY
       *******************/
        $orderby = array();
        // The first priority is those who were recently specified (such as a menu header clicked).
        if(isset($_GET['orderby']) && in_array(strtolower($_GET['orderby']), $this->approved_selects)){
            $orderby[$_GET['orderby']] = $ascdesc;
            
            // For continuity Params we set this to true to verify it's been checked as matching an approved val
            $this->orderbyChecked = true;
        }

      /*******************
       * COUNT
       *******************/
        // After, we append those who werere selected previously and order desc.
        if(!isset($_GET['count'])){
            $orderby = array_merge($orderby, $this->_calcOrderBy($selected, $orderby));
        }

        // If we are counting, we need to add A column for it
        if (isset($_GET['count'])){
            // set the count variable
            $next =sizeof($selected)+1;
            $selected[$next]['field'] = 'count';
            $selected[$next]['title'] = 'Amount';

            // Hardcode host_id
            $_GET['count'] = 'report_id'; // XXX we just hardcode this (just easier for now, and all people will be doing).
            // XX NOTE:  We don't escape count below because 'host_id' != `host_id`.

            /*******************
             * ORDER BY
             *******************/
            if (isset($_GET['count'])){
                if(!isset($orderby['count'])){
                    $orderby['count'] = 'desc'; // initially hardcode to desc
                }
                if(!isset($orderby['host_hostname'])){
                    $orderby['host_hostname'] = 'desc';
                }
            }
        }

        /*******************
         * PRODUCT FAMILY
         *******************/
        $product_family = "";
        if(isset($_GET['product_family'])){
            $product_family = $_GET['product_family'];
        }

        /*******************
         * WHERE
         *******************/
        $where = array();
        reset($_GET);
        while (list($column, $value) = each($_GET)) {
            /* To help prevent stupidity with columns, we only add
               it to the WHERE statement if it's passes as a column
               we allow.  Others simly don't make sense, or we don't
               allow them for security purposes.
            */
            if (in_array(strtolower($column), $this->approved_wheres) && strtolower($column) != 'count' && strtolower($column)){
                // these are our various ways of saying "no value"
                if (($value != -1) && ($value != null) && ($value != '0')){
                    // if there's a wildcard (%,_) we should use 'LIKE', otherwise '='
                    if ((strpos($value, "%") === false) && (strpos($value, "_") === false)){
                        $operator = "=";
                    } else {
                        $operator = "LIKE";
                    }
                    // Add to query
                    if (in_array(strtolower($column), $this->approved_wheres)){
                        $where[] = array($column, $operator, $value);
                    }
                }
            }
        }
        return array('selected'               => $selected,
                     'where'                  => $where,
                     'orderby'                => $orderby,
                     'show'                   => $show,
                     'page'                   => $page,
                     'count'                  => $count,
                     'product_family'          => $product_family,
                     'artificialReportID'     => $artificialReportID
        );
    }

    function doQuery($select, $where, $orderby, $show, $page, $product_family, $count){
        global $db;

        /************
         * SELECT
         ************/
        $sql_select = 'SELECT ';
        foreach($select as $select_child){
            // we don't $db->quote here since unless it's in our approved array (exactly), we drop it anyway. i.e. report_id is on our list, 'report_id' is not.
            // we sanitize on our own
            if ($select_child['field'] == 'count'){
                $sql_select .= 'COUNT( '.$count.' ) AS count';
                if(!isset($orderby['count'])){
                    $orderby = array_merge(array('count'=> 'DESC'), $orderby);
                }
            } else {
                $sql_select .= $select_child['field'];
            }
            $sql_select .= ', ';
        }
        if(sizeof($select) > 0){
            $sql_select = substr($sql_select, 0, -2);
            $sql_select = $sql_select.' ';
        }

        /************
         * FROM
         ************/
        $sql_from = 'FROM `report`, `host`';

        /************
         * WHERE
         ************/
        $sql_where = 'WHERE ';
        foreach($where as $where_child){
            // we make sure to use quote() here to escape any evil
            $sql_where .= $where_child[0].' '.$where_child[1].' '.$db->quote($where_child[2]).' AND ';
        }

        // Dates aar special
        // if the user didn't delete the default YYYY-MM-DD mask, we do it for them
        if (isset($_GET['report_file_date_start']) && $_GET['report_file_date_start'] == 'YYYY-MM-DD'){
            $_GET['report_file_date_start'] = null;
        }
        if (isset($_GET['report_file_date_end']) && $_GET['report_file_date_end'] == 'YYYY-MM-DD'){
            $_GET['report_file_date_end'] = null;
        }
        if (isset($_GET['report_file_date_start']) &&(($_GET['report_file_date_start'] != null)  || ($_GET['report_file_date_end'] != null))){

            // if we have both, we do a BETWEEN
            if ($_GET['report_file_date_start'] && $_GET['report_file_date_end']){
                $sql_where .= "(report_file_date BETWEEN ".$db->quote($_GET['report_file_date_start'])." and ".$db->quote($_GET['report_file_date_end']).") AND ";
            }

            // if we have only a start, then we do a >
            else if ($_GET['report_file_date_start']){
                $sql_where .= "report_file_date > ".$db->quote($_GET['report_file_date_start'])." AND ";
            }

            // if we have only a end, we do a <
            else if ($_GET['report_file_date_end']){
                $sql_where .= "report_file_date < ".$db->quote($_GET['report_file_date_end'])." AND ";
            }
        }

        if(sizeof(trim($sql_where)) <= 0){
            $sql_where .= ' AND ';
        }
        $sql_where .= 'host.host_id = report_host_id ';

        /*******************
         * ORDER BY
         *******************/
         // product_family
         $prodFamQuery = $db->Execute("SELECT product.product_value
                                       FROM product
                                       WHERE product.product_family = ".$db->quote($product_family)." ");
         $sql_product_family = "";
         if($prodFamQuery){
             $prodFamCount = 0;
             while(!$prodFamQuery->EOF){
                 if($prodFamCount > 0){
                     $sql_product_family .= ' OR ';
                 }
                 $sql_product_family .= 'report.report_product = '.$db->quote($prodFamQuery->fields['product_value']).' ';
                 $prodFamCount++;
                 $prodFamQuery->MoveNext();
             }

             // If we had results, wrap it in ()
             if($prodFamCount > 0){
                 $sql_product_family = ' AND ( '.$sql_product_family;
                 $sql_product_family .= ')';
             }
         }
         $sql_where .= $sql_product_family;

        /*******************
         * ORDER BY
         *******************/
         $sql_orderby = '';
         if(isset($orderby) && sizeof($orderby) > 0){
             $sql_orderby = 'ORDER BY ';
             foreach($orderby as $orderChild => $orderDir){
                 $sql_orderby .= $orderChild.' '.$orderDir;
                 $sql_orderby .= ',';
             }
             $sql_orderby = substr($sql_orderby,0,-1).' ';
         }

        /*******************
         * Count
         *******************/
        $sql_groupby = null;
        if (isset($_GET['count'])){
            $sql_groupby = 'GROUP BY host_hostname DESC ';
        }

        $sql = $sql_select." \r".$sql_from." \r".$sql_where." \r".$sql_groupby.$sql_orderby;

        // Calculate Start
        $start = ($page-1)*$show;

        /**************
         * QUERY
         **************/
        $dbQuery = $db->SelectLimit($sql,$show,$start,$inputarr=false);

        $dbResult = array();
        if ($dbQuery){
            while (!$dbQuery->EOF) {
                $dbResult[] = $dbQuery->fields;
                $dbQuery->MoveNext();
 	    }
        }
        /**************
         * Count Total
         **************/
        if($dbQuery){
            $totalQuery = $db->Execute("SELECT `report_id`
                                        FROM `report`, `host`
                                        $sql_where");
            $totalResults = array();
            if($totalQuery){
                while(!$totalQuery->EOF){
                    $totalResults[] = $totalQuery->fields['report_id'];
                    $totalQuery->MoveNext();
                }
            }
        }

        return array('data' => $dbResult, 'totalResults' => sizeof($totalResults), 'reportList' => $totalResults);
    }

    function continuityParams($query_input, $omit){
        reset($query_input['where']);
        $standard = '';
        
        // if $omit is empty, make it a blank array
        if($omit == null){ $omit = array(); }

        // Where
        if(isset($query_input['where']) && sizeof($query_input['where']) > 0){
            foreach($query_input['where'] as $node => $item){
                if(!($item[0] == 'report_id' && $query_input['artificialReportID'])){
                    if(!in_array($item[0], $omit)){
                        if(is_numeric($item[2])){
                            $standard .= $item[0].'='.$item[2].'&amp;';;
                        } else {
                            $standard .= $item[0].'='.urlencode($item[2]).'&amp;';;
                        }
                    }
                }
            }
        }

        // Selected
        if(isset($query_input['selected']) && sizeof($query_input['selected']) > 0 && !in_array('selected', $omit)){
            foreach($query_input['selected'] as $selectedNode){
                if(!($selectedNode['field'] == 'report_id' && $query_input['artificialReportID'])){
                    if($selectedNode['field'] != 'count' && !in_array($selectedNode['field'], $omit)){
                        $standard .= 'selected%5B%5D='.$selectedNode['field'].'&amp;';
                    }
                }
            }
        }

        // Ascdesc
        if((isset($_GET['ascdesc']) && !in_array('ascdesc', $omit)) &&
           ($_GET['ascdesc'] == 'asc' || $_GET['ascdesc'] == 'desc')){
             $standard .= 'ascdesc='.$_GET['ascdesc'].'&amp;';
        }

        // Orderby
        if(isset($_GET['orderby']) && !in_array('orderby', $omit) && $this->orderbyChecked){
             $standard .= 'orderby='.$_GET['orderby'].'&amp;';
        }

        // Count
        if(isset($query_input['count']) && !in_array('count', $omit)){
             $standard .= 'count=on'.'&amp;';
        }

        // Show
        if(isset($query_input['show']) && !in_array('show', $omit)){
             $standard .= 'show='.$query_input['show'].'&amp;';
        }

        // ProductFam
        if(isset($query_input['product_family']) && !in_array('product_family', $omit)){
             $standard .= 'product_family='.$query_input['product_family'].'&amp;';
        }

        // Page
        if(isset($query_input['page']) && !in_array('page', $omit)){
             $standard .= 'page='.$query_input['page'].'&amp;';
        }

        // strip off any remaining &amp; that may be on the end
        if(substr($standard, -5) == '&amp;'){
            $standard = substr($standard, 0, -5);
        }

        // lets return
        
        return $standard;
    }

    function columnHeaders($query_input){
        $columnCount = 0;
        $column[$columnCount]['text'] = 'Detail';
        $columnCount++;

        foreach($query_input['selected'] as $selectedChild){
            if(!($selectedChild['field'] == 'report_id' && $query_input['artificialReportID'])){
                $column[$columnCount]['text'] = $selectedChild['title'];

                $o_orderby = $selectedChild['field'];
                // Figure out if it should be an asc or desc link
                if($selectedChild['field'] == $this->_firstChildKey($query_input['orderby']) && $query_input['orderby'][$selectedChild['field']] == 'desc'){
                    $o_ascdesc = 'asc';
                } else {
                    $o_ascdesc = 'desc';
                }

                if((isset($query_input['count'])) || !isset($query_input['count'])){
                    $column[$columnCount]['url'] = '?'.$this->continuityParams($query_input, array('ascdesc')).'&amp;orderby='.$o_orderby.'&amp;ascdesc='.$o_ascdesc;
                }
                $columnCount++;
            }
        }
        return $column;
    }

    function outputHTML($result, $query_input, $columnHeaders){
        global $iolib;

        $continuity_params = $this->continuityParams($query_input, array('count', 'ascdesc', 'orderby'));
        // Data
        $data = array();
        $rowNum = 0;
        if(sizeof($result['data']) > 0){
            foreach($result['data'] as $row){
                $colNum = 0;

                // Prepend if new_front;
                $data[$rowNum][0]['text'] = 'Detail';
                if (isset($row['count'])){
                    $data[$rowNum][0]['url']  = '/query/?host_hostname='.$row['host_hostname'].'&amp;'.$continuity_params.'&amp;selected%5B%5D=report_file_date';
                }
                else {
                    $data[$rowNum][0]['url']  = '/report/?report_id='.$row['report_id'].'&amp;'.$continuity_params;
                }
                $colNum++;
    
                foreach($row as $cellName => $cellData){
                    if($cellName == 'report_id' && $query_input['artificialReportID']){
                    } else {
                        $data[$rowNum][$colNum]['col'] = $cellName;

                        if($cellName == 'report_problem_type'){
                            $cellData = resolveProblemTypes($cellData);
                        }
                        else if($cellName == 'report_behind_login'){
                            $cellData = resolveBehindLogin($cellData);
                        }
                        $data[$rowNum][$colNum]['text'] = $cellData;
                    }
                    $colNum++;
                }
                $rowNum++;
            }
        }
        return array('data' => $data);
    }

    function outputXML(){}
    function outputCSV(){}
    function outputXLS(){}
    function outputRSS(){}

    function _calcOrderBy($selected, $orderby){
        $result = array();
        foreach($this->approved_selects as $selectedChild){
            if($this->_searchQueryInput($selected, $selectedChild) !== false &&
               !array_key_exists($selectedChild, $orderby))
            {
                $result[$selectedChild] = 'desc';
            }
        }

        return $result;
    }

    function _searchQueryInput($input, $query){
        for($i=0; $i< sizeof($input); $i++){
            if ($input[$i]['field'] == $query){
                return $i;
            }
        }
        return false;
    }

    function _firstChildKey($keyOf){
        $key = array_keys($keyOf);
        return $key[0];
    }
}
?>