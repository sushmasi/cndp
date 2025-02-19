/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016-2022 Intel Corporation
 */

#ifndef __CNET_STK_H
#define __CNET_STK_H

/**
 * @file
 * CNET Stack instance routines and constants.
 */

#include <sys/queue.h>         // for TAILQ_HEAD
#include <pthread.h>           // for pthread_t, pthread_cond_t, pthread_mutex_t
#include <cne_atomic.h>        // for atomic_fetch_sub, atomic_load
#include <stddef.h>            // for NULL
#include <stdint.h>            // for uint32_t, uint16_t, uint64_t, uint8_t
#include <sys/select.h>        // for fd_set
#include <unistd.h>            // for usleep, pid_t

#ifdef __cplusplus
extern "C" {
#endif

#include <cne_system.h>        // for cne_get_timer_hz
#include <cne_vec.h>           // for vec_at_index, vec_len
#include <hmap.h>              // for hmap_t

#include "cne_common.h"            // for __cne_cache_aligned
#include "cne_per_thread.h"        // for CNE_PER_THREAD, CNE_DECLARE_PER_THREAD
#include "cnet.h"                  // for cnet, cnet_cfg (ptr only), per_thread_cnet
#include "cnet_const.h"            // for iofunc_t, PROTO_IO_MAX
#include "mempool.h"               // for mempool_t
#include "pktmbuf.h"               // for pktmbuf_alloc, pktmbuf_t

#define DEFAULT_RING_SIZE      8192
#define DEFAULT_MBUFS_PER_PORT (8192 - 1)
#define DEFAULT_RX_DESC        128
#define DEFAULT_TX_DESC        256

struct netlink_info;

typedef struct stk_s {
    pthread_mutex_t mutex;              /**< Stack Mutex */
    volatile uint16_t running;          /**< Network instance is running */
    uint16_t idx;                       /**< Index number of stack instance */
    uint16_t lid;                       /**< lcore ID for the the network instance */
    uint16_t reserved;                  /**< Reserved for future use */
    pid_t tid;                          /**< Thread process id */
    char name[32];                      /**< Name of the network instance */
    struct cne_graph *graph;            /**< Graph structure pointer for this instance */
    TAILQ_HEAD(, chnl) chnls;           /**< List of chnl structures */
    TAILQ_HEAD(, tcb_entry) tcbs;       /**< List of TCB structures */
    uint32_t tcp_now;                   /**< TCP now timer tick on slow timeout */
    uint32_t gflags;                    /**< Global flags */
    uint64_t ticks;                     /**< Number of ticks from start */
    mempool_t *tcb_objs;                /**< List of free TCB structures */
    mempool_t *seg_objs;                /**< List of free Segment structures */
    mempool_t *pcb_objs;                /**< PCB cnet_objpool pointer */
    mempool_t *chnl_objs;               /**< Channel cnet_objpool pointer */
    struct protosw_entry **protosw_vec; /**< protosw vector entries */
    struct icmp_entry *icmp;            /**< ICMP information */
    struct icmp6_entry *icmp6;          /**< ICMP6 information */
    struct ipv4_entry *ipv4;            /**< IPv4 information */
    struct ipv6_entry *ipv6;            /**< IPv6 information */
    struct tcp_entry *tcp;              /**< TCP information */
    struct raw_entry *raw;              /**< Raw information */
    struct udp_entry *udp;              /**< UDP information */
    struct chnl_optsw **chnlopt;        /**< Channel Option pointers */
} stk_t __cne_cache_aligned;

CNE_DECLARE_PER_THREAD(stk_t *, stk);
#define this_stk CNE_PER_THREAD(stk)

#define PROTOSW_FREE_SLOT 0xFFFF

/* Flags values for stk_entry.gflags */
enum {
    TCP_TIMEOUT_ENABLED    = 0x00000001, /**< Enable TCP Timeouts */
    RFC1323_TSTAMP_ENABLED = 0x00004000, /**< Enable RFC1323 Timestamp */
    RFC1323_SCALE_ENABLED  = 0x00008000, /**< Enable RFC1323 window scaling */
};

static inline uint64_t
clks_to_ns(uint64_t clks)
{
    uint64_t ns = cne_get_timer_hz();

    ns = 1000000000ULL / ((ns == 0) ? 1 : ns); /* nsecs per clk */
    ns *= clks;                                /* nsec per clk times clks */

    return ns;
}

static inline uint32_t
stk_get_timer_ticks(void)
{
    return this_stk->tcp_now;
}

static inline void
stk_set(stk_t *stk)
{
    CNE_PER_THREAD(stk) = stk;
}

static inline stk_t *
stk_get(void)
{
    return CNE_PER_THREAD(stk);
}

static inline stk_t *
cnet_stk_find_by_lcore(uint8_t lid)
{
    struct cnet *cnet = this_cnet;
    stk_t *stk;

    vec_foreach_ptr (stk, cnet->stks) {
        if (stk->lid == lid)
            return stk;
    }
    return NULL;
}

static inline void
cnet_stk_stop_running(stk_t *stk)
{
    if (stk)
        stk->running = 0;
}

static inline int
cnet_stk_is_running(void)
{
    return (this_stk) ? this_stk->running : 0;
}

/**
 * @brief Initialize the stack instance.
 *
 * @param cnet
 *   The pointer to the cnet structure.
 * @return
 *   -1 on error or 0 on success.
 */
CNDP_API int cnet_stk_initialize(struct cnet *cnet);

/**
 * @brief Stop the stack instance and free resources.
 *
 * @return
 *   -1 on error or 0 on success
 */
CNDP_API int cnet_stk_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __CNET_STK_H */
