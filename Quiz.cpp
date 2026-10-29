#include "Quiz.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "Utils.h"

namespace {

const char* const kOopTopic = "Object-Oriented Programming";
const char* const kPointerTopic = "Pointers";

const int kWrongPenalty = 5;      // negative marking
const double kFastSeconds = 5.0;  // "Speed Coder" threshold

std::string fmtSeconds(double s) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(1) << s << "s";
    return o.str();
}

void printQuestion(const Question& q, const std::vector<bool>& hidden) {
    std::cout << util::paint(q.text(), util::Tone::Bold) << "\n\n";
    for (std::size_t i = 0; i < q.options().size(); ++i) {
        if (hidden[i]) continue;
        std::cout << "  " << util::paint(std::string("[") + static_cast<char>('A' + i) + "]",
                                         util::Tone::Cyan)
                  << " " << q.options()[i] << "\n";
    }
    std::cout << "\n";
}

}  // namespace

Quiz::Quiz(const std::string& questionFile) : bank_(Question::loadFromFile(questionFile)) {
    std::random_device rd;
    rng_.seed(rd());
}

int Quiz::randomIn(int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng_);
}

std::vector<Question> Quiz::select(const std::function<bool(const Question&)>& pred) const {
    std::vector<Question> out;
    for (const Question& q : bank_)
        if (pred(q)) out.push_back(q);
    return out;
}

// ---------------------------------------------------------------- game modes

void Quiz::playStandard(Player& p) {
    util::banner("CHOOSE YOUR QUEST");
    std::cout << "Category:\n  1) C\n  2) C++\n  3) Mixed"
              << (p.canPlayMixed() ? "" : "   [locked - reach Level 2]") << "\n";
    const int cat = util::readInt("Choose category (1-3): ", 1, 3);
    if (cat == 3 && !p.canPlayMixed()) {
        std::cout << "Mixed mode is still locked. Reach Level 2 to unlock it!\n";
        return;
    }

    std::cout << "\nDifficulty:\n  1) Easy\n  2) Medium\n  3) Hard"
              << (p.canPlayHard() ? "" : "   [locked - reach Level 3]") << "\n";
    const int diff = util::readInt("Choose difficulty (1-3): ", 1, 3);
    if (diff == 3 && !p.canPlayHard()) {
        std::cout << "Hard Mode is still locked. Reach Level 3 to unlock it!\n";
        return;
    }

    auto inCategory = [cat](const Question& q) {
        return cat == 3 || (cat == 1 ? q.category() == Category::C : q.category() == Category::CPP);
    };
    std::vector<Question> pool =
        select([&](const Question& q) { return inCategory(q) && q.difficulty() == diff; });
    if (pool.size() < 5) {
        std::cout << "(Not many questions at that difficulty, using all difficulties for this category.)\n";
        pool = select(inCategory);
    }

    SessionConfig cfg;
    cfg.title = std::string(cat == 1 ? "C" : cat == 2 ? "C++" : "MIXED") + " QUIZ - " +
                util::toUpper(difficultyName(diff));
    runSession(p, cfg, pool);
}

void Quiz::playPointerChallenge(Player& p) {
    SessionConfig cfg;
    cfg.title = "POINTER CHALLENGE";
    cfg.questionCount = 5;
    cfg.passMark = 4;
    cfg.bonusXp = 30;
    cfg.winBadge = "Pointer Master";
    cfg.guaranteedMystery = true;
    std::cout << "\nAnswer at least 4 of 5 pointer questions correctly to earn the Pointer Master badge.\n";
    runSession(p, cfg, select([](const Question& q) { return q.topic() == kPointerTopic; }));
}

void Quiz::playBoss(Player& p) {
    if (!p.canFightBoss()) {
        std::cout << "The Boss Battle unlocks at Level 4 (C++ Developer).\n";
        return;
    }
    SessionConfig cfg;
    cfg.title = "BOSS BATTLE: THE C++ OOP BOSS";
    cfg.questionCount = 10;
    cfg.lives = Player::BASE_LIVES;
    cfg.timeLimit = 15;
    cfg.lifelines = false;
    cfg.bonusXp = p.bossesDefeated() == 0 ? 150 : 50;
    cfg.winBadge = "Boss Slayer";
    cfg.guaranteedMystery = true;
    std::cout << "\nThe OOP Boss throws 10 hard questions at you. 15 seconds each, no lifelines!\n";
    if (runSession(p, cfg, select([](const Question& q) {
                       return q.topic() == kOopTopic && q.difficulty() == 3;
                   })))
        p.recordBossWin();
}

void Quiz::playExpert(Player& p) {
    if (!p.canPlayExpert()) {
        std::cout << "The Expert Challenge unlocks at Level 5 (Code Master).\n";
        return;
    }
    SessionConfig cfg;
    cfg.title = "EXPERT CHALLENGE";
    cfg.questionCount = 15;
    cfg.lives = 2;
    cfg.timeLimit = 20;
    cfg.lifelines = false;
    cfg.bonusXp = 200;
    cfg.winBadge = "Expert Challenger";
    cfg.guaranteedMystery = true;
    std::cout << "\n15 hard questions from both languages, only 2 lives, 20 seconds each.\n";
    runSession(p, cfg, select([](const Question& q) { return q.difficulty() == 3; }));
}

// ------------------------------------------------------------- core game loop

bool Quiz::runSession(Player& p, const SessionConfig& cfg, std::vector<Question> qs) {
    if (qs.empty()) {
        std::cout << "\nNo questions available for this challenge.\n";
        return false;
    }
    std::shuffle(qs.begin(), qs.end(), rng_);
    if (static_cast<int>(qs.size()) > cfg.questionCount) qs.resize(cfg.questionCount);
    const int total = static_cast<int>(qs.size());

    int lives = cfg.lives + p.consumeExtraLives();
    const bool doubleXp = p.hasDoubleXp();
    if (doubleXp) p.setDoubleXp(false);

    int score = 0, xp = 0, correct = 0, wrong = 0, skipped = 0;
    int streak = 0, bestStreak = 0, fast = 0;
    bool usedLifeline = false, fiftyUsed = false, skipUsed = false, freeHintUsed = false;
    bool failed = false;

    util::banner(cfg.title);
    std::cout << util::paint("QUEST", util::Tone::Magenta) << "  " << total << " rounds"
              << "   " << util::paint("LIVES", util::Tone::Red) << "  " << lives
              << "   " << util::paint("TIMER", util::Tone::Yellow) << "  "
              << (cfg.timeLimit > 0 ? std::to_string(cfg.timeLimit) + "s" : "none") << "\n";
    if (cfg.lifelines)
        std::cout << util::paint("POWER-UPS", util::Tone::Cyan)
                  << "  F = 50-50   H = Hint   S = Skip\n";
    if (doubleXp)
        std::cout << util::paint("DOUBLE XP is active for this quest!", util::Tone::Yellow) << "\n";

    for (int i = 0; i < total && !failed; ++i) {
        Question& q = qs[i];
        q.shuffleOptions(rng_);
        std::vector<bool> hidden(q.options().size(), false);

        std::cout << "\n" << util::paint(util::repeat('-', 52), util::Tone::Muted) << "\n";
        std::cout << util::paint("ROUND " + std::to_string(i + 1) + "/" + std::to_string(total),
                                 util::Tone::Cyan)
                  << "  [" << categoryName(q.category()) << ", " << difficultyName(q.difficulty())
                  << "]   " << util::paint("LIVES", util::Tone::Red) << ": " << lives
                  << "   " << util::paint("STREAK", util::Tone::Yellow) << ": " << streak << "\n\n";
        printQuestion(q, hidden);

        const auto start = std::chrono::steady_clock::now();
        int answer = -1;
        bool skipThis = false;

        while (true) {
            std::string prompt = "Answer (A-D";
            if (cfg.lifelines) {
                if (!fiftyUsed) prompt += ", F=50-50";
                if ((!freeHintUsed ? 1 : 0) + p.hintTokens() > 0)
                    prompt += ", H=Hint";
                if (!skipUsed) prompt += ", S=Skip";
            }
            prompt += "): ";
            const std::string in = util::toUpper(util::readLine(prompt));

            if (in.size() == 1 && in[0] >= 'A' &&
                in[0] < static_cast<char>('A' + q.options().size())) {
                const int idx = in[0] - 'A';
                if (hidden[idx]) {
                    std::cout << "  That option was removed by 50-50. Pick another.\n";
                    continue;
                }
                answer = idx;
                break;
            }

            if (in == "F" || in == "H" || in == "S") {
                if (!cfg.lifelines) {
                    std::cout << "  Lifelines are disabled in this challenge.\n";
                    continue;
                }
                if (in == "F") {
                    if (fiftyUsed) {
                        std::cout << "  You already used 50-50 this quiz.\n";
                        continue;
                    }
                    for (int idx : q.pickEliminations(rng_)) hidden[idx] = true;
                    fiftyUsed = usedLifeline = true;
                    std::cout << "  50-50 used: two wrong options removed.\n\n";
                    printQuestion(q, hidden);
                } else if (in == "H") {
                    if (!freeHintUsed) {
                        freeHintUsed = usedLifeline = true;
                        std::cout << "  Hint (free): " << q.hint() << "\n";
                    } else if (p.useHintToken()) {
                        usedLifeline = true;
                        std::cout << "  Hint (1 token spent, " << p.hintTokens()
                                  << " left): " << q.hint() << "\n";
                    } else {
                        std::cout << "  No hints left. Earn tokens through rewards or buy one in the shop.\n";
                    }
                } else {  // Skip
                    if (skipUsed) {
                        std::cout << "  You already used Skip this quiz.\n";
                        continue;
                    }
                    skipUsed = usedLifeline = true;
                    skipThis = true;
                    break;
                }
                continue;
            }
            std::cout << "  Invalid choice. Type a letter shown above.\n";
        }

        const double secs =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        if (skipThis) {
            ++skipped;
            std::cout << "  Question skipped - no penalty.\n";
            continue;
        }

        // The timer is checked when Enter is pressed (console input cannot be interrupted portably).
        const bool timedOut = cfg.timeLimit > 0 && secs > cfg.timeLimit;

        if (!timedOut && q.isCorrect(answer)) {
            ++correct;
            ++streak;
            bestStreak = std::max(bestStreak, streak);
            score += q.points();
            xp += q.xpValue();
            p.addCoins(1);
            p.recordTopicCorrect(q.topic());
            std::cout << "  " << util::paint("CORRECT!", util::Tone::Green) << " +"
                      << q.points() << " points, +" << q.xpValue() << " XP, +1 coin ("
                      << fmtSeconds(secs) << ")\n";
            if (secs <= kFastSeconds) {
                ++fast;
                std::cout << "  " << util::paint("LIGHTNING FAST!", util::Tone::Yellow) << "\n";
            }
            if (streak == 3) {
                xp += 15;
                std::cout << "  3-question streak! +15 XP\n";
            } else if (streak >= 5 && streak % 5 == 0) {
                xp += 30;
                std::cout << "  " << streak << "-question streak! +30 XP\n";
            }
        } else {
            ++wrong;
            streak = 0;
            score -= kWrongPenalty;
            --lives;
            if (timedOut)
                std::cout << "  Time's up! (" << fmtSeconds(secs) << " > " << cfg.timeLimit << "s)\n";
            else
                std::cout << "  " << util::paint("WRONG!", util::Tone::Red) << "\n";
            std::cout << "  Correct answer: " << static_cast<char>('A' + q.correctIndex()) << ") "
                      << q.options()[q.correctIndex()] << "\n";
            std::cout << "  -" << kWrongPenalty << " points, lost a life. Lives left: " << lives << "\n";
            if (lives <= 0) {
                failed = true;
                std::cout << "\n  You are out of lives!\n";
            }
        }
    }

    // ---------------------------------------------------------- results
    score = std::max(0, score);
    const bool passed = !failed && (cfg.passMark > 0 ? correct >= cfg.passMark : true);
    const bool perfect = passed && wrong == 0 && skipped == 0 && correct == total;

    util::banner(passed ? "QUEST COMPLETE - VICTORY" : "QUEST FAILED");
    if (passed) {
        xp += 25;
        std::cout << "  Quiz completed:            +25 XP\n";
        if (perfect) {
            xp += 100;
            std::cout << "  Perfect score:             +100 XP\n";
        }
        if (!usedLifeline) {
            xp += 50;
            std::cout << "  No lifelines used:         +50 XP\n";
        }
        if (cfg.bonusXp > 0) {
            xp += cfg.bonusXp;
            std::cout << "  Challenge bonus:           +" << cfg.bonusXp << " XP\n";
        }
    } else if (!failed) {
        std::cout << "  You needed " << cfg.passMark << " correct answers but got " << correct << ".\n";
    }
    if (doubleXp && xp > 0) {
        xp *= 2;
        std::cout << "  DOUBLE XP applied!\n";
    }

    std::cout << "\n  " << util::paint("SCORE", util::Tone::Yellow) << "        " << score << "\n"
              << "  " << util::paint("ACCURACY", util::Tone::Cyan) << "     " << correct << "/" << total
              << "   Wrong: " << wrong << "   Skipped: " << skipped << "\n"
              << "  Best streak:  " << bestStreak << "\n"
              << "  " << util::paint("XP EARNED", util::Tone::Magenta) << "    " << xp << "\n";

    p.recordQuiz(score, correct, bestStreak, fast, passed);
    grantXp(p, xp);

    // ---------------------------------------------------- achievements
    if (p.quizzesCompleted() >= 1) awardBadge(p, "First Step");
    if (p.bestStreak() >= 7) awardBadge(p, "On Fire");
    if (p.totalCorrect() >= 50) awardBadge(p, "Sharpshooter");
    if (perfect) awardBadge(p, "Perfect");
    if (p.fastAnswers() >= 10) awardBadge(p, "Speed Coder");
    if (p.level() >= 5) awardBadge(p, "Code Master");
    if (passed && !cfg.winBadge.empty()) awardBadge(p, cfg.winBadge);

    if (passed && (cfg.guaranteedMystery || randomIn(0, 1) == 1)) mysteryReward(p);

    std::cout << "\n  Level " << p.level() << " - " << p.title() << "   |   XP: " << p.xp();
    if (p.xpToNext() > 0) std::cout << "   |   " << p.xpToNext() << " XP to next level";
    std::cout << "\n";
    return passed;
}

// ------------------------------------------------------- rewards and levels

void Quiz::grantXp(Player& p, int amount) {
    const int before = p.level();
    p.addXp(amount);
    for (int lvl = before + 1; lvl <= p.level(); ++lvl) levelUp(p, lvl);
}

void Quiz::levelUp(Player& p, int newLevel) {
    std::cout << "\n" << util::repeat('*', 52) << "\n"
              << "  LEVEL UP!  You reached Level " << newLevel << " - "
              << Player::titleForLevel(newLevel) << "\n"
              << util::repeat('*', 52) << "\n";
    p.addCoins(25);
    std::cout << "  Reward: +25 coins\n";
    switch (newLevel) {
        case 2:
            p.addHintTokens(1);
            std::cout << "  Unlocked: Mixed category\n  Reward: +1 Hint Token\n";
            break;
        case 3:
            p.addExtraLife();
            std::cout << "  Unlocked: Hard Mode\n  Reward: +1 Extra Life (used in your next quiz)\n";
            break;
        case 4:
            p.addHintTokens(2);
            std::cout << "  Unlocked: Boss Battle (C++ OOP Boss)\n  Reward: +2 Hint Tokens\n";
            break;
        case 5:
            p.setDoubleXp(true);
            std::cout << "  Unlocked: Expert Challenge\n  Reward: Double XP for your next quiz\n";
            break;
        case 6:
            p.addExtraLife();
            p.addCoins(100);
            std::cout << "  You reached the top rank!\n  Reward: +1 Extra Life, +100 coins\n";
            break;
        default:
            break;
    }
}

void Quiz::awardBadge(Player& p, const std::string& badge) {
    if (!p.addBadge(badge)) return;
    p.addCoins(10);
    std::cout << "\n  *** NEW BADGE UNLOCKED: " << badge << " ***  (+10 coins)\n";
}

void Quiz::mysteryReward(Player& p) {
    std::cout << "\n  ?? MYSTERY REWARD ?? Opening the box...\n";
    switch (randomIn(0, 5)) {
        case 0: {
            const int amount = randomIn(5, 10) * 5;
            std::cout << "  You found a bonus of " << amount << " XP!\n";
            grantXp(p, amount);
            break;
        }
        case 1: {
            const int amount = randomIn(4, 10) * 5;
            p.addCoins(amount);
            std::cout << "  You found " << amount << " coins!\n";
            break;
        }
        case 2:
            p.setDoubleXp(true);
            std::cout << "  Double XP for your next quiz!\n";
            break;
        case 3:
            if (p.addBadge("Lucky Draw")) {
                std::cout << "  A rare badge: Lucky Draw!\n";
            } else {
                p.addCoins(30);
                std::cout << "  You found 30 coins!\n";
            }
            break;
        case 4:
            p.addExtraLife();
            std::cout << "  An Extra Life (used in your next quiz)!\n";
            break;
        default:
            p.addHintTokens(1);
            std::cout << "  A Hint Token!\n";
            break;
    }
}
