#ifndef GENESIS_UTILS_MATH_HISTOGRAM_DISTANCE_H_
#define GENESIS_UTILS_MATH_HISTOGRAM_DISTANCE_H_

/**
 * @brief Header of Histogram distance functions.
 *
 * @file
 * @ingroup utils
 */

#include "utils/math/histogram/histogram.hpp"

namespace genesis {

// =================================================================================================
//     Histogram Distances
// =================================================================================================

double earth_movers_distance (const Histogram& h1, const Histogram& h2);

} // namespace genesis

#endif // include guard
