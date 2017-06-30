
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
//
// The author gratefully acknowleges the support of David Turner,
// Robert Wilhelm, and Werner Lemberg - the authors of the FreeType
// libray - in producing this work. See http://www.freetype.org for details.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------
//
// Adaptation for 32-bit screen coordinates has been sponsored by
// Liberty Technology Systems, Inc., visit http://lib-sys.com
//
// Liberty Technology Systems, Inc. is the provider of
// PostScript and PDF technology for software developers.
//
//----------------------------------------------------------------------------
#ifndef AGG_RASTERIZER_SCANLINE_AA_INCLUDED
#define AGG_RASTERIZER_SCANLINE_AA_INCLUDED

#include "agg_array.h"
#include "agg_basics.h"
#include "agg_clip_liang_barsky.h"
#include "agg_math.h"
#include "agg_render_scanlines.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_memory.h"

namespace agg
{
enum poly_base_scale_e {
    poly_base_shift = 8,
    poly_base_size  = 1 << poly_base_shift,
    poly_base_mask  = poly_base_size - 1
};
inline int poly_coord(FX_FLOAT c)
{
    return int(c * poly_base_size);
}
struct cell_aa  {
    int x;
    int y;
    int cover;
    int area;
    void set(int x, int y, int c, int a);
    void set_coord(int x, int y);
    void set_cover(int c, int a);
    void add_cover(int c, int a);
};
class outline_aa 
{
    enum cell_block_scale_e {
        cell_block_shift = 12,
        cell_block_size  = 1 << cell_block_shift,
        cell_block_mask  = cell_block_size - 1,
        cell_block_pool  = 256,
        cell_block_limit = 1024
    };
    struct sorted_y  {
        unsigned start;
        unsigned num;
    };
public:
    ~outline_aa();
    outline_aa();
    void reset();
    void move_to(int x, int y);
    void line_to(int x, int y);
    int min_x() const
    {
        return m_min_x;
    }
    int min_y() const
    {
        return m_min_y;
    }
    int max_x() const
    {
        return m_max_x;
    }
    int max_y() const
    {
        return m_max_y;
    }
    void sort_cells();
    unsigned total_cells() const
    {
        return m_num_cells;
    }
    unsigned scanline_num_cells(unsigned y) const
    {
        return m_sorted_y[y - m_min_y].num;
    }
    const cell_aa* const* scanline_cells(unsigned y) const
    {
        return m_sorted_cells.data() + m_sorted_y[y - m_min_y].start;
    }
    bool sorted() const
    {
        return m_sorted;
    }
private:
    outline_aa(const outline_aa&);
    const outline_aa& operator = (const outline_aa&);
    void set_cur_cell(int x, int y);
    void add_cur_cell();
    void render_hline(int ey, int x1, int y1, int x2, int y2);
    void render_line(int x1, int y1, int x2, int y2);
    void allocate_block();
private:
    unsigned  m_num_blocks;
    unsigned  m_max_blocks;
    unsigned  m_cur_block;
    unsigned  m_num_cells;
    cell_aa** m_cells;
    cell_aa*  m_cur_cell_ptr;
    pod_array<cell_aa*> m_sorted_cells;
    pod_array<sorted_y> m_sorted_y;
    cell_aa   m_cur_cell;
    int       m_cur_x;
    int       m_cur_y;
    int       m_min_x;
    int       m_min_y;
    int       m_max_x;
    int       m_max_y;
    bool      m_sorted;
};
class scanline_hit_test 
{
public:
    scanline_hit_test(int x) : m_x(x), m_hit(false) {}
    void reset_spans() {}
    void finalize(int) {}
    void add_cell(int x, int)
    {
        if(m_x == x) {
            m_hit = true;
        }
    }
    void add_span(int x, int len, int)
    {
        if(m_x >= x && m_x < x + len) {
            m_hit = true;
        }
    }
    unsigned num_spans() const
    {
        return 1;
    }
    bool hit() const
    {
        return m_hit;
    }
private:
    int  m_x;
    bool m_hit;
};
enum filling_rule_e {
    fill_non_zero,
    fill_even_odd
};
class rasterizer_scanline_aa 
{
    enum status {
        status_initial,
        status_line_to,
        status_closed
    };
public:
    enum aa_scale_e {
        aa_num   = 1 << 8,
        aa_mask  = aa_num - 1,
        aa_2num  = aa_num * 2,
        aa_2mask = aa_2num - 1
    };
    rasterizer_scanline_aa() :
        m_filling_rule(fill_non_zero),
        m_clipped_start_x(0),
        m_clipped_start_y(0),
        m_status(status_initial),
        m_clipping(false)
    {
    }
    ~rasterizer_scanline_aa() {}
    void filling_rule(filling_rule_e filling_rule)
    {
        m_filling_rule = filling_rule;
    }
    int min_x() const
    {
        return m_outline.min_x();
    }
    int min_y() const
    {
        return m_outline.min_y();
    }
    int max_x() const
    {
        return m_outline.max_x();
    }
    int max_y() const
    {
        return m_outline.max_y();
    }
    void reset()
    {
        m_outline.reset();
        m_status = status_initial;
    }
    void clip_box(FX_FLOAT x1, FX_FLOAT y1, FX_FLOAT x2, FX_FLOAT y2)
    {
        m_clip_box = rect(poly_coord(x1), poly_coord(y1),
                          poly_coord(x2), poly_coord(y2));
        m_clip_box.normalize();
        m_clipping = true;
    }
    void add_vertex(FX_FLOAT x, FX_FLOAT y, unsigned cmd)
    {
        if(is_close(cmd)) {
            close_polygon();
        } else {
            if(is_move_to(cmd)) {
                move_to(poly_coord(x), poly_coord(y));
            } else {
                if(is_vertex(cmd)) {
                    line_to(poly_coord(x), poly_coord(y));
                }
            }
        }
    }
    void move_to(int x, int y)
    {
        if(m_clipping) {
            if(m_outline.sorted()) {
                reset();
            }
            if(m_status == status_line_to) {
                close_polygon();
            }
            m_prev_x = m_start_x = x;
            m_prev_y = m_start_y = y;
            m_status = status_initial;
            m_prev_flags = clipping_flags(x, y, m_clip_box);
            if(m_prev_flags == 0) {
                move_to_no_clip(x, y);
            }
        } else {
            move_to_no_clip(x, y);
        }
    }
    void line_to(int x, int y)
    {
        if(m_clipping) {
            clip_segment(x, y);
        } else {
            line_to_no_clip(x, y);
        }
    }
    void close_polygon()
    {
        if (m_status != status_line_to) {
            return;
        }
        if(m_clipping) {
            clip_segment(m_start_x, m_start_y);
        }
        close_polygon_no_clip();
    }
    AGG_INLINE unsigned calculate_alpha(int area, bool no_smooth) const
    {
        int cover = area >> (poly_base_shift * 2 + 1 - 8);
        if(cover < 0) {
            cover = -cover;
        }
        if(m_filling_rule == fill_even_odd) {
            cover &= aa_2mask;
            if(cover > aa_num) {
                cover = aa_2num - cover;
            }
        }
        if (no_smooth) {
            cover = cover > aa_mask / 2 ? aa_mask : 0;
        }
        if(cover > aa_mask) {
            cover = aa_mask;
        }
        return cover;
    }
    AGG_INLINE void sort()
    {
        m_outline.sort_cells();
    }
    AGG_INLINE bool rewind_scanlines()
    {
        close_polygon();
        m_outline.sort_cells();
        if(m_outline.total_cells() == 0) {
            return false;
        }
        m_cur_y = m_outline.min_y();
        return true;
    }
    AGG_INLINE bool navigate_scanline(int y)
    {
        close_polygon();
        m_outline.sort_cells();
        if(m_outline.total_cells() == 0 ||
                y < m_outline.min_y() ||
                y > m_outline.max_y()) {
            return false;
        }
        m_cur_y = y;
        return true;
    }
    template<class Scanline> bool sweep_scanline(Scanline& sl, bool no_smooth)
    {
        for(;;) {
            if(m_cur_y > m_outline.max_y()) {
                return false;
            }
            sl.reset_spans();
            unsigned num_cells = m_outline.scanline_num_cells(m_cur_y);
            const cell_aa* const* cells = m_outline.scanline_cells(m_cur_y);
            int cover = 0;
            while(num_cells) {
                const cell_aa* cur_cell = *cells;
                int x    = cur_cell->x;
                int area = cur_cell->area;
                unsigned alpha;
                cover += cur_cell->cover;
                while(--num_cells) {
                    cur_cell = *++cells;
                    if(cur_cell->x != x) {
                        break;
                    }
                    area  += cur_cell->area;
                    cover += cur_cell->cover;
                }
                if(area) {
                    alpha = calculate_alpha((cover << (poly_base_shift + 1)) - area, no_smooth);
                    if(alpha) {
                        sl.add_cell(x, alpha);
                    }
                    x++;
                }
                if(num_cells && cur_cell->x > x) {
                    alpha = calculate_alpha(cover << (poly_base_shift + 1), no_smooth);
                    if(alpha) {
                        sl.add_span(x, cur_cell->x - x, alpha);
                    }
                }
            }
            if(sl.num_spans()) {
                break;
            }
            ++m_cur_y;
        }
        sl.finalize(m_cur_y);
        ++m_cur_y;
        return true;
    }
    template<class VertexSource>
    void add_path(VertexSource& vs, unsigned path_id = 0)
    {
        FX_FLOAT x;
        FX_FLOAT y;
        unsigned cmd;
        vs.rewind(path_id);
        while(!is_stop(cmd = vs.vertex(&x, &y))) {
            add_vertex(x, y, cmd);
        }
    }
    template<class VertexSource>
    void add_path_transformed(VertexSource& vs, const CFX_Matrix* pMatrix, unsigned path_id = 0)
    {
        FX_FLOAT x;
        FX_FLOAT y;
        unsigned cmd;
        vs.rewind(path_id);
        while(!is_stop(cmd = vs.vertex(&x, &y))) {
          if (pMatrix) {
            CFX_PointF ret = pMatrix->Transform(CFX_PointF(x, y));
            x = ret.x;
            y = ret.y;
          }
          add_vertex(x, y, cmd);
        }
    }
private:
    rasterizer_scanline_aa(const rasterizer_scanline_aa&);
    const rasterizer_scanline_aa&
    operator = (const rasterizer_scanline_aa&);
    void move_to_no_clip(int x, int y)
    {
        if(m_status == status_line_to) {
            close_polygon_no_clip();
        }
        m_outline.move_to(x * 1, y);
        m_clipped_start_x = x;
        m_clipped_start_y = y;
        m_status = status_line_to;
    }
    void line_to_no_clip(int x, int y)
    {
        if(m_status != status_initial) {
            m_outline.line_to(x * 1, y);
            m_status = status_line_to;
        }
    }
    void close_polygon_no_clip()
    {
        if(m_status == status_line_to) {
            m_outline.line_to(m_clipped_start_x * 1, m_clipped_start_y);
            m_status = status_closed;
        }
    }
    void clip_segment(int x, int y)
    {
        unsigned flags = clipping_flags(x, y, m_clip_box);
        if(m_prev_flags == flags) {
            if(flags == 0) {
                if(m_status == status_initial) {
                    move_to_no_clip(x, y);
                } else {
                    line_to_no_clip(x, y);
                }
            }
        } else {
            int cx[4];
            int cy[4];
            unsigned n = clip_liang_barsky(m_prev_x, m_prev_y,
                                           x, y,
                                           m_clip_box,
                                           cx, cy);
            const int* px = cx;
            const int* py = cy;
            while(n--) {
                if(m_status == status_initial) {
                    move_to_no_clip(*px++, *py++);
                } else {
                    line_to_no_clip(*px++, *py++);
                }
            }
        }
        m_prev_flags = flags;
        m_prev_x = x;
        m_prev_y = y;
    }
private:
    outline_aa     m_outline;
    filling_rule_e m_filling_rule;
    int            m_clipped_start_x;
    int            m_clipped_start_y;
    int            m_start_x;
    int            m_start_y;
    int            m_prev_x;
    int            m_prev_y;
    unsigned       m_prev_flags;
    unsigned       m_status;
    rect           m_clip_box;
    bool           m_clipping;
    int            m_cur_y;
};
}
#endif
