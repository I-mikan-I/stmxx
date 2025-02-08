module;
#include <memory>
#include <ranges>
#include <type_traits>
#include <concepts>
#include <utility>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <atomic>
#include <optional>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <ostream>
export module Lib;
/*
  for all global data:
    * update version
    * lock
  one global version
  per transaction:
    * read set
    * write set
    * read-version

  option 1:
    * dynamically detect new objects on first access
      * requires global store and address comparison on each access to data
  option 2:
    * static declaration of shared data
    * templated type
*/
template <typename T>
constexpr T version_start = 0;
using version_t = unsigned int;
/*
GLOBAL TRANSACTION OBJECT
*/
class unique_to_module final {};
template <typename T>
class transaction_friend final {
 public:
  using unique_identifier = T::unique_identifier;
};
template <typename T>
concept Transaction = requires(T t) {
  std::same_as<typename transaction_friend<T>::unique_identifier, unique_to_module>;
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
};
bool check_tval_version(tval &tval, version_t read_version) {
  return tval._check_version(read_version);
}
class written {
 public:
  virtual ~written() {};
  virtual const void *get() const = 0;
  virtual bool try_lock() = 0;
  virtual bool try_set(version_t write_version) && = 0;

 protected:
  written() {}
};
export template <typename T, Transaction Context>
class transaction_t;
template <typename T, Transaction Context>
class written_t final : public written {
 public:
  written_t(T &&val, transaction_t<T, Context> &to_set) : t(std::forward<T>(val)), to_set(to_set) {}
  const void *get() const { return &t; }
  bool try_lock() {
    lock = std::move(to_set.try_lock());
    return lock.has_value();
  }
  bool try_set(version_t write_version) && {
    if (lock.has_value()) {
      to_set.set_val(std::move(t), write_version, std::move(lock.value()));
      lock = std::nullopt;
      return true;
    }
    return false;
  }

 private:
  T t;
  transaction_t<T, Context> &to_set;
  std::optional<typename transaction_t<T, Context>::transaction_lock> lock = std::nullopt;
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
  using unique_identifier = unique_to_module;
  transaction() : read_version(global_version.load(std::memory_order_acquire)) {};
  template <typename K, Transaction M>
  friend class transaction_t;
  friend transaction_friend<transaction<T, N>>;

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
  transaction<T, N> alloc;
  typename std::invoke_result<F>::type result;
  thread_transaction = &alloc;
  do {
    alloc.failed = false;
    result = f();
    if (!alloc.failed) {
      for (auto &written : alloc.write_map | std::views::values) {
        if (!written->try_lock()) {
          continue;
        }
      }
      for (auto &read : alloc.read_set) {
        if (!check_tval_version(*read, alloc.read_version)) {
          continue;
        }
      }
      auto write_version =
          std::atomic_fetch_add_explicit(&global_version, 1, std::memory_order::acq_rel);
      for (auto &written : alloc.write_map | std::views::values) {
        if (!std::move(*written).try_set(write_version)) {
          continue;
        }
      }
    }
  } while (alloc.failed);
  thread_transaction = nullptr;
  return result;
}

export template <typename T, Transaction Context>
class transaction_t final : public tval {
 public:
  class transaction_lock final {
   public:
    transaction_lock(transaction_lock &other) = delete;
    transaction_lock(transaction_lock &&other)
        : is_locked(other.is_locked), locked(other.locked), version(other.version) {
      other.is_locked = false;
    };
    void operator=(transaction_lock &other) = delete;
    transaction_lock &operator=(transaction_lock &&other) {
      unlock();
      version = other.version;
      locked = other.locked;
      is_locked = other.is_locked;
      other.is_locked = false;
      return *this;
    }
    ~transaction_lock() { unlock(); }
    void set_version(version_t write_version) { version = write_version; }

   private:
    friend transaction_t<T, Context>;
    explicit transaction_lock(transaction_t<T, Context> &locked, version_t version)
        : locked(&locked), version(version), is_locked(true) {}
    void unlock() {
      if (is_locked) {
        is_locked = false;
        std::optional<version_t> lockedval = std::nullopt;
        auto success = locked->version.compare_exchange_strong(lockedval, version);
        if (!success) {
          std::cerr << "BUG: tried unlocking already unlocked lock" << std::endl;
          std::exit(EXIT_FAILURE);
        }
      }
    }
    transaction_t<T, Context> *locked;
    version_t version;
    bool is_locked;
  };
  template <typename... U>
  transaction_t(U &&...args) : t(T(std::forward<U>(args)...)) {}

  transaction_t(T &&t) : t(std::move(t)) {}

  transaction_t(transaction_t<T, Context> &other) = delete;

  void operator=(transaction_t<T, Context> &other) = delete;

  const T *read() const {
    if (Context::thread_transaction) {
      return get_val(Context::thread_transaction->read_version);
    } else {
      return &t;
    }
  }

  template <typename U>
  transaction_t<T, Context> &operator=(U &&val) {
    if (Context::thread_transaction) {
      if (!Context::thread_transaction->failed) {
        Context::thread_transaction->write_map.insert_or_assign(
            this, std::make_unique<written_t<T, Context>>(written_t(std::forward<U>(val), *this)));
      }
    } else {
      t = std::forward<T>(val);
    }
    return *this;
  }

 private:
  static constexpr void _static_checks() noexcept;

  const T *get_val(version_t read_version) const {
    assert(Context::thread_transaction);
    if (_check_version(read_version) && !Context::thread_transaction->failed) {
      Context::thread_transaction->read_set.insert(
          const_cast<transaction_t<T, Context> *const>(this));
      auto non_committed_val = Context::thread_transaction->write_map.find(
          const_cast<transaction_t<T, Context> *>(this));
      return non_committed_val != Context::thread_transaction->write_map.end()
                 ? static_cast<const T *>(non_committed_val->second->get())
                 : &t;
    } else {
      Context::thread_transaction->failed = true;
      return nullptr;
    }
  }

  friend written_t<T, Context>;
  void set_val(T &&val, version_t write_version, transaction_lock lock) {
    t = std::forward<T>(val);
    lock.set_version(write_version);
  }

  std::optional<transaction_lock> try_lock() {
    assert(Context::thread_transaction);
    std::optional<version_t> ver = version.exchange(std::nullopt);
    std::optional<transaction_lock> res =
        ver.transform([this](auto &ver2) { return transaction_lock(*this, ver2); });
    if (!res.has_value()) {
      Context::thread_transaction->failed = true;
    }
    return res;
  }

  T t;
  // lock: atomic read version
  //
  std::atomic<std::optional<version_t>> version = std::optional(version_start<version_t>);

  bool _check_version(version_t read_version) const override {
    auto maybe_version = version.load(std::memory_order_acquire);
    return maybe_version
        .transform([read_version](auto &version) { return version <= read_version; })
        .value_or(false);
  }
};

module :private;

namespace static_checks {
using T = transaction<unique_to_module, 0>;
template <typename M>
concept lockCopyable = requires(M t) {
  transaction_t<M, T>(t).set_val(100, version_start<version_t> + 1,
                                 std::declval<transaction_t<long, T>::transaction_lock &>());
};
}  // namespace static_checks
template <typename T, Transaction Context>
constexpr void transaction_t<T, Context>::_static_checks() noexcept {
  using namespace static_checks;
  static_assert(requires {
    { transaction_t<int, T>(5) } -> std::same_as<transaction_t<int, T>>;
    { 4 } -> std::convertible_to<transaction_t<int, T>>;
    {
      std::initializer_list<int>({1, 2, 3, 4, 5})
    } -> std::convertible_to<transaction_t<std::vector<int>, T>>;
    requires !std::copy_constructible<transaction_t<int, T>>;
    requires !std::assignable_from<transaction_t<int, T> &, transaction_t<int, T> &>;
    requires !std::assignable_from<transaction_t<int, T>, transaction_t<int, T>>;
    requires std::atomic<std::optional<version_t>>::is_always_lock_free;
    transaction_t<long, T>(5l).try_lock();
    transaction_t<long, T>(5l).get_val(version_start<version_t> + 1);
    transaction_t<long, T>(5l).set_val(100, version_start<version_t> + 1,
                                       (transaction_t<long, T>(0l).try_lock().value()));
    transaction_t<long, T>(5l).set_val(100, version_start<version_t> + 1,
                                       std::declval<transaction_t<long, T>::transaction_lock &&>());
    requires !lockCopyable<int>;
  });
}
