module;
#include <atomic>
#include <thread>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>
export module Test;
import Lib;
using namespace std::chrono_literals;

template <typename V, typename T>
void modify_tval(transaction_t<V, T> *val, V &&to_set) {
  *val = std::forward<V>(to_set);
}

TEST_CASE("Single Threaded basic case.") {
  using T = transaction<int, 1>;
  transaction_t<long long unsigned, T> tval = 5;

  REQUIRE(*tval.read() == 5);
  T::start([&] {
    const auto *val = tval.read();
    REQUIRE((val && *val == 5));
    modify_tval(&tval, 100LLU);
    val = tval.read();
    REQUIRE((val && *val == 100));
    return 0;
  });
  REQUIRE(*tval.read() == 100);
}

TEST_CASE("Multi Threaded basic case.") {
  constexpr int THREADS = 200;
  auto sleep_for = GENERATE(take(100, chunk(THREADS, random(0, 20))));
  using T = transaction<int, 2>;
  std::vector<std::thread> threads;
  transaction_t<long long unsigned, T> tval = 0;
  for (int i = 0; i < THREADS; ++i) {
    threads.emplace_back([&] {
      T::start([&] {
        const auto *val = tval.read();
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
  REQUIRE(*tval.read() == THREADS);
}