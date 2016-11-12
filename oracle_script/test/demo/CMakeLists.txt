#project name :oracle_executer_demo
PROJECT(oracle_executer_demo)

#包含prep.cmake(定义编译时的选项)
INCLUDE($ENV{ABCMAKE}/common/prep.cmake)

#设置版本号，和向下兼容性
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
CMAKE_POLICY(VERSION 2.8)

SET(NEED_XPCOM TRUE)
SET(NEED_CFL TRUE)
SET(NEED_SYSWRAP TRUE)
SET(NEED_NCUTIL TRUE)
SET(NEED_BACKUPENGINE TRUE)
SET(NEED_FASTCLIENT TRUE)

INCLUDE_DIRECTORIES($ENV{ORACLEBACKUP}/private;
					$ENV{ORACLEBACKUP}/oracle/oraclebackupengine/src;
					$ENV{ORACLEBACKUP}/oracle/private;
					$ENV{ORACLEBACKUP}/oracle/oraclescriptexecuter/src;
					)
					
#target name:oracle_executer_demo
SET(TARGET_NAME oracle_executer_demo)

#TARGET_TYPE 
#LIBRARY | ARCHIVE | RUNTIME | COMPONENT
#动态库  | 静态库  | 可执行	 |  组件

SET(TARGET_TYPE RUNTIME)

INCLUDE($ENV{ABCMAKE}/common/target.cmake)