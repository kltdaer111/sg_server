#!/bin/bash

. ~/sg_server/sh/config.sh

#####################################################################################################
#卸载编译环境
sh env_uninstall.sh

#####################################################################################################
#安装编译环境
echo -e "==========================================================="
echo -e "\033[1;36m开始安装编译环境......\033[0m"
aclocal 1>aclocal.txt
libtoolize 1>libtoolize.txt
autoconf 1>autoconf.txt
autoheader 1>autoheader.txt
automake --add-missing 1>/dev/null
if [ "$1" = "release" ];then
    ./configure CXX=gcc LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu LD=/usr/lib/x86_64-linux-gnu --enable-release 1>/dev/null
elif [ "$1" = "valgrind" ];then
    ./configure CXX=gcc LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu LD=/usr/lib/x86_64-linux-gnu --enable-valgrind 1>/dev/null
elif [ "$1" = "online" ];then
    ./configure CXX=gcc LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu LD=/usr/lib/x86_64-linux-gnu --enable-online 1>/dev/null
elif [ "$1" = "gprof" ];then
    ./configure CXX=gcc LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu LD=/usr/lib/x86_64-linux-gnu --enable-gprof 1>/dev/null
else
    ./configure CXX=gcc LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu LD=/usr/lib/x86_64-linux-gnu --enable-debug 1>/dev/null
fi

#####################################################################################################
#创建服务器的logs文件夹,多开服务器的logs会run时创建
for s in $run_group_server_list
do
    if [ ! -d ./$s"_server/logs" ];then
        mkdir ./$s"_server/logs"
    fi
done
for s in $all_global_server_list
do
    if [ ! -d ./$s"_server/logs" ];then
        mkdir ./$s"_server/logs"
    fi
done
if [ ! -d ./robot_server/logs ];then
    mkdir ./robot_server/logs
fi

#####################################################################################################
#生成协议
bash gen_proto.sh

#####################################################################################################
echo -e "\033[1;36m编译环境安装成功!\033[0m"
