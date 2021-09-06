
// mysql8.0安装以及开启远程连接
/*
    版本：8.0.26-0ubuntu0.20.04.2 (Ubuntu)

    安装命令：sudo apt-get install mysql-server

    修改密码：alter user 'root'@'localhost' identified by '123456'；

    以权限用户root登录：mysql -u root -p

    选择mysql库：use mysql;

    查看mysql库中的user表：select host, user, authentication_string, plugin from user;

    修改host值：update user set host = '%' where user ='root';

    刷新MySQL的系统权限相关表：flush privileges;

    重起mysql服务：service mysql restart

    备注：
        在所有的设置都完毕之后仍然不能远程访问mysql，找了很多博客之后终于看到需要修改mysql的配置表
        在/etc/mysql目录下的my.cnf中找找真正的mysql配置文件mysql.conf.d/mysqld.cnf
        将bind-address           = 127.0.0.1这一句注释掉
        重启mysql服务，终于可以远程访问mysql了！

        记于2021.9.31 15:16！
*/