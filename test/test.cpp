module;
#include <algorithm>
#include <functional>
#include <atomic>
#include <thread>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>
export module Test;
import Lib;
using namespace std::chrono_literals;

#if THREAD_SANITIZER
constexpr int THREADS = 10;
#else
constexpr int THREADS = 100;
#endif

template <typename V, typename T>
void modify_tval(transaction_t<V, T> *val, V &&to_set) {
  *val = std::forward<V>(to_set);
}

TEST_CASE("Single Threaded basic case.") {
  using T = transaction<int, 1>;
  transaction_t<long long unsigned, T> tval = 5;

  REQUIRE(*tval == 5);
  T::start([&] {
    auto val = *tval;
    REQUIRE((val && *val == 5));
    modify_tval(&tval, 100LLU);
    val = *tval;
    REQUIRE((val && *val == 100));
    return 0;
  });
  REQUIRE(**tval == 100);
}

TEST_CASE("Multi Threaded basic case.") {
  auto sleep_for = GENERATE(take(10, chunk(THREADS, random(0, 2))));
  using T = transaction<int, 2>;
  std::vector<std::thread> threads;
  transaction_t<long long unsigned, T> tval = 0;
  for (int i = 0; i < THREADS; ++i) {
    threads.emplace_back([&, i] {
      T::start([&] {
        const auto val = *tval;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for[i]));
        if (val) {
          modify_tval(&tval, *val + 1);
        }
        return 0;
      });
    });
  }

  for (auto &t : threads) {
    t.join();
  }
  std::atomic_thread_fence(std::memory_order_seq_cst);
  REQUIRE(**tval == THREADS);
}

TEST_CASE("Multi Threaded recursive case.") {
  auto sleep_for = GENERATE(take(10, chunk(THREADS, random(0, 2))));
  using T = transaction<int, 3>;
  std::vector<std::thread> threads;
  transaction_t<long long unsigned, T> tval = 0;
  for (int i = 0; i < THREADS; ++i) {
    threads.emplace_back([&, i] {
      std::function<int(int)> fn = [&](int ctr) -> int {
        return T::start([&] {
          auto val = *tval;
          std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for[i]));
          if (val) {
            modify_tval(&tval, *val + 1);
          }
          if (ctr) {
            return fn(ctr - 1);
          }
          return 0;
        });
      };
      fn(9);
    });
  }

  for (auto &t : threads) {
    t.join();
  }
  std::atomic_thread_fence(std::memory_order_seq_cst);
  REQUIRE(**tval == THREADS * 10);
}

TEST_CASE("Multi Threaded multi transactions.") {
  auto sleep_for = GENERATE(take(20, chunk(2, random(0, 30))));
  std::atomic<bool> FAILED = false;
  using T = transaction<int, 4>;
  using T2 = transaction<int, 5>;
  std::vector<std::thread> threads;
  transaction_t<long long unsigned, T> tval = 0;
  transaction_t<long long unsigned, T2> tval2 = 0;
  threads.emplace_back([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for[0]));
    for (int i = 0; i < 100; ++i) {
      T::start([&] {
        auto val = *tval;
        if (val) {
          modify_tval(&tval, *val + 1);
        } else {
          FAILED = true;
        }
        return 0;
      });
    }
  });
  threads.emplace_back([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for[1]));
    for (int i = 0; i < 100; ++i) {
      T2::start([&] {
        auto val = *tval2;
        if (val) {
          modify_tval(&tval2, *val + 1);
        } else {
          FAILED = true;
        }
        return 0;
      });
    }
  });

  for (auto &t : threads) {
    t.join();
  }
  std::atomic_thread_fence(std::memory_order_seq_cst);
  REQUIRE(**tval == 100);
  REQUIRE(**tval2 == 100);
  REQUIRE(!FAILED);
}

TEST_CASE("Class member access single threaded.") {
  struct MyClass {
    long long unsigned x;
    long long unsigned y;
  };

  using T = transaction<int, 6>;
  transaction_t<MyClass, T> tval = {};

  auto xptr = &MyClass::x;
  auto yptr = &MyClass::y;

  const unsigned long long val = *(tval->*xptr);
  REQUIRE(val == 0);
}

TEST_CASE("Class member access multi threaded.") {
  struct MyClass {
    long long unsigned x;
    long long unsigned y;
  };

  using T = transaction<int, 7>;

  auto sleep_for = GENERATE(take(10, chunk(THREADS, random(0, 10))));
  transaction_t<MyClass, T> tval = {};

  std::vector<std::thread> threads;

  for (int i = 0; i < THREADS; i++) {
    threads.emplace_back([&, i] {
      return T::start([&] {
        auto xval = tval->*&MyClass::x;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for[i]));
        if (xval) {
          tval = (MyClass){.x = *xval + 1, .y = *xval * 2};
        }
        return 0;
      });
    });
  }
  std::ranges::for_each(threads, [](auto &thread) { thread.join(); });

  std::atomic_thread_fence(std::memory_order_seq_cst);
  REQUIRE((**tval).x == THREADS);
  REQUIRE((**tval).y == THREADS * 2 - 2);
}

TEST_CASE("Class function member access multi threaded.") {
  struct MyClass {
    long long unsigned x;
    long long unsigned y;
    long long unsigned getX() const {
      return x;
    }
  };

  using T = transaction<int, 7>;

  auto sleep_for = GENERATE(take(10, chunk(THREADS, random(0, 10))));
  transaction_t<MyClass, T> tval = {};

  std::vector<std::thread> threads;

  for (int i = 0; i < THREADS; i++) {
    threads.emplace_back([&, i] {
      return T::start([&] {
        auto xval = (tval->*&MyClass::getX)();
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for[i]));
        if (xval) {
          tval = (MyClass){.x = *xval + 1, .y = *xval * 2};
        }
        return 0;
      });
    });
  }
  std::ranges::for_each(threads, [](auto &thread) { thread.join(); });

  std::atomic_thread_fence(std::memory_order_seq_cst);
  REQUIRE((**tval).x == THREADS);
  REQUIRE((**tval).y == THREADS * 2 - 2);
}