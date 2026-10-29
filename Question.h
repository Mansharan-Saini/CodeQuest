#ifndef QUESTION_H
#define QUESTION_H

#include <random>
#include <string>
#include <vector>

enum class Category { C, CPP };

std::string categoryName(Category c);
std::string difficultyName(int difficulty);  // 1 = Easy, 2 = Medium, 3 = Hard

// One multiple-choice question: text, 4 options, correct answer, category, difficulty, topic.
//
// questions.txt line format (fields separated by '|'; the FIRST option is the correct one,
// options are shuffled at run time):
//   CATEGORY|DIFFICULTY|TOPIC|QUESTION|CORRECT|WRONG1|WRONG2|WRONG3
//   CATEGORY   = C or CPP        DIFFICULTY = 1 (easy), 2 (medium), 3 (hard)
class Question {
public:
    Question() = default;
    Question(Category category, int difficulty, std::string topic, std::string text,
             std::vector<std::string> options);

    Category category() const { return category_; }
    int difficulty() const { return difficulty_; }
    const std::string& topic() const { return topic_; }
    const std::string& text() const { return text_; }
    const std::vector<std::string>& options() const { return options_; }
    int correctIndex() const { return correct_; }
    bool isCorrect(int index) const { return index == correct_; }

    int points() const;   // score value: 10 / 15 / 20
    int xpValue() const;  // XP value: 10 (20 for hard questions)

    void shuffleOptions(std::mt19937& rng);
    std::vector<int> pickEliminations(std::mt19937& rng) const;  // two wrong options (50-50)
    std::string hint() const;

    static bool fromLine(const std::string& line, Question& out);
    static std::vector<Question> loadFromFile(const std::string& path);

private:
    Category category_ = Category::C;
    int difficulty_ = 1;
    std::string topic_;
    std::string text_;
    std::vector<std::string> options_;
    int correct_ = 0;  // index of the right answer inside options_
};

#endif
