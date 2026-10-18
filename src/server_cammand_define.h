/*
 * Redis 命令处理函数声明，供命令表注册及模块间调用使用。
 * 参数 c 为发起命令的客户端，命令参数保存在 c->argc 和 c->argv 中；
 * 处理函数通过客户端回复接口返回结果，具体实现位于对应的源文件中。
 */
#ifndef SERVER_CAMMAND_DEFINE_H
#define SERVER_CAMMAND_DEFINE_H

#include "server.h"

/* 连接检查、服务器信息与诊断。 */
/* 检查连接是否正常，可回显指定消息。 */
void pingCommand(client *c);
/* 原样返回客户端提供的消息。 */
void echoCommand(client *c);
/* 返回服务器当前的 Unix 秒时间戳和微秒部分。 */
void timeCommand(client *c);
/* 返回服务器运行状态和统计信息。 */
void infoCommand(client *c);
/* 返回本项目定义的 Redis 版本字符串。 */
void versionCommand(client *c);
/* 将当前客户端加入监控列表，接收命令执行信息。 */
void monitorCommand(client *c);
/* 查询、重置或分析延迟监控记录。 */
void latencyCommand(client *c);
/* 加载、卸载或查询 Redis 扩展模块。 */
void moduleCommand(client *c);
/* 执行服务器调试和诊断子命令。 */
void debugCommand(client *c);

/* 访问控制与认证。 */
/* 管理用户、命令权限和键访问规则。 */
void aclCommand(client *c);
/* 校验用户名和密码，设置客户端认证状态。 */
void authCommand(client *c);

/* 命令元数据与字符串操作。 */
/* 查询命令元数据、命令数量或命令涉及的键。 */
void commandCommand(client *c);
/* 设置字符串值，支持条件写入和过期选项。 */
void setCommand(client *c);
/* 仅在键不存在时设置字符串值。 */
void setnxCommand(client *c);
/* 设置字符串值及以秒为单位的有效期。 */
void setexCommand(client *c);
/* 设置字符串值及以毫秒为单位的有效期。 */
void psetexCommand(client *c);
/* 读取字符串值，键不存在时返回空值。 */
void getCommand(client *c);
/* 读取字符串值，并按选项设置或移除过期时间。 */
void getexCommand(client *c);
/* 读取字符串值并删除对应键。 */
void getdelCommand(client *c);
/* 从指定字节偏移开始覆盖字符串，长度不足时补零。 */
void setrangeCommand(client *c);
/* 读取指定字节范围的子串，也用于 SUBSTR 命令。 */
void getrangeCommand(client *c);
/* 将键保存的整数加一。 */
void incrCommand(client *c);
/* 将键保存的整数减一。 */
void decrCommand(client *c);
/* 将键保存的整数加上指定增量。 */
void incrbyCommand(client *c);
/* 将键保存的整数减去指定数值。 */
void decrbyCommand(client *c);
/* 将键保存的数值加上指定浮点增量。 */
void incrbyfloatCommand(client *c);
/* 批量读取字符串值，不存在或非字符串的键返回空值。 */
void mgetCommand(client *c);
/* 设置新的字符串值并返回旧值。 */
void getsetCommand(client *c);
/* 原子地设置多个字符串键值对。 */
void msetCommand(client *c);
/* 仅在所有目标键均不存在时，原子地设置多个键值对。 */
void msetnxCommand(client *c);
/* 向字符串末尾追加内容，键不存在时创建。 */
void appendCommand(client *c);
/* 返回字符串的字节长度。 */
void strlenCommand(client *c);
/* 执行字符串算法，当前支持最长公共子序列 LCS。 */
void stralgoCommand(client *c);

/* 键管理与数据库操作。 */
/* 删除指定键，按 lazyfree-lazy-user-del 配置决定值的释放方式。 */
void delCommand(client *c);
/* 移除指定键，并按对象释放成本决定是否后台释放值。 */
void unlinkCommand(client *c);
/* 统计给定键中存在的数量，重复键重复计数。 */
void existsCommand(client *c);
/* 切换当前客户端使用的逻辑数据库。 */
void selectCommand(client *c);
/* 交换两个逻辑数据库的数据。 */
void swapdbCommand(client *c);
/* 从当前数据库随机返回一个未过期的键。 */
void randomkeyCommand(client *c);
/* 遍历当前数据库，返回匹配模式的键。 */
void keysCommand(client *c);
/* 通过游标分批遍历当前数据库的键。 */
void scanCommand(client *c);
/* 返回当前数据库键字典中的键数量。 */
void dbsizeCommand(client *c);
/* 返回最近一次成功保存 RDB 的 Unix 秒时间戳。 */
void lastsaveCommand(client *c);
/* 按选项执行持久化处理并关闭服务器。 */
void shutdownCommand(client *c);
/* 将键移动到指定逻辑数据库，目标键必须不存在。 */
void moveCommand(client *c);
/* 复制键的值，支持指定目标数据库和覆盖选项。 */
void copyCommand(client *c);
/* 重命名键，目标键存在时覆盖。 */
void renameCommand(client *c);
/* 仅在目标键不存在时重命名键。 */
void renamenxCommand(client *c);
/* 清空当前数据库，支持同步或异步释放。 */
void flushdbCommand(client *c);
/* 清空全部数据库，支持同步或异步释放。 */
void flushallCommand(client *c);

/* 位图与位字段操作。 */
/* 对字符串执行按位运算，并将结果保存到目标键。 */
void bitopCommand(client *c);
/* 统计字符串指定字节范围内值为一的位数。 */
void bitcountCommand(client *c);
/* 查找字符串中第一个指定值的位位置。 */
void bitposCommand(client *c);
/* 设置指定偏移处的位，并返回原位值。 */
void setbitCommand(client *c);
/* 读取指定偏移处的位值。 */
void getbitCommand(client *c);
/* 读取或修改字符串中的整数字段，支持溢出策略。 */
void bitfieldCommand(client *c);
/* 只读访问字符串中的整数字段，仅支持 GET 子命令。 */
void bitfieldroCommand(client *c);

/* RDB 快照持久化。 */
/* 在当前进程同步生成 RDB 快照。 */
void saveCommand(client *c);
/* 启动后台 RDB 保存，支持在已有后台任务时安排后续保存。 */
void bgsaveCommand(client *c);

/* AOF 日志重写。 */
/* 启动或安排后台 AOF 重写。 */
void bgrewriteaofCommand(client *c);

/* 列表操作与键类型查询。 */
/* 将一个或多个元素依次插入列表头部。 */
void lpushCommand(client *c);
/* 将一个或多个元素依次插入列表尾部。 */
void rpushCommand(client *c);
/* 仅在列表存在时，将元素插入头部。 */
void lpushxCommand(client *c);
/* 仅在列表存在时，将元素插入尾部。 */
void rpushxCommand(client *c);
/* 在指定基准元素之前或之后插入元素。 */
void linsertCommand(client *c);
/* 移除并返回列表头部的一个或指定数量的元素。 */
void lpopCommand(client *c);
/* 移除并返回列表尾部的一个或指定数量的元素。 */
void rpopCommand(client *c);
/* 返回列表元素数量。 */
void llenCommand(client *c);
/* 读取指定索引的列表元素，支持负索引。 */
void lindexCommand(client *c);
/* 读取指定索引范围内的列表元素。 */
void lrangeCommand(client *c);
/* 仅保留指定索引范围内的列表元素。 */
void ltrimCommand(client *c);
/* 返回键所保存值的数据类型。 */
void typeCommand(client *c);
/* 修改指定索引位置的列表元素。 */
void lsetCommand(client *c);
/* 按数量和方向删除与给定值相同的列表元素。 */
void lremCommand(client *c);
/* 查找列表中匹配元素的位置，支持返回多个位置。 */
void lposCommand(client *c);
/* 将源列表尾部元素原子地移动到目标列表头部。 */
void rpoplpushCommand(client *c);
/* 在指定的列表两端之间原子地移动一个元素。 */
void lmoveCommand(client *c);
/* 从首个非空列表头部弹出元素，无可用元素时可阻塞等待。 */
void blpopCommand(client *c);
/* 从首个非空列表尾部弹出元素，无可用元素时可阻塞等待。 */
void brpopCommand(client *c);
/* 将源列表尾部元素移到目标列表头部，源列表为空时可阻塞等待。 */
void brpoplpushCommand(client *c);
/* 在列表指定两端之间移动元素，源列表为空时可阻塞等待。 */
void blmoveCommand(client *c);

/* 集合操作。 */
/* 向集合添加成员，自动忽略重复成员。 */
void saddCommand(client *c);
/* 删除集合中的指定成员。 */
void sremCommand(client *c);
/* 将指定成员从源集合移动到目标集合。 */
void smoveCommand(client *c);
/* 判断指定成员是否存在于集合中。 */
void sismemberCommand(client *c);
/* 批量判断指定成员是否存在于集合中。 */
void smismemberCommand(client *c);
/* 返回集合成员数量。 */
void scardCommand(client *c);
/* 随机移除并返回一个或指定数量的集合成员。 */
void spopCommand(client *c);
/* 随机返回集合成员，不删除成员。 */
void srandmemberCommand(client *c);
/* 返回集合交集，也用于读取单个集合全部成员的 SMEMBERS 命令。 */
void sinterCommand(client *c);
/* 计算集合交集并保存到目标键。 */
void sinterstoreCommand(client *c);
/* 返回多个集合的并集。 */
void sunionCommand(client *c);
/* 计算集合并集并保存到目标键。 */
void sunionstoreCommand(client *c);
/* 返回首个集合相对于后续集合的差集。 */
void sdiffCommand(client *c);
/* 计算集合差集并保存到目标键。 */
void sdiffstoreCommand(client *c);
/* 通过游标分批遍历集合成员。 */
void sscanCommand(client *c);

/* 排序操作。 */
/* 对列表、集合或有序集合排序，支持外部排序键及结果存储。 */
void sortCommand(client *c);

/* 过期时间与访问统计。 */
/* 设置键的有效期，单位为秒。 */
void expireCommand(client *c);
/* 以 Unix 秒时间戳设置键的过期时刻。 */
void expireatCommand(client *c);
/* 设置键的有效期，单位为毫秒。 */
void pexpireCommand(client *c);
/* 以 Unix 毫秒时间戳设置键的过期时刻。 */
void pexpireatCommand(client *c);
/* 返回键的剩余有效期（秒），不存在返回 -2，无过期时间返回 -1。 */
void ttlCommand(client *c);
/* 访问指定键以更新访问统计，返回存在的键数量，不续期。 */
void touchCommand(client *c);
/* 返回键的剩余有效期（毫秒），不存在返回 -2，无过期时间返回 -1。 */
void pttlCommand(client *c);
/* 移除键的过期时间，使其持久保留。 */
void persistCommand(client *c);

/* 有序集合操作。 */
/* 添加有序集合成员或更新分数，支持条件更新和增量选项。 */
void zaddCommand(client *c);
/* 增加有序集合成员的分数，成员不存在时创建。 */
void zincrbyCommand(client *c);
/* 按排名、分数或字典序查询范围，支持逆序和返回分数。 */
void zrangeCommand(client *c);
/* 按分数从低到高返回指定分数范围的成员。 */
void zrangebyscoreCommand(client *c);
/* 按分数从高到低返回指定分数范围的成员。 */
void zrevrangebyscoreCommand(client *c);
/* 返回指定字典序范围的成员，适用于成员分数相同的集合。 */
void zrangebylexCommand(client *c);
/* 逆序返回指定字典序范围的成员，适用于成员分数相同的集合。 */
void zrevrangebylexCommand(client *c);
/* 统计指定分数范围内的成员数量。 */
void zcountCommand(client *c);
/* 统计字典序范围内的成员数量，适用于成员分数相同的集合。 */
void zlexcountCommand(client *c);
/* 按分数逆序返回指定排名范围的成员。 */
void zrevrangeCommand(client *c);
/* 返回有序集合成员数量。 */
void zcardCommand(client *c);
/* 删除有序集合中的指定成员。 */
void zremCommand(client *c);
/* 返回指定成员的分数。 */
void zscoreCommand(client *c);
/* 批量返回指定成员的分数。 */
void zmscoreCommand(client *c);
/* 删除指定分数范围内的成员。 */
void zremrangebyscoreCommand(client *c);
/* 删除字典序范围内的成员，适用于成员分数相同的集合。 */
void zremrangebylexCommand(client *c);
/* 移除并返回分数最低的一个或指定数量的成员及分数。 */
void zpopminCommand(client *c);
/* 移除并返回分数最高的一个或指定数量的成员及分数。 */
void zpopmaxCommand(client *c);
/* 从首个非空有序集合弹出最低分成员，无可用成员时可阻塞等待。 */
void bzpopminCommand(client *c);
/* 从首个非空有序集合弹出最高分成员，无可用成员时可阻塞等待。 */
void bzpopmaxCommand(client *c);
/* 随机返回有序集合成员，可同时返回分数。 */
void zrandmemberCommand(client *c);
/* 返回成员按分数升序排列的排名，从零开始。 */
void zrankCommand(client *c);
/* 返回成员按分数降序排列的排名，从零开始。 */
void zrevrankCommand(client *c);
/* 删除指定排名范围内的成员。 */
void zremrangebyrankCommand(client *c);
/* 计算有序集合并集及聚合分数，保存到目标键。 */
void zunionstoreCommand(client *c);
/* 计算有序集合交集及聚合分数，保存到目标键。 */
void zinterstoreCommand(client *c);
/* 计算有序集合差集并保存到目标键。 */
void zdiffstoreCommand(client *c);
/* 返回有序集合并集，可同时返回聚合分数。 */
void zunionCommand(client *c);
/* 返回有序集合交集，可同时返回聚合分数。 */
void zinterCommand(client *c);
/* 将指定范围的有序集合成员和分数保存到目标键。 */
void zrangestoreCommand(client *c);
/* 返回首个有序集合相对于后续集合的差集。 */
void zdiffCommand(client *c);
/* 通过游标分批遍历有序集合成员及分数。 */
void zscanCommand(client *c);

/* 哈希操作。 */
/* 设置一个或多个哈希字段，也用于 HMSET 命令。 */
void hsetCommand(client *c);
/* 仅在哈希字段不存在时设置值。 */
void hsetnxCommand(client *c);
/* 读取指定哈希字段的值。 */
void hgetCommand(client *c);
/* HMSET 复用 hsetCommand，不单独声明处理函数。 */
// void hmsetCommand(client *c);
/* 批量读取指定哈希字段的值。 */
void hmgetCommand(client *c);
/* 删除指定哈希字段。 */
void hdelCommand(client *c);
/* 返回哈希字段数量。 */
void hlenCommand(client *c);
/* 返回指定哈希字段值的字节长度。 */
void hstrlenCommand(client *c);
/* 返回哈希的全部字段名。 */
void hkeysCommand(client *c);
/* 返回哈希的全部字段值。 */
void hvalsCommand(client *c);
/* 返回哈希的全部字段及对应值。 */
void hgetallCommand(client *c);
/* 判断指定哈希字段是否存在。 */
void hexistsCommand(client *c);
/* 通过游标分批遍历哈希字段和值。 */
void hscanCommand(client *c);
/* 随机返回哈希字段，可同时返回字段值。 */
void hrandfieldCommand(client *c);
/* 将哈希字段的整数值加上指定增量。 */
void hincrbyCommand(client *c);
/* 将哈希字段的数值加上指定浮点增量。 */
void hincrbyfloatCommand(client *c);

/* 服务器配置管理。 */
/* 查询、修改、重写服务器配置或重置统计信息。 */
void configCommand(client *c);

/* 发布与订阅。 */
/* 订阅指定频道，接收后续发布的消息。 */
void subscribeCommand(client *c);
/* 取消指定频道的订阅，不指定频道时取消全部频道订阅。 */
void unsubscribeCommand(client *c);
/* 订阅匹配指定模式的频道。 */
void psubscribeCommand(client *c);
/* 取消指定模式的订阅，不指定模式时取消全部模式订阅。 */
void punsubscribeCommand(client *c);
/* 向指定频道发布消息，返回消息投递的订阅计数。 */
void publishCommand(client *c);
/* 查询活跃频道、频道订阅数量和模式订阅统计。 */
void pubsubCommand(client *c);

/* 事务与乐观锁。 */
/* 开启事务，将后续命令加入事务队列。 */
void multiCommand(client *c);
/* 检查事务状态和监视键后，执行事务队列中的命令。 */
void execCommand(client *c);
/* 丢弃事务队列并退出事务状态。 */
void discardCommand(client *c);
/* 监视指定键，其变化可使后续事务执行失败。 */
void watchCommand(client *c);
/* 取消当前客户端对全部键的监视。 */
void unwatchCommand(client *c);

/* 集群管理与键迁移。 */
/* 查询或管理集群节点、槽位和集群状态。 */
void clusterCommand(client *c);
/* 将序列化数据恢复为键值，支持有效期和覆盖选项。 */
void restoreCommand(client *c);
/* 将键传输到目标实例，支持复制和覆盖选项。 */
void migrateCommand(client *c);
/* 为当前连接的下一条命令设置槽迁移访问标记。 */
void askingCommand(client *c);
/* 允许当前连接在集群从节点读取其主节点负责的键。 */
void readonlyCommand(client *c);
/* 取消当前连接的集群从节点只读访问模式。 */
void readwriteCommand(client *c);
/* 返回键值的序列化数据，不包含过期时间。 */
void dumpCommand(client *c);

/* 对象与内存诊断。 */
/* 查询值的编码、引用计数和访问统计等内部信息。 */
void objectCommand(client *c);
/* 查询内存使用、分配器统计，或执行内存诊断与清理。 */
void memoryCommand(client *c);

/* 客户端连接与协议管理。 */
/* 查询和管理客户端连接及相关连接属性。 */
void clientCommand(client *c);
/* 协商 RESP 协议版本，可同时认证及设置客户端名称。 */
void helloCommand(client *c);
/* 识别 POST、Host: 等跨协议请求，记录告警并关闭连接。 */
void securityWarningCommand(client *c);
/* 重置普通客户端的认证、协议、事务和订阅等连接状态。 */
void resetCommand(client *c);

/* 复制目标设置与 Lua 脚本。 */
/* 设置复制主节点或取消复制，也用于 SLAVEOF 命令。 */
void replicaofCommand(client *c);
/* 执行客户端提供的 Lua 脚本。 */
void evalCommand(client *c);
/* 按 SHA1 摘要执行已缓存的 Lua 脚本。 */
void evalShaCommand(client *c);
/* 管理 Lua 脚本缓存和执行状态。 */
void scriptCommand(client *c);

/* 复制状态与主从同步。 */
/* 返回实例的复制角色及相关状态。 */
void roleCommand(client *c);
/* 处理 SYNC 和 PSYNC 请求，执行全量同步或尝试部分同步。 */
void syncCommand(client *c);
/* 处理复制握手参数、能力协商和复制偏移确认。 */
void replconfCommand(client *c);
/* 等待当前连接此前的写入被指定数量的从节点确认，或超时返回。 */
void waitCommand(client *c);
/* 协调主从切换，或中止正在进行的切换。 */
void failoverCommand(client *c);

/* HyperLogLog 基数估算。 */
/* 执行 HyperLogLog 内部算法自检。 */
void pfselftestCommand(client *c);
/* 将元素加入 HyperLogLog，用于估算去重数量。 */
void pfaddCommand(client *c);
/* 估算一个或多个 HyperLogLog 所表示集合的并集基数。 */
void pfcountCommand(client *c);
/* 合并多个 HyperLogLog 并保存到目标键。 */
void pfmergeCommand(client *c);
/* 检查或转换 HyperLogLog 内部编码，用于调试。 */
void pfdebugCommand(client *c);

/* 地理位置操作。 */
/* 以已有成员为圆心查询半径内成员，支持存储结果。 */
void georadiusbymemberCommand(client *c);
/* 以已有成员为圆心只读查询半径内成员。 */
void georadiusbymemberroCommand(client *c);
/* 以给定经纬度为圆心查询半径内成员，支持存储结果。 */
void georadiusCommand(client *c);
/* 以给定经纬度为圆心只读查询半径内成员。 */
void georadiusroCommand(client *c);
/* 添加或更新成员的经纬度位置。 */
void geoaddCommand(client *c);
/* 返回指定成员的 Geohash 字符串。 */
void geohashCommand(client *c);
/* 返回指定成员的经纬度。 */
void geoposCommand(client *c);
/* 计算两个地理位置成员之间的距离。 */
void geodistCommand(client *c);
/* 按圆形或矩形区域查询地理位置成员。 */
void geosearchCommand(client *c);
/* 按圆形或矩形区域查询成员，并将结果保存到目标键。 */
void geosearchstoreCommand(client *c);

/* 流消息与消费组。 */
/* 向流追加消息，支持指定消息 ID 和裁剪选项。 */
void xaddCommand(client *c);
/* 按消息 ID 升序返回指定范围的流消息。 */
void xrangeCommand(client *c);
/* 按消息 ID 降序返回指定范围的流消息。 */
void xrevrangeCommand(client *c);
/* 返回流中的消息数量。 */
void xlenCommand(client *c);
/* 处理 XREAD 和 XREADGROUP，支持普通读取、消费组读取及阻塞等待。 */
void xreadCommand(client *c);
/* 管理流的消费组和消费者。 */
void xgroupCommand(client *c);
/* 设置流的最后生成 ID，供后续消息 ID 生成使用。 */
void xsetidCommand(client *c);
/* 确认消息已处理，并从消费组待确认列表中移除记录。 */
void xackCommand(client *c);
/* 查询消费组待确认消息的汇总或明细。 */
void xpendingCommand(client *c);
/* 将满足空闲时间等条件的待确认消息转交给指定消费者。 */
void xclaimCommand(client *c);
/* 通过游标扫描并转交满足空闲时间条件的待确认消息。 */
void xautoclaimCommand(client *c);
/* 查询流、消费组和消费者的信息。 */
void xinfoCommand(client *c);
/* 删除指定 ID 的流消息。 */
void xdelCommand(client *c);
/* 按长度或最小消息 ID 裁剪流，支持精确和近似裁剪。 */
void xtrimCommand(client *c);

/* 趣味展示。 */
/* 展示当前版本的趣味图案或版本信息。 */
void lolwutCommand(client *c);

#endif // SERVER_CAMMAND_DEFINE_H
