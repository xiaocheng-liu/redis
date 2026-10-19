# AGENTS.md

给 AI 编码助手（CodeBuddy / Codex / Cursor 等）的项目指引。阅读代码或改动前请先读完本文件。

## 1. 项目是什么

- 这是 **Redis 6.x 源码的中文注解版**仓库（fork 自 `xindoo/redis`，上游又是 Redis 官方仓库的 fork）。
- 目的：**阅读、调试、理解 Redis 内核**，并在此基础上做**服务端代码模块化拆分**与**中文注释补全**。它不是业务项目，也**不是 Redis 的使用/配置教程**。
- 版本基线：`src/version.h` 标 `REDIS_VERSION "6.0.0"` + `REDIS_VERSION_NUM 0x00ffffff`（unstable 标记），但代码已含 `lolwut6.c`、`tracking.c`（客户端缓存）、`tls.c`、`sha256.c`、`timeout.c` 等 6.2 期特性。**引用 API 时以源码为准，不要只信 version.h。**
- 当前分支 `my`，HEAD 提交主题为「拆分服务端代码并整理源码注释」。

## 2. 构建

根目录 `Makefile` 只是转发，真正的构建在 `src/Makefile`。

```bash
make -j8                 # 等价于 cd src && make -j8
cd src && make -j8       # 推荐，输出更直接
make distclean           # 深度清理（含 deps），重建耗时长
```

- 默认 `MALLOC=libc`（`src/Makefile` 第 58 行）。切换：`make MALLOC=jemalloc`。
- 依赖在 `deps/`：jemalloc、lua、hiredis、linenoise、hdr_histogram，**首次构建会编译它们**。
- 产物：`src/redis-server`、`redis-cli`、`redis-benchmark`、`redis-sentinel`、`redis-check-aof`、`redis-check-rdb`。
- 验证版本：`./src/redis-server --version`。

## 3. 运行与调试

```bash
./src/redis-server ./redis.conf
./src/redis-cli
./src/redis-sentinel ./sentinel.conf
```

- 调试配置已就绪：`.vscode/launch.json`（lldb）+ `.vscode/tasks.json`（makefile-tools），断点入口建议打在 `src/server.c:main()`。
- IntelliSense：`C_Cpp.default.configurationProvider = ms-vscode.makefile-tools`，编译器 `/usr/bin/clang`，标准 `c11`。

## 4. 测试

用 TCL（需 tclsh 8.5+）：

```bash
./runtest                          # 全量单测
./runtest --single unit/expire     # 跑单个文件
./runtest --single unit/expire --verbose
./runtest-cluster                  # 集群
./runtest-sentinel                 # 哨兵
./runtest-moduleapi                # 模块 API
```

测试代码在 `tests/`：`unit/`（138 个 .tcl）、`integration/`、`cluster/`、`sentinel/`、`modules/`、`helpers/`。

**任何对 `src/` 的改动，尤其是 `server_*.c` 拆分相关的跨文件调整，都要跑一次 `./runtest` 回归。**

## 5. 目录与模块地图

```
src/     核心源码（113 个 .c / 61 个 .h，约 12 万行）
deps/    第三方依赖
tests/   TCL 测试
utils/   脚本工具（create-cluster、hashtable、lru、graphs…）
document/ 笔记与文档（原为空，逐步补充）
```

### server.c 拆分约定（本项目最重要的自定义结构）

原 Redis `server.c` 是 6000+ 行巨石文件，已按职责拆为 15 个 `server_*.c`，`server.c` 只剩 `main()` 和全局变量。**新代码必须放进职责对应的文件，不要往 `server.c` 里塞。**

| 文件 | 行数 | 职责 |
| --- | --- | --- |
| `server.c` | 159 | 主入口 `main()`、全局 `server`/`shared` |
| `server_command_exec.c` | 983 | `call()`、`processCommand()`、命令传播、停机 |
| `server_info.c` | 667 | `genRedisInfoString()`、`infoCommand` |
| `server_commands.c/.h` | 679 / 498 | `redisCommandTable[]`、`populateCommandTable()`、PING/ECHO/TIME/COMMAND/MONITOR |
| `server_process.c` | 590 | 进程/子进程、信号、daemonize、proc title |
| `server_cron.c` | 527 | `serverCron()`、`beforeSleep()`/`afterSleep()` |
| `server_startup.c` | 382 | 启动日志、usage、ASCII logo、`loadDataFromDisk()` |
| `server_dict.c` | 377 | dict 回调、渐进式 rehash、resize 策略 |
| `server_memory.c` | 355 | OOM score、`adjustOpenFilesLimit()`、内存告警 |
| `server_init.c` | 349 | `initServer()`、`InitServerLast()`、`listenToPort()` |
| `server_clients.c` | 182 | `clientsCron()`、查询缓冲裁剪 |
| `server_config_init.c` | 165 | `initServerConfig()` |
| `server_shared.c` | 155 | `createSharedObjects()` |
| `server_log.c` | 114 | `serverLog()` 系列 |
| `server_time.c` / `server_stats.c` | 78 / 85 | 时间缓存、瞬时指标、命令/错误统计 |

新增目标文件时**必须同步 `src/Makefile` 的 `REDIS_SERVER_OBJ`**，否则不会参与链接。

### 关键函数导航

| 想看什么 | 去哪 |
| --- | --- |
| 启动流程 | `src/server.c` → `server_startup.c` → `server_init.c` |
| 事件循环 | `src/ae.c`、`ae_kqueue.c`（macOS）、`server_cron.c` |
| 命令分发 | `server_command_exec.c:processCommand()`、`call()` |
| 命令表 | `server_commands.c:redisCommandTable[]` |
| 网络 I/O | `src/networking.c`、`connection.c` |
| 数据结构 | `sds.c`、`adlist.c`、`dict.c`、`ziplist.c`、`quicklist.c`、`intset.c`、`rax.c`、`hyperloglog.c` |
| 类型命令 | `t_string.c`、`t_list.c`、`t_hash.c`、`t_set.c`、`t_zset.c`、`t_stream.c` |
| 对象系统/淘汰 | `object.c`、`evict.c`、`expire.c`、`db.c` |
| 持久化 | `rdb.c`、`aof.c`、`rio.c` |
| 复制/集群/哨兵 | `replication.c`、`cluster.c`、`sentinel.c` |
| 内存分配 | `zmalloc.c`、`deps/jemalloc` |

## 6. 代码风格

- 语言标准 C11，编译器 clang（macOS/arm64）。
- 格式化规则见根目录 `.clang-format`：`BasedOnStyle: LLVM`，**缩进 4 空格**，**列宽 120**，`AfterFunction: false`（函数左花括号**不换行**，与函数签名同一行）。
- 注释一律用 `/* */`，不用 `//`（存量代码里有少量 `//`，新代码遵循前者）。
- 命名沿用 Redis 风格：小写驼峰（`createSharedObjects`、`dictSdsHash`），宏全大写。

## 7. 中文注释规范

这是本仓库的核心价值，**改代码时必须一并维护注释**：

1. 注释写在**被解释代码的上方**（不是行尾），保持与代码相同缩进。
2. 保留有价值的**原始英文注释**，在其下方或同块追加中文解释；纯翻译性的中文注释不要删掉英文原文。
3. 注释粒度：解释「**为什么这么做**」和「这段在整体流程中的位置」，不要逐行翻译语法。
4. 使用简体中文，术语保留英文原词（robj、dict、rehash、AOF、RDB、client…），不要生造译名。
5. 修改函数逻辑后，**同步更新该函数头部的中文注释**，避免注释与实现脱节。

已含中文注释的覆盖率：`src/` 下 68/113 个 `.c`。密度最高：`networking.c`、`dict.c`、`sentinel.c`、`ziplist.c`、`sds.c`、`ae.c`、`server_command_exec.c`。基本未注释：`server_info.c`、`server_clients.c`、`module.c`、`lolwut*.c`。

## 8. 修改约定

- **只做读代码 / 加注释 / 拆分整理时，不要改变 Redis 的运行时行为。**任何行为变更都要明确告知用户。
- 拆分、移动函数时保证**符号可见性**：跨文件调用的符号必须保留为非 `static`，并在 `server.h` / `server_commands.h` 中声明。
- `server.h` 目前仍有 1950 行，是拆分后剩下的"巨石头文件"；若继续拆分，优先考虑拆出结构体定义与函数声明。
- **不要提交构建产物**：`*.o`、`*.d`、`*.gcda`、`*.gcno`、`redis-server`、`redis-cli` 等（`.gitignore` 已覆盖大部分）。
- `src/` 下存在运行残留 `appendonly.aof`、`dump.rdb`、`nodes.conf`、`redis.log`，**不要**把它们当作源码修改，也不要提交。
- `redis.conf` / `sentinel.conf` 是根目录下的正式配置，`src/redis.conf`、`src/nodes.conf` 是运行残留，**改配置改根目录那份**。

## 9. Git 约定

- 提交信息用**中文**，简明动宾短语，例如：`docs: 补充 dict 渐进式 rehash 注释`。
- 常用前缀：`feat:` / `fix:` / `docs:` / `refactor:` / `style:`。
- 远端有两个：`github`（GitHub）和 `origin`（Gitee 镜像），推送时注意区分。
- 提交前确认工作区没有混入 `.o`、`.rdb`、`.aof`、`redis.log`。

## 10. 已知问题（可作为改进方向）

1. `src/version.h` 版本号与实际代码基线（含 6.2 特性）不一致。
2. `server.h` 过大，待进一步拆分。
3. `document/` 目录内容尚待补充，模块地图可沉淀于此。
4. `src/` 下运行产物待清理。
