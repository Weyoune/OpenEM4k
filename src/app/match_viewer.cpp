// SPDX-License-Identifier: GPL-3.0-only
// em4k_match - viewer of the match scenes (docs/MATCH_SCENES.md).
//
//   em4k_match [--data <EM dir>] [--script 5.at] [--arena 0..4] [--home RGB] [--away RGB]
//              [--names <home> <away>] [--scorer <name>] [--size WxH] [--fullscreen]
//              [--shot out.png [--frame n[.f]] [--after seconds]]   one picture without a window
//              [--bench]                                            time per picture without a window
//
// Keys: Left / Right script, Space pause, , / . one tick back / on, I smooth motion and camera on / off, A arena (how
// full the stands are), + / - speed, F11 or F fullscreen, Esc quit.
#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "match/png.hpp"
#include "match/scene.hpp"

using namespace em4k;
namespace fs = std::filesystem;

namespace {

constexpr double kTickSeconds = 0.054925;  // one BIOS tick per script frame, as in the original
constexpr double kCelebration = 3.2;       // seconds the outcome of a goal stays, 1.4 for a miss

// The game data is not part of this repository: the TORE\ folder of an installed Eishockey Manager.
fs::path find_data(const char* given) {
    std::vector<fs::path> tries;
    if (given) tries.emplace_back(given);
    if (const char* env = std::getenv("OPENEM_DATA")) tries.emplace_back(env);
    for (const char* rel : {"EM", "data", ".", "../OpenEM/work/EM"}) tries.emplace_back(rel);
    for (const fs::path& p : tries)
        if (fs::exists(p / "TORE")) return p;
    return {};
}

std::vector<std::string> scripts(const fs::path& data) {
    std::vector<std::string> out;
    for (const auto& e : fs::directory_iterator(data / "TORE")) {
        const std::string name = e.path().filename().string();
        if (name != "anzahl" && name.find('.') != std::string::npos) out.push_back(name);
    }
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
        const int na = std::atoi(a.c_str()), nb = std::atoi(b.c_str());
        return na != nb ? na < nb : a < b;
    });
    return out;
}

void describe(MatchSceneInput& scene, const std::string& script) {
    const size_t dot = script.find('.');
    scene.script = script;
    scene.side = dot + 1 < script.size() && script[dot + 1] == 'a' ? 1 : 0;
    scene.goal = dot + 2 < script.size() && script[dot + 2] == 't';
}

}  // namespace

int main(int argc, char** argv) {
    const char* data_arg = nullptr;
    const char* shot = nullptr;
    std::string script = "5.at";
    double shot_frame = 38, shot_after = 0.45;
    int width = 1280, height = 720;
    bool fullscreen = false, bench = false, smooth_arg = true;
    MatchSceneInput scene;
    scene.arena = 3;
    scene.colour = {0x03C, 0xEEE};
    scene.names = {"EHC FREIBURG", "KÖLNER EC"};
    scene.goals = {3, 2};
    scene.info = "4500 ZUSCHAUER";
    scene.minute = 16;
    scene.scorer = "BERGER";
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        const auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--data") data_arg = next();
        else if (a == "--script") script = next();
        else if (a == "--arena") scene.arena = std::atoi(next());
        else if (a == "--home") scene.colour[0] = int(std::strtol(next(), nullptr, 16));
        else if (a == "--away") scene.colour[1] = int(std::strtol(next(), nullptr, 16));
        else if (a == "--names") scene.names[0] = next(), scene.names[1] = next();
        else if (a == "--scorer") scene.scorer = next();
        else if (a == "--size") std::sscanf(next(), "%dx%d", &width, &height);
        else if (a == "--fullscreen") fullscreen = true;
        else if (a == "--shot") shot = next();
        else if (a == "--frame") shot_frame = std::atof(next());
        else if (a == "--after") shot_after = std::atof(next());
        else if (a == "--bench") bench = true;
        else if (a == "--ticks") smooth_arg = false;
        else {
            std::fprintf(stderr, "usage: em4k_match [--data <EM dir>] [--script 5.at] [--arena 0..4] [--home RGB] [--away RGB]\n"
                                 "       [--names <home> <away>] [--scorer <name>] [--size WxH] [--fullscreen]\n"
                                 "       [--shot out.png [--frame n[.f]] [--after seconds]] [--bench]\n");
            return 2;
        }
    }
    const fs::path data = find_data(data_arg);
    if (data.empty()) {
        std::fprintf(stderr, "No game data found: the TORE folder of Eishockey Manager. Use --data <EM dir> or set OPENEM_DATA.\n");
        return 1;
    }
    try {
        MatchScene hd(data);
        describe(scene, script);
        const std::array<int, 2> goals = scene.goals;
        // the scene at a time: script ticks, then seconds of the outcome
        const auto at = [&](double tick, double after) {
            const int length = hd.length(scene.script);
            scene.frame = int(std::min(tick, double(length)));
            scene.final = tick >= length;
            scene.after = scene.final ? after : 0;
            scene.goals = goals;
            if (scene.final && scene.goal) scene.goals[size_t(scene.side)]++;
            return std::min(tick, double(length)) - scene.frame;
        };
        if (bench) {
            Surface picture(width, height);
            const int length = hd.length(script);
            for (int pass = 0; pass < 2; pass++) {
                const Uint64 start = SDL_GetPerformanceCounter();
                int pictures = 0;
                for (double t = 0; t < length; t += 0.3, pictures++) hd.render(picture, scene, at(t, 0), smooth_arg);
                const double ms = double(SDL_GetPerformanceCounter() - start) * 1000.0 / double(SDL_GetPerformanceFrequency());
                std::printf("%s %dx%d %s: %d pictures, %.2f ms each\n", script.c_str(), width, height, pass ? "warm" : "cold", pictures, ms / pictures);
            }
            const Uint64 start = SDL_GetPerformanceCounter();
            int pictures = 0;
            for (double s = 0; s < 2; s += 1 / 60.0, pictures++) hd.render(picture, scene, at(length, s), true);
            const double ms = double(SDL_GetPerformanceCounter() - start) * 1000.0 / double(SDL_GetPerformanceFrequency());
            std::printf("%s %dx%d outcome: %d pictures, %.2f ms each\n", script.c_str(), width, height, pictures, ms / pictures);
            return 0;
        }
        if (shot) {
            Surface picture(width, height);
            const double fraction = at(shot_frame, shot_after);
            hd.render(picture, scene, fraction, true);
            if (!write_png(shot, picture)) throw std::runtime_error("cannot write the picture");
            std::printf("%s frame %.2f of %d, %dx%d -> %s\n", script.c_str(), shot_frame, hd.length(script), width, height, shot);
            return 0;
        }

        std::vector<std::string> all = scripts(data);
        size_t current = size_t(std::find(all.begin(), all.end(), script) - all.begin());
        if (current >= all.size()) current = 0;
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* window = SDL_CreateWindow("OpenEM4k - match scene", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
        if (!window) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) throw std::runtime_error(SDL_GetError());
        SDL_Texture* texture = nullptr;
        Surface picture;

        double tick = 0, speed = 1, after = 0;
        bool paused = false, smooth = smooth_arg;
        Uint64 before = SDL_GetPerformanceCounter(), title_at = 0;
        int frames_drawn = 0;
        for (bool run = true; run;) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) run = false;
                if (ev.type != SDL_KEYDOWN) continue;
                const auto change = [&](size_t to) {
                    current = to % all.size();
                    tick = 0, after = 0;
                };
                switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE: run = false; break;
                    case SDLK_RIGHT: change(current + 1); break;
                    case SDLK_LEFT: change(current + all.size() - 1); break;
                    case SDLK_SPACE: paused = !paused; break;
                    case SDLK_PERIOD: paused = true, tick = std::floor(tick) + 1; break;
                    case SDLK_COMMA: paused = true, after = 0, tick = std::max(0.0, std::ceil(tick) - 1); break;
                    case SDLK_i: smooth = !smooth; break;
                    case SDLK_a: scene.arena = (scene.arena + 1) % 5; break;
                    case SDLK_PLUS: case SDLK_KP_PLUS: case SDLK_EQUALS: speed = std::min(4.0, speed * 1.25); break;
                    case SDLK_MINUS: case SDLK_KP_MINUS: speed = std::max(0.1, speed / 1.25); break;
                    case SDLK_F11: case SDLK_f:
                        fullscreen = !fullscreen;
                        SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        break;
                    default: break;
                }
            }
            const Uint64 now = SDL_GetPerformanceCounter();
            const double dt = std::min(0.25, double(now - before) / double(SDL_GetPerformanceFrequency()));
            before = now;
            describe(scene, all[current]);
            const int length = hd.length(scene.script);
            if (!paused) {
                if (tick < length) tick += dt / kTickSeconds * speed;
                else if ((after += dt * speed) > (scene.goal ? kCelebration : 1.4)) {  // the outcome stays a moment, then the next script
                    current = (current + 1) % all.size();
                    tick = 0, after = 0;
                    continue;
                }
            }
            tick = std::min(tick, double(length));
            const double fraction = at(tick, after);

            int out_w = 0, out_h = 0;
            SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
            if (out_w < 16 || out_h < 16) {
                SDL_Delay(50);
                continue;
            }
            if (picture.width != out_w || picture.height != out_h) {
                picture = Surface(out_w, out_h);
                if (texture) SDL_DestroyTexture(texture);
                texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, out_w, out_h);
            }
            hd.render(picture, scene, fraction, smooth);
            SDL_UpdateTexture(texture, nullptr, picture.pixels.data(), out_w * 4);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            frames_drawn++;
            if (now - title_at > SDL_GetPerformanceFrequency()) {
                const double seconds = double(now - title_at) / double(SDL_GetPerformanceFrequency());
                const std::string title = "OpenEM4k - match scene " + scene.script + "  " + std::to_string(out_w) + "x" + std::to_string(out_h) + "  " +
                                          std::to_string(int(frames_drawn / seconds + 0.5)) + " fps" + (smooth ? "" : "  [ticks]");
                SDL_SetWindowTitle(window, title.c_str());
                title_at = now, frames_drawn = 0;
            }
        }
        SDL_Quit();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
