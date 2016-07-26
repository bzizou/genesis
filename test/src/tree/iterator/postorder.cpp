/*
    Genesis - A toolkit for working with phylogenetic data.
    Copyright (C) 2014-2016 Lucas Czech

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
 * @brief Testing Tree Iterators.
 *
 * @file
 * @ingroup test
 */

#include "common.hpp"

#include <string>
#include <utility>

#include "lib/tree/default/functions.hpp"
#include "lib/tree/default/newick_reader.hpp"
#include "lib/tree/formats/newick/reader.hpp"
#include "lib/tree/iterator/postorder.hpp"
#include "lib/tree/tree.hpp"

using namespace genesis;
using namespace tree;

// =================================================================================================
//     Postorder
// =================================================================================================

void TestPostorder(std::string node_name, std::string out_nodes)
{
    std::string input = "((B,(D,E)C)A,F,(H,I)G)R;";
    std::string nodes = "";

    Tree tree;
    DefaultTreeNewickReader().from_string(input, tree);

    auto node = find_node(tree, node_name);
    ASSERT_NE(nullptr, node);

    for( auto it : postorder(*node) ) {
        nodes += node_data_cast< DefaultNodeData >( it.node() ).name;
    }
    EXPECT_EQ(out_nodes, nodes) << " with start node " << node_name;
}

TEST (TreeIterator, Postorder)
{
    TestPostorder("R", "BDECAFHIGR");
    TestPostorder("A", "FHIGRBDECA");
    TestPostorder("B", "DECFHIGRAB");
    TestPostorder("C", "FHIGRBADEC");
    // TestPostorder("D", "");
    // TestPostorder("E", "");
    // TestPostorder("F", "");
    // TestPostorder("G", "");
    // TestPostorder("H", "");
    // TestPostorder("I", "");
}