#ifndef GENESIS_TREE_MASS_TREE_FUNCTIONS_H_
#define GENESIS_TREE_MASS_TREE_FUNCTIONS_H_

/*
    Genesis - A toolkit for working with phylogenetic data.
    Copyright (C) 2014-2017 Lucas Czech

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
    Lucas Czech <lucas.czech@h-its.org>
    Exelixis Lab, Heidelberg Institute for Theoretical Studies
    Schloss-Wolfsbrunnenweg 35, D-69118 Heidelberg, Germany
*/

/**
 * @brief
 *
 * @file
 * @ingroup tree
 */

namespace genesis {
namespace tree {

// =================================================================================================
//     Forward Declarations
// =================================================================================================

class Tree;
class TreeNode;
class TreeEdge;

using MassTree = Tree;

// =================================================================================================
//     Earth Movers Distance
// =================================================================================================

/**
 * @brief Calculate the earth mover's distance of two distributions of masses on a given tree.
 *
 * The earth mover's distance is typically a distance measure between two distributions.
 * See https://en.wikipedia.org/wiki/Earth_mover's_distance for an introduction.
 *
 * In our case, we use distibutions of masses along the branches of a tree. Each branch can have
 * multiple masses at different positions within [0.0, branch_length].
 *
 * The distance is calculated as the amount of work needed to move the masses of one distribution
 * so that they end up in the positions of the masses of the other distribution.
 * Work is here defined as mass times dislocation. Thus, the work ( = total distance ) is higher
 * if either more mass has to be moved, or mass has to be moved further.
 *
 * The resulting distance is independed of the rooting of the tree and commutative with respect
 * to the two mass distributions.
 *
 * In order to keep the calculations simple, we use the following convention for the two
 * distributions: The masses of one distribution are stored using a positive sign, the masses of the
 * other distribution use a negative sign. This way, only one storage for the masses can be used
 * and the algorithm is simplyfied.
 *
 * The earth mover's distance is only meaningful if both mass distributions contain the same amount
 * of total mass. Thus, as they use opposite signs, the sum of all masses on the tree should ideally
 * be zero (apart from numerical derivations).
 * See @link mass_tree_sum_of_masses( MassTree const& tree ) mass_tree_sum_of_masses() @endlink and
 * @link mass_tree_validate( MassTree const& tree, double valid_total_mass_difference )
 * mass_tree_validate() @endlink for functions to verify this.
 *
 * See @link placement::earth_movers_distance( const Sample& lhs, const Sample& rhs, bool with_pendant_length )
 * earth_movers_distance( Sample const&, ... ) @endlink for an exemplary
 * usage of this function, which applies the earth mover's distance to the placement weights
 * (@link placement::PqueryPlacement::like_weight_ratio like_weight_ratio@endlink) of a
 * PlacementTree.
 */
double earth_movers_distance( MassTree const& tree );

// =================================================================================================
//     Manipulate Masses
// =================================================================================================

MassTree mass_tree_merge_trees( MassTree const& lhs, MassTree const& rhs );

void mass_tree_merge_trees_inplace( MassTree& lhs, MassTree const& rhs );

/**
 * @brief Clear all masses of an MassTree, while keeping its topology.
 */
void mass_tree_clear_masses( MassTree& tree );

/**
 * @brief Reverse the sign of each mass point on an MassTree.
 */
void mass_tree_reverse_signs( MassTree& tree );

/**
 * @brief Set all branch lengths of the Tree to 1.0, while keeping the relative position of all
 * masses on the branches.
 */
void mass_tree_transform_to_unit_branch_lengths( MassTree& tree );

/**
 * @brief Accumulate all masses of the Tree on the centers of their edges. Return the work
 * (mass times distance) that was needed to move the masses to the centers.
 */
double mass_tree_center_masses_on_branches( MassTree& tree );

// =================================================================================================
//     Others
// =================================================================================================

/**
 * @brief Return the total sum of all masses on the Tree.
 *
 * In order for the earth_movers_distance() algorithm to work properly (and give meaningful
 * results), the total mass on the tree should ideally be 0.0. This function can be used to check
 * this.
 *
 * Because of numerical issues however, be aware that the result might be slighly off zero. This
 * is okay, as it usually is in the last digits of the double.
 */
double mass_tree_sum_of_masses( MassTree const& tree );

/**
 * @brief Validate the data on an Tree.
 *
 * This function returns true iff the data on the Tree is valid:
 *
 *  *  The node and edge data types have to be @link MassTreeNodeData MassTreeNodeData@endlink
 *     and @link MassTreeEdgeData MassTreeEdgeData@endlink, respectively.
 *  *  The positions of the masses are in [0.0, branch_length] on their respective branches.
 *  *  The sum of all masses is close to 0.0, using the optional arument
 *     `valid_total_mass_difference` as a measure of closeness.
 *
 * The function stops at the first encountered invalid condition and outputs a description message
 * of the invalid value to LOG_INFO.
 *
 * @param tree                        Tree to be validated.
 * @param valid_total_mass_difference (= 0.00001 by default) allowed difference from zero for the
 *                                    total sum of all masses on the tree.
 */
bool mass_tree_validate( MassTree const& tree, double valid_total_mass_difference = 0.00001 );

} // namespace tree
} // namespace genesis

#endif // include guard