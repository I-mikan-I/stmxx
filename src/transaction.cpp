module;
#include <memory>
#include <ranges>
#include <type_traits>
#include <concepts>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <optional>
#include <cassert>
#include <cstdlib>
export module STMXX:Transaction;
import Util;

export template <typename T, long long N = 0>
class transaction;

template <typename T>
class transaction_friend final {
 public:
  using unique_identifier = T::unique_identifier;
};

export template <typename T>
concept Transaction = requires(T t) {
  std::same_as<typename transaction_friend<T>::unique_identifier, unique_to_lib>;
};
class tval {
 public:
  virtual ~tval() {};
  tval(tval &other) = delete;

 protected:
  tval() {}

 private:
  virtual bool _check_version(version_t read_version) const = 0;
  friend bool check_tval_version(tval &tval, version_t read_version);
  template <Transaction Context>
  static inline Context *getCurrentTransaction() {
    return Context::thread_transaction;
  }
  template <Transaction Context>
  static inline version_t getReadVersion() {
    return Context::thread_transaction->read_version;
  }
  template <Transaction Context>
  static inline bool &getFailed() {
    return Context::thread_transaction->failed;
  }
  template <Transaction Context>
  static inline auto &getReadSet() {
    return Context::thread_transaction->read_set;
  }
  template <Transaction Context>
  static inline auto &getWriteMap() {
    return Context::thread_transaction->write_map;
  }
};
bool check_tval_version(tval &tval, version_t read_version) {
  return tval._check_version(read_version);
}
class written {
 public:
  virtual ~written() {};
  virtual const void *get() const = 0;
  virtual std::optional<version_t> try_lock() = 0;
  virtual bool try_set(version_t write_version) && = 0;

 protected:
  written() {}
};
export template <typename T, long long N = 0>
class transaction {
  // read set
  // write set
  // read version
  transaction(transaction<T, N> &other) = delete;

 public:
  template <std::invocable F>
  static auto start(F &&f) -> std::invoke_result<F>::type;

 private:
  using unique_identifier = unique_to_lib;
  transaction() : read_version(global_version.load(std::memory_order_acquire)) {};
  friend class tval;

  template <typename S>
  friend class transaction_friend;

  inline static thread_local transaction<T, N> *thread_transaction = nullptr;
  inline static std::atomic<version_t> global_version = version_start<version_t>;

  std::atomic<version_t> read_version;
  bool failed = false;
  std::unordered_set<tval *> read_set;
  std::unordered_map<tval *, std::unique_ptr<written>> write_map;
};

template <typename T, long long N>
template <std::invocable F>
auto transaction<T, N>::start(F &&f) -> std::invoke_result<F>::type {
  typename std::invoke_result<F>::type result;

  if (!thread_transaction) {
  retry:
    do {
      transaction<T, N> alloc;
      thread_transaction = &alloc;
      alloc.failed = false;
      result = f();
      std::unordered_map<tval *, version_t> owned_versions;
      if (!alloc.failed) {
        // lock all written values
        for (auto &[key, written] : alloc.write_map) {
          auto maybe_version = written->try_lock();
          if (!maybe_version) {
            goto retry;
          }
          owned_versions.insert_or_assign(key, maybe_version.value());
        }
        // check if read set is current
        for (auto &read : alloc.read_set) {
          auto [first, last] = owned_versions.equal_range(read);
          auto view = std::ranges::subrange(first, last);
          auto maybe_owned = std::ranges::empty(view)
                                 ? std::nullopt
                                 : std::optional((view | std::views::values).front());
          if (!maybe_owned
                   .transform([&alloc](auto &version) { return version <= alloc.read_version; })
                   .value_or(check_tval_version(*read, alloc.read_version))) {
            goto retry;
          }
        }
        // update write version
        auto write_version =
            std::atomic_fetch_add_explicit(&global_version, 1, std::memory_order::acq_rel) + 1;
        // update written values and unlock
        for (auto &written : alloc.write_map | std::views::values) {
          if (!std::move(*written).try_set(write_version)) {
            assert(false);
            goto retry;
          }
        }
      }
      if (!alloc.failed) {
        break;
      }
    } while (true);
    thread_transaction = nullptr;
  } else {
    result = f();
  }
  return result;
}