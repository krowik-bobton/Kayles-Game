#ifndef GAME_SESSION_H
#define GAME_SESSION_H
#include <cstdint>
#include <map>
#include <vector>
#include <netinet/in.h>
#include <chrono>
#include <cstdlib>

#include "err.h"
#include "messages.h"

// Structure representing the game session (state, players, timeouts) for each active game.
struct game_session {
    bool has_player_b = false;

    std::chrono::steady_clock::time_point last_activity_a;
    std::chrono::steady_clock::time_point last_activity_b;
    std::chrono::steady_clock::time_point game_end_time;

    // Pointer to the game state
    game_state* state = nullptr;
    size_t state_size = 0;

    game_session() = default;

    ~game_session() {
        if (state != nullptr) {
            std::free(state);
        }
    }

    game_session(const game_session&) = delete;
    game_session& operator=(const game_session&) = delete;

    // Safe move constructor and move assignment operator to allow storing game_session in std::map without copying.
    game_session(game_session&& other) noexcept {
        has_player_b = other.has_player_b;
        
        state = other.state;
        state_size = other.state_size;
        
        other.state = nullptr;
        other.state_size = 0;
    }

    game_session& operator=(game_session&& other) noexcept {
        if (this != &other) {
            if (state) std::free(state);
            
            has_player_b = other.has_player_b;
            
            state = other.state;
            state_size = other.state_size;
            
            other.state = nullptr;
            other.state_size = 0;
        }
        return *this;
    }
};

#endif // GAME_SESSION_H