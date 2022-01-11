#ifndef GENESIS_UTILS_CONTAINERS_INTERVAL_TREE_FWD_H_
#define GENESIS_UTILS_CONTAINERS_INTERVAL_TREE_FWD_H_

/*
    Genesis - A toolkit for working with phylogenetic data.
    Copyright (C) 2014-2021 Lucas Czech

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Contact:
    Lucas Czech <lczech@carnegiescience.edu>
    Department of Plant Biology, Carnegie Institution For Science
    260 Panama Street, Stanford, CA 94305, USA
*/

/**
 * @brief
 *
 * @file
 * @ingroup utils
 */

namespace genesis {
namespace utils {

// =================================================================================================
//     Forward Declarations
// =================================================================================================

template <typename DataType, typename NumericalType, typename IntervalKind>
struct Interval;

template <typename DataType, typename NumericalType, typename IntervalKind>
class IntervalTree;

template <typename DataType, typename NumericalType, typename IntervalKind>
class IntervalTreeNode;

template <typename NodeType, bool is_const>
class IntervalTreeIterator;

} // namespace utils
} // namespace genesis

#endif // include guard