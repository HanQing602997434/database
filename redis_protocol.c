
//reids协议及异步方式
/*
    redis网络层
    单reactor：一个线程同时处理 网络IO + 命令
    mysql：连接处理是并发，受限于核心数 8
           命令处理是并发
    redis：连接处理是并发
           命令处理是串行

    redis pipeline：
        客户端和redis通信方式，redis pipeline是一个客户端提供的，而不是服务端提供的。
        对于一个连接的请求，一个request收到一个response才开启下一个request，使用的阻塞IO。
        对于多个redis请求，采用队列的形式对回包进行处理，先请求的先回，后请求的后回就是pipeline。
        对于request操作，只是将数据写到fd对应的写缓冲区，时间非常快，真正耗时的操作在读取response

    redis事务：
    将命令组合到一起形成的一个原子操作称之为事务，MULTI开启事务，事务执行过程中，单个命令是入队操作，
    直到调用EXEC才会一起执行；

    MULTI：开启事务
        mysql开启事务：begin，start，transaction
    EXEC：提交事务
        mysql提交事务：commit
    DISCARD：取消事务，将QUEUED队列中的命令全部取消掉
        mysql回滚事务：rollback，通过undolog进行命令的逆运算回滚
    WATCH：检测key的变动，若在事务执行中，key变动则取消事务；在事务开启前调用，乐观锁实现（cas）;
           若被取消则事务返回nil;
           WATCH不能再MULTI和EXEC之间使用，若key变动，事务执行失败，相当于DISCARD
    应用
        事务实现 zpop
        事务实现 加倍操作

    lua脚本：
        lua脚本实现原子性；
        
        redis中加载一个lua虚拟机；用来执行redis lua脚本；redis lua脚本的执行是原子性的；当某个
        脚本正在执行的时候，不会有其他命令或者脚本被执行；

        lua脚本当中的命令会直接修改数据状态

        注意：如果项目中使用lua脚本，不需要使用上面的事务命令；

        
*/