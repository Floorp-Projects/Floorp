<?php
class Result extends AppModel {
    var $name = 'Result';

    var $belongsTo = array('Application', 'Intention');

    var $hasAndBelongsToMany = array('Issue' =>
                                   array('className' => 'Issue',
                                         'joinTable' => 'issues_results',
                                         'foreignKey'=> 'issue_id',
                                         'assocationForeignKey'=>'result_id',
                                         'uniq'      => true
                                        )
                               );

    /**
     * Count's all the comments, according to the parameters.
     * @param array URL parameters
     * @return Cake's findCount() value
     */
    function getCommentCount($params)
    {
        // Clean parameters
        $params = $this->cleanArrayForSql($params);

        // We only want to see rows with comments
        $_conditions = array("comments NOT LIKE ''");

        if (!empty($params['start_date'])) {
            $_timestamp = strtotime($params['start_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                array_push($_conditions, "`created` >= '{$_date}'");
            }
        }
        if (!empty($params['end_date'])) {
            $_timestamp = strtotime($params['end_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                array_push($_conditions, "`created` <= '{$_date}'");
            }
        }

        if (!empty($params['product'])) {
            // product's come in looking like:
            //      Mozilla Firefox 1.5.0.1
            $_exp = explode(' ',urldecode($params['product']));

            if(count($_exp) == 3) {
                $_product = $_exp[0].' '.$_exp[1];

                $_version = $_exp[2];

                /* Note that 'Application' is not the actual name of the table!  You can
                 * thank cake for that.*/
                array_push($_conditions, "`Application`.`name` LIKE '%{$_product}%'");
                array_push($_conditions, "`Application`.`version` LIKE '%{$_version}%'");
            } else {
                // defaults I guess?
                array_push($_conditions, "`Application`.`name` LIKE 'Mozilla Firefox'");
                array_push($_conditions, "`Application`.`version` LIKE '1.5'");
            }

        } else {
            // I'm providing a default here, because otherwise all results will be
            // returned (across all applications) and that is not desired
            array_push($_conditions, "`Application`.`name` LIKE 'Mozilla Firefox'");
            array_push($_conditions, "`Application`.`version` LIKE '1.5'");
        }

        // Do the actual query
        $comments = $this->findCount($_conditions);

        return $comments;
    }

    /**
     * Will retrieve all the comments within param's and pagination's parameters
     * @param array URL parameters
     * @param array pagination values from the controller
     * @param boolean if privacy is true phone numbers and email addresses will be
     * masked
     * @return cake result set
     */
    function getComments($params, $pagination, $privacy=true)
    {
        $params = $this->cleanArrayForSql($params);

        // We only want to see rows with comments
        $_conditions = array("comments NOT LIKE ''");

        if (!empty($params['start_date'])) {
            $_timestamp = strtotime($params['start_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                array_push($_conditions, "`created` >= '{$_date}'");
            }
        }
        if (!empty($params['end_date'])) {
            $_timestamp = strtotime($params['end_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                array_push($_conditions, "`created` <= '{$_date}'");
            }
        }

        if (!empty($params['product'])) {
            // product's come in looking like:
            //      Mozilla Firefox 1.5.0.1
            $_exp = explode(' ',urldecode($params['product']));

            if(count($_exp) == 3) {
                $_product = $_exp[0].' '.$_exp[1];

                $_version = $_exp[2];

                /* Note that 'Application' is not the actual name of the table!  You can
                 * thank cake for that.*/
                array_push($_conditions, "`Application`.`name` LIKE '%{$_product}%'");
                array_push($_conditions, "`Application`.`version` LIKE '%{$_version}%'");
            } else {
                // defaults I guess?
                array_push($_conditions, "`Application`.`name` LIKE 'Mozilla Firefox'");
                array_push($_conditions, "`Application`.`version` LIKE '1.5'");
            }

        } else {
            // I'm providing a default here, because otherwise all results will be
            // returned (across all applications) and that is not desired
            array_push($_conditions, "`Application`.`name` LIKE 'Mozilla Firefox'");
            array_push($_conditions, "`Application`.`version` LIKE '1.5'");
        }

        $comments = $this->findAll($_conditions, null, $pagination['order'], $pagination['show'], $pagination['page']);

        if ($privacy) {
            // Pull out all the email addresses and phone numbers
            foreach ($comments as $var => $val) {

                // Handle foo@bar.com
                $_email_regex = '/\ ?(.+)?@(.+)?\.(.+)?\ ?/';
                $comments[$var]['Result']['comments'] = preg_replace($_email_regex,'$1@****.$3',$comments[$var]['Result']['comments']);
                $comments[$var]['Result']['intention_text'] = preg_replace($_email_regex,'$1@****.$3',$comments[$var]['Result']['intention_text']);

                // Handle xxx-xxx-xxxx
                $_phone_regex = '/([0-9]{3})[ .-]?[0-9]{4}/';
                $comments[$var]['Result']['comments'] = preg_replace($_phone_regex,'$1-****',$comments[$var]['Result']['comments']);
                $comments[$var]['Result']['intention_text'] = preg_replace($_phone_regex,'$1-****',$comments[$var]['Result']['intention_text']);
            }
        }


        return $comments;
    }


    /**
     * This function runs the query to get the export data for the CSV file.
     *
     * @param array URL parameters
     * @param boolean if privacy is true phone numbers and email addresses will be
     * masked
     * @return array two dimensional array that should be pretty easy to transform
     *  into a CSV.
     */
    function getCsvExportData($params, $privacy=true)
    {
        $params = $this->cleanArrayForSql($params);

        // We have to use a left join here because there isn't always an intention
        $_query = "
            SELECT
                `results`.`id`,
                `results`.`created`,
                `results`.`intention_text` as `intention_other`,
                `results`.`comments`,
                `intentions`.`description` as `intention`
            FROM `results` 
            LEFT JOIN `intentions` ON `results`.`intention_id`=`intentions`.`id` 
            INNER JOIN `applications` ON `applications`.`id` = `results`.`application_id`
            WHERE
                1=1
        ";
        
        if (!empty($params['start_date'])) {
            $_timestamp = strtotime($params['start_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                $_query .= " AND `results`.`created` >= '{$_date}'";
            }
        }

        if (!empty($params['end_date'])) {
            $_timestamp = strtotime($params['end_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                $_query .= " AND `results`.`created` <= '{$_date}'";
            }
        }

        if (!empty($params['product'])) {
            // product's come in looking like:
            //      Mozilla Firefox 1.5.0.1
            $_exp = explode(' ',urldecode($params['product']));

            if(count($_exp) == 3) {
                $_product = $_exp[0].' '.$_exp[1];

                $_version = $_exp[2];

                $_query .= " AND `applications`.`name` LIKE '{$_product}'";
                $_query .= " AND `applications`.`version` LIKE '{$_version}'";
            } else {
                // defaults I guess?
                $_query .= " AND `applications`.`name` LIKE 'Mozilla Firefox'";
                $_query .= " AND `applications`.`version` LIKE '1.5'";
            }
        } else {
            // I'm providing a default here, because otherwise all results will be
            // returned (across all applications) and that is not desired
            $_query .= " AND `applications`.`name` LIKE 'Mozilla Firefox'";
            $_query .= " AND `applications`.`version` LIKE '1.5'";
        }

        $_query .= " ORDER BY `results`.`created` ASC";

        $res = $this->query($_query);

        // Since we're exporting to a CSV, we need to flatten the results into a 2
        // dimensional table array
        $newdata = array();

        foreach ($res as $result) {

            $newdata[] = array_merge($result['results'], $result['intentions']);
        }

        if ($privacy) {
            // Pull out all the email addresses and phone numbers
            foreach ($newdata as $var => $val) {

                // Handle foo@bar.com
                $_email_regex = '/\ ?(.+)?@(.+)?\.(.+)?\ ?/';
                $newdata[$var]['comments'] = preg_replace($_email_regex,'$1@****.$3',$newdata[$var]['comments']);
                $newdata[$var]['intention_other'] = preg_replace($_email_regex,'$1@****.$3',$newdata[$var]['intention_other']);

                // Handle xxx-xxx-xxxx
                $_phone_regex = '/([0-9]{3})[ .-]?[0-9]{4}/';
                $newdata[$var]['comments'] = preg_replace($_phone_regex,'$1-****',$newdata[$var]['comments']);
                $newdata[$var]['intention_other'] = preg_replace($_phone_regex,'$1-****',$newdata[$var]['intention_other']);
            }
        }

        // Our CSV library just prints out everything in order, so we have to put the
        // column labels on here ourselves
        $newdata = array_merge(array(array_keys($newdata[0])), $newdata);

        return $newdata;
    }

    /**
     * Will retrieve the information used for graphing.
     * @param the url parameters (unescaped)
     * @return a result set
     */
    function getDescriptionAndTotalsData($params)
    {
        // Clean parameters for inserting into SQL
        $params = $this->cleanArrayForSql($params);

        /* It would be nice to drop something like this in the SELECT:
         * 
         *   CONCAT(COUNT(*)/(SELECT COUNT(*) FROM our_giant_query_all_over_again)*100,'%') AS `percentage`
         */

        $_query = "
              SELECT
                  issues.description,
                  COUNT( DISTINCT results.id ) AS total
              FROM
                  issues
              LEFT JOIN 
                issues_results ON issues_results.issue_id=issues.id
              LEFT JOIN results ON results.id=issues_results.result_id AND results.application_id=applications.id
              JOIN applications_issues ON applications_issues.issue_id=issues.id
              JOIN applications ON applications.id=applications_issues.application_id
              WHERE 1=1
        ";

        if (!empty($params['start_date'])) {
            $_timestamp = strtotime($params['start_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                $_query.= " AND `results`.`created` >= '{$_date}'";
            }
        }

        if (!empty($params['end_date'])) {
            $_timestamp = strtotime($params['end_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                $_query .= " AND `results`.`created` <= '{$_date}'";
            }
        }

        if (!empty($params['product'])) {
            // product's come in looking like:
            //      Mozilla Firefox 1.5.0.1
            $_exp = explode(' ',urldecode($params['product']));
            
            if(count($_exp) == 3) {
                $_product = $_exp[0].' '.$_exp[1];

                $_version = $_exp[2];

                $_query .= " AND `applications`.`name` LIKE '{$_product}'";
                $_query .= " AND `applications`.`version` LIKE '{$_version}'";
            } else {
                // defaults I guess?
                $_query .= " AND `applications`.`name` LIKE 'Mozilla Firefox'";
                $_query .= " AND `applications`.`version` LIKE '1.5'";
            }
        } else {
            // I'm providing a default here, because otherwise all results will be
            // returned (across all applications) and that is not desired
            $_query .= " AND `applications`.`name` LIKE 'Mozilla Firefox'";
            $_query .= " AND `applications`.`version` LIKE '1.5'";
        }

        $_query .= " GROUP BY `issues`.`description`
                     ORDER BY `issues`.`description` DESC";

        return $this->query($_query); 
    }

    /**
     * We've got complex info to save, so I'm overriding the default save method.
     * Too bad we can't leverage some cake awesomeness. :(
     *
     * @param array Big cake array, filled with juicy $_POST goodness.  It should be
     * the same structure as any other array created by the html helper.
     * @return boolean true on success, false on failure
     */
    function save($data)
    {
        // Apparently these are all escaped for us by cake.  It still makes me
        // nervous.
        $_application_id  = $data['Application']['id'];
        $_intention_id    = $data['Result']['intention_id'];
        $_comments        = $data['Result']['comments'];
        $_issues_text     = $data['issues_results']['other'];
        $_intention_text  = $data['Result']['intention_text'];
        // Joined for legacy reasons
            $_user_agent  = mysql_real_escape_string("{$data['ua'][0]} {$data['lang'][0]}");
        $_http_user_agent = mysql_real_escape_string($_SERVER['HTTP_USER_AGENT']);

        // Make sure our required variables are set and correct
        if (!is_numeric($_application_id) || !is_numeric($_intention_id)) {
            return false;
        }

        // Special cases for the "other" fields.  If their corresponding option isn't
        // set, we don't want the field values.
            // issue is determined below
            $_issue_array     = $this->Issue->findByDescription('other');
            $_intention_array = $this->Intention->findByDescription('other');
            
            if ($_intention_id != $_intention_array['Intention']['id']) {
                $_intention_text = '';
            }

        $this->set('application_id', $_application_id);
        $this->set('intention_id', $_intention_id);
        $this->set('intention_text', $_intention_text);
        $this->set('comments', $_comments);
        $this->set('useragent', $_user_agent);
        $this->set('http_user_agent', $_http_user_agent);

        // We kinda overrode $this's save(), so we'll have to ask our guardians
        parent::save();


        // The issues_results table isn't represented by a class in cake, so we have
        // to do the query manually.
        if (!empty($data['Issue']['id'])) {

            $_result_id = $this->getLastInsertID();
            $_query     = '';

            foreach ($data['Issue']['id'] as $var => $val) {
                // This should never happen, but hey...
                if (!is_numeric($val)) {
                    continue;
                }

                // If the 'other' id matches the id we're putting in, add the issue text
                $_other_text = ($val == $_issue_array['Issue']['id']) ? $_issues_text : '';

                $_query .= empty($_query) ? "({$_result_id},{$val},'{$_other_text}')" : ",({$_result_id},{$val},'{$_other_text}')";
            }

            $_query = "
            INSERT INTO issues_results(
                result_id,
                issue_id,
                other
            ) VALUES 
                {$_query}
            ";

            $this->query($_query);
        }

        return false;
    }
}
?>
