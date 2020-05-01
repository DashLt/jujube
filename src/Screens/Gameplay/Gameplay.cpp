#include "Gameplay.hpp"

#include <chrono>
#include <optional>
#include <thread>

#include <imgui/imgui.h>
#include <imgui-sfml/imgui-SFML.h>

#include "../../Input/Buttons.hpp"

namespace Gameplay {
    Screen::Screen(const Data::SongDifficulty& t_song_selection, ScreenResources& t_resources) :
        song_selection(t_song_selection),
        chart(*t_song_selection.song.get_chart(t_song_selection.difficulty)),
        HoldsResources(t_resources),
        marker(t_resources.shared.get_selected_marker()),
        notes(t_song_selection.song.get_chart(t_song_selection.difficulty)->notes.size())
    {
        auto it = chart.notes.begin();
        std::size_t i = 0;
        while (it != chart.notes.end()) {
            notes[i] = GradedNote{*it};
            ++it;
            ++i;
        }
        note_cursor = notes.begin();
        std::variant<PreciseMusic, Silence> music;
        auto music_path = song_selection.song.full_audio_path();
        if (music_path) {
            music.emplace<PreciseMusic>(music_path->string());
        } else {
            music.emplace<Silence>(chart.get_duration_based_on_notes());
        }
        getPlayingOffset = std::visit(GetPlayingOffsetVisitor{}, music);
        getStatus = std::visit(GetStatusVisitor{}, music);
    }

    Data::Score Screen::play_chart(sf::RenderWindow& window) {
        std::thread render_thread(&Screen::render, this, std::ref(window));
        while ((not song_finished) and window.isOpen()) {
            auto music_time = getPlayingOffset();
            sf::Event event;
            while (window.pollEvent(event)) {
                ImGui::SFML::ProcessEvent(event);
                switch (event.type) {
                case sf::Event::KeyPressed:
                    handle_raw_event(event.key.code, music_time);
                    break;
                case sf::Event::JoystickButtonPressed:
                    handle_raw_event(event.joystickButton, music_time);
                    break;
                case sf::Event::MouseButtonPressed:
                    handle_mouse_click(event.mouseButton, music_time);
                    break;
                case sf::Event::Closed:
                    window.close();
                    break;
                case sf::Event::Resized:
                    // update the view to the new size of the window
                    window.setView(sf::View({0, 0, static_cast<float>(event.size.width), static_cast<float>(event.size.height)}));
                    break;
                default:
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void Screen::render(sf::RenderWindow& window) {
        window.setFramerateLimit(60);
        ImGui::SFML::Init(window);
        sf::Clock imguiClock;
        while ((not song_finished) and window.isOpen()) {
            ImGui::SFML::Update(window, imguiClock.restart());
            auto music_time = getPlayingOffset();
            update_note_cursor(music_time);
            window.clear(sf::Color(7, 23, 53));
            for (auto it = note_cursor.load(); it != notes.end(); ++it) {
                auto note = it->load();
                std::optional<sf::Sprite> sprite;
                if (note.timed_judgment) {
                    sprite = marker.get_sprite(
                        judgement_to_animation(note.timed_judgment->judgement),
                        music_time-note.timing+note.timed_judgment->delta
                    );
                } else {
                    sprite = marker.get_sprite(
                        Resources::MarkerAnimation::APPROACH,
                        music_time-note.timing
                    );
                }
                if (sprite) {
                    auto rect = sprite->getLocalBounds();
                    sprite->setScale(get_panel_size()/rect.width, get_panel_size()/rect.height);
                    auto pos = Input::button_to_coords(note.position);
                    sprite->setPosition(
                        get_ribbon_x()+get_panel_step()*pos.x,
                        get_ribbon_y()+get_panel_step()*pos.y
                    );
                    window.draw(*sprite);
                }
            }
            window.draw(shared.button_highlight);
            window.draw(shared.black_frame);
            draw_debug();
            ImGui::SFML::Render(window);
            window.display();
        }
        ImGui::SFML::Shutdown();
    }

    void Gameplay::Screen::handle_mouse_click(const sf::Event::MouseButtonEvent& mouse_button_event, const sf::Time& music_time) {
        if (mouse_button_event.button != sf::Mouse::Left) {
            return;
        }

        sf::Vector2i mouse_position{mouse_button_event.x, mouse_button_event.y};
        sf::Vector2i ribbon_origin{
            static_cast<int>(get_ribbon_x()),
            static_cast<int>(get_ribbon_y())
        };
        auto panels_area_size = static_cast<int>(get_panel_size() * 4.f + get_panel_spacing() * 3.f);
        sf::IntRect panels_area{ribbon_origin, sf::Vector2i{panels_area_size, panels_area_size}};
        if (not panels_area.contains(mouse_position)) {
            return;
        }

        sf::Vector2i relative_mouse_pos = mouse_position - ribbon_origin;
        int clicked_panel_index = (
            (relative_mouse_pos.x / (panels_area_size/4))
            + 4 * (relative_mouse_pos.y / (panels_area_size/4))
        );
        if (clicked_panel_index < 0) {
            return;
        }
        auto button = Input::index_to_button(static_cast<std::size_t>(clicked_panel_index));
        if (button) {
            handle_button(*button, music_time);
        }
    }

    void Screen::handle_raw_event(const Input::Event& event, const sf::Time& music_time) {
        auto button = preferences.key_mapping.key_to_button(event);
        if (button) {
            handle_button(*button, music_time);
        }
    }

    void Screen::handle_button(const Input::Button& button, const sf::Time& music_time) {
        shared.button_highlight.button_pressed(button);
        // Is the music even playing ?
        if (getStatus() == sf::SoundSource::Playing) {
            update_note_cursor(music_time);
            for (auto it = note_cursor.load(); it != notes.end(); ++it) {
                auto note = it->load();
                // is the note still visible ?
                if (note.timing > music_time + sf::seconds(16.f/30.f)) {
                    break;
                }
                // is it even the right button ?
                if (note.position != button) {
                    continue;
                }
                // has it already been graded ?
                if (note.timed_judgment) {
                    continue;
                }
                it->store(GradedNote{note, music_time-note.timing});
                break;
            }
        }
    }
    void Screen::update_note_cursor(const sf::Time& music_time) {
        for (auto it = note_cursor.load(); it != notes.end(); ++it) {
            if (it->load().timing >= music_time - sf::seconds(16.f/30.f)) {
                note_cursor = it;
                break;
            } else {
                auto note = it->load();
                note.timed_judgment.emplace(sf::Time::Zero, Judgement::Miss);
                it->store(note);
            }
        }
    }
}
