#include "Question.h"

#include <algorithm>
#include <fstream>
#include <utility>

#include "Utils.h"

std::string categoryName(Category c) { return c == Category::C ? "C" : "C++"; }

std::string difficultyName(int difficulty) {
    switch (difficulty) {
        case 1: return "Easy";
        case 2: return "Medium";
        default: return "Hard";
    }
}

Question::Question(Category category, int difficulty, std::string topic, std::string text,
                   std::vector<std::string> options)
    : category_(category),
      difficulty_(difficulty),
      topic_(std::move(topic)),
      text_(std::move(text)),
      options_(std::move(options)),
      correct_(0) {}

int Question::points() const {
    switch (difficulty_) {
        case 1: return 10;
        case 2: return 15;
        default: return 20;
    }
}

int Question::xpValue() const { return difficulty_ == 3 ? 20 : 10; }

void Question::shuffleOptions(std::mt19937& rng) {
    const std::string right = options_[correct_];
    std::shuffle(options_.begin(), options_.end(), rng);
    correct_ = static_cast<int>(std::find(options_.begin(), options_.end(), right) - options_.begin());
}

std::vector<int> Question::pickEliminations(std::mt19937& rng) const {
    std::vector<int> wrong;
    for (int i = 0; i < static_cast<int>(options_.size()); ++i)
        if (i != correct_) wrong.push_back(i);
    std::shuffle(wrong.begin(), wrong.end(), rng);
    if (wrong.size() > 2) wrong.resize(2);
    return wrong;
}

std::string Question::hint() const {
    return "Think about \"" + topic_ + "\". The correct answer starts with '" +
           options_[correct_].substr(0, 1) + "'.";
}

bool Question::fromLine(const std::string& line, Question& out) {
    const std::string t = util::trim(line);
    if (t.empty() || t[0] == '#') return false;  // blank line or comment

    const std::vector<std::string> f = util::split(t, '|');
    if (f.size() != 8) return false;

    Category cat;
    if (f[0] == "C") cat = Category::C;
    else if (f[0] == "CPP") cat = Category::CPP;
    else return false;

    int diff = 0;
    try {
        diff = std::stoi(f[1]);
    } catch (...) {
        return false;
    }
    if (diff < 1 || diff > 3) return false;

    out = Question(cat, diff, f[2], f[3], {f[4], f[5], f[6], f[7]});
    return true;
}

std::vector<Question> Question::loadFromFile(const std::string& path) {
    std::vector<Question> result;
    std::ifstream in(path);
    if (!in) return result;
    std::string line;
    while (std::getline(in, line)) {
        Question q;
        if (fromLine(line, q)) result.push_back(q);
    }
    return result;
}
