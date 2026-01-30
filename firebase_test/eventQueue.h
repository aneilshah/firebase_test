// EventQueue.h
#pragma once

#include <Arduino.h>

#define DATA_SIZE 240
#define PATH_SIZE 120

class EventQueue {
public:
  static constexpr uint8_t kCapacity = 50;

  struct Item {
    char path[PATH_SIZE];          // Firebase RTDB path for this payload
    char data[DATA_SIZE];          // JSON payload string
    unsigned long createdLoop = 0; // loopCount when enqueued
    uint8_t retries = 0;
    bool used = false;
  };

  EventQueue() { clear(); }

  void clear() {
    _count = 0;
    _head = 0;
    _tail = 0;
    for (uint8_t i = 0; i < kCapacity; i++) {
      _items[i].path[0] = '\0';
      _items[i].data[0] = '\0';
      _items[i].createdLoop = 0;
      _items[i].retries = 0;
      _items[i].used = false;
    }
  }

  uint8_t size() const { return _count; }
  uint8_t capacity() const { return kCapacity; }
  bool empty() const { return _count == 0; }
  bool full() const { return _count == kCapacity; }

  // Add (path + JSON) to the queue.
  // If full, drops oldest first.
  // Returns false only if path/json are too large.
  bool enqueue(const String& pathStr, const String& jsonStr, unsigned long loopCount) {
    if (pathStr.length() >= PATH_SIZE) return false; // too large
    if (jsonStr.length() >= DATA_SIZE) return false; // too large (don't truncate JSON)

    if (full()) dropOldest();

    Item& slot = _items[_tail];

    slot.path[0] = '\0';
    slot.data[0] = '\0';

    pathStr.toCharArray(slot.path, PATH_SIZE);
    jsonStr.toCharArray(slot.data, DATA_SIZE);

    slot.createdLoop = loopCount;
    slot.retries = 0;
    slot.used = true;

    _tail = (_tail + 1) % kCapacity;
    _count++;
    return true;
  }

  // Delete the oldest item (FIFO)
  bool dropOldest() {
    if (empty()) return false;

    Item& slot = _items[_head];
    slot.path[0] = '\0';
    slot.data[0] = '\0';
    slot.createdLoop = 0;
    slot.retries = 0;
    slot.used = false;

    _head = (_head + 1) % kCapacity;
    _count--;
    return true;
  }

  // Read-only access to oldest item
  const Item* peekOldest() const {
    if (empty()) return nullptr;
    return &_items[_head];
  }

  // Mutable access to oldest item
  Item* peekOldestMutable() {
    if (empty()) return nullptr;
    return &_items[_head];
  }

  // Increment retry counter for oldest item
  bool incOldestRetries() {
    if (empty()) return false;
    if (_items[_head].retries < 255) _items[_head].retries++;
    return true;
  }

  // Set retry counter for oldest item
  bool setOldestRetries(uint8_t r) {
    if (empty()) return false;
    _items[_head].retries = r;
    return true;
  }

private:
  Item _items[kCapacity];
  uint16_t _count = 0;
  uint16_t _head = 0; // oldest
  uint16_t _tail = 0; // next insert
};
