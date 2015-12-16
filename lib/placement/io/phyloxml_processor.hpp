#ifndef GENESIS_PLACEMENT_IO_PHYLOXML_PROCESSOR_H_
#define GENESIS_PLACEMENT_IO_PHYLOXML_PROCESSOR_H_

/**
 * @brief Header of Placement Tree Phyloxml Processor class.
 *
 * @file
 * @ingroup placement
 */

#include "placement/placement_tree.hpp"
#include "tree/default/phyloxml_mixin.hpp"
#include "tree/io/phyloxml/processor.hpp"

namespace genesis {

// =================================================================================================
//     Placement Tree Phyloxml Processor
// =================================================================================================

typedef DefaultTreePhyloxmlMixin< PhyloxmlProcessor< PlacementTree > >
    PlacementTreePhyloxmlProcessor;

} // namespace genesis

#endif // include guard