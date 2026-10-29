#include "Player.h"

#include <algorithm>
#include <sstream>

#include "Utils.h"

namespace {
const int kThresholds[Player::MAX_LEVEL] = {0, 100, 250, 500, 800, 1200};
const char* const kTitles[Player::MAX_LEVEL] = {"Beginner",      "C Learner",   "Code Explorer",
                                                "C++ Developer", "Code Master", "Programming Pro"};
}  // namespace

Player::Player() : name_("Player") {}
Player::Player(const std::string& name) : name_(name) {}

int Player::xpForLevel(int level) {
    level = std::max(1, std::min(level, MAX_LEVEL));
    return kThresholds[level - 1];
}

std::string Player::titleForLevel(int level) {
    level = std::max(1, std::min(level, MAX_LEVEL));
    return kTitles[level - 1];
}

int Player::level() const {
    int lvl = 1;
    for (int i = 1; i < MAX_LEVEL; ++i)
        if (xp_ >= kThresholds[i]) lvl = i + 1;
    return lvl;
}

int Player::xpToNext() const {
    const int lvl = level();
    return lvl >= MAX_LEVEL ? 0 : kThresholds[lvl] - xp_;
}

int Player::addXp(int amount) {
    if (amount <= 0) return 0;
    const int before = level();
    xp_ += amount;
    return level() - before;
}

bool Player::spendCoins(int amount) {
    if (coins_ < amount) return false;
    coins_ -= amount;
    return true;
}

bool Player::useHintToken() {
    if (hintTokens_ <= 0) return false;
    --hintTokens_;
    return true;
}

int Player::consumeExtraLives() {
    const int n = extraLives_;
    extraLives_ = 0;
    return n;
}

bool Player::hasBadge(const std::string& badge) const {
    return std::find(badges_.begin(), badges_.end(), badge) != badges_.end();
}

bool Player::addBadge(const std::string& badge) {
    if (hasBadge(badge)) return false;
    badges_.push_back(badge);
    return true;
}

int Player::topicCorrect(const std::string& topic) const {
    const auto it = topicCorrect_.find(topic);
    return it == topicCorrect_.end() ? 0 : it->second;
}

void Player::recordQuiz(int score, int correct, int streak, int fastAnswers, bool completed) {
    totalScore_ += score;
    highScore_ = std::max(highScore_, score);
    totalCorrect_ += correct;
    bestStreak_ = std::max(bestStreak_, streak);
    fastAnswers_ += fastAnswers;
    if (completed) ++quizzes_;
}

const std::vector<BadgeInfo>& Player::badgeCatalog() {
    static const std::vector<BadgeInfo> catalog = {
        {"First Step", "Complete your first quiz"},
        {"On Fire", "Answer 7 questions correctly in a row"},
        {"Sharpshooter", "Answer 50 questions correctly in total"},
        {"Perfect", "Get a perfect score in a quiz"},
        {"Speed Coder", "Answer 10 questions in 5 seconds or less"},
        {"Pointer Master", "Pass the Pointer Challenge"},
        {"Code Master", "Reach Level 5"},
        {"Boss Slayer", "Defeat the C++ OOP Boss"},
        {"Expert Challenger", "Beat the Expert Challenge"},
        {"Lucky Draw", "Win it as a Mystery Reward"},
    };
    return catalog;
}

const std::vector<std::string>& Player::journeyTopics() {
    static const std::vector<std::string> topics = {
        "C Basics", "Conditions and Loops", "Arrays and Strings", "Functions",
        "Pointers", "C++ Basics",           "Object-Oriented Programming", "Advanced C++"};
    return topics;
}

// Line format (15 fields, '|' separated):
// name|xp|coins|hintTokens|extraLives|quizzes|totalScore|highScore|totalCorrect|bestStreak|
// fastAnswers|bosses|doubleXp|badge,badge,...|topic:count;topic:count;...
std::string Player::serialize() const {
    std::ostringstream o;
    o << name_ << '|' << xp_ << '|' << coins_ << '|' << hintTokens_ << '|' << extraLives_ << '|'
      << quizzes_ << '|' << totalScore_ << '|' << highScore_ << '|' << totalCorrect_ << '|'
      << bestStreak_ << '|' << fastAnswers_ << '|' << bosses_ << '|' << (doubleXp_ ? 1 : 0) << '|'
      << util::join(badges_, ",") << '|';
    bool first = true;
    for (const auto& kv : topicCorrect_) {
        if (!first) o << ';';
        o << kv.first << ':' << kv.second;
        first = false;
    }
    return o.str();
}

bool Player::deserialize(const std::string& line, Player& out) {
    const std::string t = util::trim(line);
    if (t.empty() || t[0] == '#') return false;
    const std::vector<std::string> f = util::split(t, '|');
    if (f.size() < 15 || f[0].empty()) return false;

    try {
        Player p(f[0]);
        p.xp_ = std::stoi(f[1]);
        p.coins_ = std::stoi(f[2]);
        p.hintTokens_ = std::stoi(f[3]);
        p.extraLives_ = std::stoi(f[4]);
        p.quizzes_ = std::stoi(f[5]);
        p.totalScore_ = std::stoi(f[6]);
        p.highScore_ = std::stoi(f[7]);
        p.totalCorrect_ = std::stoi(f[8]);
        p.bestStreak_ = std::stoi(f[9]);
        p.fastAnswers_ = std::stoi(f[10]);
        p.bosses_ = std::stoi(f[11]);
        p.doubleXp_ = (f[12] == "1");
        if (!f[13].empty()) p.badges_ = util::split(f[13], ',');
        if (!f[14].empty()) {
            for (const std::string& pair : util::split(f[14], ';')) {
                const std::size_t colon = pair.rfind(':');
                if (colon == std::string::npos) continue;
                p.topicCorrect_[pair.substr(0, colon)] = std::stoi(pair.substr(colon + 1));
            }
        }
        out = p;
        return true;
    } catch (...) {
        return false;
    }
}
