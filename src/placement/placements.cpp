/**
 * @brief Implementation of Placements class.
 *
 * @file
 * @ingroup placement
 */

#include "placement/placements.hpp"

#include <assert.h>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_map>

#include "placement/jplace_parser.hpp"
#include "utils/logging.hpp"
#include "utils/math.hpp"

namespace genesis {

// =============================================================================
//     Constructor & Destructor
// =============================================================================

void Placements::clear()
{
    for (Pquery* pqry : pqueries) {
        delete pqry;
    }

    std::deque<Pquery*>().swap(pqueries);
    tree.clear();
    edge_num_map.clear();
    metadata.clear();
}

Placements::~Placements()
{
    clear();
}

bool Placements::FromJplaceFile (const std::string&  fn)
{
    return JplaceParser::ProcessFile(fn, *this);
}

bool Placements::FromJplaceString (const std::string&  jplace)
{
    return JplaceParser::ProcessString(jplace, *this);
}

bool Placements::FromJsonLexer (const JsonLexer&    lexer)
{
    return JplaceParser::ProcessLexer(lexer, *this);
}

bool Placements::FromJsonDocument (const JsonDocument& doc)
{
    return JplaceParser::ProcessDocument(doc, *this);
}

/**
 * @brief Adds the pqueries from another Placements objects to this one.
 */
bool Placements::Merge(Placements& other)
{
    // TODO identical data so far checks for branch length and edge num, but not placements on the edge.
    // TODO if it did, this function would never return true for two different placements...
    if (!tree.HasIdenticalTopology(other.tree) || !tree.HasIdenticalData(other.tree)) {
        LOG_WARN << "Cannot merge Placements with different reference trees.";
        return false;
    }

    for (Pquery* pqry : other.pqueries) {
        Pquery* npqry = new Pquery;
        *npqry = *pqry;
        this->pqueries.push_back(npqry);
    }
    return true;
}

/**
 * @brief Removes all placements but the most likely one from all pqueries.
 */
void Placements::RestrainToMaxWeightPlacements()
{
    for (Pquery* pqry : pqueries) {
        // init
        double max_w = -1.0;
        int    max_i;

        for (size_t i = 0; i < pqry->placements.size(); ++i) {
            // find the maximum of the weight ratios in the placements of this pquery
            Pquery::Placement& place = pqry->placements[i];
            if (place.like_weight_ratio > max_w) {
                max_w = place.like_weight_ratio;
                max_i = i;
            }

            // delete the reference from the edge to this pquery. we will later add the one that
            // points to the remaining placement.
            PlacementTree::EdgeType* edge = PlacementToEdge(place);
            std::deque<Pquery*>::iterator it = edge->data.pqueries.begin();
            for (; it != edge->data.pqueries.end(); ++it) {
                if (*it == pqry) {
                    break;
                }
            }
            // assert that the edge actually contains a reference to this pquery. if not,
            // this means that we messed up somewhere else while adding/removing placements...
            assert(it != edge->data.pqueries.end());
            edge->data.pqueries.erase(it);
        }

        // we do not allow empty placement objects (should we?)
        assert(max_w > -1.0);

        // remove all but the max element from placements by creating a new deque and swapping it.
        // this is faster and simpler than removing all but one element.
        std::deque<Pquery::Placement> np;
        np.push_back(pqry->placements[max_i]);
        pqry->placements.swap(np);

        // now add back the reference from the edge to the pquery.
        // assert that we now have a single placement in the pquery (the most likely one).
        assert(pqry->placements.size() == 1);
        PlacementToEdge(pqry->placements[0])->data.pqueries.push_back(pqry);

        // also, set the like_weight_ratio to 1.0, because we do not have any other placements left.
        pqry->placements[0].like_weight_ratio = 1.0;
    }
}

// =============================================================================
//     Placement Mass
// =============================================================================

/**
 * @brief Get the summed mass of all placements on the tree.
 */
double Placements::PlacementMassSum()
{
    double sum = 0.0;
    for (
        PlacementTree::IteratorEdges it = tree.BeginEdges();
        it != tree.EndEdges();
        ++it
    ) {
        sum += (*it)->data.PlacementMass();
    }
    return sum;
}

/**
 * @brief Calculates the Earth Movers Distance to another sets of placements on a fixed reference
 * tree.
 */
double Placements::EMD(Placements& right)
{
    return Placements::EMD(*this, right);
}

/**
 * @brief Calculates the Earth Movers Distance between two sets of placements on a fixed reference
 * tree.
 */
double Placements::EMD(Placements& lhs, Placements& rhs)
{
    // keep track of the total resulting distance.
    double distance = 0.0;

    // store a per-node balance of mass. each element in this map contains information about how
    // much placement mass is pushing from the direction of this node towards the root.
    // caveat: the masses that are stored here are already fully pushed towards the root, but are
    // stored here using the node at the lower end of the branch as key.
    std::unordered_map<PlacementTree::NodeType*, double> balance;

    // use the sum of masses as normalization factor for the masses.
    double totalmass_l = lhs.PlacementMassSum();
    double totalmass_r = rhs.PlacementMassSum();

    // do a postorder traversal on both trees in parallel. while doing so, move placements
    // from the tips towards the root and store their movement (mass * distance) in balance[].
    // in theory, it does not matter where we start the traversal - however, the positions of the
    // placements are given as "distal_length" on their branch, which always points away from the
    // root. thus, if we decided to traverse from a different node than the root, we would have to
    // take this into account.
    PlacementTree::IteratorPostorder it_l = lhs.tree.BeginPostorder();
    PlacementTree::IteratorPostorder it_r = rhs.tree.BeginPostorder();
    for (
        ;
        it_l != lhs.tree.EndPostorder() && it_r != rhs.tree.EndPostorder();
        ++it_l, ++it_r
    ) {
        // check whether both trees have identical topology. if they have, the ranks of all nodes
        // are the same. however, if not, at some point their ranks will differ.
        if (it_l.Node()->Rank() != it_r.Node()->Rank()) {
            LOG_WARN << "Calculating EMD on different reference trees not possible.";
            return -1.0;
        }

        // if we are at the last iteration, we reached the root, thus we have moved all masses now
        // and don't need to proceed. if we did, we would count an edge of the root again.
        if (it_l.IsLastIteration()) {
            // we do a check for the mass at the root here for debug purposes.
            double root_mass = 0.0;
            for (
                PlacementTree::NodeType::IteratorLinks n_it = it_l.Node()->BeginLinks();
                n_it != it_l.Node()->EndLinks();
                ++n_it
            ) {
                assert(balance.count(n_it.Link()->Outer()->Node()));
                root_mass += balance[n_it.Link()->Outer()->Node()];
            }
            LOG_DBG << "Mass at root: " << root_mass;

            continue;
        }

        // check whether the data on both reference trees is the same. this has to be done after the
        // check for last iteration / root node, because we don't want to check this for the root.
        if (it_l.Edge()->data.branch_length != it_r.Edge()->data.branch_length ||
            it_l.Edge()->data.edge_num      != it_r.Edge()->data.edge_num
        ) {
            LOG_WARN << "Inconsistent reference trees in EMD calculation.";
            return -1.0;
        }

        // move placements around between children, and collect the remaining mass in mass_s.
        // mass_s then contains the rest mass of the subtree that could not be distributed among
        // the children and thus has to be moved upwards.
        double mass_s = 0.0;
        PlacementTree::LinkType* link = it_l.Link()->Next();
        while (link != it_l.Link()) {
            // we do postorder traversal, so we have seen the child nodes of the current node,
            // which means, they should be in the balance list already.
            assert(balance.count(link->Outer()->Node()));

            mass_s += balance[link->Outer()->Node()];
            link = link->Next();
        }

        // we now start a "normal" EMD caluclation on the current edge. for this, we store the
        // masses of all placements sorted by their position on the branch.
        std::multimap<double, double> edge_balance;

        // add all placements of the branch from the left tree (using positive mass)...
        for (
            PlacementEdgeData::IteratorPlacements it_p = it_l.Edge()->data.BeginPlacements();
            it_p != it_l.Edge()->data.EndPlacements();
            ++it_p
        ) {
            if (it_p->pendant_length < 0.0 || it_p->distal_length < 0.0) {
                LOG_INFO << "Tree contains placement with pendant_length or distal_length < 0.0 "
                         << "at node '" << it_l.Node()->data.name << "'.";
            }
            if (it_p->distal_length > it_l.Edge()->data.branch_length) {
                LOG_INFO << "Tree contains placement with distal_length > branch_length "
                         << "at node '" << it_l.Node()->data.name << "'.";
            }

            distance += it_p->pendant_length / totalmass_l;
            edge_balance.emplace(it_p->distal_length, +1.0 / totalmass_l);
        }

        // ... and the branch from the right tree (using negative mass)
        for (
            PlacementEdgeData::IteratorPlacements it_p = it_r.Edge()->data.BeginPlacements();
            it_p != it_r.Edge()->data.EndPlacements();
            ++it_p
        ) {
            if (it_p->pendant_length < 0.0 || it_p->distal_length < 0.0) {
                LOG_INFO << "Tree contains placement with pendant_length or distal_length < 0.0 "
                         << "at node '" << it_r.Node()->data.name << "'.";
            }
            if (it_p->distal_length > it_r.Edge()->data.branch_length) {
                LOG_INFO << "Tree contains placement with distal_length > branch_length "
                         << "at node '" << it_r.Node()->data.name << "'.";
            }

            distance += it_p->pendant_length / totalmass_r;
            edge_balance.emplace(it_p->distal_length, -1.0 / totalmass_r);
        }

        // start the EMD with the mass that is left over from the subtrees...
        double cur_pos  = it_l.Edge()->data.branch_length;
        double cur_mass = mass_s;

        // ... and move it along the branch, balancing it with the placements found on the branches.
        // this is basically a standard EMD calculation along the branch.
        std::multimap<double, double>::reverse_iterator rit;
        for (rit = edge_balance.rbegin(); rit != edge_balance.rend(); ++rit) {
            distance += std::abs(cur_mass) * (cur_pos - rit->first);
            cur_mass += rit->second;
            cur_pos   = rit->first;
        }

        // finally, move the rest to the end of the branch and store its mass in balance[],
        // so that it can be used for the nodes further up in the tree.
        distance += std::abs(cur_mass) * cur_pos;
        balance[it_l.Node()] = cur_mass;
    }

    // check whether we are done with both trees.
    if (it_l != lhs.tree.EndPostorder() || it_r != rhs.tree.EndPostorder()) {
        LOG_WARN << "Inconsistent reference trees in EMD calculation.";
        return -1.0;
    }

    return distance;
}

/**
 * @brief Calculate the Center of Gravity of the placements on a tree.
 */
void Placements::COG()
{
    // store a balance of mass per link, so that each element contains the mass that lies
    // in the direction of this link
    std::unordered_map<PlacementTree::LinkType*, double> balance;

    // do a postorder traversal
    for (
        PlacementTree::IteratorPostorder it = this->tree.BeginPostorder();
        it != this->tree.EndPostorder();
        ++it
    ) {
        // node does not have a corresponding edge (eg the root)
        if (!it.Edge()) {
            continue;
        }

        double mass = 0.0;

        // add up the masses from children
        PlacementTree::LinkType* link = it.Link()->Next();
        while (link != it.Link()) {
            // we do postorder traversal, so we have seen the child links of the current node,
            // which means, they should be in the balance list already.
            assert(balance.count(link));

            mass += balance[link] * it.Edge()->data.branch_length;
            link  = link->Next();
        }

        // add up the masses of placements on the current branch
        for (
            PlacementEdgeData::IteratorPlacements it_p = it.Edge()->data.BeginPlacements();
            it_p != it.Edge()->data.EndPlacements();
            ++it_p
        ) {
            mass += it_p->pendant_length + it_p->distal_length;
        }

        assert(balance.count(it.Link()->Outer()) == 0);
        balance[it.Link()->Outer()] = mass;
    }

    PlacementTree::LinkType* p_prev = tree.RootLink();
    PlacementTree::LinkType* p_link = tree.RootLink();
    double                   p_mass = -1.0;
    bool                     found  = false;
    while (!found) {
        LOG_DBG1 << "a " << p_link->Node()->data.name;
        p_mass = -1.0;
        for (
            PlacementTree::NodeType::IteratorLinks it_l = p_link->Node()->BeginLinks();
            it_l != p_link->Node()->EndLinks();
            ++it_l
        ) {
            LOG_DBG2 << it_l.Node()->data.name << " " << balance[it_l.Link()];
            if (balance[it_l.Link()] > p_mass) {
                p_link = it_l.Link();
            }
        }
        LOG_DBG1 << "b " << p_link->Node()->data.name;
        p_link = p_link->Outer();
        if (p_link == p_prev) {
            found = true;
            break;
        }
        p_prev = p_link;
    }

    //~ PlacementTree::LinkType* link = tree.RootLink();
    //~ while (link != it.Link()) {
        //~ // we do postorder traversal, so we have seen the child links of the current node,
        //~ // which means, they should be in the balance list already.
        //~ assert(balance.count(link));
//~
        //~ mass += balance[link] * it.Edge()->data.branch_length;
        //~ link = link->Next();
    //~ }

    for (auto pair : balance) {
        LOG_DBG1 << pair.first->Node()->data.name << ": " << pair.second << "\n";
        //~ distance += std::abs(pair.second);
    }
}

double Placements::Variance()
{
    Matrix<double>* distances = tree.NodeDistanceMatrix();
    double variance = 0.0;

    for (Pquery* pqry_a : pqueries) {
        for (Pquery::Placement& place_a : pqry_a->placements) {
            //~ p.distal_length;

            for (Pquery* pqry_b : pqueries) {
                for (Pquery::Placement& place_b : pqry_b->placements) {
                    //~ p.distal_length;
                    LOG_DBG << place_a.distal_length << " " << place_b.distal_length;
                }
            }

        }
    }

    LOG_DBG << distances->Dump();
    LOG_DBG << "Variance: " << variance;
    delete distances;
    return 0.0;
}

// =============================================================================
//     Dump and Debug
// =============================================================================

/**
 * @brief Returns a list of all Pqueries with their %Placements and Names.
 */
std::string Placements::Dump()
{
    std::ostringstream out;
    for (Pquery* pqry : pqueries) {
        for (Pquery::Name& n : pqry->names) {
            out << n.name;
            if (n.multiplicity != 0.0) {
                out << " (" << n.multiplicity << ")";
            }
            out << "\n";
        }
        for (Pquery::Placement& p : pqry->placements) {
            out << p.edge_num << ": ";
            if (p.likelihood != 0.0 || p.like_weight_ratio != 0.0) {
                out << p.likelihood << "|" << p.like_weight_ratio << " ";
            }
            if (p.parsimony != 0.0) {
                out << p.parsimony << " ";
            }
            out << p.distal_length << "|" << p.pendant_length << "\n";
        }
        out << "\n";
    }
    return out.str();
}

/**
 * @brief Validates the integrity of the data in this Placement object.
 */
bool Placements::Validate()
{
    // check tree
    if (!tree.Validate()) {
        LOG_INFO << "Invalid placement tree.";
        return false;
    }

    // check pqueries
    for (Pquery* pqry : pqueries) {
        double ratio_sum = 0.0;
        for (Pquery::Placement& p : pqry->placements) {
            // check edge references
            if (edge_num_map.count(p.edge_num) != 1) {
                LOG_INFO << "Invalid edge number '" << p.edge_num << "' in placement.";
                return false;
            }
            bool found_pqry_on_edge = false;
            PlacementTree::EdgeType* edge = edge_num_map[p.edge_num];
            for (Pquery* edge_pqry : edge->data.pqueries) {
                if (edge_pqry == pqry) {
                    found_pqry_on_edge = true;
                }
            }
            if (!found_pqry_on_edge) {
                LOG_INFO << "Invalid placement that refers to edge_num " << p.edge_num << ", but "
                         << "this edge does not have a reference to the pquery.";
                return false;
            }

            // check numerical values
            if (p.like_weight_ratio < 0.0 || p.like_weight_ratio > 1.0) {
                LOG_INFO << "Invalid placement with like_weight_ratio not in [0.0, 1.0].";
                return false;
            }
            if (p.pendant_length < 0.0 || p.distal_length < 0.0) {
                LOG_INFO << "Invalid placement with pendant_length or distal_length < 0.0.";
                return false;
            }
            if (p.distal_length > edge_num_map[p.edge_num]->data.branch_length) {
                LOG_INFO << "Invalid placement with distal_length > branch_length.";
                return false;
            }
            ratio_sum += p.like_weight_ratio;
        }
        if (ratio_sum > 1.0) {
            LOG_INFO << "Invalid pquery with sum of like_weight_ratio > 1.0.";
            return false;
        }
    }

    // check references from the edges to the pqueries
    for (
        PlacementTree::IteratorEdges it_e = tree.BeginEdges();
        it_e != tree.EndEdges();
        ++it_e
    ) {
        int found_edge_num = 0;
        for (
            PlacementEdgeData::IteratorPlacements it_p = (*it_e)->data.BeginPlacements();
            it_p != (*it_e)->data.EndPlacements();
            ++it_p
        ) {
            if (it_p->edge_num == (*it_e)->data.edge_num) {
                ++found_edge_num;
            }
        }
        if ((*it_e)->data.pqueries.size() > 0 && found_edge_num == 0) {
            LOG_INFO << "The edge with number " << (*it_e)->data.edge_num << " refers to no pquery "
                     << "with a placement that has this edge number.";
            return false;
        }
        if (found_edge_num > 1) {
            LOG_INFO << "The edge with number " << (*it_e)->data.edge_num << " refers to a pquery "
                     << "more than once.";
            return false;
        }
    }

    return true;
}

} // namespace genesis
