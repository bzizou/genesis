#ifndef GENESIS_TREE_TREE_H_
#define GENESIS_TREE_TREE_H_

/**
 * @brief Provides the basic interface for phylogenetic trees.
 *
 * For more information, see Tree class.
 *
 * @file
 * @ingroup tree
 */

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "tree/default/tree.hpp"
#include "tree/tree_edge.hpp"
#include "tree/tree_link.hpp"
#include "tree/tree_node.hpp"

namespace genesis {
namespace tree {

// =================================================================================================
//     Forward Declarations
// =================================================================================================

template <class TreeType>
bool validate( TreeType const& tree );

// =================================================================================================
//     Tree
// =================================================================================================

/**
 * @brief Basic class template for representing phylogenetic trees.
 *
 * A tree in this implementation consists of three types of elements: Links, Nodes and Edges.
 * The topoloty of the tree is completely described by the links, while nodes and edges add the
 * capability to store data on the tree.
 *
 * Data in the Tree is not directly stored in the elements (Links, Nodes, Edges) of the Tree.
 * Instead, data belonging to nodes and edges can be stored there by using appropriate template
 * parameters when creating a tree, with classes that are able to hold the needed data.
 * This ensures a maximum of flexibility for using Trees, because any type of data can be plugged
 * in this way.
 *
 * Additional information concerning the whole tree, as well as special algorithms working on the
 * tree and its data are also not direct members of the tree - for the same reason of flexibility
 * and extensibility: Instead of adding more and more data and algorithms to the tree, we create
 * new classes that encapsulate a tree and all the other needed code to work with it.
 * See the Sample class for an example.
 *
 * Thus, the tree itself only contains the needed information to store and work with a topology.
 *
 * This class has several redundant ways of representing the same information. On the one hand, this
 * makes using a Tree easy, as one can chose the representaion of data that best fits a given task.
 * On the other hand, maintaining all those invariants when changing the tree topology is tedious.
 * Here is a (comprehensive?) list of invariants of this class:
 *
 *  *  The (virtual) root node and one of its links are stored as first element in the arrays:
 *     `nodes_[0]` and `links_[0]`.
 *  *  The indices in all three arrays (`nodes_`, `links_` and `edges_`) have to match the index
 *     integers stored in those elements: `nodes_[i]->index_ == i`.
 *  *  The link that is stored in a node has to be the one pointing towards the root.
 *  *  The primary link of an edge has to point towards the root, the secondary away from it.
 *
 * Those invariants are established when the Tree is constructed.
 */
template <class NodeDataType = DefaultTreeNodeData, class EdgeDataType = DefaultTreeEdgeData>
class Tree
{
public:

    // -------------------------------------------------------------------------
    //     Typedefs
    // -------------------------------------------------------------------------

    using TreeType = Tree    <NodeDataType, EdgeDataType>;
    using LinkType = TreeLink<NodeDataType, EdgeDataType>;
    using NodeType = TreeNode<NodeDataType, EdgeDataType>;
    using EdgeType = TreeEdge<NodeDataType, EdgeDataType>;

    using LinkContainer = std::vector<std::unique_ptr<LinkType>>;
    using NodeContainer = std::vector<std::unique_ptr<NodeType>>;
    using EdgeContainer = std::vector<std::unique_ptr<EdgeType>>;

    using      IteratorLinks = typename std::vector<std::unique_ptr<LinkType>>::iterator;
    using ConstIteratorLinks = typename std::vector<std::unique_ptr<LinkType>>::const_iterator;

    using      IteratorNodes = typename std::vector<std::unique_ptr<NodeType>>::iterator;
    using ConstIteratorNodes = typename std::vector<std::unique_ptr<NodeType>>::const_iterator;

    using      IteratorEdges = typename std::vector<std::unique_ptr<EdgeType>>::iterator;
    using ConstIteratorEdges = typename std::vector<std::unique_ptr<EdgeType>>::const_iterator;

    // -------------------------------------------------------------------------
    //     Construction and Destruction
    // -------------------------------------------------------------------------

    Tree()  = default;
    ~Tree() = default;

    Tree( const Tree<NodeDataType, EdgeDataType>& other );
    Tree( Tree&& other ) = default;

    Tree& operator= ( Tree<NodeDataType, EdgeDataType> const& other );
    Tree& operator= ( Tree&& other ) = default;

    void swap (TreeType& other);

    void import_content (LinkContainer& links, NodeContainer& nodes, EdgeContainer& edges);
    void export_content (LinkContainer& links, NodeContainer& nodes, EdgeContainer& edges);

    void clear();

    // -------------------------------------------------------------------------
    //     Accessors
    // -------------------------------------------------------------------------

    bool empty() const;

    LinkType      & root_link();
    LinkType const& root_link() const;

    NodeType      & root_node();
    NodeType const& root_node() const;

    LinkType      & link_at(size_t index);
    LinkType const& link_at(size_t index) const;

    NodeType      & node_at(size_t index);
    NodeType const& node_at(size_t index) const;

    EdgeType      & edge_at(size_t index);
    EdgeType const& edge_at(size_t index) const;

    size_t link_count() const;
    size_t node_count() const;
    size_t edge_count() const;

    // -------------------------------------------------------------------------
    //     Iterators
    // -------------------------------------------------------------------------

    // -----------------------------------------------------
    //     Links
    // -----------------------------------------------------

    IteratorLinks      begin_links();
    ConstIteratorLinks begin_links() const;

    IteratorLinks      end_links();
    ConstIteratorLinks end_links() const;

    // -----------------------------------------------------
    //     Nodes
    // -----------------------------------------------------

    IteratorNodes      begin_nodes();
    ConstIteratorNodes begin_nodes() const;

    IteratorNodes      end_nodes();
    ConstIteratorNodes end_nodes() const;

    // -----------------------------------------------------
    //     Edges
    // -----------------------------------------------------

    IteratorEdges      begin_edges();
    ConstIteratorEdges begin_edges() const;

    IteratorEdges      end_edges();
    ConstIteratorEdges end_edges() const;

    // -------------------------------------------------------------------------
    //     Debug and Dump
    // -------------------------------------------------------------------------

    friend std::ostream& operator << ( std::ostream& out, TreeType const& tree )
    {
        out << "Node Count: " << tree.node_count() << "\n";
        out << "Edge Count: " << tree.edge_count() << "\n";
        out << "Link Count: " << tree.link_count() << "\n";
        return out;
    }

    /**
     * @brief Validate the correctness of all Tree pointers etc.
     *
     * This function is defined in tree/operators/functions.hpp
     */
    friend bool validate<TreeType>( TreeType const& tree );

    // -------------------------------------------------------------------------
    //     Data Members
    // -------------------------------------------------------------------------

private:

    std::vector<std::unique_ptr<LinkType>> links_;
    std::vector<std::unique_ptr<NodeType>> nodes_;
    std::vector<std::unique_ptr<EdgeType>> edges_;
};

} // namespace tree
} // namespace genesis

// =================================================================================================
//     Inclusion of the implementation
// =================================================================================================

// This is a class template, so do the inclusion here.
#include "tree/tree.tpp"

#endif // include guard
