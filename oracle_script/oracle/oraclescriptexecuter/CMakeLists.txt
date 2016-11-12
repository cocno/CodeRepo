#project name : oracleexecuter
PROJECT(oracleexecuter)

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

SET(CUSTOM_IDL_PATH $ENV{ORACLEBACKUP}/ocisession/public)

#编译目录
IF(WIN32)
	INCLUDE_DIRECTORIES($ENV{ORACLEBACKUP}/private;
						$ENV{ORACLEBACKUP}/oracle/private;
						$ENV{ORACLEBACKUP}/oracle/oraclebackupengine/src;
						$ENV{ABDEPS}/orasvr/include;
						)

ELSE()
	INCLUDE_DIRECTORIES($ENV{ORACLEBACKUP}/private;
						$ENV{ORACLEBACKUP}/oracle/private;
						$ENV{ORACLEBACKUP}/oracle/oraclebackupengine/src;
						$ENV{ABDEPS}/orasvr/include;
						)

	SET(LINK_CUSTOM_LIBS -lrt)

ENDIF()

#target name:oracleexecuter
SET(TARGET_NAME oracleexecuter)

#TARGET_TYPE
#LIBRARY | ARCHIVE | RUNTIME | COMPONENT
#动态库  | 静态库  | 可执行	 |  组件

SET(TARGET_TYPE COMPONENT)

INCLUDE($ENV{ABCMAKE}/common/target.cmake)
