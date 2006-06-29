<?php

/* Include the CSV library.  It would be nice to make this OO sometime */
vendor('csv/csv');

class ResultsController extends AppController {

    var $name = 'Results';

    /**
     * Model's this controller uses
     * @var array
     */
    var $uses = array('Application','Collection','Choice','Result');

    /**
     * Cake Helpers
     * @var array
     */
    var $helpers = array('Html', 'Javascript', 'Export', 'Pagination','Time','Breadcrumb');

    /**
     * Pagination helper variable array
     * @access public
     * @var array
     */
    var $pagination_parameters = array();

    /**
     * Will hold a sanitize object
     * @var object
     */
    var $Sanitize;

    /**
     * Constructor - sets up the sanitizer and the pagination
     */
    function ResultsController()
    {
        parent::AppController();

        $this->Sanitize = new Sanitize();

        // Pagination Stuff
            $this->pagination_parameters['show']      = empty($_GET['show'])?            '10' : $this->Sanitize->paranoid($_GET['show']);
            $this->pagination_parameters['sortBy']    = empty($_GET['sort'])?       'created' : $this->Sanitize->paranoid($_GET['sort']);
            $this->pagination_parameters['direction'] = empty($_GET['direction'])?      'desc': $this->Sanitize->paranoid($_GET['direction']);
            $this->pagination_parameters['page']      = empty($_GET['page'])?              '1': $this->Sanitize->paranoid($_GET['page']);
            $this->pagination_parameters['order']     = $this->modelClass.'.'.$this->pagination_parameters['sortBy'].' '.strtoupper($this->pagination_parameters['direction']);
    }

    /**
     * Add a new result.  This also means we're going to be adding to several other
     * tables (for the many to many relationships).  First we'll show a form, then if
     * there is a $_POST, we'll process.
     */
    function add()
    {
        // If these are set in $_POST, we'll use that for the query.  If this is set
        // in $_GET, then use that.  If neither are set, make it blank, and we'll
        // fail later on
        $_input_name = $this->Sanitize->Sql(isset($this->data['application'][0]) ?  $this->data['application'][0] : (isset($this->params['url']['application']) ?  $this->params['url']['application'] : ''));
        $_input_ua   = $this->Sanitize->Sql(isset($this->data['ua'][0]) ?  $this->data['ua'][0] : (isset($this->params['url']['ua']) ?  $this->params['url']['ua'] : ''));

        // The ua comes over $_GET in the form:
        //          x.x.x.x (aa-bb)
        // Where x is the versions and a and b are locale information.  We're not
        // interested in the locale information, 

        $_conditions = "name LIKE '{$_input_name}' AND version LIKE '{$_input_ua}'";
        $_application = $this->Application->findAll($_conditions);

        if (empty($_application)) {
            // The application they entered in the URL is not in the db.  We'll have
            // to put it in the db, but we don't want it to show up (in case they
            // just typed something in manually) so we'll flag it as not visible.
            $app = new Application();
            $app->set('name', $_input_name);
            $app->set('version', $_input_ua);
            $app->set('visible', 0);
            $app->save();

            $app_id = $app->getLastInsertID();

            // The database will handle any combination of questions
            // (issues/intentions) and applications+versions.  However, since we're
            // adding stuff in that comes in over the URL, we kinda have to guess at
            // what questions should be associated.  So, I'm running a stristr() on
            // the $_GET values to get a general set of questions to associate, and 
            // then manually adding those values to the table.
            if (stristr($this->params['url']['application'], 'Firefox') !== false) {
                // Intention Id's
                $this->Application->query("INSERT INTO applications_collections VALUES ({$app_id}, ".DEFAULT_FIREFOX_INTENTION_SET_ID.")");

                // Issue Id's
                $this->Application->query("INSERT INTO applications_collections VALUES ({$app_id}, ".DEFAULT_FIREFOX_ISSUE_SET_ID.")");

            } elseif (stristr($this->params['url']['application'], 'Thunderbird') !== false) {

                // Intention Id's
                $this->Application->query("INSERT INTO applications_collections VALUES ({$app_id}, ".DEFAULT_THUNDERBIRD_INTENTION_SET_ID.")");

                // Issue Id's
                $this->Application->query("INSERT INTO applications_collections VALUES ({$app_id}, ".DEFAULT_THUNDERBIRD_ISSUE_SET_ID.")"); 

            } else {
                // Whatever they entered doesn't have firefox or thunderbird in it.
                // All they're going to see is a comment box on the other end.
            }
            

            // We could just get the last inserted id, but we need all the info
            // below.  Also, cake caches the query, and won't return
            // anything if we don't alter it (add 1=1 to the end), since we already
            // did the same query earlier
            $_conditions = "name LIKE '{$_input_name}' AND version LIKE '{$_input_ua}' AND 1=1";
            $_application = $this->Application->findAll($_conditions);
        }
        // Pull the information for our radio buttons (only the
        // questions for their applications will be shown)
        $this->set('intentions', $this->Application->getIntentions($this->Sanitize->sql($_application[0]['Application']['id'])));

        // Checkboxes
        $this->set('issues', $this->Application->getIssues($this->Sanitize->sql($_application[0]['Application']['id'])));

        // We'll need the url parameters to put in hidden fields
        $this->set('url_params', $this->Sanitize->html($this->params['url']));

        // If there is no $_POST, show the form, otherwise, process the data and
        // forward the user on.
        if (empty($this->params['data'])) {
            $this->render();
        } else {
            // Add the application id from the last cake query
            $this->params['data'] = $this->params['data'] + $_application[0];
            if ($this->Result->save($this->params['data'])) {
                // Redirect
                $this->flash('Thank you.', '/results');
                exit;
            } else {
                // Saving failed.  This probably means a required field wasn't set.
                // Should we tell them it failed, or just redirect?  Hmm...
                $this->flash('Thank you.', '/results');
                exit;
            }
        }
    }

    /**
     * Front page will show the graph
     */
    function index() 
    {
        // Products dropdown
        $this->set('products', $this->Application->findAll('visible=1', null, 'Application.id ASC'));

        // Fill in all the data passed in $_GET
        $this->set('url_params',$this->decodeAndSanitize($this->params['url']));

        // Give us some breadcrumbs
        $this->set('breadcrumbs', array('Home' => 'http://mozilla.org', 'Uninstall Survey Results' => ''));

        // We'll need to include the graphing libraries
        $this->set('include_graph_libraries', true);

        // Fill in our question sets
        $this->set('collections',$this->Application->getCollectionsFromUrl($this->params['url'],'issue'));

        // Core data to show on page
        $this->set('descriptionAndTotalsData',$this->Result->getDescriptionAndTotalsData($this->params['url']));
    }

    /**
     * Display a table of user comments
     */
    function comments()
    {
        // Products dropdown
        $this->set('products', $this->Application->findAll('visible=1', null, 'Application.id ASC'));

        // Fill in all the data passed in $_GET
        $this->set('url_params',$this->decodeAndSanitize($this->params['url']));

        // Give us some breadcrumbs
        $this->set('breadcrumbs', array('Home' => 'http://mozilla.org', 'Uninstall Survey Results' => 'results/', 'Comments' => ''));

        // Pagination settings
            $paging['style'] = 'html';
            $paging['link']  = "/results/comments/?product=".urlencode($this->params['url']['product'])."&start_date=".urlencode($this->params['url']['start_date'])."&end_date=".urlencode($this->params['url']['end_date'])."&show={$this->pagination_parameters['show']}&sort={$this->pagination_parameters['sortBy']}&direction={$this->pagination_parameters['direction']}&page=";
            $paging['count'] = $this->Result->getCommentCount($this->params['url']);
            $paging['page']  = $this->pagination_parameters['page'];
            $paging['limit'] = $this->pagination_parameters['show'];
            $paging['show']  = array('10','25','50');

            // No point in showing them an error if they click on "show 50" but they are
            // already on the last page.
            if ($paging['count'] < ($this->pagination_parameters['page'] * ($this->pagination_parameters['show']/2))) {
                $this->pagination_parameters['page'] = $paging['page'] = 1;
            }

            // Set pagination array
            $this->set('paging',$paging);

        // Core data to show on page
        $this->set('commentsData',$this->Result->getComments($this->params['url'], $this->pagination_parameters));


    }

    /**
     * Display a csv
     */
    function csv()
    {
        // Get rid of the header/footer/etc.
        $this->layout = null;

        // Auto generated .csv's are turned off since they were taking too much
        // cpu/ram.  If you turn them back on, be sure to check the code - there was
        // a substantial database change between the time they were disabled and now.
        return false;

        $csv = new csv();

        $csv->loadDataFromArray($this->Result->getCsvExportData($this->params['url'], false));

        // Our CSV library sends headers and everything.  Keep the view empty!
        $csv->sendCSV();

        // I'm not exiting here in case someone is going to use post callback stuff.
        // In development, that means extra lines get added to our CSVs, but in
        // production it should be clean.
    }


}
?>
