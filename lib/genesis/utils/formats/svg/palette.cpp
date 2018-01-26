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
 * @ingroup utils
 */

#include "genesis/utils/formats/svg/palette.hpp"

#include "genesis/utils/formats/svg/attributes.hpp"
#include "genesis/utils/formats/svg/helper.hpp"
#include "genesis/utils/formats/svg/object.hpp"
#include "genesis/utils/formats/svg/shapes.hpp"
#include "genesis/utils/formats/svg/text.hpp"
#include "genesis/utils/text/string.hpp"
#include "genesis/utils/tools/color/functions.hpp"
#include "genesis/utils/tools/tickmarks.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <stdexcept>

namespace genesis {
namespace utils {

// =================================================================================================
//     Svg Color Palette
// =================================================================================================

std::pair<SvgGradientLinear, SvgGroup> SvgPalette::make(
    ColorMap const& map,
    ColorNormalization const& norm,
    std::string const& id
) const {

    if( map.palette().size() < 2 ) {
        throw std::runtime_error(
            "Cannot make SvgPalette with a ColorMap of less than two colors."
        );
    }
    if( ! norm.range_check() ) {
        throw std::runtime_error(
            "Invaid ColorNormalization settings."
        );
    }

    // Use a gradient ID with randomness so that we get a different one for each palette.
    std::string const gradient_id = ( id.empty()
        ? "PaletteGradient_" + std::to_string( std::rand() )
        : id
    );

    // Depending on the orientation, set gradient points.
    SvgPoint point_1;
    SvgPoint point_2;
    switch( direction ) {
        case Direction::kBottomToTop: {
            point_1 = SvgPoint( 0.0, 1.0 );
            point_2 = SvgPoint( 0.0, 0.0 );
            break;
        }
        case Direction::kTopToBottom: {
            point_1 = SvgPoint( 0.0, 0.0 );
            point_2 = SvgPoint( 0.0, 1.0 );
            break;
        }
        case Direction::kLeftToRight: {
            point_1 = SvgPoint( 0.0, 0.0 );
            point_2 = SvgPoint( 1.0, 0.0 );
            break;
        }
        case Direction::kRightToLeft: {
            point_1 = SvgPoint( 1.0, 0.0 );
            point_2 = SvgPoint( 0.0, 0.0 );
            break;
        }
        default: {
            throw std::runtime_error( "Invalid SvgPalette direction." );
        }
    }

    // Fill gradient with the colors, add it to a group as a colored rect.
    auto grad = SvgGradientLinear( gradient_id, point_1, point_2 );
    for( auto const& g : norm.gradient( map ) ) {
        if( g.first < 0.0 || g.first > 1.0 ) {
            throw std::runtime_error( "Color Normalization gradient out of [ 0.0, 1.0 ]" );
        }
        grad.add_stop({ g.first, g.second });
    }

    // Make group
    SvgGroup group;
    group << SvgRect(
        0.0, 0.0, width, height,
        SvgStroke(),
        // SvgStroke( SvgStroke::Type::kNone ),
        SvgFill( gradient_id )
    );

    // Helper function to make a tick mark with line and text
    // at a relative position [ 0.0, 1.0 ] along the rect.
    auto make_tick = [&]( double rel_pos, std::string label ){
        assert( 0.0 <= rel_pos && rel_pos <= 1.0 );

        // Get positions for needed elements.
        double v = -1.0;
        double h = -1.0;
        switch( direction ) {
            case Direction::kBottomToTop: {
                v = height - ( rel_pos * height );
                break;
            }
            case Direction::kTopToBottom: {
                v = rel_pos * height;
                break;
            }
            case Direction::kLeftToRight: {
                h = rel_pos * width;
                break;
            }
            case Direction::kRightToLeft: {
                h = width - ( rel_pos * width );
                break;
            }
            default: {
                throw std::runtime_error( "Invalid SvgPalette direction." );
            }
        }

        // Set elements.
        SvgPoint line1_p1;
        SvgPoint line1_p2;
        SvgPoint line2_p1;
        SvgPoint line2_p2;
        SvgPoint text_p;
        switch( direction ) {
            case Direction::kTopToBottom:
            case Direction::kBottomToTop:
            {
                assert( v > -1.0 );
                line1_p1 = SvgPoint( 0.0, v );
                line1_p2 = SvgPoint( width * 0.15, v );
                line2_p1 = SvgPoint( width * 0.85, v );
                line2_p2 = SvgPoint( width, v );
                text_p  = SvgPoint( width * 1.05, v );
                break;
            }
            case Direction::kLeftToRight:
            case Direction::kRightToLeft:
            {
                assert( h > -1.0 );
                line1_p1 = SvgPoint( h, 0.0 );
                line1_p2 = SvgPoint( h, height * 0.15 );
                line2_p1 = SvgPoint( h, height * 0.85 );
                line2_p2 = SvgPoint( h, height );
                text_p  = SvgPoint( h, height * 1.05 );
                break;
            }
            default: {
                throw std::runtime_error( "Invalid SvgPalette direction." );
            }
        }

        // Draw lines and text. Lines only for inners, as we already have a box around the scale.
        if( rel_pos != 0.0 && rel_pos != 1.0 ) {
            group << SvgLine( line1_p1, line1_p2 );
            group << SvgLine( line2_p1, line2_p2 );
        }
        if( with_labels ) {
            if( rel_pos == 1.0 && map.clip_over() ) {
                label = "≥ " + label;
            }
            if( rel_pos == 0.0 && map.clip_under() ) {
                label = "≤ " + label;
            }
            auto text_s = SvgText( label, text_p );
            // text_s.dominant_baseline = SvgText::DominantBaseline::kMiddle;
            // text_s.alignment_baseline = SvgText::AlignmentBaseline::kMiddle;
            // text_s.dy = "0.33em";
            group << text_s;
        }
    };

    // Make tickmarks and labels.
    if( with_tickmarks ) {
        for( auto const& tick : norm.tickmarks( num_ticks ) ) {
            if( tick.first < 0.0 || tick.first > 1.0 ) {
                throw std::runtime_error( "Color Normalization tickmark out of [ 0.0, 1.0 ]" );
            }
            make_tick( tick.first, tick.second );
        }
    }

    return { grad, group };
}

} // namespace utils
} // namespace genesis