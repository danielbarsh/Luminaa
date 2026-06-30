#pragma once

#include <array>
#include <cstddef>
#include <vector>

// A flat, fixed-capacity pool. Slots are reused instead of allocated/freed,
// which keeps frame time predictable -- the kind of thing that matters once
// you start caring about worst-case latency instead of just average FPS.
template <typename T, std::size_t Capacity>
class ObjectPool {
public:
    ObjectPool() {
        freeIndices_.reserve(Capacity);
        for (std::size_t i = Capacity; i-- > 0;)
            freeIndices_.push_back(i);
    }

    T* acquire() {
        if (freeIndices_.empty())
            return nullptr;
        const std::size_t idx = freeIndices_.back();
        freeIndices_.pop_back();
        used_[idx] = true;
        storage_[idx] = T{};
        return &storage_[idx];
    }

    void release(T* ptr) {
        if (ptr == nullptr)
            return;
        const std::size_t idx = static_cast<std::size_t>(ptr - storage_.data());
        if (idx >= Capacity || !used_[idx])
            return;
        used_[idx] = false;
        freeIndices_.push_back(idx);
    }

    void clear() {
        used_.fill(false);
        freeIndices_.clear();
        freeIndices_.reserve(Capacity);
        for (std::size_t i = Capacity; i-- > 0;)
            freeIndices_.push_back(i);
    }

    template <typename Fn>
    void forEach(Fn&& fn) {
        for (std::size_t i = 0; i < Capacity; ++i)
            if (used_[i])
                fn(storage_[i]);
    }

    std::size_t aliveCount() const { return Capacity - freeIndices_.size(); }
    static constexpr std::size_t capacity() { return Capacity; }

private:
    std::array<T, Capacity> storage_{};
    std::array<bool, Capacity> used_{};
    std::vector<std::size_t> freeIndices_;
};
