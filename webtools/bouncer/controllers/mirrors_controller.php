<?php
uses('sanitize');

class MirrorsController extends AppController
{
    var $name = 'Mirrors';
    var $helpers = array('Html', 'Pagination');
    var $show;
    var $sortBy;
    var $direction;
    var $page;
    var $order;
    var $sanitize;
    var $scaffold;
 
    function __construct()
    {
        $this->sanitize = &new Sanitize;
        $this->show = empty($_GET['show'])? '10': $this->sanitize->sql($_GET['show']);
        $this->sortBy = empty($_GET['sort'])? 'Mirror.mirror_id': $this->sanitize->sql($_GET['sort']);
        $this->direction = empty($_GET['direction'])? 'desc': $this->sanitize->sql($_GET['direction']);
        $this->page = empty($_GET['page'])? '1': $this->sanitize->sql($_GET['page']);
        $this->order = $this->sortBy.' '.strtoupper($this->direction);
        parent::__construct();
    }
 
    function index()
    {
        $data = $this->Mirror->findAll($criteria=null, $fields=null, $this->order, $this->show, $this->page);
 
        $paging['style'] = 'html'; //set the style of the links: html or ajax

        foreach ($this->Mirror->_tableInfo->value as $column) {

            // If the sortBy is the same as the current link, we want to switch it.
            // By default, we don't -- so that when a user changes columns, the order doesn't also reverse.
            if ($this->sortBy == $column['name']) {
                switch ($this->direction) {
                    case 'desc':
                        $link_direction = 'asc';
                        break;
                    case 'asc':
                    default:
                        $link_direction = 'desc';
                        break;
                }
            } else {
                $link_direction =& $this->direction;
            }
            $paging['headers'][$column['name']] = $this->sanitize->html('/mirrors/?show='.$this->show.'&sort='.$column['name'].'&direction='.$link_direction.'&page='.$this->page);
        }

        $paging['link'] = $this->sanitize->html('./?show='.$this->show.'&sort='.$this->sortBy.'&direction='.$this->direction.'&page=');
        $paging['count'] = $this->Mirror->findCount($criteria=null,'1000');
        $paging['page'] = $this->page;
        $paging['limit'] = $this->show;
        $paging['show'] = array('10','25','50','100');
        
        $this->set('paging',$paging);
        $this->set('data',$data);
    }

    function view($id) {
        $this->Mirror->setId($id);
        $this->set('data', $this->Mirror->read());
    }

    function destroy($id) {
        if (empty($this->params['data'])) {
            $this->set('data', $this->Mirror->read());
            $this->render();
        } elseif ($this->Mirror->del($id)) {
            $this->flash('Mirror '.$id.' has been deleted.', '/mirrors');
        }
    }
}
?>
