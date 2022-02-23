#pragma once

#include "../types.h"

namespace intgemm {
namespace callbacks {

struct OutputBufferInfo {
  Index row_idx;
  Index col_idx;

  Index rows; // = A_rows
  Index cols; // = B_cols

  OutputBufferInfo(Index row_idx, Index col_idx, Index rows, Index cols)
    : row_idx(row_idx), col_idx(col_idx), rows(rows), cols(cols) {}
};

}
}
