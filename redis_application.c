
//redis应用
/*
    关于redis计数：
        zset：
            命令：zincrby/zcard/zscore
            案例：某天某网页点击的人数并按点击数排序，能获取具体某人点击次数
            解决：
                key：zset_url_click_20210608
                add：zincrby key userid 1
                count：zcard key
                user click count：zscore key userid
                max click：zrange key -1 -1

        hash：
            命令：hincrby/hlen
            案例：某天某网页点击的人数，能获取具体某人点击次数
            解决：
                key：hash_url_click_20210608
                add：hincrby key userid 1
                count：hlen key
                user click count：hget key userid

        set：
            命令：sadd/scard
            案例：某天某网页点击的人数
            解决：
                key：set_url_click_20210608
                add：sadd key userid
                count：scard key

        string：
            命令：incr/incrby
            案例：某天某网页点击次数
            解决：
                key：str_url_click_20210608
                add：incr key
                count：get key
            改进：计算某天某网页点击次数
                add：
                    bf.exists bfkey userid 不存在
                    incr key
                    bf.add bfkey userid
                count：get key

        hyberloglog：
            命令：pfadd/pfcount
            案例：某天某网页的点击人数
            解决：
                key：hll_url_click_20210608
                add：pfadd key userid
                count：pfcount key

    redis扩展：
        redis通过对外提供一套API和一些数据类型，可以供开发者开发自己的模块并且加载到redis中。
        通过API可以直接操作redis中的数据，也可以通过调用redis命令来操作数据（类似lua script）。
        通过编写模块可以注册自己的命令到redis中。
        
        本质：
            不侵入redis源码基础上，提供了一种高效的扩展数据结构的方式，也可以用来实现原子操作。

        入口函数：
            模块加载
            int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
            模块初始化（第一个被调用的函数，定义模块唯一的函数名name）：
            static int RedisModule_Init(RedisModuleCtx *ctx, const char *name, int ver, int
            apiver);
            创建命令（第二个被调用的函数）
            REDISMODULE_API int (*RedisModule_CreateCommand)(RedisModuleCtx *ctx, const char
            *name, RedisModuleCmdFunc cmdfunc, const char *strflags, int firstkey, int lastkey,
            int keystep);
            回调函数
            typedef int (*RedisModuleCmdFunc)(RedisModuleCtx *ctx, RedisModuleString **argv,
            int argc);

        API以及数据结构
            redismodule.h
            提供了丰富API加载模块，并且提供了API可以操作redis内部提供的数据结构；
            
            需要掌握的接口：
                1.如何解析命令参数以及如何恢复内容给客户端；
                2.使用定时器；
                3.使用数据结构；尤其是zset, hash, set, list;
                4.使用阻塞连接；
                5.事件回调处理；
                6.模块传递配置参数；

        RedisBloom
            git clone https://github.com/RedisBloom/RedisBloom.git
            cd RedisBloom
            make
            cp redisbloom.so /path/to
            vi redis.conf
            # loadmodules /path/to/redisbloom.so

            接口：
                # 为key所对应的布隆过滤器分配内存，参数是误差率以及预期元素，根据运算得出需要多少hash函数
                以及所需的位图大小
                bf.reserve hotkey 0.0000001 10000
                # 检测key所对应的布隆过滤器是否包含field字段
                bf.exists key field
                # 往key所对应的布隆过滤器中添加field字段
                bf.add key field

            测试：
                bf.reserve hotkey 0.00000001 10000
                bf.add hotkey mark
                bf.add hotkey king
                bf.add hotkey darren
                bf.exists hotkey mark

            部署：
                1.部署在server，如果只有一个server，部署在server，可以减少网络交互；
                2.部署在redis中，如果有多个server，避免维护多个布隆过滤器，可直接部署
                到redis当中。

    hyberloglog
        在Redis实现中，每个键只使用12kb进行计数，使用16384（2^14）个寄存器，标准误差为0.81%，
        并且对可以计数的项目没有限制，除非接近2^64个项目数（这似乎不太可能）。

        空间复杂度：O(M*log2(log2N(max)))

        hyberloglog使用随机化（hash函数实现）来提供一个集合中唯一元素数量的近似值，仅使用少量
        的内存（只记录对数值）。

        HLL的误差率为：1.04/squrt(m)；m是寄存器的个数；

        算法：
            当一个元素到来时，它会散列到一个桶中，以一定的概率影响这个桶的计数值，因为是概率算法，
            单个桶的计数值并不准确，但是将所有的桶计数值进行调和均值累加起来，结果就会非常接近真实
            的计数值；

            在Redis的HLL中共有2^14个桶，而每个桶是一个6bit的数组；

            hash生成64位整数，其中后14位用来索引桶子；后面50位共有2^50；保存对数的话最大值为49；
            6位对应的是2^6对应整数值为64可以容纳存储49；

    漏斗限流
        前面介绍了时间窗口限流，方法简单，但是有很多缺陷；保存了所有元素，当量大的时候，不值得
        推荐；漏斗限流可以很好的限制元素的容量以及速率；

        漏斗的结构特点：容量是有限的，当漏斗水装满，水将装不进去；水从漏嘴按一定速率流出，当流
        水速度大于灌水速度，那么漏斗永远无法装满；当流水速度小于灌水速度，一旦漏斗装满，需要阻塞
        等待漏斗腾出空间后再灌水；

        所以，漏斗的剩余空间就代表着当前行为可以持续进行的流量，流水速度代表着系统允许该行为的
        最大频率；

        Redis-Cell
            Redis-Cell是一个扩展模块，它采用rust编写；它采用了漏斗算法，并提供了原子的限流
            指令；
            该模块只提供了一个指令cl.throttle；该指令描述了允许某用户某行为的频率为每x秒n次
            行为（六流水速度）；

            # key 为某漏斗容器
            # capacity 为某漏斗容器容量
            # operations 为单位时间内某行为的操作次数
            # seconds 为单位时间 operations / seconds = 流水速度
            # quota 单次行为操作次数 默认值为1
            cl.throttle key capacity operations seconds quota

            安装：
            # 源码安装 然后按照readme.md编译安装
            git clone https://gitee.com/yaowenqiang/redis-cell.git
            # bin安装 如下网址下载相应平台的.so动态库文件
            https://github.com/brandur/redis-cell/releases
            cp libredis_cell.so /path/to
            vi redis.conf
            # loadmodules /path/to/libredis_cell.so

            测试：
            cl.throttle cell 1 1 10 1
            1) (integer) 0  # 0 标识允许 1 标识拒绝
            2) (integer) 2  # 漏斗容量
            3) (integer) 1  # 漏斗剩余空间
            4) (integer) -1 # 如果被拒绝了，需要多长时间后再试（单位秒）
            5) (integer) 10 # 多长时间后，漏斗完全空出来
*/