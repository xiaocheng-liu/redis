/* 本仓库基于 Redis 6.2 代码基线（含 tracking.c、lolwut6.c、timeout.c、tls.c、
 * sha256.c、listpack.c 等 6.2 期特性），原 version.h 误标为 6.0.0，此处校正为
 * 6.2.0 以与实际代码一致。
 *
 * REDIS_VERSION_NUM 仍为 0x00ffffff：这是 Redis 源码构建的 "unstable" 标记，
 * 表示并非官方发布版本（本仓库为阅读/注释用途的 fork），故保留该标记而非写成
 * 0x00060200。module.c 中 RM_GetServerVersion() 会返回此值供模块判断版本。 */
#define REDIS_VERSION "6.2.0"
#define REDIS_VERSION_NUM 0x00ffffff
