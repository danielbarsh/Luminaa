#pragma once

#include <SFML/System/Vector2.hpp>

struct Ship {
    sf::Vector2f position{640.f, 360.f};
    sf::Vector2f velocity{0.f, 0.f};
    float rotationDeg = -90.f;
    float invulnerableTime = 2.f;
    bool thrusting = false;
};

struct Asteroid {
    sf::Vector2f position{};
    sf::Vector2f velocity{};
    float radius = 40.f;
    float rotationDeg = 0.f;
    float rotationSpeedDeg = 0.f;
    int generation = 0; // 0 = full size, increases as it splits
    bool hit = false; // set by the (parallel) collision pass, consumed on the next update
};

struct Bullet {
    sf::Vector2f position{};
    sf::Vector2f velocity{};
    float timeToLive = 1.1f;
    bool hit = false;
};
