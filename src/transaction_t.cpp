module;
#include <memory>
#include <type_traits>
#include <concepts>
#include <utility>
#include <atomic>
#include <optional>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <ostream>
export module STMXX:TransactionVal;
import :Transaction;
import Util;

export template <std::copyable T, Transaction Context>
class transaction_t;
template <typename T, Transaction Context>
class written_t final : public written {
 public:
  written_t(T &&val, transaction_t<T, Context> &to_set) : t(std::forward<T>(val)), to_set(to_set) {}
  const void *get() const { return &t; }
  std::optional<version_t> try_lock() {
    lock = std::move(to_set.try_lock());
    return lock.transform([](auto &lock) { return lock.getVersion(); });
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

export template <std::copyable T, Transaction Context>
class transaction_t final : public tval {
 public:
  class transaction_lock final {
   public:
    version_t getVersion() const { return version; }
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

   private:
    friend transaction_t<T, Context>;
    explicit transaction_lock(transaction_t<T, Context> &locked, version_t version)
        : locked(&locked), version(version), is_locked(true) {}
    void set_version(version_t write_version) { version = write_version; }
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

  const std::optional<T> operator*() const {
    if (getCurrentTransaction<Context>()) {
      return get_val(getReadVersion<Context>());
    } else {
      return t;
    }
  }

  template <typename S>
    requires(!std::is_member_function_pointer_v<S>)
  using MemberPtrTo = std::remove_reference_t<decltype(std::declval<T *>()->*std::declval<S>())>;

  template <typename S>
    requires(!std::is_member_function_pointer_v<S>) && (!std::is_pointer_v<MemberPtrTo<S>>) &&
            (!std::is_lvalue_reference_v<MemberPtrTo<S>>)
  const std::optional<MemberPtrTo<S>> operator->*(S objptr) const {
    if (getCurrentTransaction<Context>()) {
      return get_member(objptr, getReadVersion<Context>());
    } else {
      return t.*objptr;
    }
  }

  template <typename S, typename... Args>
    requires(std::is_member_function_pointer_v<S>)
  auto operator->*(S objptr) const {
    return [&, objptr](Args... args) {
      if (getCurrentTransaction<Context>()) {
        return call_accessor(getReadVersion<Context>(), objptr, args...);
      } else {
        return std::optional((t.*objptr)(args...));
      }
    };
  }

  template <typename U>
  transaction_t<T, Context> &operator=(U &&val) {
    if (getCurrentTransaction<Context>()) {
      if (!getFailed<Context>()) {
        getWriteMap<Context>().insert_or_assign(
            this, std::make_unique<written_t<T, Context>>(written_t(std::forward<U>(val), *this)));
      }
    } else {
      t = std::forward<T>(val);
    }
    return *this;
  }

 private:
  static constexpr void _static_checks() noexcept;

  std::optional<transaction_lock> try_lock() {
    assert(getCurrentTransaction<Context>());
    std::optional<version_t> ver = version.exchange(std::nullopt);
    std::optional<transaction_lock> res =
        ver.transform([this](auto &ver2) { return transaction_lock(*this, ver2); });
    if (!res.has_value()) {
      getFailed<Context>() = true;
    }
    return res;
  }

  const T *_get_ptr_in_transaction() const {
    assert(getCurrentTransaction<Context>());
    auto non_committed_val = getWriteMap<Context>().find(const_cast<transaction_t *>(this));
    return non_committed_val != getWriteMap<Context>().end()
               ? static_cast<const T *>(non_committed_val->second->get())
               : &t;
  }

  template <typename V, typename Fn>
    requires std::is_invocable_r<V, Fn, T *>::value
  const std::optional<V> issue_read_op(Fn accessor, version_t read_version) const {
    assert(getCurrentTransaction<Context>());
    // check read_version twice: first check for memory order acquire. second read_version for
    // consistency guarantee. data race is possible, but any data race must also update read
    // version, making the second test fail.
    if (!getFailed<Context>() && _check_version(read_version)) {
// don't register data race by thread sanitizer
#if THREAD_SANITIZER
      std::optional<transaction_lock> lock;
      lock = ((transaction_t *)this)->try_lock();
      if (!lock) {
        return std::nullopt;
      }
#endif
      std::optional<V> result = accessor(_get_ptr_in_transaction());
#if THREAD_SANITIZER
      if (lock->getVersion() <= read_version) {
#else
      if (_check_version(read_version)) {
#endif
        getReadSet<Context>().insert(const_cast<transaction_t<T, Context> *const>(this));
        return result;
      }
    }
    getFailed<Context>() = true;
    return std::nullopt;
  }

  template <typename S>
    requires(!std::is_member_function_pointer_v<S>) && (!std::is_pointer_v<MemberPtrTo<S>>) &&
            (!std::is_lvalue_reference_v<MemberPtrTo<S>>)
  const std::optional<MemberPtrTo<S>> get_member(S objptr, version_t read_version) const {
    return issue_read_op<MemberPtrTo<S>>([&](const T *ptr) { return ptr->*objptr; }, read_version);
  }

  const std::optional<T> get_val(version_t read_version) const {
    return issue_read_op<T>([&](const T *ptr) { return *ptr; }, read_version);
  }

  template <typename S, typename... Args>
  const auto call_accessor(version_t read_version, S objptr, Args... args) const {
    return issue_read_op<decltype((std::declval<const T *>()->*objptr)(args...))>(
        [&](const T *ptr) { return (ptr->*objptr)(args...); }, read_version);
  }

  friend written_t<T, Context>;
  void set_val(T &&val, version_t write_version, transaction_lock lock) {
    t = std::forward<T>(val);
    lock.set_version(write_version);
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
