 #project name :sbt
PROJECT(oracle_sbt)
#设置版本号，和向下兼容性
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
CMAKE_POLICY(VERSION 2.8)

#包含prep.cmake(定义编译时的选项)
INCLUDE($ENV{ABCMAKE}/common/prep.cmake)

INCLUDE_DIRECTORIES($ENV{DBPIPE};
		    $ENV{ORACLEBACKUP}/ocisession/src;
		    $ENV{ORACLEBACKUP}/private;)
				 
#target name:sbt
IF(WIN32)
	SET(TARGET_NAME orasbt)
ELSE()
	SET(TARGET_NAME obk)
ENDIF()

IF(REDHAT5)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
ENDIF()

IF(SOLARIS)
SET(CMAKE_C_FLAGS "-m64 ${CMAKE_C_FLAGS}")
SET(CMAKE_CXX_FLAGS "-m64 ${CMAKE_CXX_FLAGS} ")
SET(LINK_CUSTOM_LIBS -lCstd -lCrun)
ENDIF()

IF(AIX)
SET(CMAKE_C_FLAGS "-q64 -bhalt:5 -qlanglvl=extc89 -qcpluscmt -qthreaded -qrtti -qkeyword=inline -qarch=com -qstaticinline ")
SET(CMAKE_CXX_FLAGS "-q64 -bhalt:5 -qlanglvl=extc89 -qcpluscmt -qthreaded -qrtti -qkeyword=inline -qarch=com -qstaticinline -qmkshrobj ")
set(CMAKE_SHARED_LIBRARY_RUNTIME_CXX_FLAG "-Wl,-blibpath:/usr/lib/threads:")
SET(LINK_CUSTOM_LIBS -ldl -lC_r -lC -lc_r -lm)
LINK_DIRECTORIES(/usr/lib/threads)
ENDIF()

#
# mkdir build
# cd build
# cmake -DCMAKE_CXX_COMPILER="/opt/aCC/bin/aCC" ..
# make
#
IF(HPPA)
SET(CMAKE_CXX_FLAGS "-AA -mt +DA2.0W +DS2.0 ${CMAKE_CXX_FLAGS}")
SET(LINK_CUSTOM_LIBS -lpthread -lm -lCsup_v2 -lstd_v2)
ENDIF()

IF(HPIA)
SET(CMAKE_CXX_FLAGS "-ext +u1 -AA -mt +DD64 +DSblended ${CMAKE_CXX_FLAGS}")
SET(LINK_CUSTOM_LIBS -lpthread -lm -lunwind -lCsup -lstd_v2 -lc)
ENDIF()

#
#依赖库
#
#LINK_LIBRARIES(dbpipe)
SET(INCLUDE_SOURCE_FILES $ENV{DBPIPE}/rwpipe.cpp;
			 $ENV{DBPIPE}/clipipe.cpp)
#
# 编译 参数
#
ADD_DEFINITIONS("-DORA_BSP")

IF (WIN32)
	#SET(CUSTOM_LDFLAGS "/DEF:orasbt.def")
	SET(CMAKE_SHARED_LINKER_FLAGS "/DEF:$ENV{ORACLEBACKUP}/mml/orasbt.def")
ENDIF()

#
# 排除的源文件
#
IF (WIN32)
	SET(EXCLUDE_SOURCE_FILES skgfqsbt.cpp)
else ()
	SET(EXCLUDE_SOURCE_FILES skgfqsbt_windows.cpp)
endif()

#TARGET_TYPE 
#LIBRARY | ARCHIVE | RUNTIME | COMPONENT
#动态库  | 静态库  | 可执行	 |  组件

SET(TARGET_TYPE LIBRARY)
INCLUDE($ENV{ABCMAKE}/common/target.cmake)
