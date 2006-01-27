package p4jdb ;
import p4jdb.* ;
import java.applet.* ;
import java.net.* ;
import java.lang.System ;
import java.io.* ;
import java.lang.Throwable ;
import java.awt.* ;
import java.awt.event.* ;
import java.util.* ;




/**
 * Displays a p4 depot graphically
 * 
 */
public class P4DirTree 
{
    /**
     * Frame used to display data.
     */
    public Frame frame_ ;
    
    /**
     * Parent Applet
     */
    static public Applet applet ;

    /**
     * True if deleted files should be viewed
     */
    static public boolean showDeleted = true ;
    
    /**
     * True if files should be viewed
     */
    static public boolean showFiles = true ;

    /**
     * DataManager object required by TreeDisplay class
     */
    private class Manager implements TreeDisplay.DataManager {
	
	private Vector visible_ = new Vector();
	private int lineHeight_ ;

	/**
	 * Base object for tree (gotta be a folder)
	 */
	public P4Folder base_ ;

	/**
	 * Constructor
	 *@param height Line height
	 */
	public Manager(int height) {
	    lineHeight_ = height ;
	    base_ = new P4Folder("",
				 "",
				 0) ;
	}

	/**
	 *@return Number of lines (items) to display
	 */
	public int getLines() { 
	    if( visible_.size() == 0) {
		updateVisible() ;
	    }
	    return visible_.size() ; 
	} ;
	
	/**
	 * Get Data object by offset
	 *@param offset Offset of object to return
	 *@return Item at specified offset
	 *@throws IndexOutOfBoundsException if offset out of bounds
	 */
	public TreeDisplay.Data getItemAt(int offset) 
	    throws java.lang.IndexOutOfBoundsException {
	    if( visible_.size() == 0) {
		updateVisible() ;
	    }	    
	    return ((TreeDisplay.Data) visible_.elementAt(offset)) ;
	}

	/**
	 * Get line height
	 *@return line height in pixels
	 */
	public int getLineHeight() {
	    return lineHeight_ ;
	} ;

	/**
	 * Get indent value
	 *@return number of pixels to indent for each level
	 */
	public int indentValue() {
	    return lineHeight_ ;
	}

	/**
	 * Get background color
	 *@return background color
	 */
	public Color getBackground() { return Color.white ; } ;

	public void prePaint() 
	{
	    markVisibleModified() ;
	} ;

	
	/**
	 * Update visible vector
	 */
	private void updateVisible() {
	    if( visible_.size() > 0) {
		visible_.removeAllElements() ;
	    } ;
	    base_.addToVector(visible_) ;
	} ;

	/**
	 * Mark visible vector for update
	 */
	public void markVisibleModified() {
	    visible_.removeAllElements() ;
	} ;

	/**
	 * Refresh base from stream
	 */
	public void refresh() {
	    base_ = null ;
	    base_ = new P4Folder("",
				 "/",
				 0) ;
	    markVisibleModified() ;
	} ;

    } ;    

    Manager theManager_ ;

    public TreeDisplay theTreeDisplay ;

    
    /**
     * Constructor
     *@Param app Parent applet object
     */
    public P4DirTree(Applet app) {
	applet = app ;
	initData() ;
    } ;

    /**
     * Initialize data (from url)
     */
    public void initData() {
	theManager_ = new Manager(15) ;
    }
    
    /**
     * Start to display (if initiated)
     */
    public void start() {
				// If frame already created, move it to top
	if(frame_ != null) {
	    frame_.setVisible(true) ;
	    return ;
	}
					    // Create frame
	try {
	    frame_ = new Frame("P4DB Depot Directory Browser") ;
	    frame_.setSize(300,400) ;
	    frame_.addWindowListener(new  WindowAdapter() {
		public void windowClosing(WindowEvent e) {
		    frame_.dispose() ;
		    frame_ = null ;
		} ;
	    }) ;			      
	    
	    
	    theTreeDisplay = new TreeDisplay(theManager_) ;
					    // Build "File" menu
	    Menu file = new Menu("File") ;
	    final String EXIT=("Exit") ;
	    file.add(EXIT) ;
	    file.addActionListener(new ActionListener() {	
		public void actionPerformed(ActionEvent ae) {
		    if(ae.getActionCommand() == EXIT) {
			frame_.dispose() ;
			frame_ = null ;
		    }		    
		}
	    }) ;
	    
	    
	    Menu view = new Menu("View") ;
	    final String COLLAPSE="Collapse all" ;
	    view.add(COLLAPSE) ;
	    final String SHOWDEL="Show deleted" ;	    
	    final String HIDEDEL="Hide deleted" ;
	    final MenuItem showHidedel = new MenuItem(HIDEDEL) ;
	    view.add(showHidedel) ;
	    final String REFRESH = "Refresh data from depot" ;
	    view.add(REFRESH) ;
	    view.addActionListener(new ActionListener() {	
		public void actionPerformed(ActionEvent ae) {
		    //		    System.err.println(ae) ;
		    if(ae.getActionCommand() == COLLAPSE) {
			frame_.setCursor(new Cursor(Cursor.WAIT_CURSOR));
			theManager_.base_.setOpen(false) ;
			theManager_.updateVisible() ;
			theTreeDisplay.paint(theTreeDisplay.getGraphics()) ;
			frame_.setCursor(new Cursor(Cursor.DEFAULT_CURSOR));
		    }
		    else if(ae.getActionCommand() == SHOWDEL) {
			frame_.setCursor(new Cursor(Cursor.WAIT_CURSOR));
			showDeleted = true ;
			showHidedel.setLabel(HIDEDEL);
			theManager_.updateVisible() ;
			theTreeDisplay.paint(theTreeDisplay.getGraphics()) ;
			frame_.setCursor(new Cursor(Cursor.DEFAULT_CURSOR));
		    }
		    else if(ae.getActionCommand() == HIDEDEL) {
			frame_.setCursor(new Cursor(Cursor.WAIT_CURSOR));
			showDeleted = false ;
			showHidedel.setLabel(SHOWDEL);
			theManager_.updateVisible() ;
			theTreeDisplay.paint(theTreeDisplay.getGraphics()) ;
			frame_.setCursor(new Cursor(Cursor.DEFAULT_CURSOR));
		    }
		    else if(ae.getActionCommand() == REFRESH) {
			frame_.setCursor(new Cursor(Cursor.WAIT_CURSOR));
			initData() ;
			theTreeDisplay.setManager(theManager_) ;
			frame_.setCursor(new Cursor(Cursor.DEFAULT_CURSOR));
		    }		    
		}
	    }) ;
	    
	    MenuBar mbar = new MenuBar() ;
	    mbar.add(file) ;
	    mbar.add(view) ;
	    
	    frame_.setMenuBar(mbar) ;
	    frame_.add(theTreeDisplay,"Center") ;
	    frame_.setVisible(true) ;
	    frame_.paint(frame_.getGraphics()) ;
	}
	catch (IllegalArgumentException e) {
	    System.err.println("Ilegal argument: "+e.getMessage()) ;
	} ;
    }  
}

