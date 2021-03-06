#include <iostream>

#include <imgui-sfml/imgui-SFML.h>
#include <SFML/Graphics.hpp>
#include <whereami/whereami++.hpp>

#include "Data/Song.hpp"
#include "Data/Preferences.hpp"
#include "Drawables/GradedDensityGraph.hpp"
#include "Resources/Marker.hpp"
#include "Resources/SharedResources.hpp"
// #include "Data/Chart.hpp"
// #include "Data/Score.hpp"

#include "Screens/MusicSelect/Resources.hpp"
#include "Screens/MusicSelect/MusicSelect.hpp"
#include "Screens/Gameplay/Resources.hpp"
#include "Screens/Gameplay/Gameplay.hpp"
#include "Screens/Results/Results.hpp"
#if defined(__unix__) && defined(__linux__)
    #include <X11/Xlib.h>
#endif

int main(int, char const **) {

    #if defined(__unix__) && defined(__linux__)
        XInitThreads();
    #endif

    // Load prefs, music, markers
    const std::string jujube_path = whereami::executable_dir();
    Data::Preferences preferences{jujube_path};
    Data::SongList song_list{jujube_path};
    Resources::SharedResources shared_resources{preferences};
    MusicSelect::ScreenResources music_select_resources{shared_resources};
    if (shared_resources.markers.find(preferences.options.marker) == shared_resources.markers.end()) {
        preferences.options.marker = shared_resources.markers.begin()->first;
    }
    if (shared_resources.ln_markers.find(preferences.options.ln_marker) == shared_resources.ln_markers.end()) {
        preferences.options.ln_marker = shared_resources.ln_markers.begin()->first;
    }
    MusicSelect::Screen music_select{song_list, music_select_resources};
    
    Gameplay::ScreenResources gameplay_resources{shared_resources};
    Results::ScreenResources results_resources{shared_resources};

    // Create the window
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    sf::RenderWindow window{
        preferences.screen.video_mode,
        "jujube",
        preferences.screen.style == Data::DisplayStyle::Windowed ? sf::Style::Default : sf::Style::Fullscreen,
        settings
    };
    window.setFramerateLimit(60);
    ImGui::SFML::Init(window);

    while (window.isOpen()) {
        auto chart = music_select.select_chart(window);
        if (chart) {
            std::cout << "Selected Chart : " << chart->song.title << " [" << chart->difficulty << "]" << '\n';
        } else {
            std::cout << "Exited MusicSelect::Screen without selecting a chart" << '\n';
            break;
        }

        Gameplay::Screen gameplay{*chart, gameplay_resources};
        auto detailed_score = gameplay.play_chart(window);
        
        Results::Screen result_screen{detailed_score.gdg, *chart, detailed_score.score, results_resources};
        result_screen.display(window);
    }
    ImGui::SFML::Shutdown();
    return 0;
}
