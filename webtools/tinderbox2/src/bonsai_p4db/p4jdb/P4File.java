package p4jdb ;
import p4jdb.* ;
import java.lang.* ;
import java.util.* ;
import java.net.* ;
import java.applet.* ;
import java.awt.* ;
import java.awt.event.* ;

/**
 * Represents a directory folder in the p4 depot.
 */
public class P4File implements TreeDisplay.Data {

    /**
     * Name and full path of directory
     */
    private String name_,path_,urlPath_ ;

    /**
     * file revision
     */
    private int rev_ ;

    /**
     * file status code
     */
    public String status_ ;

    /**
     * "level" in directory tree
     */
    private int level_ ;
    
    public P4File(String name,		  
		  String path,
		  int rev,
		  int level,
		  String status) 
    {
	name_   = name ;
	path_   = path ;
	urlPath_ = URLEncoder.encode(path_+"/"+name_) ;
	rev_    = rev ;
	level_  = level ;
	status_ = status ;
    }

    /**
     * Draw line info
     *@param height Line height
     *@param p      Start point (lower right)
     *@param g      Graphics object to use
     *@return width of object drawn (or 0 if no object)
     */
    public int draw(int height, Point p, Graphics g) 
    {
	final String name = name_ + "#" + rev_ ;
	final FontMetrics fm = g.getFontMetrics() ;
	final int width = fm.stringWidth(name) ;
	g.setColor(Color.black) ;
	g.drawString(name,p.x,p.y) ;
	if(status_.equals("de")) {
	    g.setColor(Color.red) ;
	    final int ylevel = p.y - (fm.getHeight()/3) ;
	    g.drawLine(p.x,ylevel,
		       p.x+width,ylevel) ;
	} ;
	return width ;
    }
	
    /**
     *@return level in tree (number of tics to indent)
     */
    public int level() 
    {
	return level_ ;
    }

    /**
     * Invoked when the mouse has been clicked on an object.
     *@param cp  Component where object is drawn
     *@param pt  Point within component where mouse is clicked
     *@param rpt Point within object (relative to zero position
     *           given in draw() metod
     */
    public void mouseClicked(Component cp, Point pt, Point rpt) 
    {
	final String HISTORY = "View file history" ;
	final String VIEW = "View file" ;
	final String VIEW_CHANGES = "View changes for file" ;
	PopupMenu popup = new PopupMenu(path_+"/"+name_) ;
	popup.add(HISTORY) ;
	popup.add(VIEW) ;
	popup.add(VIEW_CHANGES) ;
	popup.addActionListener(new ActionListener() {
	    public void actionPerformed(ActionEvent e) {
		AppletContext ac = P4DirTree.applet.getAppletContext() ;
		URL url ;
		try {
		    if(e.getActionCommand() == HISTORY) 
			url = new URL(P4DirTree.applet.getDocumentBase(),
				      "fileLogView.cgi?FSPC="+urlPath_) ;
		    else if (e.getActionCommand() == VIEW)
			url = new URL(P4DirTree.applet.getDocumentBase(),
				      "fileViewer.cgi?FSPC="+urlPath_+"&REV="+rev_) ;
		    else // if (e.getActionCommand() == VIEW_CHANGES)
			url = new URL(P4DirTree.applet.getDocumentBase(),
				      "changeList.cgi?FSPC="+urlPath_) ;
		    ac.showDocument(url) ;
		}
		catch (Throwable t) {
		    System.err.println("Exception: "+t.toString()) ;
		}
	    }
	}) ;	
	cp.add(popup) ;
	popup.show(cp,pt.x,pt.y) ;
    } ;    
} ;










