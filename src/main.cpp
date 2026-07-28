#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <charconv>
#include <cmath>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    void expect(char expected) {
        skipWhitespace();
        if (position_ >= input_.size() || input_[position_] != expected) {
            throw std::runtime_error("Invalid JSON input");
        }
        ++position_;
    }

    bool consume(char expected) {
        skipWhitespace();
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    std::string_view parseStringView() {
        expect('"');
        const std::size_t start = position_;
        while (position_ < input_.size()) {
            const char character = input_[position_];
            if (character == '"') {
                std::string_view result = input_.substr(start, position_ - start);
                ++position_;
                return result;
            }
            if (character == '\\') {
                ++position_;
            }
            ++position_;
        }
        throw std::runtime_error("Unterminated JSON string");
    }

    std::string parseString() {
        expect('"');
        std::string result;
        result.reserve(32);
        while (position_ < input_.size()) {
            const char character = input_[position_++];
            if (character == '"') {
                return result;
            }
            if (character == '\\') {
                if (position_ >= input_.size()) {
                    throw std::runtime_error("Invalid JSON escape sequence");
                }
                const char escaped = input_[position_++];
                switch (escaped) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: 
                        result += '\\';
                        result += escaped;
                        break;
                }
            } else {
                result += character;
            }
        }
        throw std::runtime_error("Unterminated JSON string");
    }

    double parseNumber() {
        skipWhitespace();
        const std::size_t start = position_;
        if (position_ < input_.size() && input_[position_] == '-') {
            ++position_;
        }
        while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            ++position_;
        }
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_;
            while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                ++position_;
            }
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) {
                ++position_;
            }
            while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
                ++position_;
            }
        }
        if (start == position_) {
            throw std::runtime_error("Expected JSON number");
        }
        
        double value = 0.0;
        std::from_chars(input_.data() + start, input_.data() + position_, value);
        return value;
    }

    void skipValue() {
        skipWhitespace();
        if (position_ >= input_.size()) {
            throw std::runtime_error("Unexpected end of JSON input");
        }
        if (input_[position_] == '"') {
            ++position_;
            while (position_ < input_.size()) {
                if (input_[position_] == '"') {
                    ++position_;
                    break;
                }
                if (input_[position_] == '\\') ++position_;
                ++position_;
            }
        } else if (input_[position_] == '{') {
            expect('{');
            if (!consume('}')) {
                do {
                    parseStringView();
                    expect(':');
                    skipValue();
                } while (consume(','));
                expect('}');
            }
        } else if (input_[position_] == '[') {
            expect('[');
            if (!consume(']')) {
                do {
                    skipValue();
                } while (consume(','));
                expect(']');
            }
        } else if (input_.substr(position_, 4) == "true") {
            position_ += 4;
        } else if (input_.substr(position_, 5) == "false") {
            position_ += 5;
        } else if (input_.substr(position_, 4) == "null") {
            position_ += 4;
        } else {
            if (input_[position_] == '-') ++position_;
            while (position_ < input_.size() && (std::isdigit(input_[position_]) || input_[position_] == '.' || input_[position_] == 'e' || input_[position_] == 'E' || input_[position_] == '+' || input_[position_] == '-')) {
                ++position_;
            }
        }
    }

private:
    void skipWhitespace() {
        while (position_ < input_.size() && (input_[position_] == ' ' || input_[position_] == '\n' || input_[position_] == '\r' || input_[position_] == '\t')) {
            ++position_;
        }
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

struct Transaction {
    double amount = 0.0;
    std::string category;
    std::string status;
};

Transaction parseTransaction(JsonParser& parser) {
    Transaction transaction;
    parser.expect('{');
    if (parser.consume('}')) {
        return transaction;
    }

    do {
        const std::string_view key = parser.parseStringView();
        parser.expect(':');
        
        if (key == "amount") {
            transaction.amount = parser.parseNumber();
        } else if (key == "category") {
            transaction.category = parser.parseString();
        } else if (key == "status") {
            transaction.status = parser.parseString();
        } else {
            parser.skipValue();
        }
    } while (parser.consume(','));
    parser.expect('}');
    return transaction;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.json>\n";
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        std::cerr << "Unable to open input file: " << argv[1] << '\n';
        return 1;
    }
    struct stat sb;
    if (fstat(fd, &sb) < 0) return 1;
    size_t size = sb.st_size;
    if (size == 0) return 0;
    const char* data = static_cast<const char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) return 1;

    const std::string_view input(data, size);

    try {
        JsonParser parser(input);
        parser.expect('[');

        double totalRevenue = 0.0;
        std::size_t transactionCount = 0;
        std::size_t failedCount = 0;
        
        std::unordered_map<std::string, double> categoryRevenue;
        categoryRevenue.reserve(128);

        if (!parser.consume(']')) {
            do {
                const Transaction transaction = parseTransaction(parser);
                ++transactionCount;
                if (transaction.status == "completed") {
                    totalRevenue += transaction.amount;
                    categoryRevenue[transaction.category] += transaction.amount;
                } else if (transaction.status == "failed") {
                    ++failedCount;
                }
            } while (parser.consume(','));
            parser.expect(']');
        }

        std::string topCategory;
        double highestRevenue = -1.0;
        
        for (const auto& categoryEntry : categoryRevenue) {
            if (categoryEntry.second > highestRevenue) {
                highestRevenue = categoryEntry.second;
                topCategory = categoryEntry.first;
            } else if (categoryEntry.second == highestRevenue) {
                if (categoryEntry.first < topCategory) {
                    topCategory = categoryEntry.first;
                }
            }
        }

        const double failureRate = transactionCount == 0
            ? 0.0
            : static_cast<double>(failedCount) * 100.0 / static_cast<double>(transactionCount);

        std::cout << std::fixed << std::setprecision(2) << totalRevenue << '\n';
        std::cout << std::fixed << std::setprecision(2) << failureRate << "%\n";
        std::cout << topCategory << '\n';
        
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        munmap((void*)data, size);
        close(fd);
        return 1;
    }

    munmap((void*)data, size);
    close(fd);
    return 0;
}
