<?php

/* Include the CSV library.  It would be nice to make this OO sometime */
vendor('csv/csv');

class ResultsController extends AppController {

    var $name = 'Results';

    /**
     * Model's this controller uses
     * @var array
     */
    var $uses = array('Application','Result');

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
     * Front page will show the graph
     */
    function index() 
    {
        // Products dropdown
        $this->set('products', $this->Application->getApplications());

        // Fill in all the data passed in $_GET
        $this->set('url_params',$this->decodeAndSanitize($this->params['url']));

        // Give us some breadcrumbs
        $this->set('breadcrumbs', array('Home' => 'http://mozilla.org', 'Uninstall Survey Results' => ''));

        // We'll need to include the graphing libraries
        $this->set('include_graph_libraries', true);

        // Core data to show on page
        $this->set('descriptionAndTotalsData',$this->Result->getDescriptionAndTotalsData($this->params['url']));
    }

    /**
     * Display a table of user comments
     */
    function comments()
    {
        // Products dropdown
        $this->set('products', $this->Application->getApplications());

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

        // Our CSV library sends headers and everything.  Keep the view empty!
        csv_send_csv($this->Result->getCsvExportData($this->params['url']));

        // I'm not exiting here in case someone is going to use post callback stuff.
        // In development, that means extra lines get added to our CSVs, but in
        // production it should be clean.
    }


}
?>
