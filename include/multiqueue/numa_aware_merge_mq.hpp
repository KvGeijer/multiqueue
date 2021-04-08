/**
******************************************************************************
* @file:   numa_aware_merge_mq.hpp
*
* @author: Marvin Williams
* @date:   2021/03/18 21:08
* @brief:
*******************************************************************************
**/
#pragma once
#ifndef NUMA_AWARE_MERGE_MQ_HPP_INCLUDED
#define NUMA_AWARE_MERGE_MQ_HPP_INCLUDED

#include <algorithm>

#include "multiqueue/default_configuration.hpp"
#include "multiqueue/sequential/heap/merge_heap.hpp"
#include "multiqueue/util/buffer.hpp"
#include "multiqueue/util/extractors.hpp"
#include "multiqueue/util/ring_buffer.hpp"
#include "system_config.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <vector>

namespace multiqueue {

template <typename Key, typename T, typename Comparator = std::less<Key>, typename Configuration = DefaultConfiguration,
          typename Allocator = std::allocator<Key>>
class numa_aware_merge_mq {
   public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<key_type, mapped_type>;
    using key_comparator = Comparator;
    class value_comparator : private key_comparator {
        friend numa_aware_merge_mq;
        explicit value_comparator(key_comparator const &comp) : key_comparator{comp} {
        }

       public:
        constexpr bool operator()(value_type const &lhs, value_type const &rhs) const {
            return key_comparator::operator()(util::get_nth<value_type>{}(lhs), util::get_nth<value_type>{}(rhs));
        }
    };

    struct handle_type {
        friend numa_aware_merge_mq;

       private:
        unsigned int id_;

       private:
        explicit handle_type(unsigned int id) : id_{id} {
        }
    };

   private:
    static constexpr unsigned int C = Configuration::C;
    static constexpr std::size_t NodeSize = Configuration::NodeSize;
    static constexpr std::size_t InsertionBufferSize = NodeSize;
    static constexpr std::size_t DeletionBufferSize = 2 * NodeSize;

    static_assert(InsertionBufferSize % NodeSize == 0, "Insertion buffer size must be a multiple of the NodeSize");
    static_assert(DeletionBufferSize >= InsertionBufferSize + NodeSize,
                  "Deletion buffer size must be at least insertion buffer size plus node size");

    using heap_type = sequential::merge_heap<value_type, key_type, util::get_nth<value_type>, key_comparator, NodeSize,
                                             typename Configuration::template HeapAllocator<value_type>>;


    struct alignas(PAGESIZE) guarded_heap {
        using allocator_type = typename heap_type::allocator_type;
        std::atomic_bool in_use = false;
        util::buffer<value_type, InsertionBufferSize> insertion_buffer;
        util::ring_buffer<value_type, DeletionBufferSize> deletion_buffer;
        heap_type heap;

        explicit guarded_heap() = default;
        explicit guarded_heap(allocator_type const &alloc) : heap{alloc} {
        }

        inline bool try_lock() noexcept {
            bool expect_in_use = false;
            return in_use.compare_exchange_strong(expect_in_use, true, std::memory_order_acquire,
                                                  std::memory_order_relaxed);
        }

        inline void unlock() noexcept {
            in_use.store(false, std::memory_order_release);
        }

        inline void flush_insertion_buffer(value_comparator const &comp) {
            assert(insertion_buffer.size() == InsertionBufferSize);
            std::sort(insertion_buffer.begin(), insertion_buffer.end(), comp);
            for (std::size_t i = 0u; i < insertion_buffer.size(); i += NodeSize) {
                heap.insert(insertion_buffer.begin() + (i * NodeSize), insertion_buffer.begin() + ((i + 1) * NodeSize));
            }
            insertion_buffer.clear();
        }

        inline void refill_deletion_buffer(value_comparator const &comp) {
            assert(deletion_buffer.empty());
            if (insertion_buffer.size() == InsertionBufferSize) {
                flush_insertion_buffer(comp);
                while (deletion_buffer.size() != DeletionBufferSize && !heap.empty()) {
                    std::copy(heap.top_node().begin(), heap.top_node().end(), std::back_inserter(deletion_buffer));
                    heap.pop_node();
                }
            } else if (heap.empty()) {
                std::sort(insertion_buffer.begin(), insertion_buffer.end(), comp);
                std::copy(insertion_buffer.begin(), insertion_buffer.end(), std::back_inserter(deletion_buffer));
                insertion_buffer.clear();
            } else {
                auto it = std::partition(insertion_buffer.begin(), insertion_buffer.end(),
                                         [&](auto const &v) { return comp(heap.top_node().back(), v); });
                auto heap_it = heap.top_node().begin();
                while (it != insertion_buffer.end()) {
                    auto min_it = std::min_element(it, insertion_buffer.end(), comp);
                    while (comp(*heap_it, *min_it)) {
                        deletion_buffer.push_back(*heap_it++);
                    }
                    deletion_buffer.push_back(*min_it);
                    if (min_it + 1u != insertion_buffer.end()) {
                        *min_it = std::move(insertion_buffer.back());
                    }
                    insertion_buffer.pop_back();
                }
                std::copy(heap_it, heap.top_node().end(), std::back_inserter(deletion_buffer));
                heap.pop_node();
            }
        }

        // We try to insert the new value into the deletion buffer, if it is smaller than the largest element in the
        // deletion buffer. If the deletion buffer is full, we therefore need to evict the largest element. This element
        // then gets inserted into the insertion buffer to avoid accessing the heap. If the insertion buffer is full, we
        // flush it. If the new value is too large for the deletion buffer, it is inserted into the insertion buffer
        // which might get flushed in the process.
        void push(value_type const &value, value_comparator const &comp) {
            if (!deletion_buffer.empty()) {
                std::size_t pos = deletion_buffer.size();
                while (pos > 0 && comp(value, deletion_buffer[pos - 1u])) {
                    --pos;
                }
                if (pos < deletion_buffer.size()) {
                    if (deletion_buffer.size() == DeletionBufferSize) {
                        if (insertion_buffer.size() == InsertionBufferSize) {
                            flush_insertion_buffer(comp);
                        }
                        insertion_buffer.push_back(std::move(deletion_buffer.back()));
                        deletion_buffer.pop_back();
                    }
                    deletion_buffer.insert_at(pos, value);
                    return;
                }
            }
            if (insertion_buffer.size() == InsertionBufferSize) {
                flush_insertion_buffer(comp);
            }
            insertion_buffer.push_back(value);
        }
    };

   public:
    using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<guarded_heap>;

   private:
    std::vector<guarded_heap, allocator_type> heap_list_;
    size_t num_queues_;
    key_comparator comp_;

   private:
    inline std::mt19937 &get_rng() const {
        static thread_local std::mt19937 gen;
        return gen;
    }

    inline size_t random_queue_index() const {
        std::uniform_int_distribution<std::size_t> dist{0, num_queues_ - 1};
        return dist(get_rng());
    }

    inline size_t random_local_queue_index() const {
        std::uniform_int_distribution<std::size_t> dist{0, C - 1};
        return dist(get_rng());
    }

   public:
    explicit numa_aware_merge_mq(unsigned int const num_threads)
        : heap_list_(num_threads * C), num_queues_{static_cast<unsigned int>(heap_list_.size())}, comp_{} {
        assert(num_threads >= 1);
    }

    explicit numa_aware_merge_mq(unsigned int const num_threads, allocator_type const &alloc)
        : heap_list_(num_threads * C, alloc), num_queues_{static_cast<unsigned int>(heap_list_.size())}, comp_{} {
        assert(num_threads >= 1);
    }

    handle_type get_handle(unsigned int id) const noexcept {
        return handle_type{id};
    }

    void push(value_type const &value) {
        size_t index;
        do {
            index = random_queue_index();
        } while (!heap_list_[index].try_lock());
        heap_list_[index].push(value, value_comparator{comp_});
        heap_list_[index].unlock();
    }

    bool extract_top(value_type &retval, handle_type const &handle) {
        size_t start_index = random_local_queue_index();
        size_t first_index;
        for (unsigned int i = 0; i < C; ++i) {
            first_index = C * handle.id_ + ((start_index + i) % C);
            if (heap_list_[first_index].try_lock()) {
                break;
            }
            if (i == 3) {
                do {
                    first_index = random_queue_index();
                } while (!heap_list_[first_index].try_lock());
            }
        }
        bool first_empty = false;
        if (heap_list_[first_index].deletion_buffer.empty()) {
            heap_list_[first_index].refill_deletion_buffer(value_comparator{comp_});
        }
        if (heap_list_[first_index].deletion_buffer.empty()) {
            heap_list_[first_index].unlock();
            first_empty = true;
        }
        // When we get here, we hold the lock for the first heap, which has a nonempty buffer
        size_t second_index;
        do {
            second_index = random_queue_index();
        } while (!heap_list_[second_index].try_lock());
        if (heap_list_[second_index].deletion_buffer.empty()) {
            heap_list_[second_index].refill_deletion_buffer(value_comparator{comp_});
        }
        if (heap_list_[second_index].deletion_buffer.empty()) {
            heap_list_[second_index].unlock();
            if (first_empty) {
                return false;
            }
            retval = std::move(heap_list_[first_index].deletion_buffer.front());
            heap_list_[first_index].deletion_buffer.pop_front();
            heap_list_[first_index].unlock();
            return true;
        }
        if (first_empty ||
            comp_(heap_list_[second_index].deletion_buffer.front().first,
                  heap_list_[first_index].deletion_buffer.front().first)) {
            if (!first_empty) {
                heap_list_[first_index].unlock();
            }
            retval = std::move(heap_list_[second_index].deletion_buffer.front());
            heap_list_[second_index].deletion_buffer.pop_front();
            heap_list_[second_index].unlock();
        } else {
            heap_list_[second_index].unlock();
            retval = std::move(heap_list_[first_index].deletion_buffer.front());
            heap_list_[first_index].deletion_buffer.pop_front();
            heap_list_[first_index].unlock();
        }
        return true;
    }

    inline void init_touch(handle_type const &handle, std::size_t size) {
        for (unsigned int i = 0; i < C; ++i) {
            heap_list_[C * handle.id_ + i].heap.init_touch(size);
        }
    }
};

}  // namespace multiqueue

#endif  //! NUMA_AWARE_MERGE_MQ_HPP_INCLUDED