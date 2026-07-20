#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <stdexcept>

class FastCsvImporter {
private:
    char m_delimiter;
    std::string m_buffer;
    std::vector<std::string_view> m_headers;

    std::vector<std::string_view> parse_line(std::string_view line) {
        std::vector<std::string_view> fields;
        fields.reserve(m_headers.empty() ? 16 : m_headers.size());

        size_t start = 0;
        size_t pos = 0;
        bool in_quotes = false;

        while (pos < line.size()) {
            char c = line[pos];
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (c == m_delimiter && !in_quotes) {
                fields.push_back(trim_quotes(line.substr(start, pos - start)));
                start = pos + 1;
            }
            pos++;
        }

        fields.push_back(trim_quotes(line.substr(start, pos - start)));
        return fields;
    }

    std::string_view trim_quotes(std::string_view str) {
        if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
            return str.substr(1, str.size() - 2);
        }
        return str;
    }

public:
    explicit FastCsvImporter(char delimiter = ',') : m_delimiter(delimiter) {}

    void import(const std::string& filePath,
                const std::function<void(const std::vector<std::string_view>& headers,
                                         const std::vector<std::string_view>& row)>& rowCallback)
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Не удалось открыть файл: " + filePath);
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        m_buffer.resize(size);
        if (!file.read(&m_buffer[0], size)) {
            throw std::runtime_error("Ошибка при чтении файла");
        }
        file.close();

        std::string_view content(m_buffer);
        size_t line_start = 0;
        bool is_first_line = true;

        // Remove  UTF-8 BOM, if an exist
        if (content.rfind("\xEF\xBB\xBF", 0) == 0) {
            line_start = 3;
        }

        for (size_t i = line_start; i < content.size(); ++i) {
            if (content[i] == '\n' || content[i] == '\r') {
                size_t line_len = i - line_start;
                if (line_len > 0) {
                    std::string_view line = content.substr(line_start, line_len);

                    if (!line.empty() && line.back() == '\r') {
                        line.remove_suffix(1);
                    }

                    if (!line.empty()) {
                        if (is_first_line) {
                            m_headers = parse_line(line);
                            is_first_line = false;
                        } else {
                            auto row = parse_line(line);
                            rowCallback(m_headers, row);
                        }
                    }
                }

                // Обработка \r\n
                if (content[i] == '\r' && (i + 1) < content.size() && content[i + 1] == '\n') {
                    i++;
                }
                line_start = i + 1;
            }
        }

        if (line_start < content.size()) {
            std::string_view line = content.substr(line_start);
            if (!line.empty()) {
                if (!is_first_line) {
                    auto row = parse_line(line);
                    rowCallback(m_headers, row);
                }
            }
        }
    }
};
