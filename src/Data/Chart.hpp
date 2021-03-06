#pragma once

#include <cstddef>
#include <set>

#include <memon/memon.hpp>
#include <SFML/System/Time.hpp>

#include "../Input/Buttons.hpp"
#include "Note.hpp"
#include "TimeBounds.hpp"

namespace Data {
    struct Chart {
        Chart(const stepland::memon& memon, const std::string& difficulty);
        int level;
        std::set<Note> notes;
        std::size_t resolution;
        // get the time at which the very last scorable event happens
        // (i.e. note tap or long note release)
        sf::Time get_last_event_timing() const;
        // get the time interval covered by the notes, including offset zero
        TimeBounds get_time_bounds_from_notes() const;
    };

    Input::Button convert_memon_tail(Input::Button note, unsigned int tail_position);
}