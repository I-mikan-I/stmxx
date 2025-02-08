module;
#include <cassert>
export module Main;
import Lib;

using T = transaction<int>;
transaction_t<int, T> tval = 5;

int test(int argc, char const *argv[]) {
  const int *val = tval.read();
  assert(val && *val == 5);
  tval = 100;
  val = tval.read();
  assert(val && *val == 100);
  return 0;
}

int main(int argc, char const *argv[]) {
  T::start([&]() { return test(argc, argv); });
  assert(*tval.read() == 100);
  return 0;
}