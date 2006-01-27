package p4jdb ;
import p4jdb.TreeDisplay ;
import p4jdb.P4DirTree ;
import java.applet.*;
import java.awt.* ;
import java.awt.event.* ;

/**
 * Display p4 depot
 */
public class P4DirTreeApplet extends Applet 
{
    P4DirTree theP4DirTree_ ;

    /**
     * init applet
     */
    public void init() {
	
	theP4DirTree_ = new P4DirTree(this) ;

	setBackground(Color.white) ;
	
	theP4DirTree_.initData() ;
	
	final String buttonText = "Graphic Browser" ;
	final String fontName = "SansSerif" ;
	final Dimension windim = getSize() ;
	int fontSize ;
	{
	    FontMetrics fm = getFontMetrics(new Font(fontName,Font.BOLD,14)) ;
	    final int len = fm.stringWidth(buttonText) ;
	    final int height = fm.getHeight() ;
	    final int fs1 = (new Double((windim.width*13/len))).intValue() ;
	    final int fs2 = (new Double((windim.height*13/height))).intValue() ;
	    fontSize = fs1 ;
	    if(fs2 < fs1) {
		fontSize = fs2 ;	
	    }
	}
	Button butt = new Button(buttonText) ;
	butt.setBackground(new Color(0xe0,0xe0,0xe0)) ;
	butt.setForeground(Color.blue) ;
	butt.setFont(new Font(fontName,Font.BOLD,fontSize)) ;   
	butt.setSize(windim) ;
	add(butt) ;
	butt.addActionListener(new ActionListener() {
	    public void actionPerformed(ActionEvent e) {
		theP4DirTree_.start() ;
	    } ;
	}) ;        
    }

    public String[][] getParameterInfo() {
	String[][] s = {
	    {"File", "String", "Name of data file"}} ;
	return s ;
    } ;
      
}






