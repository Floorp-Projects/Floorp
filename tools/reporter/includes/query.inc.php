<?php

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
                                  'report_charset',
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
                                 'report_file_date',
                                 'report_charset'
    );

    var $orderbyChecked = false; // If we are ordering, used by continuity params

    var $selected;               // Selected part of query
    var $where;                  // Where part of query
    var $orderby;                // How to order query
    var $show;                   // How many to show on results page
    var $page;                   // The page we are (page*show = starting result #)
    var $count;                  // Should we count (top 25 page)
    var $product_family;         // The product family we are searching on
    var $artificialReportID;     // Only used when report_id is not in query results, as we need a report id,
                                 //  regardless of if the user wants to see it.

    var $totalResults;           // How many results total in the database for the query
    var $reportList;             // if totalResults < max_nav_count has list of report_id's for next/prev nav
    var $resultSet;              // Actual data
    var $queryHost = false;      // Does this query involve the host table explicity?

    function query(){
        $this->processQueryInputs();
        return;
    }

    function processQueryInputs(){
        global $config;


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
            $this->show = $config['show'];
        } else {
            $this->show = $_GET['show'];
        }


        /*******************
         * PAGE (really other part of "Limit"
         *******************/
        if (!isset($_GET['page']) ||
            $_GET['page'] == null)
        {
            $this->page = 1;
        } else {
            $this->page = $_GET['page'];
        }


        /*******************
         * Count
         *******************/
        if (isset($_GET['count'])){
            $this->count = 'report_id'; // XX limitation for now
        }
        // if nothing... it's nothing


        /*******************
         * SELECT
         *******************/
        /*
           If user defines what to select and were not counting, just
           use their input.
        */
        if (isset($_GET['selected']) && !isset($_GET['count'])){
            $this->selected = array();
            foreach($_GET['selected'] as $selectedChild){
                if(in_array(strtolower($selectedChild), $this->approved_selects)){
                    $this->selected[] = array('field' => $selectedChild,
                                              'title' => $config['fields'][$selectedChild]);
                }
            }
        } else {
            // Otherwise, we do it for  them
            $this->selected[] = array('field' => 'host_hostname',
                                      'title' => $config['fields']['host_hostname']);

            // Showing date is good just about always, except when using a count
            if(!isset($_GET['count'])){
                $this->selected[] = array('field' => 'report_file_date',
                                          'title' => $config['fields']['report_file_date']);
            }
        }

        // Find out if we are working with a specific host
        if(isset($_GET['host_hostname']) && $_GET['host_hostname'] != null){
            $this->queryHost = true;
        }

         // We need the report_id regardless of if the user wants to see it, so we add it ourselves,
        // but also set artificalReportID, so we know to pull it before we display results.
        if($this->_searchQueryInput($this->selected, 'report_id') === false) {
            $this->artificialReportID = true;
            $this->selected[] = array('field' => 'report_id',
                                      'title' => $config['fields']['report_id']);
        }


      /*******************
       * ORDER BY
       *******************/
        $this->orderby = array();
        // The first priority is those who were recently specified (such as a menu header clicked).
        if(isset($_GET['orderby']) && in_array(strtolower($_GET['orderby']), $this->approved_selects)){
            $this->orderby[$_GET['orderby']] = $ascdesc;

            // For continuity Params we set this to true to verify it's been checked as matching an approved val
            $this->orderbyChecked = true;
        }


      /*******************
       * COUNT
       *******************/
        // After, we append those who werere selected previously and order desc.
        if(!isset($_GET['count'])){
            $this->orderby = array_merge($this->orderby, $this->_calcOrderBy());
        }

        // If we are counting, we need to add A column for it
        if (isset($_GET['count'])){
            // set the count variable
            $next =sizeof($this->selected)+1;
            $this->selected[$next]['field'] = 'count';
            $this->selected[$next]['title'] = 'Amount';

            // Hardcode host_id
            $_GET['count'] = 'report_id'; // XXX we just hardcode this (just easier for now, and all people will be doing).
            // XX NOTE:  We don't escape count below because 'host_id' != `host_id`.


            /*******************
             * ORDER BY
             *******************/
            if (isset($_GET['count'])){
                if(!isset($this->orderby['count'])){
                    $this->orderby['count'] = 'desc'; // initially hardcode to desc
                }
                if(!isset($this->orderby['host_hostname'])){
                    $this->orderby['host_hostname'] = 'desc';
                }
            }
        }

        /*******************
         * PRODUCT FAMILY
         *******************/
        $this->product_family = "";
        if(isset($_GET['product_family'])){
            $this->product_family = $_GET['product_family'];
        }


        /*******************
         * WHERE
         *******************/
        $this->where = array();
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
                        $this->where[] = array($column, $operator, $value);
                    }
                }
            }
        }

        return true;
    }

    function doQuery(){
        global $db, $config;


        /************
         * SELECT
         ************/
        $sql_select = 'SELECT ';

        foreach($this->selected as $select_child){
            // we don't $db->quote here since unless it's in our approved array (exactly), we drop it anyway. i.e. report_id is on our list, 'report_id' is not.
            // we sanitize on our own
            if ($select_child['field'] == 'count'){
                $sql_select .= 'COUNT( '.$this->count.' ) AS count';
                if(!isset($this->orderby['count'])){
                    $this->orderby = array_merge(array('count'=> 'DESC'), $this->orderby);
                }
            } else {
                $sql_select .= $select_child['field'];
            }
            $sql_select .= ', ';
        }
        if(sizeof($this->selected) > 0){
            $sql_select = substr($sql_select, 0, -2);
            $sql_select = $sql_select.' ';
        }


        /************
         * FROM
         ************/
        $sql_from = 'FROM report, host';
        if($this->queryHost != true){
            $sql_from_count = 'FROM report';
        } else {
            $sql_from_count = $sql_from;
        }


        /************
         * WHERE
         ************/
        $sql_where = 'WHERE ';

        $host_join_str = 'host.host_id = report_host_id AND '; // this is a var to help them remain consistant for when it's deleted
                                                 
        $sql_where .= $host_join_str;

        foreach($this->where as $where_child){
            // we make sure to use quote() here to escape any evil on the value.  The others are sane
            $sql_where .= $where_child[0].' '.$where_child[1].' '.$db->quote($where_child[2]).' AND ';
        }

        // Dates are special
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

        // Notice: we now have a sql_where_count, this is what's used when we query again for the COUNT() of all records
        // that match the query (for pagination purposes).  Because we don't get actual results (just a COUNT()), it can
        // be trimmed down a bit for performance reasons.

        // Note this must be done before we strip the trailing "AND" or else the str_replace() won't work
        if($this->queryHost != true){
            $sql_where_count = str_replace($host_join_str, '', $sql_where);
        } else {
           $sql_where_count = $sql_where;
        }

        // If there is a trailing "AND", lets get rid of it
        if(substr(strtoupper(trim($sql_where)),-3) == 'AND'){
            $sql_where = substr(trim($sql_where),0, -3);
        }
        // and again for the where count
        if(substr(strtoupper(trim($sql_where_count)),-3) == 'AND'){
            $sql_where_count = substr(trim($sql_where_count),0, -3);
        }

        // If the where clause is simply "WHERE", delete it
        if(strtoupper(trim($sql_where_count)) == 'WHERE'){
            $sql_where_count = '';
        }


        /*******************
         * ORDER BY
         *******************/
         if ($this->product_family){
             // product_family
             $prodFamQuery = $db->Execute("SELECT product.product_value
                                           FROM product
                                           WHERE product.product_family = ".$db->quote($this->product_family)." ");
             $sql_product_family = "";
             if(!$prodFamQuery){
                 trigger_error("A database error occured.", E_USER_ERROR);
                 return false;
             }

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
                 $sql_product_family = " AND ( $sql_product_family )";

             // For when something silly is found ("like Firefox 2000")
             } else {
                 trigger_error("No products were found that matched: ".htmlentities($this->product_family), E_USER_ERROR);
                 return false;
             }
             $sql_where .= $sql_product_family;
         }


        /*******************
         * ORDER BY
         *******************/
         $sql_orderby = '';
         if(isset($this->orderby) && sizeof($this->orderby) > 0){
             $sql_orderby = 'ORDER BY ';
             foreach($this->orderby as $orderChild => $orderDir){
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
            $sql_groupby = 'GROUP BY host.host_hostname DESC ';
        }

        $sql = $sql_select." \r".$sql_from." \r".$sql_where." \r".$sql_groupby.$sql_orderby;

        // Calculate Start
        $start = ($this->page-1)*$this->show;

        
        /**************
         * QUERY
         **************/
        $dbQuery = $db->SelectLimit($sql,$this->show,$start,$inputarr=false);

        if (!$dbQuery){
           trigger_error("A database error occured.", E_USER_ERROR);
           return false;
        }
        $this->resultSet = array();
        while (!$dbQuery->EOF) {
            $this->resultSet[] = $dbQuery->fields;
            $dbQuery->MoveNext();
        }

        
        /**************
         * Count Total
         **************/
        $totalCount = $db->Execute("SELECT COUNT(report.report_id) AS total
                                    $sql_from_count
                                    $sql_where_count");
        if(!$totalCount){
            trigger_error("A database error occured.", E_USER_ERROR);
            return false;
        }
        $this->totalResults = $totalCount->fields['total'];

        // Get the first 2000 report id's for prev/next navigation, only if < 2000 results and
        // only if count isn't being done (since you can't next/prev through that).
        if($this->totalResults < $config['max_nav_count']  && $this->count == null){
            $listQuerySQL = "SELECT report.report_id
                             FROM report, host ".
                             $sql_where." \r".
                             $sql_groupby.$sql_orderby;

            $listQuery = $db->SelectLimit($listQuerySQL,2001,0,$inputarr=false);

            if(!$listQuery){
                trigger_error("A database error occured.", E_USER_ERROR);
                return false;
            }
            $this->reportList = array();
            while(!$listQuery->EOF){
                $this->reportList[] = $listQuery->fields['report_id'];
                $listQuery->MoveNext();
            }
	}
        return true;
    }

    function continuityParams($omit = null){
        reset($this->where);
        $standard = '';

        // if $omit is empty, make it a blank array
        if($omit == null){ $omit = array(); }

        // Where
        if(isset($this->where) && sizeof($this->where) > 0){
            foreach($this->where as $node => $item){
                if(!($item[0] == 'report_id' && $this->artificialReportID)){
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
        if(isset($this->selected) && sizeof($this->selected) > 0 && !in_array('selected', $omit)){
            foreach($this->selected as $selectedNode){
                if(!($selectedNode['field'] == 'report_id' && $this->artificialReportID)){
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
        if(isset($this->count) && !in_array('count', $omit)){
             $standard .= 'count=on'.'&amp;';
        }

        // Show
        if(isset($this->show) && !in_array('show', $omit)){
             $standard .= 'show='.$this->show.'&amp;';
        }

        // ProductFam
        if(isset($this->product_family) && !in_array('product_family', $omit)){
             $standard .= 'product_family='.$this->product_family.'&amp;';
        }

        // Page
        if(isset($this->page) && !in_array('page', $omit)){
             $standard .= 'page='.$this->page.'&amp;';
        }

        // strip off any remaining &amp; that may be on the end
        if(substr($standard, -5) == '&amp;'){
            $standard = substr($standard, 0, -5);
        }

        // lets return

        return $standard;
    }

    function columnHeaders(){
        $columnCount = 0;
        $column[$columnCount]['text'] = 'Detail';
        $columnCount++;

        $contParams = $this->continuityParams(array('ascdesc'));
        foreach($this->selected as $selectedChild){
            if(!($selectedChild['field'] == 'report_id' && $this->artificialReportID)){
                $column[$columnCount]['text'] = $selectedChild['title'];

                $o_orderby = $selectedChild['field'];
                // Figure out if it should be an asc or desc link
                if($selectedChild['field'] == $this->_firstChildKey($this->orderby) && $this->orderby[$selectedChild['field']] == 'desc'){
                    $o_ascdesc = 'asc';
                } else {
                    $o_ascdesc = 'desc';
                }

                if((isset($this->count)) || !isset($this->count)){
                    $column[$columnCount]['url'] = '?'.$contParams.'&amp;orderby='.$o_orderby.'&amp;ascdesc='.$o_ascdesc;
                }
                $columnCount++;
            }
        }
        return $column;
    }

    function outputHTML(){
        global $iolib;

        $continuity_params = $this->continuityParams(array('count', 'ascdesc', 'orderby'));
        // Data
        $output = array();
        $rowNum = 0;
        if(sizeof($this->resultSet) > 0){
            foreach($this->resultSet as $row){
                $colNum = 0;

                // Prepend if new_front;
                $output[$rowNum][0]['text'] = 'Detail';
                if (isset($row['count'])){
                    $output[$rowNum][0]['url']  = '/query/?host_hostname='.$row['host_hostname'].'&amp;'.$continuity_params.'&amp;selected%5B%5D=report_file_date';
                }
                else {
                    $output[$rowNum][0]['url']  = '/report/?report_id='.$row['report_id'].'&amp;'.$continuity_params;
                }
                $colNum++;

                foreach($row as $cellName => $cellData){
                    if(!($cellName == 'report_id' && $this->artificialReportID)){
                        $output[$rowNum][$colNum]['col'] = $cellName;
                        if($cellName == 'report_problem_type'){
                            $cellData = resolveProblemTypes($cellData);
                        }
                        else if($cellName == 'report_behind_login'){
                            $cellData = resolveBehindLogin($cellData);
                        }
                        $output[$rowNum][$colNum]['text'] = $cellData;

                        $colNum++;
                    }
                }
		$rowNum++;
            }
        }
        return $output;
    }

    function outputXML(){}
    function outputCSV(){}
    function outputXLS(){}
    function outputRSS(){}

    function _calcOrderBy(){
        $result = array();
        foreach($this->approved_selects as $selectedChild){
            if($this->_searchQueryInput($this->selected, $selectedChild) !== false &&
               !array_key_exists($selectedChild, $this->orderby))
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