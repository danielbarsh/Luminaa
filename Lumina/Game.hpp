#pragma once

#include <SFML/Graphics.hpp>
#include <random>
#include <vector>

#include "Entities.hpp"
#include "ObjectPool.hpp"
#include "Profiler.hpp"
#include "ThreadPool.hpp"

class Game {
public:
    Game();
    void run();

private:
    static constexpr unsigned WindowWidth = 1280;
    static constexpr unsigned WindowHeight = 720;
    static constexpr std::size_t MaxAsteroids = 256;
    static constexpr std::size_t MaxBullets = 128;

    void processEvents();
    void handleInput(float dt);
    void update(float dt);
    void render();

    void updateShip(float dt);
    void updateAsteroids(float dt);
    void updateBullets(float dt);
    void runCollisionPass();
    void resolveHits();
    void spawnWave(int count);
    void splitAsteroid(Asteroid& asteroid);
    void fireBullet();
    void wrapPosition(sf::Vector2f& pos) const;

    sf::RenderWindow window_;
    sf::Font font_;
    bool fontLoaded_ = false;

    Ship ship_;
    ObjectPool<Asteroid, MaxAsteroids> asteroids_;
    ObjectPool<Bullet, MaxBullets> bullets_;

    Profiler profiler_;
    ThreadPool threadPool_;

    sf::Clock frameClock_;
    sf::Clock shotCooldownClock_;
    float shotCooldown_ = 0.22f;

    std::mt19937 rng_{std::random_device{}()};

    int score_ = 0;
    int lives_ = 3;
    bool gameOver_ = false;
};
