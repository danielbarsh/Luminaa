#pragma once

#include <SFML/Graphics.hpp>
#include <array>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

// Fixed-size ring buffer of recent sample values, used both for the live
// graph and for rolling min/max/avg stats.
template <std::size_t N>
class RingBuffer {
public:
    void push(float value) {
        data_[head_] = value;
        head_ = (head_ + 1) % N;
        if (count_ < N) ++count_;
    }

    float average() const {
        if (count_ == 0) return 0.f;
        float sum = 0.f;
        for (std::size_t i = 0; i < count_; ++i) sum += data_[i];
        return sum / static_cast<float>(count_);
    }

    float max() const {
        float m = 0.f;
        for (std::size_t i = 0; i < count_; ++i) m = std::max(m, data_[i]);
        return m;
    }

    std::size_t count() const { return count_; }
    std::size_t headIndex() const { return head_; }
    float at(std::size_t i) const { return data_[i]; }
    static constexpr std::size_t capacity() { return N; }

private:
    std::array<float, N> data_{};
    std::size_t head_ = 0;
    std::size_t count_ = 0;
};

// Records named timing samples (in milliseconds) per frame and can render
// itself as an overlay. Categories are created lazily on first use so call
// sites stay one-liners.
class Profiler {
public:
    static constexpr std::size_t HistorySize = 180; // ~3 seconds at 60fps

    class ScopedTimer {
    public:
        ScopedTimer(Profiler& profiler, std::string category)
            : profiler_(profiler), category_(std::move(category)),
              start_(std::chrono::high_resolution_clock::now()) {}

        ~ScopedTimer() {
            const auto end = std::chrono::high_resolution_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(end - start_).count();
            profiler_.record(category_, static_cast<float>(ms));
        }

    private:
        Profiler& profiler_;
        std::string category_;
        std::chrono::high_resolution_clock::time_point start_;
    };

    void record(const std::string& category, float milliseconds) {
        history_[category].push(milliseconds);
    }

    void setEntityCount(const std::string& label, std::size_t count) {
        entityCounts_[label] = count;
    }

    void setThreadCount(std::size_t n) { threadCount_ = n; }

    bool visible = false;

    void draw(sf::RenderWindow& window, const sf::Font* font) {
        if (!visible) return;

        const float panelWidth = 360.f;
        const float panelHeight = 230.f;
        const sf::Vector2f origin{10.f, 10.f};

        sf::RectangleShape bg({panelWidth, panelHeight});
        bg.setPosition(origin);
        bg.setFillColor(sf::Color(10, 10, 18, 200));
        bg.setOutlineColor(sf::Color(90, 200, 255, 180));
        bg.setOutlineThickness(1.f);
        window.draw(bg);

        drawGraph(window, "frame", origin + sf::Vector2f(10.f, 10.f), {panelWidth - 20.f, 60.f},
                  sf::Color(90, 200, 255));

        if (font) {
            float y = origin.y + 80.f;
            for (const auto& name : {"frame", "update", "render", "collision"}) {
                auto it = history_.find(name);
                if (it == history_.end()) continue;
                const float avg = it->second.average();
                const float mx = it->second.max();
                drawLine(window, *font, origin.x + 10.f, y,
                         std::string(name) + ": avg " + format(avg) + "ms  max " + format(mx) + "ms");
                y += 18.f;
            }

            y += 6.f;
            for (const auto& [label, count] : entityCounts_) {
                drawLine(window, *font, origin.x + 10.f, y, label + ": " + std::to_string(count));
                y += 18.f;
            }

            drawLine(window, *font, origin.x + 10.f, y, "worker threads: " + std::to_string(threadCount_));
        }
    }

private:
    static std::string format(float v) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", v);
        return buf;
    }

    void drawLine(sf::RenderWindow& window, const sf::Font& font, float x, float y, const std::string& s) {
        sf::Text text(font, s, 14);
        text.setPosition({x, y});
        text.setFillColor(sf::Color(220, 230, 240));
        window.draw(text);
    }

    void drawGraph(sf::RenderWindow& window, const std::string& category, sf::Vector2f pos,
                   sf::Vector2f size, sf::Color color) {
        auto it = history_.find(category);
        if (it == history_.end() || it->second.count() < 2) return;

        const auto& buf = it->second;
        const float scaleMax = std::max(buf.max(), 16.6f); // keep ~60fps line visible
        const std::size_t n = buf.count();

        sf::VertexArray line(sf::PrimitiveType::LineStrip, n);
        const std::size_t startIdx = (buf.headIndex() + RingBuffer<HistorySize>::capacity() - n) %
                                      RingBuffer<HistorySize>::capacity();

        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t idx = (startIdx + i) % RingBuffer<HistorySize>::capacity();
            const float v = buf.at(idx);
            const float px = pos.x + (static_cast<float>(i) / static_cast<float>(n - 1)) * size.x;
            const float py = pos.y + size.y - (v / scaleMax) * size.y;
            line[i].position = {px, py};
            line[i].color = color;
        }
        window.draw(line);

        sf::RectangleShape frame(size);
        frame.setPosition(pos);
        frame.setFillColor(sf::Color::Transparent);
        frame.setOutlineColor(sf::Color(60, 70, 90));
        frame.setOutlineThickness(1.f);
        window.draw(frame);
    }

    std::unordered_map<std::string, RingBuffer<HistorySize>> history_;
    std::unordered_map<std::string, std::size_t> entityCounts_;
    std::size_t threadCount_ = 0;
};

#define PROFILE_CONCAT_(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_(a, b)
#define PROFILE_SCOPE(profiler, name) \
    Profiler::ScopedTimer PROFILE_CONCAT(_profScope, __LINE__)((profiler), (name))
