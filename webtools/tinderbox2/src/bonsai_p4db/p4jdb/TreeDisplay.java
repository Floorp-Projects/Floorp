package p4jdb ;
import java.awt.* ;
import java.awt.event.* ;
import java.util.* ;

/**
 * Yet another Java object that displays a directory tree.
 * The main differences, apart from that I wrote it myself (NIH ;^)  )
 * is that it is tested with large trees (>20k entries) and does not
 * bother with images and other fancy stuff. The need for speed.....
 * <br>
 * The class defines two interfaces, <tt>TreeDisplay::Data</tt> and
 * <tt>TreeDisplay::DataManager</tt>, that connects the display to the
 * data. The constructor takes a <tt>TreeDisplay::DataManager</tt>
 * object that provides an interface to <tt>TreeDisplay::Data</tt>
 * objects. <br>
 *   <i>/Fredric</i><br>
 *  PS. It is also my first java ever so be gentle.... 
*/
public class TreeDisplay extends Panel implements AdjustmentListener {

    /**
     * Represents a data item displayed in the tree (typically a file)
     * <br>
     * The data for the object is displayed on a line
     */
    public interface Data {

	/**
	 * Draw a line representing a data object.
	 *@param height Line height
	 *@param p      Start point (lower right)
	 *@param g      Graphics object to use
	 *@return width of object drawn in pixels (or 0 if no object). The
	 *        width is used to determine clickable area for the object.
	 */
	public int draw(int height, Point p, Graphics g) ;
	
	/**
	 * Get level in tree
	 *@return level in tree (number of tics to indent)
	 */
	public int level() ; 

	/**
	 * This member is called when the mouse has been clicked on the 
	 * line.
	 *@param cp  Component where object is drawn
	 *@param pt  Point within component where mouse is clicked
	 *@param rpt Point relative to position given in draw() metod
	 */
	public void mouseClicked(Component cp, Point pt, Point rpt) ;
    } ;

    /**
     * Interface for an object that manages the data items (typically a file
     * structure)
     */
    public interface DataManager {
	
	/**
	 * Used by <tt>TreeDisplay</tt> to get the total number of
	 * lines to display.
	 *@return Number of lines (items) to display
	 */
	public int getLines() ;
	
	/**
	 * Get Data object by offset
	 *@param offset Offset of object to return
	 *@return Item at specified offset
	 *@throws IndexOutOfBoundsException if offset out of bounds
	 */
	public Data getItemAt(int offset) 
	    throws java.lang.IndexOutOfBoundsException ;

	/**
	 * Get line height. Current implementation assumes same height
	 * for all lines.
	 *@return line height in pixels
	 */
	public int getLineHeight() ;

	/**
	 * Get indent value
	 *@return number of pixels to indent for each level in tree
	 */
	public int indentValue() ;

	/**
	 * Get background color for panel
	 *@return background color
	 */
	public Color getBackground() ;

	/**
	 * Called before repaint. The idea is to give the <tt>DataManager</tt>
	 * object a chance to prepare itself before data is painted.
	 */
	public void prePaint() ;
	
    } ;
  
    private DataManager theDataManager_ ;

    
    /**
     * Contains data for a line
     */
    private class LineData {
	/**
	 * Min and max x/y positions
	 */
	public int minY,maxY,minX,maxX ;
	
	/**
	 * Data object
	 */
	public Data data ;
    } ;
    
    private Vector lineDataV_ = new Vector() ;
    
    /**
     * Internal class that represents a panel 
     */
    private class MyPanel extends Panel {
	public Image offScreenImage ;
	public void paint(Graphics graphics) {
	    if(offScreenImage == null) {
		TreeDisplay.this.paint(TreeDisplay.this.getGraphics()) ;
	    }
	    graphics.drawImage(offScreenImage,0,0,this) ;      
	}
    } ;  
    
    /**
     * Graphic objects
     */
    //private Scrollbar hadj_ ;
    private Scrollbar vadj_ ;
    private MyPanel   panel_ ;

    /**
     * No. of top line in window
     */
    private int firstLineNo_ ;
    
    /**
     * Constructor
     *@param dataManager DataManager object
     */
    public TreeDisplay(DataManager dataManager)
    {
	theDataManager_ = dataManager ;
	firstLineNo_ = 0 ;
	
				// Create graphic objects
	panel_ = new MyPanel() ;
	//hadj_ = new Scrollbar(Scrollbar.HORIZONTAL) ;
	vadj_ = new Scrollbar(Scrollbar.VERTICAL) ;
	
				// Re-direct mouseclicks to parent class
	panel_.addMouseListener(new MouseAdapter()
				{
				    public void mouseClicked(MouseEvent e) {
					TreeDisplay.this.mouseClicked(e) ;
				    }
				}) ;  
				// Make object listen to scrollbars
	//hadj_.addAdjustmentListener(this) ;
	vadj_.addAdjustmentListener(this) ;
	
				// Define a layout
	GridBagLayout gridbag = new GridBagLayout() ;
	setLayout(gridbag) ;
	GridBagConstraints c = new GridBagConstraints();

				// Define layout for panel and add object
	c.fill = GridBagConstraints.BOTH ;
	c.weightx = 1;
	c.weighty = 1;
	c.gridwidth = GridBagConstraints.RELATIVE ;
	c.gridheight = GridBagConstraints.RELATIVE ;    
	gridbag.setConstraints(panel_,c) ;
	add(panel_) ;
	
				// Define layout for vertical scrollbar and
				// add object
	c.weightx = 0;
	c.weighty = 0;
	c.gridwidth = GridBagConstraints.REMAINDER ;
	c.gridheight = GridBagConstraints.RELATIVE ;    
	gridbag.setConstraints(vadj_,c) ;
	add(vadj_) ;
	
				// Define layout for horizontal scrollbar and
				// add object
	//c.gridwidth = GridBagConstraints.RELATIVE ;
	//c.gridheight = GridBagConstraints.REMAINDER ;    
	//gridbag.setConstraints(hadj_,c) ;
	//add(hadj_) ; 
	
	panel_.setBackground(theDataManager_.getBackground()) ;
	
    } ;
    
    /**
     * <B>NOTE!</B> Should really be private, bug in java<br>
     * handle mouse click
     */
    public void mouseClicked(MouseEvent event) {
	int y = event.getPoint().y ;
	Enumeration e = lineDataV_.elements() ;
	LineData ld;
	while(e.hasMoreElements()) {
	    ld = ((LineData) e.nextElement()) ;
	    if((y >= ld.minY) && (y <= ld.maxY)) {
		int x = event.getPoint().x ;
		if((x >= ld.minX) && (x <= ld.maxX)) {
		    ld.data.mouseClicked(panel_,
					 new Point(x,y),
					 new Point(x-ld.minX,y-ld.minY)) ;
		}
		else { 
		    break ; 
		} ;
		theDataManager_.prePaint() ;
		paint(getGraphics()) ;
		break ;
	    }
	}
    } ;
    
    /**
     * Change manager object
     */
    public void setManager(DataManager dataManager)
    {
	theDataManager_ = dataManager ;
	paint(getGraphics()) ;
    }


    /**
     * Called when Scrollbar object modified by user
     */
    public void adjustmentValueChanged(AdjustmentEvent e)
    {
	firstLineNo_ = vadj_.getValue() ;
	paint(getGraphics()) ;
    } ;
    
    
    /**
     * re-paint window
     */
    public void paint(Graphics graphics)     
    {
					    // Create a temp graphic object
	final Dimension dim = panel_.getSize() ;
	panel_.offScreenImage = createImage(dim.width,dim.height) ;
	Graphics tmpGraphics = panel_.offScreenImage.getGraphics() ;

					    // Clear object
	tmpGraphics.setColor(theDataManager_.getBackground()); 
	tmpGraphics.fillRect(0,0,dim.width,dim.height) ;
	
					    // Compute lines displayed
	final int visibleLines = dim.height/theDataManager_.getLineHeight() ;
					    // Adjust first line if neccesary
	final int totLines = theDataManager_.getLines() ;
	if(firstLineNo_ > totLines-visibleLines) {
	    firstLineNo_ = totLines-visibleLines ; 
	    if(firstLineNo_ < 0) firstLineNo_ = 0 ;
	}
					    // Adjust vertical scrollbar values
	vadj_.setMinimum(0) ;
	vadj_.setBlockIncrement(visibleLines) ;
	vadj_.setVisibleAmount(visibleLines) ;
	if((totLines-visibleLines)+1 > 0) {
  	    vadj_.setMaximum(totLines) ;
	}
	else
	    vadj_.setMaximum(1) ;
	vadj_.setValue(firstLineNo_) ;
					    // Clear displayed-line to data vector
	lineDataV_.removeAllElements() ;	       

					    // Draw lines
	try {
	    int line = 0 ;
	    Vector pointByLevel = new Vector() ;
	    int ypos = theDataManager_.getLineHeight() ;
	    int lastLevel = 0 ;
	    for(line = 0;
		(line <= visibleLines) && (line+firstLineNo_ < totLines);
		line++) {
		LineData ld = new LineData() ;
		ld.data = theDataManager_.getItemAt(line+firstLineNo_) ;
		ld.minY = ypos - theDataManager_.getLineHeight() ;
		ld.maxY = ypos ;
		final int lineBot = ypos - (theDataManager_.getLineHeight()*2)/3  ;
		tmpGraphics.setColor(new Color(0x40,0x40,0xff)) ;
		//		int i ;
		int xp=0 ;
		if(ld.data.level() > 0) {
		    if(pointByLevel.size() < ld.data.level()) {
			pointByLevel.setSize(ld.data.level()+1) ;
			for(int i=0;i< ld.data.level();i++) {
			    final int x = ((i*theDataManager_.indentValue()) +
					   theDataManager_.indentValue()/2) ;
				
			    pointByLevel.setElementAt(new Point(x,0),i) ;
			}
		    }

		    Point p = ((Point) pointByLevel.elementAt(ld.data.level()-1)) ;
		    tmpGraphics.drawLine(p.x,p.y,
					 p.x,lineBot) ;
		    p.y = lineBot ;	
		    tmpGraphics.drawLine(p.x,p.y,
					 p.x+theDataManager_.indentValue()/2,
					 p.y+(theDataManager_.getLineHeight()/4)) ;
		}
		
		Point startp = new Point(3+(ld.data.level())*
					 theDataManager_.indentValue(),
					 ypos) ;
		if(pointByLevel.size() < ld.data.level()+1) 
		    pointByLevel.setSize(ld.data.level()+1) ;
		
		pointByLevel.setElementAt(new 
					  Point(startp.x - 3 +
						(theDataManager_.indentValue()/2),
						startp.y),
					  ld.data.level()) ;
		ld.minX = startp.x ;
		
		ld.maxX = (startp.x +
			   ld.data.draw(theDataManager_.getLineHeight(),
					startp,
					tmpGraphics)) ;
		lineDataV_.addElement(ld) ;
		ypos += theDataManager_.getLineHeight() ;
		lastLevel = ld.data.level() ;
	    }
	    line += firstLineNo_ ;
	    for(;lastLevel > 1 && line<theDataManager_.getLines();line++) {
		Data data = theDataManager_.getItemAt(line) ;
		if(data.level() < lastLevel) {
		    lastLevel = data.level() ;
		}
		if(lastLevel > 0 && data.level() == lastLevel) {
		    Point p = ((Point) pointByLevel.elementAt(lastLevel-1)) ;
		    tmpGraphics.setColor(new Color(0x40,0x40,0xff)) ;
		    tmpGraphics.drawLine(p.x,p.y,
					 p.x,dim.height) ;
		}
	    }
	    
	}
	catch(Throwable e) {
	    System.err.println("paint() throws:" + e) ;
	    e.printStackTrace() ;
	    tmpGraphics.setColor(Color.red) ;
	    tmpGraphics.drawString(e.getMessage(),10,10) ;
	}    
	panel_.paint(panel_.getGraphics()) ;
    } 
    
} ;
