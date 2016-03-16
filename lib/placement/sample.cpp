/**
 * @brief Implementation of Sample class.
 *
 * @file
 * @ingroup placement
 */

#include "placement/sample.hpp"

#include <algorithm>
#include <assert.h>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdio.h>
#include <unordered_map>
#include <utility>

#include "placement/function/functions.hpp"
#include "tree/functions.hpp"
#include "tree/operators.hpp"
#include "utils/core/logging.hpp"
#include "utils/core/std.hpp"

namespace genesis {
namespace placement {

// =================================================================================================
//     Constructor & Destructor
// =================================================================================================

/**
 * @brief Copy constructor.
 */
Sample::Sample( Sample const& other )
{
    clear();

    // use assignment operator to create copy of the tree and metadata.
    *tree_   = *other.tree_;
    metadata = other.metadata;

    // copy all data of the tree: do a preorder traversal on both trees in parallel
    PlacementTree::IteratorPreorder it_n = tree_->begin_preorder();
    PlacementTree::IteratorPreorder it_o = other.tree_->begin_preorder();
    for (
        ;
        it_n != tree_->end_preorder() && it_o != other.tree_->end_preorder();
        ++it_n, ++it_o
    ) {
        // the trees are copies of each other, they need to have the same rank. otherwise,
        // the copy constructor is not working!
        assert(it_n.node()->rank() == it_o.node()->rank());

        // TODO once the edge and node data are copyable and tree supports full data copies, these are not needed any longer.
        it_n.edge()->data.placements.clear();
        it_n.edge()->data.branch_length = it_o.edge()->data.branch_length;
        it_n.edge()->data.edge_num = it_o.edge()->data.edge_num;

        it_n.node()->data.name = it_o.node()->data.name;
    }

    // the trees are copies. they should take equal iterations to finish a traversal.
    assert(it_n == tree_->end_preorder() && it_o == other.tree_->end_preorder());

    // copy all (o)ther pqueries to (n)ew pqueries
    // TODO this is very similar to merge(). make this code shared somehow.
    auto en_map = edge_num_to_edge_map( *tree_ );
    for (const auto& opqry : other.pqueries_) {
        auto npqry = make_unique<Pquery>();

        for (const auto& op : opqry->placements) {
            auto np = make_unique<PqueryPlacement>(*op);

            np->edge   = en_map[np->edge_num];
            np->edge->data.placements.push_back(np.get());
            np->pquery = npqry.get();
            npqry->placements.push_back(std::move(np));
        }
        for (const auto& on : opqry->names) {
            auto nn = make_unique<PqueryName>(*on);

            nn->pquery = npqry.get();
            npqry->names.push_back(std::move(nn));
        }

        pqueries_.push_back(std::move(npqry));
    }
}

/**
 * @brief Move constructor.
 */
// Sample::Sample( Sample&& other ) noexcept
// {
//     swap(other);
// }

Sample::Sample( Sample&& other ) noexcept
    : pqueries_( std::move(other.pqueries_))
    , tree_(     std::move(other.tree_))
    , metadata(  std::move(other.metadata))
{
    other.pqueries_.clear();
    if( other.tree_ ) {
        other.tree_->clear();
    }
    other.metadata.clear();
}

/**
 * @brief Assignment operator. See Copy constructor for details.
 */
Sample& Sample::operator = (const Sample& other)
{
    // check for self-assignment.
    if (&other == this) {
        return *this;
    }

    // the Sample tmp is a copy of the right hand side object (automatically created using the
    // copy constructor). we can thus simply swap the arrays, and upon leaving the function,
    // tmp is automatically destroyed, so that its arrays are cleared and the data freed.
    Sample tmp(other);
    std::swap(pqueries_, tmp.pqueries_);
    tree_->swap(*tmp.tree_);
    std::swap(metadata, tmp.metadata);
    return *this;
}

Sample& Sample::operator= (Sample&& other) noexcept
{
    swap(other);
    return *this;
}

Sample::~Sample()
{
    // We are going to destroy the Sample. Let's speed it up!
    // TODO make swapping work propery. then, this should work, too
    // (currently, it gives a segfault if a moved-from Sample is destroyed).
    // if( tree_ ) {
    //     detach_pqueries_from_tree();
    // }

    // Instead, use a weird half-copied solution for now:

    for (auto const& pqry : pqueries_) {
        for( auto const& place : pqry->placements ) {
            place->edge = nullptr;
        }
    }
}

void Sample::swap (Sample& other) noexcept
{
    using std::swap;
    swap(pqueries_, other.pqueries_);
    swap(tree_,     other.tree_);
    swap(metadata,  other.metadata);
}

// =================================================================================================
//     Modifiers
// =================================================================================================

/**
 * @brief Creats an empty Pquery, adds it to the Sample and returns a pointer to it.
 *
 * The returned pointer can then be used to add Placements and Names to the Pquery.
 */
Pquery* Sample::add_pquery()
{
    pqueries_.push_back(make_unique<Pquery>());
    return pqueries_.back().get();
}

/**
 * @brief Add the pqueries from another Sample object to this one.
 *
 * For this method to succeed, the Sample%s need to have the same topology, including identical
 * edge_nums and node names.
 *
 * The resulting tree is the original one of the Sample on which this method was called. If
 * instead the average branch length tree is needed, see SampleSet::merge_all().
 */
bool Sample::merge(const Sample& other)
{
    // Check for identical topology, taxa names and edge_nums.
    // We do not check here for branch_length, because usually those differ slightly.
    auto node_comparator = [] (
        const PlacementTree::NodeType& node_l,
        const PlacementTree::NodeType& node_r
    ) {
        return node_l.data.name == node_r.data.name;
    };

    auto edge_comparator = [] (
        const PlacementTree::EdgeType& edge_l,
        const PlacementTree::EdgeType& edge_r
    ) {
        return edge_l.data.edge_num == edge_r.data.edge_num;
    };

    if( ! tree::equal<PlacementTree, PlacementTree >(
        *tree_, *other.tree_, node_comparator, edge_comparator
    )) {
        LOG_WARN << "Cannot merge Samples with different reference trees.";
        return false;
    }

    // We need to assign edge pointers to the correct edge objects, so we need a mapping.
    auto en_map = edge_num_to_edge_map( *tree_ );

    // Copy all (o)ther pqueries to (n)ew pqueries.
    for (const auto& opqry : other.pqueries_) {
        auto npqry = make_unique<Pquery>();
        for (const auto& op : opqry->placements) {
            auto np = make_unique<PqueryPlacement>(*op);

            // Assuming that the trees have identical topology (checked at the beginning of this
            // function), there will be an edge for every placement. if this assertion fails,
            // something broke the integrity of our in memory representation of the data.
            assert(en_map.count(np->edge_num) > 0);
            np->edge = en_map[np->edge_num];
            np->edge->data.placements.push_back(np.get());
            np->pquery = npqry.get();
            npqry->placements.push_back(std::move(np));
        }
        for (const auto& on : opqry->names) {
            auto nn = make_unique<PqueryName>(*on);
            nn->pquery = npqry.get();
            npqry->names.push_back(std::move(nn));
        }
        this->pqueries_.push_back(std::move(npqry));
    }

    return true;
}

/**
 * @brief Clears all data of this object.
 *
 * The pqueries, the tree and the metadata are deleted.
 */
void Sample::clear()
{
    pqueries_.clear();
    tree_ = std::make_shared<PlacementTree>();
    metadata.clear();
}

/**
 * @brief Clears all placements of this Sample.
 *
 * All pqueries are deleted. However, the Tree and metadata are left as they are, thus this is a
 * useful method for simulating placements: Take a copy of a given map, clear its placements, then
 * generate new ones using PlacementSimulator.
 */
void Sample::clear_placements()
{
    for (auto it = tree_->begin_edges(); it != tree_->end_edges(); ++it) {
        (*it)->data.placements.clear();
    }
    pqueries_.clear();
}

// =================================================================================================
//     Helper Methods
// =================================================================================================

/**
 * @brief Delete all connecting pointers between the Pquery Placements and their edges on the tree.
 *
 * Each Placement has a pointer to its edge, and each edge has a vector of pointers to all
 * Placements that point to that edge.
 *
 * By default, each placement deletes these connections when being destroyed. This process includes
 * a linear search through the vector of the edge. If many Placements need to be deleted at the
 * same time, this is a bottleneck in speed, as it results in quadratic time complexity in the
 * number of placements.
 *
 * Thus, for operations that do a lot of Placement deletions (see restrain_to_max_weight_placements
 * or collect_duplicate_pqueries as examples), it is cheaper to first delete all pointers using
 * this method, and later restore the remaining ones using reattach_pqueries_to_tree.
 *
 * Caveat: While the pqueries are detached, the edge pointers of the placements and the placement
 * vector of the edges are empty and shall not be used.
 */
void Sample::detach_pqueries_from_tree()
{
    for( auto it = tree().begin_edges(); it != tree().end_edges(); ++it ) {
        (*it)->data.placements.clear();
    }
    for (auto const& pqry : pqueries_) {
        for( auto const& place : pqry->placements ) {
            assert( place->edge != nullptr );
            place->edge = nullptr;
        }
    }
}

/**
 * @brief Restore all connecting pointers between the Pquery Placements and their edges on the tree.
 *
 * See detach_pqueries_from_tree for more information.
 */
void Sample::reattach_pqueries_to_tree()
{
    auto enm = edge_num_to_edge_map( *tree_ );
    for (auto const& pqry : pqueries_) {
        for( auto const& place : pqry->placements ) {
            assert( enm.count(place->edge_num) == 1 );
            assert( place->edge == nullptr );
            enm[place->edge_num]->data.placements.push_back(place.get());
            place->edge = enm[place->edge_num];
        }
    }
}

// =================================================================================================
//     Placement Mass
// =================================================================================================

/**
 * @brief Get the total number of placements in all pqueries.
 */
size_t Sample::placement_count() const
{
    size_t count = 0;
    for (const auto& pqry : pqueries_) {
        count += pqry->placements.size();
    }
    return count;
}

/**
 * @brief Get the summed mass of all placements on the tree, given by their `like_weight_ratio`.
 */
double Sample::placement_mass() const
{
    double sum = 0.0;
    for (const auto& pqry : pqueries_) {
        for (const auto& place : pqry->placements) {
            sum += place->like_weight_ratio;
        }
    }
    return sum;
}

} // namespace placement
} // namespace genesis
