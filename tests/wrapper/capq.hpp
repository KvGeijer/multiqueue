#ifndef WRAPPER_CAPQ_HPP
#define WRAPPER_CAPQ_HPP

// Adapted from klsm

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

namespace multiqueue {
namespace wrapper {

struct capq_wrapper_type;

template <bool remove_min_relax = true, bool put_relax = true, bool catree_adapt = true>
class capq {
    capq_wrapper_type* pq_;

   public:
    capq(unsigned int = 0);

    void push(std::pair<uint32_t, uint32_t> const& value);
    bool extract_top(std::pair<uint32_t, uint32_t>& retval);

    static std::string description() {
        std::stringstream ss;
        ss << "capq\n";
        ss << "Remove min relax: " << std::boolalpha << remove_min_relax << '\n';
        ss << "Put relax" << std::boolalpha << put_relax << '\n';
        ss << "Catree adapt" << std::boolalpha << catree_adapt << '\n';
        return ss.str();
    }
};

}  // namespace wrapper
}  // namespace multiqueue

#endif