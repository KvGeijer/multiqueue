#include "catch2/catch.hpp"
#include "multiqueue/pq.hpp"

#include <algorithm>  // std::generate_n, std::min_element
#include <functional> // std::greater
#include <iterator>   // std::back_inserter
#include <queue>
#include <random>
#include <vector>

<<<<<<< HEAD
template <unsigned int D>
struct DegreeHeapSettings {
    template <typename T>
    struct HeapSettings : multiqueue::local_nonaddressable::default_heap_settings<T> {
        static constexpr unsigned int Degree = D;
    };
};

TEMPLATE_TEST_CASE_SIG("pq heap representation can have different degrees", "[pq][correctness][degree]",
                       ((unsigned int Degree), Degree), 1, 2, 3, 4, 99) {

    using pq_t = multiqueue::local_nonaddressable::pq<int, std::less<int>, DegreeHeapSettings<Degree>::template HeapSettings>;

    auto pq = pq_t{};

    SECTION("push increasing numbers and pop them") {
        for (int n = 0; n < 1000; ++n) {
            pq.push(n);
        }

        for (int i = 0; i < 1000; ++i) {
            REQUIRE(pq.top() == i);
            pq.pop();
        }
        REQUIRE(pq.empty());
    }

    SECTION("push decreasing numbers and pop them") {
        for (int n = 999; n >= 0; --n) {
            pq.push(n);
        }

        for (int i = 0; i < 1000; ++i) {
            REQUIRE(pq.top() == i);
            pq.pop();
        }
        REQUIRE(pq.empty());
    }

    SECTION(
        "first push increasing numbers, then push decreasing numbers and "
        "pop them") {
        for (int i = 1; i <= 500; ++i) {
            pq.push(i);
        }
        for (int i = 1000; i > 500; --i) {
            pq.push(i);
        }
        for (int i = 1; i <= 1000; ++i) {
            REQUIRE(pq.top() == i);
            pq.pop();
        }
        REQUIRE(pq.empty());
    }
}

TEST_CASE("pq use std::greater as comparator", "[pq][correctness][comparator]") {
    using pq_t = multiqueue::local_nonaddressable::pq<int, std::greater<int>>;

    auto pq = pq_t{};

    SECTION("push increasing numbers and pop them") {
        for (int n = 0; n < 1000; ++n) {
            pq.push(n);
        }

        for (int i = 999; i >= 0; --i) {
            REQUIRE(pq.top() == i);
            pq.pop();
        }
        REQUIRE(pq.empty());
    }

    SECTION("push decreasing numbers and pop them") {
        for (int n = 999; n >= 0; --n) {
            pq.push(n);
        }

        for (int i = 999; i >= 0; --i) {
            REQUIRE(pq.top() == i);
            pq.pop();
        }
        REQUIRE(pq.empty());
    }

    SECTION(
        "first push increasing numbers, then push decreasing numbers and "
        "pop them") {
        for (int i = 0; i < 500; ++i) {
            pq.push(i);
        }
        for (int i = 999; i >= 500; --i) {
            pq.push(i);
        }
        for (int i = 999; i >= 0; --i) {
            REQUIRE(pq.top() == i);
            pq.pop();
        }
        REQUIRE(pq.empty());
    }
}

    template <typename T>
    struct StrategyHeapSettings : multiqueue::local_nonaddressable::default_heap_settings<T> {
      using Strategy = multiqueue::local_nonaddressable::full_up_strategy;
    };
TEST_CASE("pq full_up_strategy", "[pq][correctness][strategy]") {
    using pq_t = multiqueue::local_nonaddressable::pq<int, std::greater<int>, StrategyHeapSettings>;

    auto pq = pq_t{};

    SECTION("push increasing numbers and pop them") {
        for (int n = 0; n < 1000; ++n) {
            pq.push(n);
        }

        for (int i = 999; i >= 0; --i) {
            REQUIRE(pq.top() == i);
            pq.pop();
        }
        REQUIRE(pq.empty());
    }

    SECTION("push decreasing numbers and pop them") {
        for (int n = 999; n >= 0; --n) {
            pq.push(n);
        }

        for (int i = 999; i >= 0; --i) {
            REQUIRE(pq.top() == i);
            pq.pop();
        }
        REQUIRE(pq.empty());
    }

    SECTION(
        "first push increasing numbers, then push decreasing numbers and "
        "pop them") {
        for (int i = 0; i < 500; ++i) {
            pq.push(i);
        }
        for (int i = 999; i >= 500; --i) {
            pq.push(i);
        }
        for (int i = 999; i >= 0; --i) {
            REQUIRE(pq.top() == i);
            pq.pop();
        }
        REQUIRE(pq.empty());
    }
}

TEST_CASE("pq test sample workloads", "[pq][correctness][workloads]") {
    using pq_t = multiqueue::local_nonaddressable::pq<int, std::less<int>>;

    auto pq = pq_t{};
    auto ref_pq = std::priority_queue<int, std::vector<int>, std::greater<int>>{};
    auto gen = std::mt19937{0};

    SECTION("push random numbers and pop them") {
        auto dist = std::uniform_int_distribution{-100, 100};

        for (size_t i = 0; i < 1000; ++i) {
            auto n = dist(gen);
            pq.push(n);
            ref_pq.push(n);
            REQUIRE(pq.top() == ref_pq.top());
        }

        for (size_t i = 0; i < 1000; ++i) {
            REQUIRE(pq.top() == ref_pq.top());
            pq.pop();
            ref_pq.pop();
        }
        REQUIRE(pq.empty());
    }

    SECTION("interleave pushing and popping random numbers") {
        auto dist = std::uniform_int_distribution{-100, 100};
        auto seq_dist = std::uniform_int_distribution{0, 10};

        for (int s = 0; s < 1000; ++s) {
            auto num_push = seq_dist(gen);
            for (int i = 0; i < num_push; ++i) {
                auto n = dist(gen);
                pq.push(n);
                ref_pq.push(n);
                REQUIRE(pq.top() == ref_pq.top());
            }
            auto num_pop = seq_dist(gen);
            for (int i = 0; i > num_pop; --i) {
                if (!pq.empty()) {
                    REQUIRE(pq.top() == ref_pq.top());
                    pq.pop();
                    ref_pq.pop();
                }
            }
        }
        while (!pq.empty()) {
            REQUIRE(pq.top() == ref_pq.top());
            pq.pop();
            ref_pq.pop();
        }
    }

    SECTION("dijkstra") {
        auto dist = std::uniform_int_distribution{-100, 100};
        auto seq_dist = std::uniform_int_distribution{1, 10};

        pq.push(0);
        ref_pq.push(0);
        for (int s = 0; s < 1000; ++s) {
            auto top = pq.top();
            pq.pop();
            ref_pq.pop();
            auto num_push = seq_dist(gen);
            for (int i = 0; i < num_push; ++i) {
                auto n = top + dist(gen);
                pq.push(n);
                ref_pq.push(n);
                REQUIRE(pq.top() == ref_pq.top());
            }
        }
        while (!pq.empty()) {
            REQUIRE(pq.top() == ref_pq.top());
            pq.pop();
            ref_pq.pop();
        }
    }
=======
TEMPLATE_TEST_CASE_SIG("pq heap representation can have different degrees",
                       "[pq][correctness][degree]",
                       ((unsigned int Degree), Degree), 1, 2, 3, 4, 99) {
  using pq_t = multiqueue::local_nonaddressable::pq<int, std::less<int>,
                                                    std::vector<int>, Degree>;

  INFO("Degree " << Degree);
  auto pq = pq_t{};

  SECTION("push increasing numbers and pop them") {
    for (int n = 0; n < 1000; ++n) {
      pq.push(n);
    }

    for (int i = 0; i < 1000; ++i) {
      REQUIRE(pq.top() == i);
      pq.pop();
    }
    REQUIRE(pq.empty());
  }

  SECTION("push decreasing numbers and pop them") {
    for (int n = 999; n >= 0; --n) {
      pq.push(n);
    }

    for (int i = 0; i < 1000; ++i) {
      REQUIRE(pq.top() == i);
      pq.pop();
    }
    REQUIRE(pq.empty());
  }

  SECTION("first push increasing numbers, then push decreasing numbers and "
          "pop them") {
    for (int i = 1; i <= 500; ++i) {
      pq.push(i);
    }
    for (int i = 1000; i > 500; --i) {
      pq.push(i);
    }
    for (int i = 1; i <= 1000; ++i) {
      REQUIRE(pq.top() == i);
      pq.pop();
    }
    REQUIRE(pq.empty());
  }
}

TEST_CASE("pq use std::greater as comparator",
          "[pq][correctness][comparator]") {
  using pq_t = multiqueue::local_nonaddressable::pq<int, std::greater<int>,
                                                    std::vector<int>, 4>;

  auto pq = pq_t{};

  SECTION("push increasing numbers and pop them") {
    for (int n = 0; n < 1000; ++n) {
      pq.push(n);
    }

    for (int i = 999; i >= 0; --i) {
      REQUIRE(pq.top() == i);
      pq.pop();
    }
    REQUIRE(pq.empty());
  }

  SECTION("push decreasing numbers and pop them") {
    for (int n = 999; n >= 0; --n) {
      pq.push(n);
    }

    for (int i = 999; i >= 0; --i) {
      REQUIRE(pq.top() == i);
      pq.pop();
    }
    REQUIRE(pq.empty());
  }

  SECTION("first push increasing numbers, then push decreasing numbers and "
          "pop them") {
    for (int i = 0; i < 500; ++i) {
      pq.push(i);
    }
    for (int i = 999; i >= 500; --i) {
      pq.push(i);
    }
    for (int i = 999; i >= 0; --i) {
      REQUIRE(pq.top() == i);
      pq.pop();
    }
    REQUIRE(pq.empty());
  }
}

TEST_CASE("pq test sample workloads", "[pq][correctness][workloads]") {
  using pq_t = multiqueue::local_nonaddressable::pq<int, std::less<int>,
                                                    std::vector<int>, 4>;

  auto pq = pq_t{};
  auto ref_pq = std::priority_queue<int, std::vector<int>, std::greater<int>>{};
  auto gen = std::mt19937{0};

  SECTION("push random numbers and pop them") {
    auto dist = std::uniform_int_distribution{-100, 100};

    for (size_t i = 0; i < 1000; ++i) {
      auto n = dist(gen);
      pq.push(n);
      ref_pq.push(n);
      REQUIRE(pq.top() == ref_pq.top());
    }

    for (size_t i = 0; i < 1000; ++i) {
      REQUIRE(pq.top() == ref_pq.top());
      pq.pop();
      ref_pq.pop();
    }
    REQUIRE(pq.empty());
  }

  SECTION("interleave pushing and popping random numbers") {
    auto dist = std::uniform_int_distribution{-100, 100};
    auto seq_dist = std::uniform_int_distribution{0, 10};

    for (int s = 0; s < 1000; ++s) {
      auto num_push = seq_dist(gen);
      for (int i = 0; i < num_push; ++i) {
        auto n = dist(gen);
        pq.push(n);
        ref_pq.push(n);
        REQUIRE(pq.top() == ref_pq.top());
      }
      auto num_pop = seq_dist(gen);
      for (int i = 0; i > num_pop; --i) {
        if (!pq.empty()) {
          REQUIRE(pq.top() == ref_pq.top());
          pq.pop();
          ref_pq.pop();
        }
      }
    }
    while (!pq.empty()) {
      REQUIRE(pq.top() == ref_pq.top());
      pq.pop();
      ref_pq.pop();
    }
  }

  SECTION("dijkstra") {
    auto dist = std::uniform_int_distribution{-100, 100};
    auto seq_dist = std::uniform_int_distribution{1, 10};

    pq.push(0);
    ref_pq.push(0);
    for (int s = 0; s < 1000; ++s) {
      auto top = pq.top();
      pq.pop();
      ref_pq.pop();
      auto num_push = seq_dist(gen);
      for (int i = 0; i < num_push; ++i) {
        auto n = top + dist(gen);
        pq.push(n);
        ref_pq.push(n);
        REQUIRE(pq.top() == ref_pq.top());
      }
    }
    while (!pq.empty()) {
      REQUIRE(pq.top() == ref_pq.top());
      pq.pop();
      ref_pq.pop();
    }
  }
>>>>>>> 9f0a435 (Devel)
}
