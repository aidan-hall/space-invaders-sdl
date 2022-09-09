#ifndef UNUSUAL_CIRCULAR_QUEUE_H
#define UNUSUAL_CIRCULAR_QUEUE_H

#include <cstddef>

namespace Unusual {
// The successor of 'x' in a 'looping' ascending sequence, length 'len'.
// E.g. loop_successor(0, 5) = 1, loop_successor(4, 5) = 0
inline constexpr size_t loop_successor(const size_t x, const size_t len) {
  const size_t succ = x + 1;
  return (succ >= len) ? succ - len : succ;
}

template <typename T, size_t N> struct CircularQueue {
  T elements[N] = {};
  size_t start = 0; // Index of the first element.
  size_t end = 0;   // Index after the last element.
  size_t size = 0;  // Number of elements in the queue.

  inline constexpr size_t capacity() { return N; }
  inline bool empty() { return size == 0; }
  inline bool full() { return size == N; }

  inline void enqueue(const T &value) {
    assert(not full());
    elements[end] = value;
    size += 1;
    end = loop_successor(end, N);
  }

  inline void dequeue() {
    assert(not empty());
    start = loop_successor(start, N);
    size -= 1;
  }

  inline T front() {
    assert(not empty());
    return elements[start];
  }
};
} // namespace Unusual

#endif // UNUSUAL_CIRCULAR_QUEUE_H
