module;
#include <concepts>
#include <utility>
#include <vector>
#include <atomic>
#include <optional>
#include <cassert>
#include <cstdlib>
export module STMCXX;
import Util;
export import :Transaction;
export import :TransactionVal;


module :private;

namespace static_checks {
using T = transaction<unique_to_lib, 0>;
template <typename M>
concept lockCopyable = requires(M t) {
  transaction_t<M, T>(t).set_val(100, version_start<version_t> + 1,
                                 std::declval<transaction_t<long, T>::transaction_lock &>());
};
}  // namespace static_checks
template <std::copyable T, Transaction Context>
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