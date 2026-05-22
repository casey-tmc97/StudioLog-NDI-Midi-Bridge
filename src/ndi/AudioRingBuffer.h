#pragma once
#include <atomic>
#include <array>
#include <cstddef>

namespace StudioLog {

/// Lock-free Single-Producer / Single-Consumer ring buffer for float32 audio samples.
///
/// Thread-safety contract:
///   - Exactly one thread may call write() at a time  (NDI receive thread)
///   - Exactly one thread may call read()  at a time  (LTC decode thread)
///
/// Capacity must be a power of two (default: 8 192 samples ≈ 170 ms at 48 kHz).
///
/// Implementation uses monotonically-increasing 64-bit indices that never wrap
/// within the lifetime of the process, masked with (Capacity-1) for array access.
/// This avoids the classic "full vs empty" ambiguity without a wasted slot.
template<std::size_t Capacity = 8192>
class AudioRingBuffer {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "AudioRingBuffer: Capacity must be a non-zero power of two");

    static constexpr std::size_t MASK = Capacity - 1u;

public:
    AudioRingBuffer() = default;

    // Non-copyable, non-movable (atomics cannot be copied)
    AudioRingBuffer(const AudioRingBuffer&)            = delete;
    AudioRingBuffer& operator=(const AudioRingBuffer&) = delete;

    // ── Producer API ─────────────────────────────────────────────────────────

    /// Write up to @p count samples from @p src.
    /// @return Number of samples actually written (may be less than @p count if
    ///         the buffer is full).
    std::size_t write(const float* src, std::size_t count) noexcept {
        const std::size_t w    = writeIdx_.load(std::memory_order_relaxed);
        const std::size_t r    = readIdx_.load(std::memory_order_acquire);
        const std::size_t free = Capacity - (w - r);       // always ≤ Capacity
        const std::size_t n    = (count < free) ? count : free;

        for (std::size_t i = 0; i < n; ++i) {
            buf_[(w + i) & MASK] = src[i];
        }

        writeIdx_.store(w + n, std::memory_order_release);
        return n;
    }

    // ── Consumer API ─────────────────────────────────────────────────────────

    /// Read up to @p count samples into @p dst.
    /// @return Number of samples actually read (may be less than @p count if
    ///         not enough data is available).
    std::size_t read(float* dst, std::size_t count) noexcept {
        const std::size_t r    = readIdx_.load(std::memory_order_relaxed);
        const std::size_t w    = writeIdx_.load(std::memory_order_acquire);
        const std::size_t avail = w - r;                    // always ≤ Capacity
        const std::size_t n    = (count < avail) ? count : avail;

        for (std::size_t i = 0; i < n; ++i) {
            dst[i] = buf_[(r + i) & MASK];
        }

        readIdx_.store(r + n, std::memory_order_release);
        return n;
    }

    // ── Introspection (approximate — can be called from either thread) ───────

    /// Samples available to read (may be stale by the time the caller acts on it).
    std::size_t readAvailable() const noexcept {
        return writeIdx_.load(std::memory_order_acquire) -
               readIdx_.load(std::memory_order_relaxed);
    }

    /// Space available for writing.
    std::size_t writeAvailable() const noexcept {
        return Capacity - readAvailable();
    }

    /// Reset to empty.  Call only when both producer and consumer are idle.
    void reset() noexcept {
        readIdx_.store(0,  std::memory_order_relaxed);
        writeIdx_.store(0, std::memory_order_relaxed);
        // No need to clear buf_ — stale data will simply be overwritten.
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    // Each index lives on its own cache line to avoid false sharing between
    // the producer (writeIdx_) and consumer (readIdx_).
    alignas(64) std::atomic<std::size_t> writeIdx_{0};
    alignas(64) std::atomic<std::size_t> readIdx_{0};
    alignas(64) std::array<float, Capacity> buf_{};
};

} // namespace StudioLog
