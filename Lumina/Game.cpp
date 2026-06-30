#include "Game.hpp"

#include <cmath>
#include <mutex>

namespace {
constexpr float kPi = 3.14159265358979323846f;

sf::Vector2f forwardVector(float rotationDeg) {
    const float rad = rotationDeg * kPi / 180.f;
    return {std::cos(rad), std::sin(rad)};
}
} // namespace

Game::Game()
    : window_(sf::VideoMode({WindowWidth, WindowHeight}), "Vector Siege"),
      threadPool_(std::max(2u, std::thread::hardware_concurrency())) {
    window_.setFramerateLimit(144);
    fontLoaded_ = font_.openFromFile("assets/font.ttf");
    profiler_.setThreadCount(threadPool_.threadCount());
    spawnWave(4);
}

void Game::run() {
    while (window_.isOpen()) {
        PROFILE_SCOPE(profiler_, "frame");

        float dt = frameClock_.restart().asSeconds();
        dt = std::min(dt, 0.05f); // avoid huge steps after a stall (e.g. window drag)

        processEvents();
        handleInput(dt);

        if (!gameOver_) {
            PROFILE_SCOPE(profiler_, "update");
            update(dt);
        }

        render();
    }
}

void Game::processEvents() {
    while (const std::optional event = window_.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window_.close();
        } else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            if (keyPressed->code == sf::Keyboard::Key::Escape) {
                window_.close();
            } else if (keyPressed->code == sf::Keyboard::Key::F1) {
                profiler_.visible = !profiler_.visible;
            } else if (keyPressed->code == sf::Keyboard::Key::R && gameOver_) {
                ship_ = Ship{};
                asteroids_.clear();
                bullets_.clear();
                score_ = 0;
                lives_ = 3;
                gameOver_ = false;
                spawnWave(4);
            }
        }
    }
}

void Game::handleInput(float dt) {
    if (gameOver_) return;

    constexpr float turnRateDeg = 220.f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
        ship_.rotationDeg -= turnRateDeg * dt;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
        ship_.rotationDeg += turnRateDeg * dt;

    ship_.thrusting = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) ||
                       sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) &&
        shotCooldownClock_.getElapsedTime().asSeconds() >= shotCooldown_) {
        fireBullet();
        shotCooldownClock_.restart();
    }
}

void Game::update(float dt) {
    updateShip(dt);
    updateAsteroids(dt);
    updateBullets(dt);

    {
        PROFILE_SCOPE(profiler_, "collision");
        runCollisionPass();
    }
    resolveHits();

    profiler_.setEntityCount("asteroids", asteroids_.aliveCount());
    profiler_.setEntityCount("bullets", bullets_.aliveCount());

    if (asteroids_.aliveCount() == 0 && !gameOver_) {
        spawnWave(5 + score_ / 200);
    }
}

void Game::updateShip(float dt) {
    constexpr float thrustAccel = 260.f;
    constexpr float maxSpeed = 380.f;
    constexpr float damping = 0.992f;

    if (ship_.thrusting) {
        ship_.velocity += forwardVector(ship_.rotationDeg) * thrustAccel * dt;
        const float speed = ship_.velocity.length();
        if (speed > maxSpeed)
            ship_.velocity *= (maxSpeed / speed);
    }
    ship_.velocity *= damping;
    ship_.position += ship_.velocity * dt;
    wrapPosition(ship_.position);

    if (ship_.invulnerableTime > 0.f)
        ship_.invulnerableTime -= dt;
}

void Game::updateAsteroids(float dt) {
    asteroids_.forEach([&](Asteroid& a) {
        a.position += a.velocity * dt;
        a.rotationDeg += a.rotationSpeedDeg * dt;
        wrapPosition(a.position);
    });
}

void Game::updateBullets(float dt) {
    bullets_.forEach([&](Bullet& b) {
        b.position += b.velocity * dt;
        wrapPosition(b.position);
        b.timeToLive -= dt;
        if (b.timeToLive <= 0.f)
            bullets_.release(&b);
    });
}

void Game::runCollisionPass() {
    std::vector<Bullet*> bulletPtrs;
    std::vector<Asteroid*> asteroidPtrs;
    bulletPtrs.reserve(bullets_.aliveCount());
    asteroidPtrs.reserve(asteroids_.aliveCount());

    bullets_.forEach([&](Bullet& b) {
        b.hit = false;
        bulletPtrs.push_back(&b);
    });
    asteroids_.forEach([&](Asteroid& a) {
        a.hit = false;
        asteroidPtrs.push_back(&a);
    });

    if (bulletPtrs.empty() || asteroidPtrs.empty())
        return;

    // Broad-phase collision is the classic "embarrassingly parallel" workload:
    // each bullet's check against every asteroid is independent. Threads only
    // synchronize once per chunk, when merging local results, rather than
    // per-pair, to keep lock contention off the hot path.
    std::mutex mergeMutex;
    std::vector<std::pair<Bullet*, Asteroid*>> hits;

    threadPool_.parallelFor(bulletPtrs.size(), [&](std::size_t begin, std::size_t end) {
        std::vector<std::pair<Bullet*, Asteroid*>> local;
        for (std::size_t i = begin; i < end; ++i) {
            Bullet* b = bulletPtrs[i];
            for (Asteroid* a : asteroidPtrs) {
                const sf::Vector2f diff = b->position - a->position;
                const float distSq = diff.x * diff.x + diff.y * diff.y;
                if (distSq <= a->radius * a->radius) {
                    local.emplace_back(b, a);
                    break;
                }
            }
        }
        if (!local.empty()) {
            std::lock_guard<std::mutex> lock(mergeMutex);
            hits.insert(hits.end(), local.begin(), local.end());
        }
    });

    for (auto& [b, a] : hits) {
        b->hit = true;
        a->hit = true;
    }
}

void Game::resolveHits() {
    std::vector<Bullet*> deadBullets;
    bullets_.forEach([&](Bullet& b) {
        if (b.hit) deadBullets.push_back(&b);
    });
    for (auto* b : deadBullets)
        bullets_.release(b);

    std::vector<Asteroid*> deadAsteroids;
    asteroids_.forEach([&](Asteroid& a) {
        if (a.hit) deadAsteroids.push_back(&a);
    });
    for (auto* a : deadAsteroids) {
        score_ += (a->generation == 0) ? 20 : (a->generation == 1 ? 50 : 100);
        splitAsteroid(*a);
        asteroids_.release(a);
    }

    if (ship_.invulnerableTime <= 0.f) {
        bool shipHit = false;
        asteroids_.forEach([&](Asteroid& a) {
            if (shipHit) return;
            const sf::Vector2f diff = ship_.position - a.position;
            const float distSq = diff.x * diff.x + diff.y * diff.y;
            const float collideR = a.radius + 12.f;
            if (distSq <= collideR * collideR)
                shipHit = true;
        });
        if (shipHit) {
            --lives_;
            ship_.velocity = {0.f, 0.f};
            ship_.position = {WindowWidth / 2.f, WindowHeight / 2.f};
            ship_.invulnerableTime = 2.f;
            if (lives_ <= 0)
                gameOver_ = true;
        }
    }
}

void Game::spawnWave(int count) {
    std::uniform_real_distribution<float> edgeT(0.f, 1.f);
    std::uniform_real_distribution<float> speedDist(30.f, 90.f);
    std::uniform_real_distribution<float> angleDist(0.f, 360.f);
    std::uniform_real_distribution<float> spinDist(-60.f, 60.f);

    for (int i = 0; i < count; ++i) {
        Asteroid* a = asteroids_.acquire();
        if (!a) break;

        // Spawn along the border so the player has a moment to react.
        const float t = edgeT(rng_);
        if (t < 0.25f) a->position = {t / 0.25f * WindowWidth, 0.f};
        else if (t < 0.5f) a->position = {WindowWidth, (t - 0.25f) / 0.25f * WindowHeight};
        else if (t < 0.75f) a->position = {(1.f - (t - 0.5f) / 0.25f) * WindowWidth, WindowHeight};
        else a->position = {0.f, (1.f - (t - 0.75f) / 0.25f) * WindowHeight};

        const float speed = speedDist(rng_);
        const float dir = angleDist(rng_);
        a->velocity = forwardVector(dir) * speed;
        a->radius = 42.f;
        a->generation = 0;
        a->rotationSpeedDeg = spinDist(rng_);
    }
}

void Game::splitAsteroid(Asteroid& asteroid) {
    if (asteroid.generation >= 2)
        return;

    std::uniform_real_distribution<float> angleDist(0.f, 360.f);
    std::uniform_real_distribution<float> speedDist(60.f, 140.f);

    for (int i = 0; i < 2; ++i) {
        Asteroid* child = asteroids_.acquire();
        if (!child) return;
        child->position = asteroid.position;
        child->radius = asteroid.radius * 0.6f;
        child->generation = asteroid.generation + 1;
        child->velocity = forwardVector(angleDist(rng_)) * speedDist(rng_);
        child->rotationSpeedDeg = asteroid.rotationSpeedDeg * 1.4f;
    }
}

void Game::fireBullet() {
    Bullet* b = bullets_.acquire();
    if (!b) return;
    const sf::Vector2f fwd = forwardVector(ship_.rotationDeg);
    b->position = ship_.position + fwd * 22.f;
    b->velocity = ship_.velocity + fwd * 420.f;
    b->timeToLive = 1.1f;
}

void Game::wrapPosition(sf::Vector2f& pos) const {
    if (pos.x < 0.f) pos.x += WindowWidth;
    if (pos.x > WindowWidth) pos.x -= WindowWidth;
    if (pos.y < 0.f) pos.y += WindowHeight;
    if (pos.y > WindowHeight) pos.y -= WindowHeight;
}

void Game::render() {
    PROFILE_SCOPE(profiler_, "render");
    window_.clear(sf::Color(8, 10, 20));

    asteroids_.forEach([&](Asteroid& a) {
        const unsigned points = a.generation == 0 ? 10 : (a.generation == 1 ? 8 : 6);
        sf::CircleShape shape(a.radius, points);
        shape.setOrigin({a.radius, a.radius});
        shape.setPosition(a.position);
        shape.setRotation(sf::degrees(a.rotationDeg));
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineColor(sf::Color(180, 190, 210));
        shape.setOutlineThickness(2.f);
        window_.draw(shape);
    });

    bullets_.forEach([&](Bullet& b) {
        sf::CircleShape shape(3.f);
        shape.setOrigin({3.f, 3.f});
        shape.setPosition(b.position);
        shape.setFillColor(sf::Color(255, 210, 90));
        window_.draw(shape);
    });

    if (!gameOver_) {
        sf::ConvexShape ship(3);
        ship.setPoint(0, {18.f, 0.f});
        ship.setPoint(1, {-14.f, 10.f});
        ship.setPoint(2, {-14.f, -10.f});
        ship.setPosition(ship_.position);
        ship.setRotation(sf::degrees(ship_.rotationDeg));
        const bool blinking = ship_.invulnerableTime > 0.f && std::fmod(ship_.invulnerableTime, 0.3f) > 0.15f;
        ship.setFillColor(blinking ? sf::Color(90, 200, 255, 90) : sf::Color(90, 200, 255));
        window_.draw(ship);
    }

    if (fontLoaded_) {
        sf::Text hud(font_, "Score: " + std::to_string(score_) + "   Lives: " + std::to_string(lives_), 22);
        hud.setPosition({WindowWidth - 320.f, 10.f});
        hud.setFillColor(sf::Color(230, 235, 245));
        window_.draw(hud);

        sf::Text hint(font_, "F1: profiler   WASD/Arrows: fly   Space: fire", 14);
        hint.setPosition({WindowWidth - 320.f, 40.f});
        hint.setFillColor(sf::Color(140, 150, 165));
        window_.draw(hint);

        if (gameOver_) {
            sf::Text over(font_, "GAME OVER  -  press R to restart", 32);
            over.setPosition({WindowWidth / 2.f - 230.f, WindowHeight / 2.f - 20.f});
            over.setFillColor(sf::Color(255, 120, 120));
            window_.draw(over);
        }
    }

    profiler_.draw(window_, fontLoaded_ ? &font_ : nullptr);
    window_.display();
}
