
//redis源码分析_存储原理与数据模型
/*
    redis源码学习：
        1.面试，对redis源码考察的比较多，也比较细致
        2.解决问题，实现一些命令
        3.redis源码：redis逻辑是单线程，redis很多技术都是围绕单线程来解决问题的，
          所以我们可以学习redis是怎样在单线程环境下做到高性能，思想：分治，内存策略
    
    建议学习方法：
        1.首先定一个小的主题，预期要得到的效果；
        2.准备测试数据以及调试环境；
        3.查看流程，把每一个细支流程拷贝出来，并在旁边写上注释；
        4.得出结论；
    
    字典实现：
        redis DB KV组织是通过字典来实现的；hash结构当节点超过512个或者单个字符串长度
        大于64时，hash结构采用字典实现；

        数据结构：
            typedef struct dictEntry {
                void *key;
                union {
                    void *val;
                    uint64_t u64;
                    int64_t s64;
                    double d;
                } v;
                struct dictEntry *next;
            } dictEntry;

            typedef struct dictht {
                dictEntry **table;
                unsigned lond size;  //数组长度
                unsigned long sizemark;  //size-1
                unsigned long used;  //当前数组当中包含的元素
            } dictht;

            typedef struct dict {
                dictType *type;
                void *privdata;
                dictht ht[2];
                long rehashidx;  //rehashing not in progress if rehashidex == -1
                int16_t pauserehash;  //if > 0 rehashing is paused (< 0 indicates coding error)用于安全遍历
            } dict;
            
            1.字符串经过hash函数运算得到64位整数；
            2.相同字符串多次通过hash函数得到相同的64位整数；
            3.整数对2的n次幂取余可以转化为位运算；
            4.在数组长度为4时，大概率会冲突

            冲突
            负载因子：
                负载因子 = used / size; used是数组存储元素的个数，size是数组的长度；
                负载因子越小，冲突越小；负载因子越大，冲突越大
                redis的负载因子是1

            扩容：
                如果负载因子 > 1，则会发生扩容，扩容的规则是翻倍；
                如果正在fork（在rdb，aof复写以及rdb-aof混用情况下）时，会阻止扩容；但是
                若负载因子 > 5，则马上扩容，这里涉及到写时复制原理，牺牲内存来提高效率；

            写时复制：
                写时复制核心思想：只有在不得不复制数据内容时才去复制数据内容；
                父进程和子进程的虚拟内存同时指向一个真实的内存块，如果此时父进程写入，子进程
                会主动复制一份内存数据出来供子进程持久化；

            缩容：
                如果负载因子 < 0.1，则会发生缩容；缩容的规则是恰好包含used的2^n;
                恰好的理解：假如此时数组存储元素个数为9，恰好包含该元素的就是2^4，也就是16

            渐进式rehash：
                当hashtable中的元素过多的时候，不能一次性rehash到ht[1]；这样会长期占用redis;
                其他命令得不到响应；所以需要使用渐进式rehash;

            rehash步骤：
                将ht[0]中的元素重新经过hash函数生成64位整数，再对ht[1]长度进行取余，从而
                映射到ht[1];
                渐进式规则：
                    1.分治的思想，将rehash分到之后的每步增删改查的操作当中；
                    2.在定时器中，最大执行1毫秒rehash；每次步长100个数组槽位；

            scan：
                间断遍历；
                采用高位进位加法的遍历顺序，rehash后的槽位在遍历顺序上是相邻的；
                scan遍历的目标是什么？不重复不遗漏，scan的遍历可能发生在扩容，缩容，渐进式rehash
                这三种情况；
                程序中的两个重要思维：分治，抽象

            expire机制：
                只支持对最外层key过期
                惰性删除：分布在每一个命令操作时检查key是否过期；若过期删除key，再进行命令操作；
                定时删除：在定时器中检查库中指定个数(25)个key;

            大KEY：
                在redis实例中形成了很大的对象，比如一个很大的hash或很大的zset，这样的对象在扩容
                的时候，会一次性申请更大的一块内存，这会导致卡顿；如果这个大key被删除，内存会一次
                性回收，卡顿现象会再次产生；
                如果观察到redis的内存大起大落，极有可能因为大key导致的；
                查找大key命令（每隔0.1秒，执行100条scan命令）：
                    redis-cli -h 127.0.0.1 --bigkeys -i 0.1
                内存异常：
                    1.key过多了，扩容fork，内存会翻倍 maxmemory = 物理内存的一半
                    2.大key
                redis线程：
                    redis-server：主线程，专门用来处理网络IO + 命令
                    bio_close_file：关闭文件
                    bio_aof_fsync：刷盘操作，持久化的时候定时刷盘
                    bio_lazy_free：专门用来释放大内存的
                    jemalloc_bg_thd：jemalloc后台线程
        
    跳表实现：
        跳表（多层级有序链表）结构用来实现有序集合；鉴于redis需要实现zrange以及zrevrange
        功能；需要节点间最好能直接相连并且增删改操作后结构依然有序；B+树时间复杂度为h*O(logn);
        鉴于B+树复杂的节点分裂操作；考虑其他数据结构；O(1) O(logn)

        有序数组通过二分查找能获得O(logn)时间复杂度；平衡二叉树也能获得O(logn)时间复杂度；

        理想跳表：
            每隔一个节点增加一个层级节点；模拟二叉树结构，以此达到搜索时间复杂度为O(logn);
            
            但是如果对理想跳表结构进行删除增加操作，很有可能改变跳表结构；如果重构理想结构，将是巨大
            的运算；考虑用概率的方法来进行优化；从每一个节点出发，每增加一个节点都有1/2的概率增加一个
            层级，1/4的概率增加两个层级，1/8的概率增加3个层级，以此类推；经过证明，当数据量足够大(256)
            时，通过概率构造的跳表趋向于理想跳表，并且此时如果删除节点，无需重构跳表结构，此时依然趋向于
            理想跳表；此时时间复杂度为(1 - 1/n^c) * O(logn);

        redis跳表：
            从节约内存触发，redis考虑牺牲一点时间复杂度让跳表结构更加扁平，就像二叉堆改成四叉堆结构；
            并且redis还限制了跳表的最高层级为32；
            节点数量大于128或者有一个字符串长度大于64，则使用跳表（skiplist）；

            数据结构：

            #define ZSKIPLIST_MAXLEVEL 32 //should be enough for 2^64 elements
            #define ZSKIPLIST_P 0.25  //skiplist p = 1/4
            //ZSETs use a speciallized version of skiplists
            typedef struct zskiplistNode {
                sds ele;
                double score;  //WRN：score只能是浮点数
                struct zskiplisNode *backward;
                struct zskiplistLevel {
                    struct zskiplistNode *forward;
                    unsigned long span;  //用于zrank
                } level[];
            } zskiplistNode;

            typedef struct zskiplist {
                struct zskiplistNode *header, *tail;
                unsigned long length;  //zcard
                int level;  //最高层
            } zskiplist;

            typedef struct zset {
                dict *dict;  //帮助快速索引到节点
                zskiplist *zsl;
            } zset;
            
        延时队列优化：
            前面实现的延时队列有很大的局限性：
                1.时间基准问题：两个应用程序如何保证时间一致性；
                2.异步轮询问题：耗性能，大量无意义的数据请求；
                3.原子性的问题：需要引入lua脚本来执行；

            解决：
                1.以redis的时间为基准，redis为数据中心且为单点，时间准确性得到大幅提升；
                2.通过阻塞接口来实现，避免轮询；
                3.修改源码，直接实现原子接口；

            思路：
                在redis当中zset结构 score = time + 5
                dq_add "delay_queue" 5 msg

                如果没有消息pop，则将此连接加入阻塞队列，然后在redis的定时器中，比较zset
                最小值与当前时间的大小，如果满足条件，通知阻塞队列中的连接；
                dq_bpop "delay_queue" timeout 60
*/