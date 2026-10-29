#ifndef QUIZ_H
#define QUIZ_H

#include <functional>
#include <random>
#include <string>
#include <vector>

#include "Player.h"
#include "Question.h"

// Rules for one play session (a normal quiz, the Pointer Challenge, the Boss Battle, ...).
struct SessionConfig {
    std::string title = "Quiz";
    int questionCount = 10;
    int lives = Player::BASE_LIVES;
    int timeLimit = 30;      // seconds per question, 0 = no timer
    bool lifelines = true;   // 50-50, Hint, Skip
    int passMark = 0;        // minimum correct answers to win (0 = just survive)
    int bonusXp = 0;         // extra XP for winning
    std::string winBadge;    // badge awarded for winning
    bool guaranteedMystery = false;
};

// Controls the question bank, answering, timer, lives, lifelines, scoring, XP,
// levels, badges and rewards.
class Quiz {
public:
    explicit Quiz(const std::string& questionFile);

    bool loaded() const { return !bank_.empty(); }
    std::size_t size() const { return bank_.size(); }

    void playStandard(Player& p);
    void playPointerChallenge(Player& p);
    void playBoss(Player& p);
    void playExpert(Player& p);

private:
    std::vector<Question> bank_;
    std::mt19937 rng_;

    std::vector<Question> select(const std::function<bool(const Question&)>& pred) const;
    bool runSession(Player& p, const SessionConfig& cfg, std::vector<Question> questions);

    void grantXp(Player& p, int amount);
    void levelUp(Player& p, int newLevel);
    void awardBadge(Player& p, const std::string& badge);
    void mysteryReward(Player& p);
    int randomIn(int lo, int hi);
};

#endif
