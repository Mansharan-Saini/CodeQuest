#include "Leaderboard.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

#include "Utils.h"

Leaderboard::Leaderboard(std::string filePath) : filePath_(std::move(filePath)) {}

bool Leaderboard::load() {
    players_.clear();
    std::ifstream in(filePath_);
    if (!in) return false;  // first run: no file yet
    std::string line;
    while (std::getline(in, line)) {
        Player p;
        if (Player::deserialize(line, p)) players_.push_back(p);
    }
    return true;
}

bool Leaderboard::save() const {
    std::ofstream out(filePath_, std::ios::trunc);
    if (!out) return false;
    out << "# CodeQuest players - managed by the game, edit with care\n";
    for (const Player& p : players_) out << p.serialize() << '\n';
    return static_cast<bool>(out);
}

Player* Leaderboard::find(const std::string& name) {
    const std::string key = util::toLower(name);
    for (Player& p : players_)
        if (util::toLower(p.name()) == key) return &p;
    return nullptr;
}

Player& Leaderboard::create(const std::string& name) {
    players_.emplace_back(name);
    return players_.back();
}

void Leaderboard::display(std::size_t top) const {
    util::banner("LEADERBOARD");
    if (players_.empty()) {
        std::cout << "No players yet. Be the first!\n";
        return;
    }

    std::vector<const Player*> ranked;
    for (const Player& p : players_) ranked.push_back(&p);
    std::sort(ranked.begin(), ranked.end(), [](const Player* a, const Player* b) {
        if (a->xp() != b->xp()) return a->xp() > b->xp();
        if (a->highScore() != b->highScore()) return a->highScore() > b->highScore();
        return a->name() < b->name();
    });

    std::ostringstream header;
    header << std::left << std::setw(5) << "Rank" << std::setw(16) << "Player" << std::setw(7)
           << "Level" << std::setw(8) << "XP" << std::setw(11) << "High Score"
           << "Achievement\n";
    std::cout << util::paint(header.str(), util::Tone::Cyan);
    std::cout << util::paint(util::repeat('-', 66), util::Tone::Muted) << "\n";
    const std::size_t n = std::min(top, ranked.size());
    for (std::size_t i = 0; i < n; ++i) {
        const Player& p = *ranked[i];
        const std::string rank = std::to_string(i + 1);
        const std::string score = std::to_string(p.xp());
        std::cout << util::paint(rank + std::string(rank.size() < 5 ? 5 - rank.size() : 0, ' '),
                                 i == 0 ? util::Tone::Yellow : util::Tone::Muted)
                  << std::left << std::setw(16) << p.name().substr(0, 15)
                  << std::setw(7) << p.level()
                  << util::paint(score + std::string(score.size() < 8 ? 8 - score.size() : 0, ' '),
                                 util::Tone::Cyan)
                  << std::setw(11) << p.highScore()
                  << util::paint(p.topBadge(), util::Tone::Magenta) << "\n";
    }
}
