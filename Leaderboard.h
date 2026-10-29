#ifndef LEADERBOARD_H
#define LEADERBOARD_H

#include <string>
#include <vector>

#include "Player.h"

// Keeps every saved player (players.txt) and shows them ranked by XP.
class Leaderboard {
public:
    explicit Leaderboard(std::string filePath);

    bool load();
    bool save() const;

    Player* find(const std::string& name);  // case-insensitive, nullptr if missing
    Player& create(const std::string& name);
    void display(std::size_t top = 10) const;

public:
    const std::vector<Player>& players() const { return players_; }
    std::vector<Player> snapshot() const { return players_; }

private:
    std::string filePath_;
    std::vector<Player> players_;
};

#endif
