
//----------------------------------------------------------------------------
// XYQ: 2006-01-22 Copied from AGG project.
// TODO: This file uses intensive floating point operations, so it's NOT suitable
// for platforms like Symbian OS. We need to change to FIX format.
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.3
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------
//
// Class path_storage
//
//----------------------------------------------------------------------------

#include "agg_math.h"
#include "agg_path_storage.h"
#include "core/fxcrt/fx_basic.h"

namespace agg
{
path_storage::~path_storage()
{
    if(m_total_blocks) {
        FX_FLOAT** coord_blk = m_coord_blocks + m_total_blocks - 1;
        while(m_total_blocks--) {
            FX_Free(*coord_blk);
            --coord_blk;
        }
        FX_Free(m_coord_blocks);
    }
}
path_storage::path_storage() :
    m_total_vertices(0),
    m_total_blocks(0),
    m_max_blocks(0),
    m_coord_blocks(0),
    m_cmd_blocks(0),
    m_iterator(0)
{
}
void path_storage::allocate_block(unsigned nb)
{
    if(nb >= m_max_blocks) {
        FX_FLOAT** new_coords =
            FX_Alloc2D(FX_FLOAT*, m_max_blocks + block_pool, 2);
        unsigned char** new_cmds =
            (unsigned char**)(new_coords + m_max_blocks + block_pool);
        if(m_coord_blocks) {
            FXSYS_memcpy(new_coords,
                           m_coord_blocks,
                           m_max_blocks * sizeof(FX_FLOAT*));
            FXSYS_memcpy(new_cmds,
                           m_cmd_blocks,
                           m_max_blocks * sizeof(unsigned char*));
            FX_Free(m_coord_blocks);
        }
        m_coord_blocks = new_coords;
        m_cmd_blocks = new_cmds;
        m_max_blocks += block_pool;
    }
    m_coord_blocks[nb] =
        FX_Alloc( FX_FLOAT, block_size * 2 +
                  block_size /
                  (sizeof(FX_FLOAT) / sizeof(unsigned char)));
    m_cmd_blocks[nb]  =
        (unsigned char*)(m_coord_blocks[nb] + block_size * 2);
    m_total_blocks++;
}
void path_storage::rewind(unsigned path_id)
{
    m_iterator = path_id;
}
void path_storage::curve4(FX_FLOAT x_ctrl1, FX_FLOAT y_ctrl1,
                          FX_FLOAT x_ctrl2, FX_FLOAT y_ctrl2,
                          FX_FLOAT x_to,    FX_FLOAT y_to)
{
    add_vertex(x_ctrl1, y_ctrl1, path_cmd_curve4);
    add_vertex(x_ctrl2, y_ctrl2, path_cmd_curve4);
    add_vertex(x_to,    y_to,    path_cmd_curve4);
}
void path_storage::end_poly()
{
    if(m_total_vertices) {
        if(is_vertex(command(m_total_vertices - 1))) {
            add_vertex(0, 0, path_cmd_end_poly | path_flags_close);
        }
    }
}
}
