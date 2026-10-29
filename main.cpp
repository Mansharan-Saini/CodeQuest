#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "Leaderboard.h"
#include "Player.h"
#include "Question.h"

namespace {

constexpr COLORREF kBackground = RGB(11, 15, 27);
constexpr COLORREF kPanel = RGB(21, 28, 45);
constexpr COLORREF kPanelRaised = RGB(29, 39, 61);
constexpr COLORREF kBorder = RGB(43, 56, 82);
constexpr COLORREF kText = RGB(238, 243, 255);
constexpr COLORREF kMuted = RGB(151, 166, 193);
constexpr COLORREF kBlue = RGB(91, 166, 255);
constexpr COLORREF kMint = RGB(75, 222, 190);
constexpr COLORREF kGold = RGB(255, 195, 91);
constexpr COLORREF kRed = RGB(255, 113, 128);

enum class Screen { Dashboard, Quiz, Results, Profile, Leaderboard, Shop };
enum class Quest { Standard, Pointers, Boss };
enum HitTarget {
    TargetNone,
    TargetDashboard,
    TargetProfile,
    TargetLeaderboard,
    TargetShop,
    TargetQuestStandard,
    TargetQuestPointers,
    TargetQuestBoss,
    TargetAnswerA,
    TargetAnswerB,
    TargetAnswerC,
    TargetAnswerD,
    TargetHint,
    TargetSkip,
    TargetHome,
    TargetBuyHint,
    TargetBuyLife,
    TargetBuyXp
};

struct AppState {
    Leaderboard board{"players.txt"};
    std::vector<Question> bank;
    std::vector<Question> round;
    Player* player = nullptr;
    std::mt19937 rng{std::random_device{}()};
    Screen screen = Screen::Dashboard;
    Quest quest = Quest::Standard;
    std::size_t questionIndex = 0;
    int score = 0;
    int correct = 0;
    int lives = Player::BASE_LIVES;
    int earnedXp = 0;
    int streak = 0;
    int bestStreak = 0;
    bool hintUsed = false;
    bool doubleXpActive = false;
    bool questWon = false;
    bool roundRecorded = false;
    std::string status = "Choose a quest to begin your journey.";
};

AppState* gApp = nullptr;
HFONT gFontSmall = nullptr;
HFONT gFontBody = nullptr;
HFONT gFontHeading = nullptr;
HFONT gFontDisplay = nullptr;
HWND gWindow = nullptr;
HICON gAppIcon = nullptr;
int gMouseX = -1;
int gMouseY = -1;
int gHoverTarget = TargetNone;
int gPressedTarget = TargetNone;
bool gTrackingMouse = false;

struct Card {
    RECT rect;
    const char* title;
    const char* detail;
    COLORREF accent;
    Quest quest;
    bool locked;
};

RECT makeRect(int x, int y, int width, int height) {
    return RECT{x, y, x + width, y + height};
}

bool inside(const RECT& rect, int x, int y) {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

void rounded(HDC dc, const RECT& rect, COLORREF fill, COLORREF outline, int radius = 14) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, outline);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void shadowedRounded(HDC dc, const RECT& rect, COLORREF fill, COLORREF outline,
                     int radius = 14) {
    RECT shadow = rect;
    OffsetRect(&shadow, 0, 5);
    rounded(dc, shadow, RGB(5, 8, 15), RGB(5, 8, 15), radius);
    rounded(dc, rect, fill, outline, radius);
}

void text(HDC dc, const std::string& value, RECT rect, COLORREF color, HFONT font,
          UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HGDIOBJ oldFont = SelectObject(dc, font);
    DrawTextA(dc, value.c_str(), static_cast<int>(value.size()), &rect, format);
    SelectObject(dc, oldFont);
}

void button(HDC dc, const RECT& rect, const std::string& caption, COLORREF accent,
            bool selected = false, bool disabled = false, int target = TargetNone) {
    const bool hovered = target != TargetNone && target == gHoverTarget && !disabled;
    const bool pressed = target != TargetNone && target == gPressedTarget && !disabled;
    RECT face = rect;
    if (pressed) OffsetRect(&face, 0, 2);
    const COLORREF background = disabled ? RGB(24, 30, 43) :
                                hovered ? RGB(39, 57, 84) :
                                selected ? RGB(33, 54, 82) : kPanelRaised;
    const COLORREF border = disabled ? RGB(39, 46, 61) :
                            hovered || selected ? accent : kBorder;
    shadowedRounded(dc, face, background, border, 12);
    if (hovered) {
        RECT highlight = face;
        InflateRect(&highlight, -3, -3);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(66, 87, 118));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, highlight.left, highlight.top, highlight.right, highlight.bottom, 9, 9);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
    RECT captionRect = face;
    InflateRect(&captionRect, -12, -3);
    text(dc, caption, captionRect, disabled ? RGB(103, 116, 140) : kText, gFontBody,
         DT_LEFT | DT_VCENTER | DT_WORDBREAK | DT_END_ELLIPSIS);
}

int clientWidth(HWND hwnd) {
    RECT r{};
    GetClientRect(hwnd, &r);
    return r.right;
}

void setStatus(const std::string& message) {
    gApp->status = message;
    InvalidateRect(gWindow, nullptr, FALSE);
}

void saveProgress() {
    if (!gApp->board.save()) {
        setStatus("Could not save player progress to players.txt.");
    }
}

void drawHeader(HDC dc, int width) {
    RECT header = makeRect(0, 0, width, 86);
    HBRUSH brush = CreateSolidBrush(RGB(15, 21, 35));
    FillRect(dc, &header, brush);
    DeleteObject(brush);
    RECT stripe = makeRect(0, 84, width, 87);
    brush = CreateSolidBrush(kBlue);
    FillRect(dc, &stripe, brush);
    DeleteObject(brush);

    shadowedRounded(dc, makeRect(25, 14, 53, 53), kMint, kMint, 15);
    text(dc, "</>", makeRect(25, 14, 53, 53), kBackground, gFontBody,
         DT_CENTER | DT_VCENTER);
    text(dc, "CODEQUEST", makeRect(91, 15, 250, 32), kText, gFontHeading);
    text(dc, "THE C & C++ SKILL ARENA", makeRect(92, 48, 320, 21), kMuted, gFontSmall);

    if (gApp->player) {
        const int right = width - 25;
        shadowedRounded(dc, makeRect(right - 290, 17, 290, 51), kPanel, kBorder, 13);
        text(dc, gApp->player->name(), makeRect(right - 276, 19, 170, 23), kText,
             gFontBody);
        text(dc, "LV " + std::to_string(gApp->player->level()) + "  |  " +
                     std::to_string(gApp->player->xp()) + " XP",
             makeRect(right - 276, 42, 190, 18), kMuted, gFontSmall);
        text(dc, "$ " + std::to_string(gApp->player->coins()),
             makeRect(right - 105, 23, 88, 30), kGold, gFontBody,
             DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

void drawSidebar(HDC dc, int height) {
    const int panelHeight = std::max(500, height - 132);
    shadowedRounded(dc, makeRect(24, 108, 225, panelHeight), kPanel, kBorder, 16);
    text(dc, "YOUR HUB", makeRect(43, 125, 185, 26), kMuted, gFontSmall);

    const struct NavItem { const char* title; Screen screen; } items[] = {
        {"Dashboard", Screen::Dashboard},
        {"Player profile", Screen::Profile},
        {"Leaderboard", Screen::Leaderboard},
        {"Reward shop", Screen::Shop}
    };
    int y = 163;
    for (const NavItem& item : items) {
        const bool selected = gApp->screen == item.screen;
        const int target = item.screen == Screen::Dashboard ? TargetDashboard :
                           item.screen == Screen::Profile ? TargetProfile :
                           item.screen == Screen::Leaderboard ? TargetLeaderboard : TargetShop;
        button(dc, makeRect(38, y, 197, 43), item.title, kBlue, selected, false, target);
        y += 52;
    }

    shadowedRounded(dc, makeRect(39, y + 9, 195, 100), RGB(24, 37, 53),
                    RGB(39, 65, 78), 12);
    text(dc, "DAILY REMINDER", makeRect(51, y + 20, 170, 19), kMint, gFontSmall);
    text(dc, "Small steps. Big", makeRect(51, y + 44, 170, 21), kText, gFontSmall);
    text(dc, "brain upgrades.", makeRect(51, y + 65, 170, 21), kText, gFontSmall);
    text(dc, "Keep your streak alive!", makeRect(51, y + 88, 175, 18), kMuted,
         gFontSmall);
}

std::vector<Card> questCards(int x, int y, int width) {
    const int gap = 14;
    const int cardWidth = (width - gap * 2) / 3;
    return {
        {makeRect(x, y, cardWidth, 171), "01  /  WARM UP", "Mixed easy questions\n5 questions  |  3 lives",
         kBlue, Quest::Standard, false},
        {makeRect(x + cardWidth + gap, y, cardWidth, 171), "02  /  POINTER RUN",
         "Trace the memory.\n5 pointer questions  |  3 lives", kMint, Quest::Pointers, false},
        {makeRect(x + 2 * (cardWidth + gap), y, cardWidth, 171), "03  /  BOSS FIGHT",
         "Face the OOP boss.\nHard questions  |  Level 4",
         kGold, Quest::Boss, !gApp->player->canFightBoss()}
    };
}

int hitTargetAt(int x, int y, int width) {
    const RECT nav[] = {
        makeRect(38, 163, 197, 43), makeRect(38, 215, 197, 43),
        makeRect(38, 267, 197, 43), makeRect(38, 319, 197, 43)
    };
    if (inside(nav[0], x, y)) return TargetDashboard;
    if (inside(nav[1], x, y)) return TargetProfile;
    if (inside(nav[2], x, y)) return TargetLeaderboard;
    if (inside(nav[3], x, y)) return TargetShop;

    if (gApp->screen == Screen::Dashboard) {
        const auto cards = questCards(278, 408, width - 306);
        for (const Card& card : cards) {
            if (inside(card.rect, x, y)) {
                if (card.quest == Quest::Standard) return TargetQuestStandard;
                if (card.quest == Quest::Pointers) return TargetQuestPointers;
                return TargetQuestBoss;
            }
        }
    } else if (gApp->screen == Screen::Quiz) {
        const int areaWidth = width - 306;
        const int gap = 14;
        const int choiceWidth = (areaWidth - gap) / 2;
        const int choiceY = 371;
        const RECT answers[] = {
            makeRect(278, choiceY, choiceWidth, 52),
            makeRect(278 + choiceWidth + gap, choiceY, choiceWidth, 52),
            makeRect(278, choiceY + 65, choiceWidth, 52),
            makeRect(278 + choiceWidth + gap, choiceY + 65, choiceWidth, 52)
        };
        for (int i = 0; i < 4; ++i) {
            if (inside(answers[i], x, y)) return TargetAnswerA + i;
        }
        if (inside(makeRect(278, 513, 157, 41), x, y)) return TargetHint;
        if (inside(makeRect(447, 513, 137, 41), x, y)) return TargetSkip;
    } else if (gApp->screen == Screen::Results) {
        const RECT home = makeRect(278 + (width - 306) / 2 - 115, 490, 230, 48);
        if (inside(home, x, y)) return TargetHome;
    } else if (gApp->screen == Screen::Shop) {
        const int buyX = width - 140;
        for (int item = 0; item < 3; ++item) {
            const RECT buy = makeRect(buyX, 199 + item * 112 + 24, 94, 44);
            const int price = item == 0 ? 30 : item == 1 ? 60 : 80;
            const bool alreadyOwned = item == 2 && gApp->player->hasDoubleXp();
            if (inside(buy, x, y) && gApp->player->coins() >= price && !alreadyOwned)
                return TargetBuyHint + item;
        }
    }
    return TargetNone;
}

void drawQuestCard(HDC dc, const Card& card) {
    const int target = card.quest == Quest::Standard ? TargetQuestStandard :
                       card.quest == Quest::Pointers ? TargetQuestPointers : TargetQuestBoss;
    const bool hovered = gHoverTarget == target && !card.locked;
    shadowedRounded(dc, card.rect, hovered ? RGB(27, 38, 58) : kPanel,
                    hovered ? card.accent : kBorder, 15);
    RECT accent = makeRect(card.rect.left + 1, card.rect.top + 13, card.rect.left + 5,
                           card.rect.bottom - 13);
    HBRUSH brush = CreateSolidBrush(card.accent);
    FillRect(dc, &accent, brush);
    DeleteObject(brush);
    text(dc, card.title, makeRect(card.rect.left + 17, card.rect.top + 17,
                                  card.rect.right - card.rect.left - 29, 23),
         card.accent, gFontSmall);
    RECT detail = makeRect(card.rect.left + 17, card.rect.top + 52,
                           card.rect.right - 13, card.rect.top + 105);
    text(dc, card.detail, detail, card.locked ? kMuted : kText, gFontBody,
         DT_LEFT | DT_TOP | DT_WORDBREAK);
    RECT action = makeRect(card.rect.left + 15, card.rect.bottom - 49,
                           card.rect.right - 15, card.rect.bottom - 14);
    button(dc, action, card.locked ? "Reach Level 4 to unlock" : "Launch quest   >",
           card.accent, false, card.locked, target);
}

void drawDashboard(HDC dc, int width) {
    const int left = 278;
    const int areaWidth = width - left - 28;
    text(dc, "Welcome back, " + gApp->player->name(), makeRect(left, 109, areaWidth, 40),
         kText, gFontDisplay);
    text(dc, "Pick a quest. Earn XP. Become a Code Master.", makeRect(left, 151, areaWidth, 25),
         kMuted, gFontBody);

    const int gap = 13;
    const int statWidth = (areaWidth - gap * 2) / 3;
    struct Stat { const char* name; std::string value; COLORREF accent; };
    const Stat stats[] = {
        {"CURRENT LEVEL", std::to_string(gApp->player->level()) + "  /  6", kBlue},
        {"EXPERIENCE", std::to_string(gApp->player->xp()) + " XP", kMint},
        {"QUESTS CLEARED", std::to_string(gApp->player->quizzesCompleted()), kGold}
    };
    for (int i = 0; i < 3; ++i) {
        const int x = left + i * (statWidth + gap);
        shadowedRounded(dc, makeRect(x, 194, statWidth, 88), kPanel, kBorder, 14);
        text(dc, stats[i].name, makeRect(x + 15, 205, statWidth - 25, 20), kMuted, gFontSmall);
        text(dc, stats[i].value, makeRect(x + 15, 229, statWidth - 25, 35), stats[i].accent,
             gFontHeading);
    }

    const int level = gApp->player->level();
    const int xpStart = Player::xpForLevel(level);
    const int xpEnd = Player::xpForLevel(std::min(level + 1, Player::MAX_LEVEL));
    const int needed = xpEnd - xpStart;
    const int progress = needed <= 0 ? 100 :
        std::clamp((gApp->player->xp() - xpStart) * 100 / needed, 0, 100);
    shadowedRounded(dc, makeRect(left, 296, areaWidth, 59), kPanel, kBorder, 12);
    text(dc, "LEVEL PROGRESS", makeRect(left + 15, 304, 170, 20), kMuted, gFontSmall);
    text(dc, level == Player::MAX_LEVEL ? "MAX LEVEL" :
             std::to_string(gApp->player->xpToNext()) + " XP to next level",
         makeRect(left + 185, 304, areaWidth - 200, 20), kMint, gFontSmall,
         DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    RECT track = makeRect(left + 15, 330, left + areaWidth - 15, 339);
    rounded(dc, track, RGB(41, 52, 73), RGB(41, 52, 73), 6);
    if (progress > 0) {
        RECT fill = track;
        fill.right = fill.left + std::max<LONG>(
            9, static_cast<LONG>((fill.right - fill.left) * progress / 100));
        rounded(dc, fill, kMint, kMint, 6);
    }

    text(dc, "CHOOSE YOUR NEXT QUEST", makeRect(left, 371, areaWidth, 26), kText,
         gFontHeading);
    for (const Card& card : questCards(left, 408, areaWidth)) drawQuestCard(dc, card);
    shadowedRounded(dc, makeRect(left, 595, areaWidth, 55), RGB(18, 35, 44),
                    RGB(33, 76, 80), 12);
    text(dc, "TIP", makeRect(left + 15, 606, 48, 25), kMint, gFontBody);
    text(dc, "Correct answers give points, XP and coins. Use Hint if you get stuck.",
         makeRect(left + 61, 606, areaWidth - 75, 25), kText, gFontSmall);
}

void drawQuiz(HDC dc, int width) {
    const int left = 278;
    const int areaWidth = width - left - 28;
    text(dc, gApp->quest == Quest::Boss ? "BOSS ARENA" :
             gApp->quest == Quest::Pointers ? "POINTER CHALLENGE" : "STANDARD QUEST",
         makeRect(left, 107, areaWidth, 36), kText, gFontDisplay);
    text(dc, "Question " + std::to_string(gApp->questionIndex + 1) + " of " +
             std::to_string(gApp->round.size()),
         makeRect(left, 146, areaWidth, 25), kMuted, gFontBody);

    const int total = static_cast<int>(gApp->round.size());
    const int progress = total == 0 ? 0 :
        static_cast<int>(gApp->questionIndex) * 100 / total;
    RECT track = makeRect(left, 179, left + areaWidth, 187);
    rounded(dc, track, RGB(41, 52, 73), RGB(41, 52, 73), 6);
    if (progress > 0) {
        RECT fill = track;
        fill.right = fill.left + (fill.right - fill.left) * progress / 100;
        rounded(dc, fill, kBlue, kBlue, 6);
    }

    shadowedRounded(dc, makeRect(left, 205, areaWidth, 150), kPanel, kBorder, 15);
    text(dc, "QUESTION", makeRect(left + 20, 219, areaWidth - 40, 20), kBlue, gFontSmall);
    const Question& q = gApp->round[gApp->questionIndex];
    text(dc, q.text(), makeRect(left + 20, 247, areaWidth - 40, 93), kText,
         gFontHeading, DT_LEFT | DT_TOP | DT_WORDBREAK);

    const int gap = 14;
    const int choiceWidth = (areaWidth - gap) / 2;
    const int choiceY = 371;
    for (int i = 0; i < 4; ++i) {
        const int x = left + (i % 2) * (choiceWidth + gap);
        const int y = choiceY + (i / 2) * 65;
        RECT r = makeRect(x, y, choiceWidth, 52);
        button(dc, r, std::string(1, static_cast<char>('A' + i)) + "    " + q.options()[i],
               kBlue, false, false, TargetAnswerA + i);
    }

    RECT hint = makeRect(left, 513, 157, 41);
    RECT skip = makeRect(left + 169, 513, 137, 41);
    button(dc, hint, "Use hint", kMint, false, false, TargetHint);
    button(dc, skip, "Skip", kGold, false, false, TargetSkip);

    shadowedRounded(dc, makeRect(left + 322, 513, areaWidth - 322, 41),
                    kPanel, kBorder, 11);
    text(dc, gApp->status, makeRect(left + 336, 513, areaWidth - 350, 41),
         gApp->status.find("Wrong") != std::string::npos ? kRed : kMuted, gFontSmall,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void drawResults(HDC dc, int width) {
    const int left = 278;
    const int areaWidth = width - left - 28;
    const int center = left + areaWidth / 2;
    shadowedRounded(dc, makeRect(left + 45, 145, areaWidth - 90, 425), kPanel, kBorder, 20);
    text(dc, gApp->questWon ? "QUEST COMPLETE" : "QUEST OVER",
         makeRect(left + 80, 188, areaWidth - 160, 48),
         gApp->lives > 0 ? kMint : kRed, gFontDisplay, DT_CENTER | DT_VCENTER);
    text(dc, std::to_string(gApp->score), makeRect(center - 100, 261, 200, 76), kGold,
         gFontDisplay, DT_CENTER | DT_VCENTER);
    text(dc, "POINTS", makeRect(center - 100, 332, 200, 25), kMuted, gFontSmall,
         DT_CENTER | DT_VCENTER);
    text(dc, "Correct  " + std::to_string(gApp->correct) + " / " +
             std::to_string(gApp->round.size()),
         makeRect(left + 100, 384, areaWidth - 200, 31), kText, gFontHeading,
         DT_CENTER | DT_VCENTER);
    text(dc, "XP earned  +" + std::to_string(gApp->earnedXp),
         makeRect(left + 100, 422, areaWidth - 200, 28), kMint, gFontBody,
         DT_CENTER | DT_VCENTER);
    RECT home = makeRect(center - 115, 490, 230, 48);
    button(dc, home, "Back to dashboard", kBlue, false, false, TargetHome);
}

void drawProfile(HDC dc, int width) {
    const int left = 278;
    const int areaWidth = width - left - 28;
    const Player& p = *gApp->player;
    text(dc, "PLAYER PROFILE", makeRect(left, 109, areaWidth, 40), kText, gFontDisplay);
    shadowedRounded(dc, makeRect(left, 164, areaWidth, 450), kPanel, kBorder, 17);
    text(dc, p.name(), makeRect(left + 27, 188, areaWidth - 54, 43), kMint, gFontDisplay);
    text(dc, "Level " + std::to_string(p.level()) + "  /  " + p.title(),
         makeRect(left + 27, 237, areaWidth - 54, 27), kText, gFontBody);

    const std::string details[] = {
        "Experience: " + std::to_string(p.xp()) + " XP",
        "Coins: " + std::to_string(p.coins()),
        "Hint tokens: " + std::to_string(p.hintTokens()),
        "Extra lives: " + std::to_string(p.extraLives()),
        "Quests completed: " + std::to_string(p.quizzesCompleted()),
        "High score: " + std::to_string(p.highScore()),
        "Correct answers: " + std::to_string(p.totalCorrect()),
        "Best streak: " + std::to_string(p.bestStreak()),
        "Bosses defeated: " + std::to_string(p.bossesDefeated())
    };
    int y = 290;
    for (const std::string& line : details) {
        text(dc, line, makeRect(left + 27, y, areaWidth - 54, 30), kText, gFontBody);
        y += 35;
    }
}

void drawLeaderboard(HDC dc, int width) {
    const int left = 278;
    const int areaWidth = width - left - 28;
    text(dc, "LEADERBOARD", makeRect(left, 109, areaWidth, 40), kText, gFontDisplay);
    shadowedRounded(dc, makeRect(left, 164, areaWidth, 475), kPanel, kBorder, 17);
    text(dc, "RANK", makeRect(left + 25, 181, 75, 25), kMuted, gFontSmall);
    text(dc, "PLAYER", makeRect(left + 110, 181, 250, 25), kMuted, gFontSmall);
    text(dc, "LEVEL", makeRect(left + 390, 181, 100, 25), kMuted, gFontSmall);
    text(dc, "XP", makeRect(left + 530, 181, 120, 25), kMuted, gFontSmall);

    std::vector<Player> players = gApp->board.snapshot();
    std::sort(players.begin(), players.end(), [](const Player& a, const Player& b) {
        if (a.xp() != b.xp()) return a.xp() > b.xp();
        if (a.highScore() != b.highScore()) return a.highScore() > b.highScore();
        return a.name() < b.name();
    });
    const int count = std::min<int>(players.size(), 9);
    for (int i = 0; i < count; ++i) {
        const int y = 218 + i * 44;
        shadowedRounded(dc, makeRect(left + 18, y, areaWidth - 36, 37),
                        i == 0 ? RGB(45, 45, 40) : kPanelRaised,
                        i == 0 ? RGB(112, 91, 49) : kPanelRaised, 9);
        text(dc, "#" + std::to_string(i + 1), makeRect(left + 30, y, 65, 37),
             i == 0 ? kGold : kMuted, gFontBody);
        text(dc, players[i].name(), makeRect(left + 110, y, 250, 37), kText, gFontBody);
        text(dc, std::to_string(players[i].level()), makeRect(left + 390, y, 100, 37),
             kMint, gFontBody);
        text(dc, std::to_string(players[i].xp()), makeRect(left + 530, y, 120, 37),
             kBlue, gFontBody);
    }
    if (players.empty())
        text(dc, "No players yet. Complete a quest to claim the first spot.",
             makeRect(left + 26, 231, areaWidth - 52, 35), kMuted, gFontBody);
}

void drawShop(HDC dc, int width) {
    const int left = 278;
    const int areaWidth = width - left - 28;
    text(dc, "REWARD SHOP", makeRect(left, 109, areaWidth, 40), kText, gFontDisplay);
    text(dc, "Spend your coins on helpful power-ups.", makeRect(left, 150, areaWidth, 25),
         kMuted, gFontBody);
    const struct Product { const char* title; const char* desc; int price; int id; } items[] = {
        {"Hint token", "A hint for a tricky question.", 30, 1},
        {"Extra life", "Bank one extra life for your next quest.", 60, 2},
        {"Double XP", "Double the XP from your next quest.", 80, 3}
    };
    for (int i = 0; i < 3; ++i) {
        const int y = 199 + i * 112;
        shadowedRounded(dc, makeRect(left, y, areaWidth, 94), kPanel, kBorder, 14);
        text(dc, items[i].title, makeRect(left + 20, y + 13, 270, 29), kText, gFontHeading);
        text(dc, items[i].desc, makeRect(left + 20, y + 46, 470, 25), kMuted, gFontSmall);
        text(dc, "$ " + std::to_string(items[i].price), makeRect(left + areaWidth - 227,
             y + 12, 100, 30), kGold, gFontBody, DT_RIGHT | DT_VCENTER);
        button(dc, makeRect(left + areaWidth - 112, y + 24, 94, 44), "Buy",
               kMint, false, gApp->player->coins() < items[i].price ||
               (items[i].id == 3 && gApp->player->hasDoubleXp()), TargetBuyHint + i);
    }
    text(dc, "Wallet:  $ " + std::to_string(gApp->player->coins()),
         makeRect(left, 548, areaWidth, 30), kGold, gFontHeading);
}

void paint(HDC dc, HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right;
    const int height = client.bottom;

    HBRUSH bg = CreateSolidBrush(kBackground);
    FillRect(dc, &client, bg);
    DeleteObject(bg);
    drawHeader(dc, width);
    drawSidebar(dc, height);

    switch (gApp->screen) {
        case Screen::Dashboard: drawDashboard(dc, width); break;
        case Screen::Quiz: drawQuiz(dc, width); break;
        case Screen::Results: drawResults(dc, width); break;
        case Screen::Profile: drawProfile(dc, width); break;
        case Screen::Leaderboard: drawLeaderboard(dc, width); break;
        case Screen::Shop: drawShop(dc, width); break;
    }
}

void finishRound(bool completed) {
    if (gApp->roundRecorded) return;
    gApp->roundRecorded = true;
    gApp->screen = Screen::Results;
    gApp->questWon = completed;

    const int baseXp = completed ? gApp->earnedXp + 25 : gApp->earnedXp;
    gApp->earnedXp = gApp->doubleXpActive ? baseXp * 2 : baseXp;
    if (gApp->player) {
        gApp->player->recordQuiz(gApp->score, gApp->correct, gApp->bestStreak, 0, completed);
        gApp->player->addXp(gApp->earnedXp);
        gApp->player->addBadge("First Step");
        if (gApp->quest == Quest::Pointers && completed && gApp->correct >= 4)
            gApp->player->addBadge("Pointer Master");
        if (gApp->quest == Quest::Boss && completed) {
            gApp->player->recordBossWin();
            gApp->player->addBadge("Boss Slayer");
        }
    }
    saveProgress();
    InvalidateRect(gWindow, nullptr, FALSE);
}

void startRound(Quest quest) {
    if (quest == Quest::Boss && !gApp->player->canFightBoss()) {
        setStatus("Boss Battle unlocks at Level 4.");
        return;
    }

    gApp->quest = quest;
    gApp->round.clear();
    for (const Question& question : gApp->bank) {
        const bool matches = quest == Quest::Standard
            ? question.difficulty() == 1
            : quest == Quest::Pointers
                ? question.topic() == "Pointers"
                : question.topic() == "Object-Oriented Programming" && question.difficulty() == 3;
        if (matches) gApp->round.push_back(question);
    }

    if (gApp->round.empty()) {
        setStatus("No questions are available for this quest.");
        return;
    }
    std::shuffle(gApp->round.begin(), gApp->round.end(), gApp->rng);
    const std::size_t limit = quest == Quest::Boss ? 10 : 5;
    if (gApp->round.size() > limit) gApp->round.resize(limit);
    for (Question& question : gApp->round) question.shuffleOptions(gApp->rng);

    gApp->questionIndex = 0;
    gApp->score = 0;
    gApp->correct = 0;
    gApp->lives = Player::BASE_LIVES + gApp->player->consumeExtraLives();
    gApp->earnedXp = 0;
    gApp->streak = 0;
    gApp->bestStreak = 0;
    gApp->hintUsed = false;
    gApp->questWon = false;
    gApp->roundRecorded = false;
    gApp->doubleXpActive = gApp->player->hasDoubleXp();
    gApp->player->setDoubleXp(false);
    gApp->screen = Screen::Quiz;
    gApp->status = "Choose the best answer.";
    InvalidateRect(gWindow, nullptr, FALSE);
}

void answerQuestion(int answer) {
    if (gApp->screen != Screen::Quiz || gApp->questionIndex >= gApp->round.size()) return;
    const Question& question = gApp->round[gApp->questionIndex];
    if (question.isCorrect(answer)) {
        ++gApp->correct;
        ++gApp->streak;
        gApp->bestStreak = std::max(gApp->bestStreak, gApp->streak);
        gApp->score += question.points();
        gApp->earnedXp += question.xpValue();
        gApp->player->addCoins(1);
        gApp->player->recordTopicCorrect(question.topic());
        gApp->status = "Correct! +" + std::to_string(question.points()) + " points.";
    } else {
        gApp->streak = 0;
        --gApp->lives;
        gApp->score = std::max(0, gApp->score - 5);
        gApp->status = "Not quite. Correct answer: " +
            std::string(1, static_cast<char>('A' + question.correctIndex()));
    }
    ++gApp->questionIndex;
    if (gApp->lives <= 0) {
        finishRound(false);
        return;
    }
    if (gApp->questionIndex >= gApp->round.size()) {
        const bool passed = gApp->quest != Quest::Pointers || gApp->correct >= 4;
        finishRound(passed);
        return;
    }
    InvalidateRect(gWindow, nullptr, FALSE);
}

void useHint() {
    if (gApp->screen != Screen::Quiz) return;
    if (!gApp->hintUsed) {
        gApp->hintUsed = true;
        setStatus(gApp->round[gApp->questionIndex].hint());
    } else if (gApp->player->useHintToken()) {
        setStatus(gApp->round[gApp->questionIndex].hint() + "  (1 token used)");
        saveProgress();
    } else {
        setStatus("No hint tokens left. Earn some or visit the shop.");
    }
}

void buyItem(int item) {
    const int price = item == 1 ? 30 : item == 2 ? 60 : 80;
    if (item == 3 && gApp->player->hasDoubleXp()) {
        setStatus("Double XP is already waiting for your next quest.");
        return;
    }
    if (!gApp->player->spendCoins(price)) {
        setStatus("Not enough coins for that item.");
        return;
    }
    if (item == 1) gApp->player->addHintTokens(1);
    else if (item == 2) gApp->player->addExtraLife(1);
    else gApp->player->setDoubleXp(true);
    saveProgress();
    setStatus("Purchase successful. Power-up added to your inventory.");
}

void performTarget(int target) {
    switch (target) {
        case TargetDashboard: gApp->screen = Screen::Dashboard; break;
        case TargetProfile: gApp->screen = Screen::Profile; break;
        case TargetLeaderboard: gApp->screen = Screen::Leaderboard; break;
        case TargetShop: gApp->screen = Screen::Shop; break;
        case TargetQuestStandard: startRound(Quest::Standard); break;
        case TargetQuestPointers: startRound(Quest::Pointers); break;
        case TargetQuestBoss: startRound(Quest::Boss); break;
        case TargetAnswerA: case TargetAnswerB: case TargetAnswerC: case TargetAnswerD:
            answerQuestion(target - TargetAnswerA);
            break;
        case TargetHint: useHint(); break;
        case TargetSkip:
            if (gApp->screen == Screen::Quiz) {
                ++gApp->questionIndex;
                if (gApp->questionIndex >= gApp->round.size()) {
                    finishRound(gApp->quest != Quest::Pointers || gApp->correct >= 4);
                } else {
                    setStatus("Question skipped.");
                }
            }
            break;
        case TargetHome:
            gApp->screen = Screen::Dashboard;
            setStatus("Choose another quest when you're ready.");
            break;
        case TargetBuyHint: buyItem(1); break;
        case TargetBuyLife: buyItem(2); break;
        case TargetBuyXp: buyItem(3); break;
        default: break;
    }
    InvalidateRect(gWindow, nullptr, FALSE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            paint(dc, hwnd);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            gMouseX = GET_X_LPARAM(lParam);
            gMouseY = GET_Y_LPARAM(lParam);
            if (!gTrackingMouse) {
                TRACKMOUSEEVENT track{};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                gTrackingMouse = TrackMouseEvent(&track) != FALSE;
            }
            const int nextTarget = hitTargetAt(gMouseX, gMouseY, clientWidth(hwnd));
            if (nextTarget != gHoverTarget) {
                gHoverTarget = nextTarget;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            SetCursor(LoadCursor(nullptr, gHoverTarget == TargetNone ? IDC_ARROW : IDC_HAND));
            return 0;
        }
        case WM_MOUSELEAVE:
            gTrackingMouse = false;
            gMouseX = -1;
            gMouseY = -1;
            gHoverTarget = TargetNone;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            gPressedTarget = hitTargetAt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
                                         clientWidth(hwnd));
            if (gPressedTarget != TargetNone) SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            const int releasedTarget = hitTargetAt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
                                                   clientWidth(hwnd));
            const int action = releasedTarget == gPressedTarget ? releasedTarget : TargetNone;
            gPressedTarget = TargetNone;
            if (GetCapture() == hwnd) ReleaseCapture();
            if (action != TargetNone) performTarget(action);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_CAPTURECHANGED:
            gPressedTarget = TargetNone;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_SIZE:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* limits = reinterpret_cast<MINMAXINFO*>(lParam);
            RECT minimum = makeRect(0, 0, 1050, 720);
            AdjustWindowRect(&minimum, WS_OVERLAPPEDWINDOW, FALSE);
            limits->ptMinTrackSize.x = minimum.right - minimum.left;
            limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE && gApp->screen != Screen::Dashboard) {
                gApp->screen = Screen::Dashboard;
                setStatus("Back at the dashboard.");
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            if (gApp) gApp->board.save();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

void createFonts() {
    gFontSmall = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    gFontBody = CreateFontA(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    gFontHeading = CreateFontA(21, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    gFontDisplay = CreateFontA(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
}

HICON createAppIcon() {
    HDC screen = GetDC(nullptr);
    if (!screen) return nullptr;
    HDC memory = CreateCompatibleDC(screen);
    HBITMAP color = CreateCompatibleBitmap(screen, 32, 32);
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, nullptr);
    if (!memory || !color || !mask) {
        if (mask) DeleteObject(mask);
        if (color) DeleteObject(color);
        if (memory) DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        return nullptr;
    }

    HGDIOBJ oldBitmap = SelectObject(memory, color);
    RECT iconBounds = makeRect(0, 0, 32, 32);
    HBRUSH background = CreateSolidBrush(RGB(16, 25, 43));
    FillRect(memory, &iconBounds, background);
    DeleteObject(background);
    rounded(memory, makeRect(2, 2, 28, 28), kMint, kMint, 8);
    HFONT iconFont = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    text(memory, "</>", makeRect(2, 2, 28, 28), RGB(12, 21, 36), iconFont,
         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(memory, oldBitmap);
    DeleteObject(iconFont);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);

    ICONINFO info{};
    info.fIcon = TRUE;
    info.hbmColor = color;
    info.hbmMask = mask;
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(mask);
    DeleteObject(color);
    return icon;
}

void destroyFonts() {
    for (HFONT font : {gFontSmall, gFontBody, gFontHeading, gFontDisplay}) {
        if (font) DeleteObject(font);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    AppState app;
    gApp = &app;
    gApp->bank = Question::loadFromFile("questions.txt");
    if (gApp->bank.empty()) {
        MessageBoxW(nullptr, L"Could not load questions.txt. Keep it beside the game executable.",
                    L"CodeQuest - question bank missing", MB_OK | MB_ICONERROR);
        return 1;
    }
    gApp->board.load();
    gApp->player = gApp->board.find("Player One");
    if (!gApp->player) {
        gApp->player = &gApp->board.create("Player One");
        gApp->board.save();
    }
    createFonts();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    gAppIcon = createAppIcon();
    wc.hIcon = gAppIcon;
    wc.hIconSm = gAppIcon;
    wc.lpszClassName = L"CodeQuestArenaWindow";
    if (!RegisterClassExW(&wc)) {
        destroyFonts();
        if (gAppIcon) DestroyIcon(gAppIcon);
        return 1;
    }

    gWindow = CreateWindowExW(0, wc.lpszClassName, L"CodeQuest | C & C++ Skill Arena",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1160, 790,
        nullptr, nullptr, instance, nullptr);
    if (!gWindow) {
        destroyFonts();
        if (gAppIcon) DestroyIcon(gAppIcon);
        return 1;
    }

    ShowWindow(gWindow, showCommand);
    UpdateWindow(gWindow);
    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    destroyFonts();
    if (gAppIcon) DestroyIcon(gAppIcon);
    return static_cast<int>(message.wParam);
}
