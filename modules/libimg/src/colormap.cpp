/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* -*- Mode: C; tab-width: 4 -*-
 *  colormap.c
 *             
 *   $Id: colormap.cpp,v 3.5 2001/06/19 08:46:56 pavlov%netscape.com Exp $
 */


#include "if.h"


/* Force il_set_color_palette() to load a new colormap for an image */
PRBool
il_reset_palette(il_container *ic)
{
    PRBool ret = PR_TRUE;
    NI_ColorMap *cmap;

    if(ic->src_header){
        if(ic->src_header->color_space){
            cmap = &ic->src_header->color_space->cmap;
            if(cmap->num_colors > 0){
                cmap->num_colors=0;
            }
        }else{
            ret = PR_FALSE;
        }
    }else{
        ret = PR_FALSE;
    }

    ic->colormap_serial_num = -1;
    ic->dont_use_custom_palette = PR_FALSE;
    ic->rendered_with_custom_palette = PR_FALSE;
    
    return ret;
   
}

