#ifndef PLAYER_H
#define PLAYER_H

#include <map>
#include <string>
#include <vector>

struct BadgeInfo {
    std::string name;
    std::string how;
};

// Stores a player's profile: name, XP, level, coins, tokens, badges and statistics.
// The level is always derived from XP, so the two can never disagree.
class Player {
public:
    static constexpr int BASE_LIVES = 3;
    static constexpr int MAX_LEVEL = 6;

    Player();
    explicit Player(const std::string& name);

    // ---- read access ----
    const std::string& name() const { return name_; }
    int xp() const { return xp_; }
    int level() const;
    std::string title() const { return titleForLevel(level()); }
    int xpToNext() const;  // 0 at max level
    int coins() const { return coins_; }
    int hintTokens() const { return hintTokens_; }
    int extraLives() const { return extraLives_; }
    bool hasDoubleXp() const { return doubleXp_; }
    int quizzesCompleted() const { return quizzes_; }
    int totalScore() const { return totalScore_; }
    int highScore() const { return highScore_; }
    int totalCorrect() const { return totalCorrect_; }
    int bestStreak() const { return bestStreak_; }
    int fastAnswers() const { return fastAnswers_; }
    int bossesDefeated() const { return bosses_; }
    const std::vector<std::string>& badges() const { return badges_; }
    bool hasBadge(const std::string& badge) const;
    std::string topBadge() const { return badges_.empty() ? "-" : badges_.back(); }
    int topicCorrect(const std::string& topic) const;

    // ---- unlockable content ----
    bool canPlayMixed() const { return level() >= 2; }
    bool canPlayHard() const { return level() >= 3; }
    bool canFightBoss() const { return level() >= 4; }
    bool canPlayExpert() const { return level() >= 5; }

    // ---- changing state ----
    int addXp(int amount);  // returns how many levels were gained
    void addCoins(int amount) { coins_ += amount; }
    bool spendCoins(int amount);
    void addHintTokens(int n) { hintTokens_ += n; }
    bool useHintToken();
    void addExtraLife(int n = 1) { extraLives_ += n; }
    int consumeExtraLives();
    void setDoubleXp(bool on) { doubleXp_ = on; }
    bool addBadge(const std::string& badge);  // false if already owned
    void recordTopicCorrect(const std::string& topic) { ++topicCorrect_[topic]; }
    void recordBossWin() { ++bosses_; }
    void recordQuiz(int score, int correct, int streak, int fastAnswers, bool completed);

    // ---- static tables ----
    static int xpForLevel(int level);
    static std::string titleForLevel(int level);
    static const std::vector<BadgeInfo>& badgeCatalog();
    static const std::vector<std::string>& journeyTopics();

    // ---- file handling (one line per player) ----
    std::string serialize() const;
    static bool deserialize(const std::string& line, Player& out);

private:
    std::string name_;
    int xp_ = 0;
    int coins_ = 0;
    int hintTokens_ = 0;
    int extraLives_ = 0;
    int quizzes_ = 0;
    int totalScore_ = 0;
    int highScore_ = 0;
    int totalCorrect_ = 0;
    int bestStreak_ = 0;
    int fastAnswers_ = 0;
    int bosses_ = 0;
    bool doubleXp_ = false;
    std::vector<std::string> badges_;
    std::map<std::string, int> topicCorrect_;
};

#endif
