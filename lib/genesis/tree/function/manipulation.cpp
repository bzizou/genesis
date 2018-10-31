/*
    Genesis - A toolkit for working with phylogenetic data.
    Copyright (C) 2014-2018 Lucas Czech and HITS gGmbH

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

#include "genesis/tree/function/manipulation.hpp"

#include "genesis/tree/function/functions.hpp"
#include "genesis/tree/function/operators.hpp"
#include "genesis/tree/iterator/node_links.hpp"
#include "genesis/tree/iterator/preorder.hpp"
#include "genesis/tree/tree.hpp"
#include "genesis/tree/tree/subtree.hpp"

#include "genesis/utils/core/algorithm.hpp"
#include "genesis/utils/core/std.hpp"

#include "genesis/utils/core/logging.hpp"
#include "genesis/utils/text/string.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

namespace genesis {
namespace tree {

// =================================================================================================
//     Add Nodes
// =================================================================================================

TreeNode& add_new_node( Tree& tree, TreeNode& target_node )
{
    // Basic check.
    if( ! belongs_to( target_node, tree ) ) {
        throw std::runtime_error(
            "Cannot add Node to a Tree where the given Node is not part of the Tree."
        );
    }

    // Get container access shortcuts.
    auto& links = tree.expose_link_container();
    auto& nodes = tree.expose_node_container();
    auto& edges = tree.expose_edge_container();

    // Create all new elements that we need:
    //  1. A link that gets added to the given node and connects it to the new node.
    //  2. The link that belongs to the newly created leaf node.
    //  3. The newly created node itself.
    //  4. The edge between the two nodes.
    auto con_link_u = utils::make_unique< TreeLink >();
    auto end_link_u = utils::make_unique< TreeLink >();
    auto end_node_u = utils::make_unique< TreeNode >();
    auto con_edge_u = utils::make_unique< TreeEdge >();

    // Get the pointers, for ease of use.
    auto const con_link = con_link_u.get();
    auto const end_link = end_link_u.get();
    auto const end_node = end_node_u.get();
    auto const con_edge = con_edge_u.get();

    // Populate the link at the given node.
    con_link->reset_index( links.size() );
    con_link->reset_node( &target_node );
    con_link->reset_edge( con_edge );
    con_link->reset_outer( end_link );

    // Find the last link of the given node (in traversal order around the node).
    auto const up_link = &target_node.primary_link();
    auto last_link = up_link;
    while( &last_link->next() != up_link ) {
        last_link = &last_link->next();
    }

    // Now insert the new link in between up_link and last_link.
    assert( &last_link->next() == up_link );
    last_link->reset_next( con_link );
    con_link->reset_next( up_link );
    assert( &last_link->next() == con_link );
    assert( &con_link->next()  == up_link  );

    // Add the link to the tree. This changes the size of the links vector, so the next call to
    // links.size() will give a new, proper value for the other link.
    links.push_back( std::move( con_link_u ));

    // Populate the link at the new node and add it to the tree.
    end_link->reset_index( links.size() );
    end_link->reset_node( end_node );
    end_link->reset_edge( con_edge );
    end_link->reset_next( end_link );
    end_link->reset_outer( con_link );
    links.push_back( std::move( end_link_u ));

    // Populate the new node and add it to the tree.
    end_node->reset_index( nodes.size() );
    end_node->reset_primary_link( end_link );
    end_node->reset_data( target_node.data_ptr()->recreate() );
    nodes.push_back( std::move( end_node_u ));

    // Populate the new edge and add it to the tree.
    con_edge->reset_index( edges.size() );
    con_edge->reset_primary_link( con_link );
    con_edge->reset_secondary_link( end_link );
    con_edge->reset_data( target_node.primary_link().edge().data_ptr()->recreate() );
    edges.push_back( std::move( con_edge_u ));

    // Return the new node. We just moved the node uniq ptr, but not the pointee, so this is valid.
    return *end_node;
}

TreeNode& add_new_node(
    Tree& tree,
    TreeEdge& target_edge,
    std::function<void( TreeEdge& target_edge, TreeEdge& new_edge )> adjust_edges
) {
    // Basic check.
    if( ! belongs_to( target_edge, tree ) ) {
        throw std::runtime_error(
            "Cannot add Node to Tree on an Edge that is not part of the Tree."
        );
    }

    // Get container access shortcuts.
    auto& links = tree.expose_link_container();
    auto& nodes = tree.expose_node_container();
    auto& edges = tree.expose_edge_container();

    // Create all new elements that we need:
    //  * Two links that build a new node in the middle of the target edge.
    //  * The new node in the middle of the target edge.
    //  * A new edge that connects to the secondary end of the target edge.
    auto pri_link_u = utils::make_unique< TreeLink >();
    auto sec_link_u = utils::make_unique< TreeLink >();
    auto mid_node_u = utils::make_unique< TreeNode >();
    auto sec_edge_u = utils::make_unique< TreeEdge >();

    // Get the pointers, for ease of use.
    auto const pri_link = pri_link_u.get();
    auto const sec_link = sec_link_u.get();
    auto const mid_node = mid_node_u.get();
    auto const sec_edge = sec_edge_u.get();

    // Populate the link towards the primary end of the target edge and add it to the tree.
    pri_link->reset_index( links.size() );
    pri_link->reset_next( sec_link );
    pri_link->reset_outer( &target_edge.primary_link() );
    pri_link->reset_node( mid_node );
    pri_link->reset_edge( &target_edge );
    links.push_back( std::move( pri_link_u ));

    // Populate the link towards the secondary end of the target edge and add it to the tree.
    sec_link->reset_index( links.size() );
    sec_link->reset_next( pri_link );
    sec_link->reset_outer( &target_edge.secondary_link() );
    sec_link->reset_node( mid_node );
    sec_link->reset_edge( sec_edge );
    links.push_back( std::move( sec_link_u ));

    // Populate the new node in the middle of the target edge and add it to the tree.
    mid_node->reset_index( nodes.size() );
    mid_node->reset_primary_link( pri_link );
    mid_node->reset_data( target_edge.primary_node().data_ptr()->recreate() );
    nodes.push_back( std::move( mid_node_u ));

    // Populate the edge at the secondary end of the target edge and add it to the tree.
    sec_edge->reset_index( edges.size() );
    sec_edge->reset_primary_link( sec_link );
    sec_edge->reset_secondary_link( &target_edge.secondary_link() );
    sec_edge->reset_data( target_edge.data_ptr()->recreate() );
    edges.push_back( std::move( sec_edge_u ));

    // Finally adjust the existing tree elements.
    target_edge.primary_link().reset_outer( pri_link );
    target_edge.secondary_link().reset_outer( sec_link );
    target_edge.secondary_link().reset_edge( sec_edge );
    target_edge.reset_secondary_link( pri_link );

    // If we have a transform function, call it.
    if( adjust_edges ) {
        adjust_edges( target_edge, *sec_edge );
    }

    return *mid_node;
}

TreeNode& add_new_leaf_node(
    Tree& tree,
    TreeEdge& target_edge,
    std::function<void( TreeEdge& target_edge, TreeEdge& new_edge )> adjust_edges
) {
    // First add a node that splits the edge, and then a new leaf node to this one.
    auto& mid_node = add_new_node( tree, target_edge, adjust_edges );
    return add_new_node( tree, mid_node );
}

TreeNode& add_root_node( Tree& tree, TreeEdge& target_edge )
{
    auto& mid_node = add_new_node( tree, target_edge );
    reroot( tree, mid_node );
    return mid_node;
}

// =================================================================================================
//     Delete Nodes
// =================================================================================================

void delete_node( Tree& tree, TreeNode& target_node )
{
    // Basic check.
    if( ! belongs_to( target_node, tree ) ) {
        throw std::runtime_error(
            "Cannot delete Node from a Tree that is not part of the Tree."
        );
    }

    auto const deg = degree( target_node );
    if( deg == 1 ) {
        delete_leaf_node( tree, target_node );
    } else if( deg == 2 ) {
        delete_linear_node( tree, target_node );
    } else {
        delete_subtree( tree, Subtree{ target_node } );
    }
}

void delete_leaf_node( Tree& tree, TreeNode& target_node )
{
    // Basic check.
    if( ! belongs_to( target_node, tree ) ) {
        throw std::runtime_error(
            "Cannot delete Node from a Tree that is not part of the Tree."
        );
    }
    if( degree( target_node ) != 1 ) {
        throw std::runtime_error(
            "Cannot delete leaf Node if the Node is not actually a leaf."
        );
    }

    // Get container access shortcuts.
    auto& links = tree.expose_link_container();
    auto& nodes = tree.expose_node_container();
    auto& edges = tree.expose_edge_container();

    // If the node to be deleted is the root, we need to reset to its adjacent node.
    // We need to check this now, and reset later, because the node indices will change...
    auto root_link = &tree.root_link();
    if( tree.root_node().index() == target_node.index() ) {
        root_link = &target_node.link().outer();
    }

    // Delete the edge and adjust indices of other edges as needed.
    // We do this first, so that the link to the edge is still valid.
    auto const edge_idx = target_node.link().edge().index();
    edges.erase( edges.begin() + edge_idx );
    for( size_t i = edge_idx; i < tree.edge_count(); ++i ) {
        assert( tree.edge_at(i).index() == i + 1 );
        tree.edge_at(i).reset_index(i);
    }

    // Make the node that the target is attached to forget about this node by skipping the link.
    auto& attach_link = target_node.link().outer();
    assert( &attach_link.edge() == &target_node.link().edge() );
    assert( &attach_link.outer() == &target_node.link() );
    auto link_ptr = &( attach_link.next() );
    while( &link_ptr->next() != &attach_link ) {
        link_ptr = &link_ptr->next();
    }
    assert( &link_ptr->next() == &attach_link );
    assert( &link_ptr->next().next() == &attach_link.next() );
    link_ptr->reset_next( &link_ptr->next().next() );

    // Delete both links, and adjust the indices of the other links as needed.
    // Deleting two elemetns is tricky, as the indices shift...
    auto const minmax_link_idx = utils::minmax_value( attach_link.index(), attach_link.outer().index() );
    links.erase( links.begin() + minmax_link_idx.first );
    links.erase( links.begin() + minmax_link_idx.second - 1 );
    for( size_t i = minmax_link_idx.first; i < links.size(); ++i ) {
        assert( tree.link_at(i).index() == i + 1 || tree.link_at(i).index() == i + 2 );
        tree.link_at(i).reset_index(i);
    }

    // Finally, delete the node and adjust indices of other nodes as needed.
    auto const node_idx = target_node.index();
    nodes.erase( nodes.begin() + node_idx );
    for( size_t i = node_idx; i < tree.node_count(); ++i ) {
        assert( tree.node_at(i).index() == i + 1 );
        tree.node_at(i).reset_index(i);
    }

    // Lastly, reset the root link (or set it to what it was before...)
    tree.reset_root_link_index( root_link->index() );
}

void delete_linear_node(
    Tree& tree,
    TreeNode& target_node,
    std::function<void( TreeEdge& remaining_edge, TreeEdge& deleted_edge )> adjust_edges
) {
    // Basic check.
    if( ! belongs_to( target_node, tree ) ) {
        throw std::runtime_error(
            "Cannot delete Node from a Tree that is not part of the Tree."
        );
    }
    if( degree( target_node ) != 2 ) {
        throw std::runtime_error(
            "Cannot delete linear Node if the Node is not actually linear (degree 2)."
        );
    }

    // Before we do anything, call the adjust function.
    if( adjust_edges ) {
        adjust_edges( target_node.primary_link().edge(), target_node.primary_link().next().edge() );
    }

    // If the node to be deleted is the root, we need to reset to its adjacent node.
    // We need to check this now, and reset later, because the node indices will change...
    auto root_link = &tree.root_link();
    if( tree.root_node().index() == target_node.index() ) {
        root_link = &target_node.link().outer();
    }

    // Get container access shortcuts.
    auto& links = tree.expose_link_container();
    auto& nodes = tree.expose_node_container();
    auto& edges = tree.expose_edge_container();

    // Adjust the links of the two adjacent nodes to point to each other.
    auto& pr_link = target_node.link();
    auto& adj_link_p = pr_link.outer();
    auto& adj_link_d = pr_link.next().outer();
    assert( &adj_link_p.outer().node() == &adj_link_d.outer().node() );
    adj_link_p.reset_outer( &adj_link_d );
    adj_link_d.reset_outer( &adj_link_p );

    // Adjust the pointers to and from the remaining edge.
    assert( &adj_link_p.edge().secondary_node() == &target_node );
    assert( &adj_link_d.edge().primary_node() == &target_node );
    assert( &adj_link_p.edge() == &target_node.link().edge() );
    assert( &adj_link_d.edge() == &target_node.link().next().edge() );
    adj_link_p.edge().reset_secondary_link( &adj_link_d );
    adj_link_d.edge().reset_primary_link( &adj_link_p );
    adj_link_d.reset_edge( &target_node.primary_link().edge() );

    // Delete the edge and adjust indices of other edges as needed.
    auto const edge_idx = pr_link.next().edge().index();
    edges.erase( edges.begin() + edge_idx );
    for( size_t i = edge_idx; i < tree.edge_count(); ++i ) {
        assert( tree.edge_at(i).index() == i + 1 );
        tree.edge_at(i).reset_index(i);
    }

    // Delete both links of the node, and adjust the indices of the other links as needed.
    // Deleting two elemetns is tricky, as the indices shift...
    auto const minmax_link_idx = utils::minmax_value(  pr_link.index(), pr_link.next().index() );
    links.erase( links.begin() + minmax_link_idx.first );
    links.erase( links.begin() + minmax_link_idx.second - 1 );
    for( size_t i = minmax_link_idx.first; i < links.size(); ++i ) {
        assert( tree.link_at(i).index() == i + 1 || tree.link_at(i).index() == i + 2 );
        tree.link_at(i).reset_index(i);
    }

    // Finally, delete the node and adjust indices of other nodes as needed.
    auto const node_idx = target_node.index();
    nodes.erase( nodes.begin() + node_idx );
    for( size_t i = node_idx; i < tree.node_count(); ++i ) {
        assert( tree.node_at(i).index() == i + 1 );
        tree.node_at(i).reset_index(i);
    }

    // Lastly, reset the root link (or set it to what it was before...)
    tree.reset_root_link_index( root_link->index() );
}

// /**
//  * @brief Local helper function template that takes one of the tree element containers
//  * and deletes all elements at given indices from it.
//  */
// template<class T>
// void delete_from_tree_container_( T& cont, std::vector<size_t> const& idcs )
// {
//
// }

void delete_subtree( Tree& tree, Subtree const& subtree )
{
    // Basic check.
    if( ! belongs_to( subtree.link(), tree ) ) {
        throw std::runtime_error(
            "Cannot delete Subtree from a Tree that is not part of the Tree."
        );
    }

    // Make lists of all indices that need to be deleted.
    // This already includes the edge towards the rest of the tree, as well as the link
    // at the attachment node!
    // Also, find out if whe are deleting the root. If so, we need to reset it later.
    std::vector<size_t> del_nodes;
    std::vector<size_t> del_edges;
    std::vector<size_t> del_links;
    bool contains_root = false;
    for( auto it : preorder( subtree )) {
        del_nodes.push_back( it.node().index() );
        del_edges.push_back( it.edge().index() );
        del_links.push_back( it.link().index() );
        del_links.push_back( it.link().outer().index() );

        if( is_root( it.node() )) {
            contains_root = true;
        }
    }

    // Sort them, so that deletion can be done. There should be no duplicats.
    std::sort( del_nodes.begin(), del_nodes.end() );
    std::sort( del_edges.begin(), del_edges.end() );
    std::sort( del_links.begin(), del_links.end() );
    assert( std::adjacent_find( del_nodes.begin(), del_nodes.end()) == del_nodes.end() );
    assert( std::adjacent_find( del_edges.begin(), del_edges.end()) == del_edges.end() );
    assert( std::adjacent_find( del_links.begin(), del_links.end()) == del_links.end() );

    // If we are about to delete the root, store the new link for later.
    // For this, we use the first link of the attachmend node.
    auto root_link = contains_root ? &subtree.link().outer().next() : &tree.root_link();

    // Get the link that points to the attachmend link
    // (the one on the remaining node that will be deleted).
    auto& attach_link = subtree.link().outer();
    assert( &attach_link.edge() == &subtree.link().edge() );
    assert( &attach_link.outer() == &subtree.link() );
    auto link_ptr = &( attach_link.next() );
    while( &link_ptr->next() != &attach_link ) {
        link_ptr = &link_ptr->next();
    }
    assert( &link_ptr->next() == &attach_link );
    assert( &link_ptr->next().next() == &attach_link.next() );
    auto& link_nc = tree.link_at( link_ptr->index() );

    // If the primary link of the attachment node is about to be deleted, reset it to another link.
    if( &link_nc.node().primary_link() == &link_nc.next() ) {
        assert( &attach_link.node().primary_link() == &attach_link );
        link_nc.node().reset_primary_link( &link_nc.next().next() );
    }

    // TODO same for other methods

    // Make the node that the target is attached to forget about this node by skipping the link.
    // Because the subtree always stores a const link, we need to get a non-const from the tree.
    link_nc.reset_next( &link_nc.next().next() );

    // Get container access shortcuts.
    auto& old_nodes = tree.expose_node_container();
    auto& old_edges = tree.expose_edge_container();
    auto& old_links = tree.expose_link_container();

    // Make new containers and move the needed elements there.
    // This is just so much easier than trying to delete within a container
    // while iterating it, adjusting the internal indices of the elemetns, etc.
    auto new_nodes = Tree::NodeContainerType();
    auto new_edges = Tree::EdgeContainerType();
    auto new_links = Tree::LinkContainerType();
    new_nodes.reserve( old_nodes.size() - del_nodes.size() );
    new_edges.reserve( old_edges.size() - del_edges.size() );
    new_links.reserve( old_links.size() - del_links.size() );

    LOG_DBG << "del_nodes " << utils::join( del_nodes, ", " );

    // Move nodes and adjust indices.
    size_t del_nodes_idx = 0;
    for( size_t i = 0; i < old_nodes.size(); ++i ) {
        assert( i == old_nodes[i]->index() );
        if( del_nodes_idx < del_nodes.size() && del_nodes[del_nodes_idx] == i ) {

            // If the current index is in the deletion list, don't move the element,
            // and go to the next element of the deletion list.
            ++del_nodes_idx;
        } else {

            // If the current index is not in the deletion list, move it to the new container.
            assert( old_nodes[i]->index() == i );
            old_nodes[i]->reset_index( i - del_nodes_idx );
            new_nodes.emplace_back( std::move( old_nodes[i] ));
            assert( new_nodes.back()->index() == new_nodes.size() - 1 );
        }
    }
    assert( new_nodes.size() + del_nodes.size() == old_nodes.size() );
    std::swap( new_nodes, old_nodes );
    // old_nodes = std::move( new_nodes );

    LOG_DBG << "del_edges " << utils::join( del_edges, ", " );

    // Move edges and adjust indices.
    size_t del_edges_idx = 0;
    for( size_t i = 0; i < old_edges.size(); ++i ) {
        assert( i == old_edges[i]->index() );
        if( del_edges_idx < del_edges.size() && del_edges[del_edges_idx] == i ) {

            // If the current index is in the deletion list, don't move the element,
            // and go to the next element of the deletion list.
            ++del_edges_idx;
        } else {

            // If the current index is not in the deletion list, move it to the new container.
            assert( old_edges[i]->index() == i );
            old_edges[i]->reset_index( i - del_edges_idx );
            new_edges.emplace_back( std::move( old_edges[i] ));
            assert( new_edges.back()->index() == new_edges.size() - 1 );
        }
    }
    assert( new_edges.size() + del_edges.size() == old_edges.size() );
    std::swap( new_edges, old_edges );
    // old_edges = std::move( new_edges );

    LOG_DBG << "del_links " << utils::join( del_links, ", " );

    // Move links and adjust indices.
    size_t del_links_idx = 0;
    for( size_t i = 0; i < old_links.size(); ++i ) {
        assert( i == old_links[i]->index() );
        if( del_links_idx < del_links.size() && del_links[del_links_idx] == i ) {

            // If the current index is in the deletion list, don't move the element,
            // and go to the next element of the deletion list.
            ++del_links_idx;
        } else {

            // If the current index is not in the deletion list, move it to the new container.
            assert( old_links[i]->index() == i );
            old_links[i]->reset_index( i - del_links_idx );
            new_links.emplace_back( std::move( old_links[i] ));
            assert( new_links.back()->index() == new_links.size() - 1 );
        }
    }
    assert( new_links.size() + del_links.size() == old_links.size() );
    std::swap( new_links, old_links );
    // old_links = std::move( new_links );

    // Reset the root if neeeded. This uses the new index of the pointee.
    tree.reset_root_link_index( root_link->index() );
}

void delete_edge(
    Tree& tree,
    TreeEdge& target_edge,
    std::function<void( TreeEdge& remaining_node, TreeEdge& deleted_node )> adjust_nodes
) {

}

// =================================================================================================
//     Rerooting
// =================================================================================================

void reroot( Tree& tree, TreeLink& at_link )
{
    if( ! belongs_to( at_link, tree ) ) {
        throw std::runtime_error( "Cannot reroot Tree on a Link that is not part of the Tree." );
    }

    // We store the old root node, becasue we will change internals of the tree, so that
    // node().is_root() won't work while this function is running.
    TreeNode* old_root = &tree.root_node();

    // Pointer to the primary link of the target node.
    TreeLink* cur_link = &tree.link_at( at_link.index() ).node().primary_link();

    // Set new root index and node link of the new root.
    tree.reset_root_link_index( at_link.index() );
    at_link.node().reset_primary_link( &tree.link_at( at_link.index() ));

    // Walk the path from the new root to the old, and change all pointers of the edges and nodes
    // on that path so that they point towards the new root.
    while( &cur_link->node() != old_root ) {

        // Assert that the primary direction is correct: Is the current link is at the secondary
        // end of its edge?
        assert( cur_link == &cur_link->edge().secondary_link() );

        // Swap the edge's links, so that they point towards the new root.
        auto link_p_tmp = &cur_link->edge().primary_link();
        auto link_s_tmp = &cur_link->edge().secondary_link();
        cur_link->edge().reset_primary_link( link_s_tmp );
        cur_link->edge().reset_secondary_link( link_p_tmp );
        // std::swap( cur_link->edge_->link_p_, cur_link->edge_->link_s_ );

        // Assert that this worked.
        assert( cur_link           == &cur_link->edge().primary_link() );
        assert( &cur_link->outer() == &cur_link->edge().secondary_link() );

        // Store the link of the next node that points towards the root.
        // We need it, because we will change this upwards link of the next node now.
        auto to_root_link = &cur_link->outer().node().primary_link();

        // Change the main link of the next node so that it points towards the new root.
        cur_link->outer().node().reset_primary_link( &cur_link->outer() );

        // Move one node towards the root.
        cur_link = to_root_link;
    }
}

void reroot( Tree& tree, TreeNode& at_node )
{
    if( ! belongs_to( at_node, tree ) ) {
        throw std::runtime_error( "Cannot reroot Tree on a Node that is not part of the Tree." );
    }
    reroot( tree, at_node.link() );
}

void reroot_at_node( Tree& tree, size_t node_index )
{
    if( node_index >= tree.node_count() ) {
        throw std::runtime_error( "Cannot reroot Tree on a Node that is not part of the Tree." );
    }
    reroot( tree, tree.node_at( node_index ));
}

// =================================================================================================
//     Ladderize
// =================================================================================================

void ladderize( Tree& tree, LadderizeOrder order )
{
    // For each node, get how many nodes its subtree (away from the root) has.
    // We use this quantity to sort each node's links.
    auto sub_sizes = subtree_sizes( tree );

    // Ladderize all nodes
    for( auto& node : tree.nodes() ) {

        // No need to ladderize a leaf. It would still work, but we can use this as a speedup.
        if( is_leaf( node )) {
            continue;
        }

        // Get the sizes of the children/subtrees of this node.
        std::vector<size_t>    child_sizes;
        std::vector<TreeLink*> child_links;
        for( auto const& link_it : node_links( node ) ) {

            // Don't treat the link towards the root; we only want to sort the subtree.
            // Assert that the first iteration is actually this link towards the root.
            if( link_it.is_first_iteration() ) {
                assert( &link_it.link() == &node.primary_link() );
                continue;
            }

            child_sizes.push_back( sub_sizes[ link_it.link().outer().node().index() ] );
            child_links.push_back( &link_it.link() );
        }

        // Sort the sizes. We use stable sort in order to not change the order of equal sized subtrees.
        auto child_order = ( order == LadderizeOrder::kSmallFirst
            ? utils::stable_sort_indices( child_sizes.begin(), child_sizes.end(), std::less<size_t>() )
            : utils::stable_sort_indices( child_sizes.begin(), child_sizes.end(), std::greater<size_t>() )
        );

        // The number of indices needs to be the rank of the node (number of immediate children).
        assert( child_order.size() == child_sizes.size() );
        assert( child_order.size() == child_links.size() );
        assert( child_order.size() == degree( node ) - 1 );

        // Change all next links of the node so that they reflect the subtree size order.
        auto cur_link = &node.primary_link();
        for( auto child_order_i : child_order ) {

            // We use this assertion to ensure that each link is only processed once.
            // At the end of this loop, we set it to nullptr, so a second encounter would fail.
            assert( child_links[ child_order_i ] );

            // Set the link's next link and move on.
            cur_link->reset_next( child_links[ child_order_i ] );
            cur_link = child_links[ child_order_i ];

            // Set the link in the vector to null, so that the above assert can check that we
            // never process it again.
            child_links[ child_order_i ] = nullptr;
        }

        // We now need to set the next pointer of the last link of the node so that it points
        // back to the original starting node (the one towards the root).
        cur_link->reset_next( &node.primary_link() );

        // Finally, assert that we processed all links. If so, all of them are null by now.
        for( auto const& cl : child_links ) {
            (void) cl;
            assert( !cl );
        }
    }
}

} // namespace tree
} // namespace genesis
