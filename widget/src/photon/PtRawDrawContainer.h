/* PtRawDrawContainer.h - widget header file */

#include <Pt.h>

/* widget resources */
#define RDC_DRAW_FUNC		Pt_RESOURCE( Pt_USER( 0 ), 0 )

/* widget instance structure */
typedef struct raw_draw_container_widget
{
	PtContainerWidget_t	container;
  void (*draw_f)( PtWidget_t *, PhTile_t * );
}	RawDrawContainerWidget;

/* widget class pointer */
extern PtWidgetClassRef_t *PtRawDrawContainer;

