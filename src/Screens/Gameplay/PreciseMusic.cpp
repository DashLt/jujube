#include "PreciseMusic.hpp"

#include <chrono>
#include <stdexcept>

namespace Gameplay {
    _PreciseMusic::_PreciseMusic(const std::string& path) {
        if (not this->openFromFile(path)) {
            throw std::invalid_argument("Could not open "+path);
        }
        position_update_watchdog = std::thread{&_PreciseMusic::position_update_watchdog_main, this};
    }

    void _PreciseMusic::position_update_watchdog_main() {
        sf::Time next_music_position = sf::Time::Zero;
        while (not should_stop_watchdog) {
            if (this->getStatus() == sf::Music::Playing) {
                next_music_position = this->getPlayingOffset();
                if (next_music_position != last_music_position) {
                    lag = lag_measurement.getElapsedTime()/2.f;
                    should_correct = true;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    _PreciseMusic::~_PreciseMusic() {
        should_stop_watchdog = true;
        position_update_watchdog.join();
    }

    sf::Time _PreciseMusic::getPrecisePlayingOffset() const {
        if (should_correct) {
            return last_music_position+last_position_update.getElapsedTime()-lag;
        } else {
            return this->getPlayingOffset();
        }
    }

    bool _PreciseMusic::onGetData(sf::SoundStream::Chunk& data) {
        if (first_onGetData_call) {
            first_onGetData_call = false;
            lag_measurement.restart();
        }
        bool continue_playback = sf::Music::onGetData(data);
        if (continue_playback) {
            last_music_position = getPlayingOffset();
            last_position_update.restart();
        }
        return continue_playback;
    }
}