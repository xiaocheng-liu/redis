/* INFO 信息生成与查询,供客户端和故障诊断复用。 */
#include <sys/utsname.h>

#include "server.h"
#include "server_commands.h"
#include "ae.h"
#include "bio.h"
#include "evict.h"
#include "cluster.h"
#include "version.h"

/* Convert an amount of bytes into a human readable string in the form
 * of 100B, 2G, 100M, 4K, and so forth. */
// 将一个表示字节数的整数值 `n` 转换为更易读的格式(如 KB、MB、GB 等)
static void bytesToHuman(char *s, unsigned long long n) {
    double d;

    if (n < 1024) {
        /* Bytes */
        sprintf(s, "%lluB", n);
    } else if (n < (1024 * 1024)) {
        d = (double) n / (1024);
        sprintf(s, "%.2fK", d);
    } else if (n < (1024LL * 1024 * 1024)) {
        d = (double) n / (1024 * 1024);
        sprintf(s, "%.2fM", d);
    } else if (n < (1024LL * 1024 * 1024 * 1024)) {
        d = (double) n / (1024LL * 1024 * 1024);
        sprintf(s, "%.2fG", d);
    } else if (n < (1024LL * 1024 * 1024 * 1024 * 1024)) {
        d = (double) n / (1024LL * 1024 * 1024 * 1024);
        sprintf(s, "%.2fT", d);
    } else if (n < (1024LL * 1024 * 1024 * 1024 * 1024 * 1024)) {
        d = (double) n / (1024LL * 1024 * 1024 * 1024 * 1024);
        sprintf(s, "%.2fP", d);
    } else {
        /* Let's hope we never need this */
        sprintf(s, "%lluB", n);
    }
}

/* Create the string returned by the INFO command. This is decoupled
 * by the INFO command itself as we need to report the same information
 * on memory corruption problems. */
// 用于生成信息字符串的函数`genRedisInfoString`的一部分,主要目的是根据传入的 `section` 参数生成 Redis
// 服务器的相关信息。
sds genRedisInfoString(const char *section) {
    sds info = sdsempty();
    time_t uptime = server.unixtime - server.stat_starttime;
    int j;
    int allsections = 0, defsections = 0, everything = 0, modules = 0;
    // 记录已经处理的 section 数量,用于在不同部分之间插入换行符 `\r\n`。
    int sections = 0;

    if (section == NULL)
        section = "default";
    allsections = strcasecmp(section, "all") == 0;
    defsections = strcasecmp(section, "default") == 0;
    everything = strcasecmp(section, "everything") == 0;
    modules = strcasecmp(section, "modules") == 0;
    if (everything)
        allsections = 1;

    /* Server */
    if (allsections || defsections || !strcasecmp(section, "server")) {
        static int call_uname = 1;
        static struct utsname name;
        char *mode;
        char *supervised;

        // 获取运行模式
        if (server.cluster_enabled)
            mode = "cluster";
        else if (server.sentinel_mode)
            mode = "sentinel";
        else
            mode = "standalone";

        // 获取监督模式
        if (server.supervised) {
            if (server.supervised_mode == SUPERVISED_UPSTART)
                supervised = "upstart";
            else if (server.supervised_mode == SUPERVISED_SYSTEMD)
                supervised = "systemd";
            else
                supervised = "unknown";
        } else {
            supervised = "no";
        }

        if (sections++)
            info = sdscat(info, "\r\n");

        if (call_uname) {
            /* Uname can be slow and is always the same output. Cache it. */
            uname(&name);
            call_uname = 0;
        }

        unsigned int lruclock;
        atomicGet(server.lruclock, lruclock);
        info = sdscatfmt(info,
                         "# Server\r\n"
                         "redis_version:%s\r\n"
                         "redis_git_sha1:%s\r\n"
                         "redis_git_dirty:%i\r\n"
                         "redis_build_id:%s\r\n"
                         "redis_mode:%s\r\n"
                         "os:%s %s %s\r\n"
                         "arch_bits:%i\r\n"
                         "multiplexing_api:%s\r\n"
                         "atomicvar_api:%s\r\n"
                         "gcc_version:%i.%i.%i\r\n"
                         "process_id:%I\r\n"
                         "process_supervised:%s\r\n"
                         "run_id:%s\r\n"
                         "tcp_port:%i\r\n"
                         "server_time_usec:%I\r\n"
                         "uptime_in_seconds:%I\r\n"
                         "uptime_in_days:%I\r\n"
                         "hz:%i\r\n"
                         "configured_hz:%i\r\n"
                         "lru_clock:%u\r\n"
                         "executable:%s\r\n"
                         "config_file:%s\r\n"
                         "io_threads_active:%i\r\n",
                         REDIS_VERSION, redisGitSHA1(), strtol(redisGitDirty(), NULL, 10) > 0, redisBuildIdString(),
                         mode, name.sysname, name.release, name.machine, server.arch_bits, aeGetApiName(),
                         REDIS_ATOMIC_API,
#ifdef __GNUC__
                         __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__,
#else
                         0, 0, 0,
#endif
                         (int64_t) getpid(), supervised, server.runid, server.port ? server.port : server.tls_port,
                         (int64_t) server.ustime, (int64_t) uptime, (int64_t) (uptime / (3600 * 24)), server.hz,
                         server.config_hz, lruclock, server.executable ? server.executable : "",
                         server.configfile ? server.configfile : "", server.io_threads_active);
    }

    /* Clients */
    if (allsections || defsections || !strcasecmp(section, "clients")) {
        size_t maxin, maxout;
        getExpansiveClientsInfo(&maxin, &maxout);
        if (sections++)
            info = sdscat(info, "\r\n");
        info = sdscatprintf(info,
                            "# Clients\r\n"
                            "connected_clients:%lu\r\n"
                            "cluster_connections:%lu\r\n"
                            "maxclients:%u\r\n"
                            "client_recent_max_input_buffer:%zu\r\n"
                            "client_recent_max_output_buffer:%zu\r\n"
                            "blocked_clients:%d\r\n"
                            "tracking_clients:%d\r\n"
                            "clients_in_timeout_table:%llu\r\n",
                            listLength(server.clients) - listLength(server.slaves), getClusterConnectionsCount(),
                            server.maxclients, maxin, maxout, server.blocked_clients, server.tracking_clients,
                            (unsigned long long) raxSize(server.clients_timeout_table));
    }

    /* Memory */
    if (allsections || defsections || !strcasecmp(section, "memory")) {
        char hmem[64];
        char peak_hmem[64];
        char total_system_hmem[64];
        char used_memory_lua_hmem[64];
        char used_memory_scripts_hmem[64];
        char used_memory_rss_hmem[64];
        char maxmemory_hmem[64];
        size_t zmalloc_used = zmalloc_used_memory();
        size_t total_system_mem = server.system_memory_size;
        const char *evict_policy = evictPolicyToString();
        long long memory_lua = server.lua ? (long long) lua_gc(server.lua, LUA_GCCOUNT, 0) * 1024 : 0;
        struct redisMemOverhead *mh = getMemoryOverheadData();

        /* Peak memory is updated from time to time by serverCron() so it
         * may happen that the instantaneous value is slightly bigger than
         * the peak value. This may confuse users, so we update the peak
         * if found smaller than the current memory usage. */
        if (zmalloc_used > server.stat_peak_memory)
            server.stat_peak_memory = zmalloc_used;

        bytesToHuman(hmem, zmalloc_used);
        bytesToHuman(peak_hmem, server.stat_peak_memory);
        bytesToHuman(total_system_hmem, total_system_mem);
        bytesToHuman(used_memory_lua_hmem, memory_lua);
        bytesToHuman(used_memory_scripts_hmem, mh->lua_caches);
        bytesToHuman(used_memory_rss_hmem, server.cron_malloc_stats.process_rss);
        bytesToHuman(maxmemory_hmem, server.maxmemory);

        if (sections++)
            info = sdscat(info, "\r\n");
        info = sdscatprintf(
                info,
                "# Memory\r\n"
                "used_memory:%zu\r\n"
                "used_memory_human:%s\r\n"
                "used_memory_rss:%zu\r\n"
                "used_memory_rss_human:%s\r\n"
                "used_memory_peak:%zu\r\n"
                "used_memory_peak_human:%s\r\n"
                "used_memory_peak_perc:%.2f%%\r\n"
                "used_memory_overhead:%zu\r\n"
                "used_memory_startup:%zu\r\n"
                "used_memory_dataset:%zu\r\n"
                "used_memory_dataset_perc:%.2f%%\r\n"
                "allocator_allocated:%zu\r\n"
                "allocator_active:%zu\r\n"
                "allocator_resident:%zu\r\n"
                "total_system_memory:%lu\r\n"
                "total_system_memory_human:%s\r\n"
                "used_memory_lua:%lld\r\n"
                "used_memory_lua_human:%s\r\n"
                "used_memory_scripts:%lld\r\n"
                "used_memory_scripts_human:%s\r\n"
                "number_of_cached_scripts:%lu\r\n"
                "maxmemory:%lld\r\n"
                "maxmemory_human:%s\r\n"
                "maxmemory_policy:%s\r\n"
                "allocator_frag_ratio:%.2f\r\n"
                "allocator_frag_bytes:%zu\r\n"
                "allocator_rss_ratio:%.2f\r\n"
                "allocator_rss_bytes:%zd\r\n"
                "rss_overhead_ratio:%.2f\r\n"
                "rss_overhead_bytes:%zd\r\n"
                "mem_fragmentation_ratio:%.2f\r\n"
                "mem_fragmentation_bytes:%zd\r\n"
                "mem_not_counted_for_evict:%zu\r\n"
                "mem_replication_backlog:%zu\r\n"
                "mem_clients_slaves:%zu\r\n"
                "mem_clients_normal:%zu\r\n"
                "mem_aof_buffer:%zu\r\n"
                "mem_allocator:%s\r\n"
                "active_defrag_running:%d\r\n"
                "lazyfree_pending_objects:%zu\r\n"
                "lazyfreed_objects:%zu\r\n",
                zmalloc_used, hmem, server.cron_malloc_stats.process_rss, used_memory_rss_hmem, server.stat_peak_memory,
                peak_hmem, mh->peak_perc, mh->overhead_total, mh->startup_allocated, mh->dataset, mh->dataset_perc,
                server.cron_malloc_stats.allocator_allocated, server.cron_malloc_stats.allocator_active,
                server.cron_malloc_stats.allocator_resident, (unsigned long) total_system_mem, total_system_hmem,
                memory_lua, used_memory_lua_hmem, (long long) mh->lua_caches, used_memory_scripts_hmem,
                dictSize(server.lua_scripts), server.maxmemory, maxmemory_hmem, evict_policy, mh->allocator_frag,
                mh->allocator_frag_bytes, mh->allocator_rss, mh->allocator_rss_bytes, mh->rss_extra,
                mh->rss_extra_bytes, mh->total_frag, /* This is the total RSS overhead, including
                                              fragmentation, but not just it. This field
                                              (and the next one) is named like that just
                                              for backward compatibility. */
                mh->total_frag_bytes, freeMemoryGetNotCountedMemory(), mh->repl_backlog, mh->clients_slaves,
                mh->clients_normal, mh->aof_buffer, ZMALLOC_LIB, server.active_defrag_running,
                lazyfreeGetPendingObjectsCount(), lazyfreeGetFreedObjectsCount());
        freeMemoryOverheadData(mh);
    }

    /* Persistence */
    /* 持久性 */
    if (allsections || defsections || !strcasecmp(section, "persistence")) {
        if (sections++)
            info = sdscat(info, "\r\n");
        info = sdscatprintf(
                info,
                "# Persistence\r\n"
                "loading:%d\r\n"
                "current_cow_size:%zu\r\n"
                "rdb_changes_since_last_save:%lld\r\n"
                "rdb_bgsave_in_progress:%d\r\n"
                "rdb_last_save_time:%jd\r\n"
                "rdb_last_bgsave_status:%s\r\n"
                "rdb_last_bgsave_time_sec:%jd\r\n"
                "rdb_current_bgsave_time_sec:%jd\r\n"
                "rdb_last_cow_size:%zu\r\n"
                "aof_enabled:%d\r\n"
                "aof_rewrite_in_progress:%d\r\n"
                "aof_rewrite_scheduled:%d\r\n"
                "aof_last_rewrite_time_sec:%jd\r\n"
                "aof_current_rewrite_time_sec:%jd\r\n"
                "aof_last_bgrewrite_status:%s\r\n"
                "aof_last_write_status:%s\r\n"
                "aof_last_cow_size:%zu\r\n"
                "module_fork_in_progress:%d\r\n"
                "module_fork_last_cow_size:%zu\r\n",
                (int) server.loading, server.stat_current_cow_bytes, server.dirty, server.child_type == CHILD_TYPE_RDB,
                (intmax_t) server.lastsave, (server.lastbgsave_status == C_OK) ? "ok" : "err",
                (intmax_t) server.rdb_save_time_last,
                (intmax_t) ((server.child_type != CHILD_TYPE_RDB) ? -1 : time(NULL) - server.rdb_save_time_start),
                server.stat_rdb_cow_bytes, server.aof_state != AOF_OFF, server.child_type == CHILD_TYPE_AOF,
                server.aof_rewrite_scheduled, (intmax_t) server.aof_rewrite_time_last,
                (intmax_t) ((server.child_type != CHILD_TYPE_AOF) ? -1 : time(NULL) - server.aof_rewrite_time_start),
                (server.aof_lastbgrewrite_status == C_OK) ? "ok" : "err",
                (server.aof_last_write_status == C_OK) ? "ok" : "err", server.stat_aof_cow_bytes,
                server.child_type == CHILD_TYPE_MODULE, server.stat_module_cow_bytes);

        if (server.aof_enabled) {
            info = sdscatprintf(info,
                                "aof_current_size:%lld\r\n"
                                "aof_base_size:%lld\r\n"
                                "aof_pending_rewrite:%d\r\n"
                                "aof_buffer_length:%zu\r\n"
                                "aof_rewrite_buffer_length:%lu\r\n"
                                "aof_pending_bio_fsync:%llu\r\n"
                                "aof_delayed_fsync:%lu\r\n",
                                (long long) server.aof_current_size, (long long) server.aof_rewrite_base_size,
                                server.aof_rewrite_scheduled, sdslen(server.aof_buf), aofRewriteBufferSize(),
                                bioPendingJobsOfType(BIO_AOF_FSYNC), server.aof_delayed_fsync);
        }

        if (server.loading) {
            double perc = 0;
            time_t eta, elapsed;
            off_t remaining_bytes = 1;

            if (server.loading_total_bytes) {
                perc = ((double) server.loading_loaded_bytes / server.loading_total_bytes) * 100;
                remaining_bytes = server.loading_total_bytes - server.loading_loaded_bytes;
            } else if (server.loading_rdb_used_mem) {
                perc = ((double) server.loading_loaded_bytes / server.loading_rdb_used_mem) * 100;
                remaining_bytes = server.loading_rdb_used_mem - server.loading_loaded_bytes;
                /* used mem is only a (bad) estimation of the rdb file size, avoid going over 100% */
                if (perc > 99.99)
                    perc = 99.99;
                if (remaining_bytes < 1)
                    remaining_bytes = 1;
            }

            elapsed = time(NULL) - server.loading_start_time;
            if (elapsed == 0) {
                eta = 1; /* A fake 1 second figure if we don't have
                            enough info */
            } else {
                eta = (elapsed * remaining_bytes) / (server.loading_loaded_bytes + 1);
            }

            info = sdscatprintf(info,
                                "loading_start_time:%jd\r\n"
                                "loading_total_bytes:%llu\r\n"
                                "loading_rdb_used_mem:%llu\r\n"
                                "loading_loaded_bytes:%llu\r\n"
                                "loading_loaded_perc:%.2f\r\n"
                                "loading_eta_seconds:%jd\r\n",
                                (intmax_t) server.loading_start_time, (unsigned long long) server.loading_total_bytes,
                                (unsigned long long) server.loading_rdb_used_mem,
                                (unsigned long long) server.loading_loaded_bytes, perc, (intmax_t) eta);
        }
    }

    /* Stats */
    /* 统计数据 */
    if (allsections || defsections || !strcasecmp(section, "stats")) {
        long long stat_total_reads_processed, stat_total_writes_processed;
        long long stat_net_input_bytes, stat_net_output_bytes;
        atomicGet(server.stat_total_reads_processed, stat_total_reads_processed);
        atomicGet(server.stat_total_writes_processed, stat_total_writes_processed);
        atomicGet(server.stat_net_input_bytes, stat_net_input_bytes);
        atomicGet(server.stat_net_output_bytes, stat_net_output_bytes);

        if (sections++)
            info = sdscat(info, "\r\n");
        info = sdscatprintf(
                info,
                "# Stats\r\n"
                "total_connections_received:%lld\r\n"
                "total_commands_processed:%lld\r\n"
                "instantaneous_ops_per_sec:%lld\r\n"
                "total_net_input_bytes:%lld\r\n"
                "total_net_output_bytes:%lld\r\n"
                "instantaneous_input_kbps:%.2f\r\n"
                "instantaneous_output_kbps:%.2f\r\n"
                "rejected_connections:%lld\r\n"
                "sync_full:%lld\r\n"
                "sync_partial_ok:%lld\r\n"
                "sync_partial_err:%lld\r\n"
                "expired_keys:%lld\r\n"
                "expired_stale_perc:%.2f\r\n"
                "expired_time_cap_reached_count:%lld\r\n"
                "expire_cycle_cpu_milliseconds:%lld\r\n"
                "evicted_keys:%lld\r\n"
                "keyspace_hits:%lld\r\n"
                "keyspace_misses:%lld\r\n"
                "pubsub_channels:%ld\r\n"
                "pubsub_patterns:%lu\r\n"
                "latest_fork_usec:%lld\r\n"
                "total_forks:%lld\r\n"
                "migrate_cached_sockets:%ld\r\n"
                "slave_expires_tracked_keys:%zu\r\n"
                "active_defrag_hits:%lld\r\n"
                "active_defrag_misses:%lld\r\n"
                "active_defrag_key_hits:%lld\r\n"
                "active_defrag_key_misses:%lld\r\n"
                "tracking_total_keys:%lld\r\n"
                "tracking_total_items:%lld\r\n"
                "tracking_total_prefixes:%lld\r\n"
                "unexpected_error_replies:%lld\r\n"
                "total_error_replies:%lld\r\n"
                "dump_payload_sanitizations:%lld\r\n"
                "total_reads_processed:%lld\r\n"
                "total_writes_processed:%lld\r\n"
                "io_threaded_reads_processed:%lld\r\n"
                "io_threaded_writes_processed:%lld\r\n",
                server.stat_numconnections, server.stat_numcommands, getInstantaneousMetric(STATS_METRIC_COMMAND),
                stat_net_input_bytes, stat_net_output_bytes,
                (float) getInstantaneousMetric(STATS_METRIC_NET_INPUT) / 1024,
                (float) getInstantaneousMetric(STATS_METRIC_NET_OUTPUT) / 1024, server.stat_rejected_conn,
                server.stat_sync_full, server.stat_sync_partial_ok, server.stat_sync_partial_err,
                server.stat_expiredkeys, server.stat_expired_stale_perc * 100,
                server.stat_expired_time_cap_reached_count, server.stat_expire_cycle_time_used / 1000,
                server.stat_evictedkeys, server.stat_keyspace_hits, server.stat_keyspace_misses,
                dictSize(server.pubsub_channels), listLength(server.pubsub_patterns), server.stat_fork_time,
                server.stat_total_forks, dictSize(server.migrate_cached_sockets), getSlaveKeyWithExpireCount(),
                server.stat_active_defrag_hits, server.stat_active_defrag_misses, server.stat_active_defrag_key_hits,
                server.stat_active_defrag_key_misses, (unsigned long long) trackingGetTotalKeys(),
                (unsigned long long) trackingGetTotalItems(), (unsigned long long) trackingGetTotalPrefixes(),
                server.stat_unexpected_error_replies, server.stat_total_error_replies,
                server.stat_dump_payload_sanitizations, stat_total_reads_processed, stat_total_writes_processed,
                server.stat_io_reads_processed, server.stat_io_writes_processed);
    }

    /* Replication */
    /* 复制 */
    if (allsections || defsections || !strcasecmp(section, "replication")) {
        if (sections++)
            info = sdscat(info, "\r\n");
        info = sdscatprintf(info,
                            "# Replication\r\n"
                            "role:%s\r\n",
                            server.masterhost == NULL ? "master" : "slave");
        if (server.masterhost) {
            long long slave_repl_offset = 1;

            if (server.master)
                slave_repl_offset = server.master->reploff;
            else if (server.cached_master)
                slave_repl_offset = server.cached_master->reploff;

            info = sdscatprintf(info,
                                "master_host:%s\r\n"
                                "master_port:%d\r\n"
                                "master_link_status:%s\r\n"
                                "master_last_io_seconds_ago:%d\r\n"
                                "master_sync_in_progress:%d\r\n"
                                "slave_repl_offset:%lld\r\n",
                                server.masterhost, server.masterport,
                                (server.repl_state == REPL_STATE_CONNECTED) ? "up" : "down",
                                server.master ? ((int) (server.unixtime - server.master->lastinteraction)) : -1,
                                server.repl_state == REPL_STATE_TRANSFER, slave_repl_offset);

            if (server.repl_state == REPL_STATE_TRANSFER) {
                double perc = 0;
                if (server.repl_transfer_size) {
                    perc = ((double) server.repl_transfer_read / server.repl_transfer_size) * 100;
                }
                info = sdscatprintf(info,
                                    "master_sync_total_bytes:%lld\r\n"
                                    "master_sync_read_bytes:%lld\r\n"
                                    "master_sync_left_bytes:%lld\r\n"
                                    "master_sync_perc:%.2f\r\n"
                                    "master_sync_last_io_seconds_ago:%d\r\n",
                                    (long long) server.repl_transfer_size, (long long) server.repl_transfer_read,
                                    (long long) (server.repl_transfer_size - server.repl_transfer_read), perc,
                                    (int) (server.unixtime - server.repl_transfer_lastio));
            }

            if (server.repl_state != REPL_STATE_CONNECTED) {
                info = sdscatprintf(info, "master_link_down_since_seconds:%jd\r\n",
                                    (intmax_t) (server.unixtime - server.repl_down_since));
            }
            info = sdscatprintf(info,
                                "slave_priority:%d\r\n"
                                "slave_read_only:%d\r\n",
                                server.slave_priority, server.repl_slave_ro);
        }

        info = sdscatprintf(info, "connected_slaves:%lu\r\n", listLength(server.slaves));

        /* If min-slaves-to-write is active, write the number of slaves
         * currently considered 'good'. */
        if (server.repl_min_slaves_to_write && server.repl_min_slaves_max_lag) {
            info = sdscatprintf(info, "min_slaves_good_slaves:%d\r\n", server.repl_good_slaves_count);
        }

        if (listLength(server.slaves)) {
            int slaveid = 0;
            listNode *ln;
            listIter li;

            listRewind(server.slaves, &li);
            while ((ln = listNext(&li))) {
                client *slave = listNodeValue(ln);
                char *state = NULL;
                char ip[NET_IP_STR_LEN], *slaveip = slave->slave_ip;
                int port;
                long lag = 0;

                if (slaveip[0] == '\0') {
                    if (connPeerToString(slave->conn, ip, sizeof(ip), &port) == -1)
                        continue;
                    slaveip = ip;
                }
                switch (slave->replstate) {
                    case SLAVE_STATE_WAIT_BGSAVE_START:
                    case SLAVE_STATE_WAIT_BGSAVE_END:
                        state = "wait_bgsave";
                        break;
                    case SLAVE_STATE_SEND_BULK:
                        state = "send_bulk";
                        break;
                    case SLAVE_STATE_ONLINE:
                        state = "online";
                        break;
                }
                if (state == NULL)
                    continue;
                if (slave->replstate == SLAVE_STATE_ONLINE)
                    lag = time(NULL) - slave->repl_ack_time;

                info = sdscatprintf(info,
                                    "slave%d:ip=%s,port=%d,state=%s,"
                                    "offset=%lld,lag=%ld\r\n",
                                    slaveid, slaveip, slave->slave_listening_port, state, slave->repl_ack_off, lag);
                slaveid++;
            }
        }
        info = sdscatprintf(info,
                            "master_failover_state:%s\r\n"
                            "master_replid:%s\r\n"
                            "master_replid2:%s\r\n"
                            "master_repl_offset:%lld\r\n"
                            "second_repl_offset:%lld\r\n"
                            "repl_backlog_active:%d\r\n"
                            "repl_backlog_size:%lld\r\n"
                            "repl_backlog_first_byte_offset:%lld\r\n"
                            "repl_backlog_histlen:%lld\r\n",
                            getFailoverStateString(), server.replid, server.replid2, server.master_repl_offset,
                            server.second_replid_offset, server.repl_backlog != NULL, server.repl_backlog_size,
                            server.repl_backlog_off, server.repl_backlog_histlen);
    }

    /* CPU */
    if (allsections || defsections || !strcasecmp(section, "cpu")) {
        if (sections++)
            info = sdscat(info, "\r\n");

        struct rusage self_ru, c_ru;
        getrusage(RUSAGE_SELF, &self_ru);
        getrusage(RUSAGE_CHILDREN, &c_ru);
        info = sdscatprintf(info,
                            "# CPU\r\n"
                            "used_cpu_sys:%ld.%06ld\r\n"
                            "used_cpu_user:%ld.%06ld\r\n"
                            "used_cpu_sys_children:%ld.%06ld\r\n"
                            "used_cpu_user_children:%ld.%06ld\r\n",
                            (long) self_ru.ru_stime.tv_sec, (long) self_ru.ru_stime.tv_usec,
                            (long) self_ru.ru_utime.tv_sec, (long) self_ru.ru_utime.tv_usec,
                            (long) c_ru.ru_stime.tv_sec, (long) c_ru.ru_stime.tv_usec, (long) c_ru.ru_utime.tv_sec,
                            (long) c_ru.ru_utime.tv_usec);
#ifdef RUSAGE_THREAD
        struct rusage m_ru;
        getrusage(RUSAGE_THREAD, &m_ru);
        info = sdscatprintf(info,
                            "used_cpu_sys_main_thread:%ld.%06ld\r\n"
                            "used_cpu_user_main_thread:%ld.%06ld\r\n",
                            (long) m_ru.ru_stime.tv_sec, (long) m_ru.ru_stime.tv_usec, (long) m_ru.ru_utime.tv_sec,
                            (long) m_ru.ru_utime.tv_usec);
#endif /* RUSAGE_THREAD */
    }

    /* Modules */
    if (allsections || defsections || !strcasecmp(section, "modules")) {
        if (sections++)
            info = sdscat(info, "\r\n");
        info = sdscatprintf(info, "# Modules\r\n");
        info = genModulesInfoString(info);
    }

    /* Command statistics */
    /*命令统计信息*/
    if (allsections || !strcasecmp(section, "commandstats")) {
        if (sections++)
            info = sdscat(info, "\r\n");
        info = sdscatprintf(info, "# Commandstats\r\n");

        struct redisCommand *c;
        dictEntry *de;
        dictIterator *di;
        di = dictGetSafeIterator(server.commands);
        while ((de = dictNext(di)) != NULL) {
            c = (struct redisCommand *) dictGetVal(de);
            if (!c->calls && !c->failed_calls && !c->rejected_calls)
                continue;
            info = sdscatprintf(info,
                                "cmdstat_%s:calls=%lld,usec=%lld,usec_per_call=%.2f"
                                ",rejected_calls=%lld,failed_calls=%lld\r\n",
                                c->name, c->calls, c->microseconds,
                                (c->calls == 0) ? 0 : ((float) c->microseconds / c->calls), c->rejected_calls,
                                c->failed_calls);
        }
        dictReleaseIterator(di);
    }

    /* Error statistics */
    /*错误统计信息*/
    if (allsections || defsections || !strcasecmp(section, "errorstats")) {
        if (sections++)
            info = sdscat(info, "\r\n");
        info = sdscat(info, "# Errorstats\r\n");
        raxIterator ri;
        raxStart(&ri, server.errors);
        raxSeek(&ri, "^", NULL, 0);
        struct redisError *e;
        while (raxNext(&ri)) {
            e = (struct redisError *) ri.data;
            info = sdscatprintf(info, "errorstat_%.*s:count=%lld\r\n", (int) ri.key_len, ri.key, e->count);
        }
        raxStop(&ri);
    }

    /* Cluster */
    /* 集群 */
    if (allsections || defsections || !strcasecmp(section, "cluster")) {
        if (sections++)
            info = sdscat(info, "\r\n");
        info = sdscatprintf(info,
                            "# Cluster\r\n"
                            "cluster_enabled:%d\r\n",
                            server.cluster_enabled);
    }

    /* Key space */
    /* 键空间 */
    if (allsections || defsections || !strcasecmp(section, "keyspace")) {
        if (sections++)
            info = sdscat(info, "\r\n");
        info = sdscatprintf(info, "# Keyspace\r\n");
        for (j = 0; j < server.dbnum; j++) {
            long long keys, vkeys;

            keys = dictSize(server.db[j].dict);
            vkeys = dictSize(server.db[j].expires);
            if (keys || vkeys) {
                info = sdscatprintf(info, "db%d:keys=%lld,expires=%lld,avg_ttl=%lld\r\n", j, keys, vkeys,
                                    server.db[j].avg_ttl);
            }
        }
    }

    /* Get info from modules.
     * if user asked for "everything" or "modules", or a specific section
     * that's not found yet. */
    if (everything || modules || (!allsections && !defsections && sections == 0)) {
        info = modulesCollectInfo(info, everything || modules ? NULL : section, 0, /* not a crash report */
                                  sections);
    }
    return info;
}

// info 命令
void infoCommand(client *c) {
    char *section = c->argc == 2 ? c->argv[1]->ptr : "default";

    if (c->argc > 2) {
        addReplyErrorObject(c, shared.syntaxerr);
        return;
    }
    sds info = genRedisInfoString(section);
    addReplyVerbatim(c, info, sdslen(info), "txt");
    sdsfree(info);
}

