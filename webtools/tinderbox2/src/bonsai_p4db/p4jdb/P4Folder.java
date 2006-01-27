package p4jdb ;
import p4jdb.* ;
import java.lang.* ;
import java.util.* ;
import java.applet.* ;
import java.awt.* ;
import java.awt.event.* ;
import java.io.* ;
import java.net.* ;

/**
 * Represents a directory folder in the p4 depot.
 */
public class P4Folder implements TreeDisplay.Data {

    /**
     * Name and full path of directory
     */
    public String name_,path_,urlPath_;

    /**
     * Vector that contains the contents of the directory
     * (other directories and files...)
     */
    private Vector contents_ ;
    
    /**
     * true if directory "open", that is: if contents is displayed
     */
    private boolean open_ ;

    /**
     * true of directory scanned
     */
    private boolean scanned_ ;

    /**
     * "level" in directory tree
     */
    private int level_ ;

    /**
     * X value for border between folder and name
     */
    private int borderX_ ;

    static private String helpText = "P4DB experimental java browser" ;

    /**
     * Constructor
     */
    public P4Folder(String name, 
		    String path,
		    int level) 
    {
	level_        = level ;
	name_         = name ;
	path_         = path+"/"+name ;
	urlPath_      = URLEncoder.encode(path_) ;
	open_         = false ;
	scanned_      = false ;
	contents_     = new Vector() ;
	if(level == 0) {
	    scan(null) ;
	    open_ = true ;
	} ;	    
    } ;

    /**
     * Scan 
     */
    public void scan(Component cp) {

	if(cp != null)
	    cp.setCursor(new Cursor(Cursor.WAIT_CURSOR)) ;
	try {

	    URL u =  new URL(P4DirTree.applet.getDocumentBase(),
			     P4DirTree.applet.getParameter("File")+
			     "?CMD=DIRSCAN&FSPC="+urlPath_+"/"+"*") ;
	    URLConnection conn = u.openConnection() ; 
	    InputStreamReader isr = new InputStreamReader(conn.getInputStream()) ;
	    BufferedReader in = new BufferedReader(isr);
	    StreamTokenizer tok = new StreamTokenizer(in) ;
	    int nxt ;
	    
	    while((nxt = tok.nextToken()) != StreamTokenizer.TT_EOF) {
		String type = tok.sval ;
		tok.nextToken() ;
		String name = tok.sval ;
		if(type.equals("D")) {
		    contents_.addElement(new P4Folder(name,
						      path_,
						      level_+1)) ;
		    
		}
		else {
		    tok.nextToken() ;
		    double rev = tok.nval ;
		    tok.nextToken() ;
		    String status = tok.sval ;
		    contents_.addElement(new P4File(name,
						    path_,
						    (new Double(rev)).intValue(),
						    level_+1,
						    status)) ;
		}
	    }
	    scanned_ = true ;	

	}
	catch (IOException e) {
	    System.err.println("IOEx:" + e) ;	
	} 
	catch (Throwable e) {
	    System.err.println(e.getMessage()) ;
	} ;
	if(cp != null)
	    cp.setCursor(new Cursor(Cursor.DEFAULT_CURSOR)) ;      
    }


    /**
     * Add folder entry to vector
     */
    void addToVector(Vector v) {
	v.addElement(this) ;
	if(open_) {
	    Enumeration enum = contents_.elements() ;	
	    while(enum.hasMoreElements()) {
		Object o = enum.nextElement() ;
		if(o instanceof P4Folder) {
		    ((P4Folder) o).addToVector(v) ;
		}
		else {
		    if(o instanceof P4File) {
			P4File f = ((P4File) o) ;
			if(P4DirTree.showDeleted ||
			   !f.status_.equals("de"))
			    v.addElement(o) ;
		    }
		    else {
			v.addElement(o) ;
		    }
		}
	    } ;
	} ;
    }

    /**
     * Collapse/expand all entries
     */
    void setOpen(boolean open) {
	//	System.err.println("setOpen("+open+"): "+name_) ; // DEBUG
	open_ = open ;
	Enumeration enum = contents_.elements() ;	
	while(enum.hasMoreElements()) {
	    Object o = enum.nextElement() ;
	    if(o instanceof P4Folder) {
		((P4Folder) o).setOpen(open) ;
	    }
	} ;
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
	//final Color FLD_COLOR   = Color.yellow ; 
				// Folder dimension
	//final Dimension FLD_DIM = new Dimension((height*2)/3,height/2) ; 
			// Folder start point (upper left of folder "main"
			// rectangle
	if(level_ > 0) {
	    final Color FLD_COLOR   = Color.yellow.brighter() ; 
				// Folder dimension
	    final Dimension FLD_DIM = new Dimension((height*2)/3,height/2) ; 
			// Folder start point (upper left of folder "main"
			// rectangle
				    // ** Draw a "folder" **
	    g.setColor(FLD_COLOR) ;	// Fill folder "main" rectangle
	    g.fillRect(p.x,p.y-FLD_DIM.height,
		       FLD_DIM.width,FLD_DIM.height) ; 
		    
	    g.setColor(Color.black) ; // Draw folder "main" outline
	    g.drawRect(p.x,p.y-FLD_DIM.height,
		       FLD_DIM.width,FLD_DIM.height) ;
	    
	    g.setColor(FLD_COLOR) ;	// Fill folder tab
	    g.fillRect(p.x+(FLD_DIM.width/2), p.y-2-FLD_DIM.height,
		       FLD_DIM.width/2,2) ;
	    g.setColor(Color.black) ; // Draw folder tab outline
	    g.drawRect(p.x+(FLD_DIM.width/2), p.y-2-FLD_DIM.height,
		       FLD_DIM.width/2,2) ;
					 // ** print string
	    borderX_ = FLD_DIM.width + 3 ;
	    g.drawString(name_,p.x+borderX_+1,p.y) ;
	}
	else {
					    // Draw "barrel"
	    final Color BARREL_COLOR = Color.gray.brighter() ;
	    final Dimension FLD_DIM = new Dimension(height,height/2) ; 
	    final int yo = 2 ;
	    g.setColor(BARREL_COLOR) ;	
	    g.fillRect(p.x,p.y-FLD_DIM.height-yo,
		       FLD_DIM.width,FLD_DIM.height) ; 
	    
	    g.setColor(Color.black) ; // Draw folder tab outline
	    g.drawRect(p.x,p.y-FLD_DIM.height-yo,
		       FLD_DIM.width,FLD_DIM.height) ; 
	    g.setColor(BARREL_COLOR) ;	
	    g.fillOval(p.x,p.y-((FLD_DIM.height*3)/2)-yo,
		       FLD_DIM.width,(FLD_DIM.height*2)/3) ;
	    g.setColor(BARREL_COLOR) ;	
	    g.fillOval(p.x,p.y-1-yo,
		       FLD_DIM.width,(FLD_DIM.height*2)/3) ;
	    g.setColor(Color.black) ; // Draw folder tab outline
	    g.drawOval(p.x,p.y-((FLD_DIM.height*3)/2)-yo,
		       FLD_DIM.width,(FLD_DIM.height*2)/3) ;
	    g.drawArc(p.x,p.y-1-yo,
		      FLD_DIM.width,(FLD_DIM.height*2)/3,180,180) ;
  
	    borderX_ = FLD_DIM.width + 3 ;
	    g.setColor(Color.blue.brighter()) ; 
	    g.drawString(helpText,p.x+borderX_+1,p.y) ;

	}
	return borderX_ + 1 + g.getFontMetrics().stringWidth(name_) ;
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
	if(rpt.x < borderX_) {
				// click on folder
	    open_ = !open_ ;
	    if(open_ && !scanned_) {
		scan(cp) ;
		helpText = "" ;	    
	    }	    
	}
	else {
				// click on name
	    PopupMenu popup = new PopupMenu(path_) ;
	    final String VIEW_CHANGES = "View changes below this point" ;
	    final String BROWSE = "Browse this directory" ;
	    popup.add(VIEW_CHANGES) ;
	    popup.add(BROWSE) ;
	    popup.addActionListener(new ActionListener() {
		public void actionPerformed(ActionEvent e) {
		    AppletContext ac = P4DirTree.applet.getAppletContext() ;
		    try {
			if(e.getActionCommand() == VIEW_CHANGES) {
			    ac.showDocument(new 
					    URL(P4DirTree.applet.getDocumentBase(),
						"changeList.cgi?FSPC="+urlPath_+"/...&MAXCH=100")) ;
			}
			else if (e.getActionCommand() == BROWSE){
			    ac.showDocument(new 
					    URL(P4DirTree.applet.getDocumentBase(),
						"depotTreeBrowser.cgi?FSPC="+urlPath_)) ;
			}
		    }
		    catch (Throwable t) {
			System.err.println("Exception: "+t.getMessage()) ;
		    }
		}
	    }) ;
	    cp.add(popup) ;
	    popup.show(cp,pt.x,pt.y) ;	    
	}
    } ;

} ;

