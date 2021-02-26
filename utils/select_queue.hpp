/**
******************************************************************************
* @file:   select_queue.hpp
*
* @author: Marvin Williams
* @date:   2021/02/22 13:23
* @brief:
*******************************************************************************
**/
#pragma once
#ifndef SELECT_QUEUE_HPP_INCLUDED
#define SELECT_QUEUE_HPP_INCLUDED

#include <cstdint>
#include <type_traits>

#if defined PQ_CAPQ || defined PQ_CAPQ1 || defined PQ_CAPQ2 || defined PQ_CAPQ3 || defined PQ_CAPQ4
#include "capq.hpp"
#elif defined PQ_LINDEN
#include "linden.hpp"
#elif defined PQ_SPRAYLIST
#include "spraylist.hpp"
#elif defined PQ_KLSM
#include "k_lsm/k_lsm.h"
#include "klsm.hpp"
#elif defined PQ_DLSM
#include "dist_lsm/dist_lsm.h"
#include "klsm.hpp"
#elif defined PQ_NBMQ
#include "multiqueue/no_buffer_mq.hpp"
#elif defined PQ_TBMQ
#include "multiqueue/top_buffer_mq.hpp"
#elif defined PQ_DBMQ
#include "multiqueue/deletion_buffer_mq.hpp"
#elif defined PQ_SMDMQ
#include "multiqueue/sm_deletion_buffer_mq.hpp"
#elif defined PQ_IDMQ
#include "multiqueue/ins_del_buffer_mq.hpp"
#else
#error No supported priority queue defined!
#endif

namespace util {

template <typename KeyType, typename ValueType>
struct QueueSelector {
#if defined PQ_KLSM
    using pq_t = multiqueue::wrapper::klsm<kpq::k_lsm<KeyType, ValueType, 256>>;
#elif defined PQ_DLSM
    using pq_t = multiqueue::wrapper::klsm<kpq::dist_lsm<KeyType, ValueType, 256>>;
#elif defined PQ_NBMQ
    using pq_t = multiqueue::rsm::no_buffer_mq<KeyType, ValueType>;
#elif defined PQ_TBMQ
    using pq_t = multiqueue::rsm::top_buffer_mq<KeyType, ValueType>;
#elif defined PQ_DBMQ
    using pq_t = multiqueue::rsm::deletion_buffer_mq<KeyType, ValueType>;
#elif defined PQ_SMDMQ
    using pq_t = multiqueue::rsm::sm_deletion_buffer_mq<KeyType, ValueType>;
#elif defined PQ_IDMQ
    using pq_t = multiqueue::rsm::ins_del_buffer_mq<KeyType, ValueType>;
#endif
};

template <>
struct QueueSelector<std::uint32_t, std::uint32_t> {
#if defined PQ_CAPQ || defined PQ_CAPQ1
    using pq_t = multiqueue::wrapper::capq<true, true, true>;
#elif defined PQ_CAPQ2
    using pq_t = multiqueue::wrapper::capq<true, false, true>;
#elif defined PQ_CAPQ3
    using pq_t = multiqueue::wrapper::capq<false, true, true>;
#elif defined PQ_CAPQ4
    using pq_t = multiqueue::wrapper::capq<false, false, true>;
#elif defined PQ_LINDEN
    using pq_t = multiqueue::wrapper::linden;
#elif defined PQ_SPRAYLIST
    using pq_t = multiqueue::wrapper::spraylist;
#elif defined PQ_KLSM
    using pq_t = multiqueue::wrapper::klsm<kpq::k_lsm<std::uint32_t, std::uint32_t, 256>>;
#elif defined PQ_DLSM
    using pq_t = multiqueue::wrapper::klsm<kpq::dist_lsm<std::uint32_t, std::uint32_t, 256>>;
#elif defined PQ_NBMQ
    using pq_t = multiqueue::rsm::no_buffer_mq<std::uint32_t, std::uint32_t>;
#elif defined PQ_TBMQ
    using pq_t = multiqueue::rsm::top_buffer_mq<std::uint32_t, std::uint32_t>;
#elif defined PQ_DBMQ
    using pq_t = multiqueue::rsm::deletion_buffer_mq<std::uint32_t, std::uint32_t>;
#elif defined PQ_SMDMQ
    using pq_t = multiqueue::rsm::sm_deletion_buffer_mq<std::uint32_t, std::uint32_t>;
#elif defined PQ_IDMQ
    using pq_t = multiqueue::rsm::ins_del_buffer_mq<std::uint32_t, std::uint32_t>;
#endif
};

template <typename Queue>
struct QueueTraits {
   private:
    template <typename T, typename = void>
    struct has_thread_init_impl : std::false_type {};

    template <typename T>
    struct has_thread_init_impl<T, std::void_t<decltype(std::declval<T>().init_thread(static_cast<std::size_t>(0)))>>
        : std::true_type {};

   public:
    static constexpr bool has_thread_init = has_thread_init_impl<Queue>::value;
};

}  // namespace util

#endif  //! SELECT_QUEUE_HPP_INCLUDED
