/****************************************************************************************
* ncOracleConfig.h
*		Copyright (c)  Eisoo Software Inc. (2004-2013), All right reserved
*
*  PursePose:
*			定义Oracle的通信消息和管道
*
*  Author: 
*		Huang kaide (huang.kaide@eisoo.com)
*  Created Time: 
*		2013-7-25
*     
***************************************************************************************/
#ifndef __NC_ORA_CONFIG_H__
#define __NC_ORA_CONFIG_H__

#ifdef __WINDOWS__
#include <tchar.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Windows.h>
#else
#include <unistd.h>
#endif

#if AB_PRAGMA_ONCE
#pragma once
#endif

#define MAX_MESSAGE_SIZE	1024 * 4

#define MAX_NAME_SIZE 125

#define MAXMSGNUM				1
#define MAXMSGSIZE				512
#define DATA_STREAM_COUNTS		8
#define INIT_BLOCK_SIZE			4 * 1024 * 1024

enum {
	MSG_PIECE_NAME		= 1;	// piece name
	MSG_BLOCK_SIZE		= 2;	// block size
	MSG_BUFFER_SIZE		= 3;	// actual buffer size
	MSG_DATA_END		= 4;	// end up
};

// a new backup set
#define ORA_NEWSET				1

// a size of the readed data every time
#define	ORA_DATASIZE			2

// backup a backuppiece complete
#define ORA_ENDSET				3

// error message 
#define ORA_ERROR				4

// backup end
#define ORA_BAKEND				5

// restore end
#define ORA_RESEND				6

// delete backup set
#define ORA_DELETE_BACKUPSET	7

#define ORA_ENDERR				8

// close pipe
#define ORA_CLOSE_PIPE			9

// sbtinit2 success
#define ORA_SBTINIT				10

// data block size
#define ORA_BLOCK_SIZE			11

// 仍然还有数据的时候
#define ORA_STILL_DATA          12

// 用于完成sbtrestore的消息 
#define ORA_SBT_RESTORE_OVER    13

// 发送Oracle备份恢复pid的消息
#define ORA_PID_MSG             14

// 发送完了的Oracle备份恢复pid消息
#define ORA_PID_MSG_OVER        15

// 备份片 crosscheck 操作
#define ORA_CROSSCHECK_BEGIN		16

// 备份片在介质中存在
#define ORA_CROSSCHECK_AVAILABLE	17

// 备份片在介质中不存在
#define ORA_CROSSCHECK_EXPIRED		18

// 备份片 crosscheck 结束
#define ORA_CROSSCHECK_END			19


#define ORA_DATA_MSG_OVER       20

// pipe 
#define ORA_PIPE_NAME_FIRST				    "EISOO_ORACLE_SERVER_"
#define ORA_PIPE_NAME_SECOND				"EISOO_ORACLE_CLIENT_"

#define AUXILIARY_DB_PWD                    _T("eisoo.com")


inline void wchar_tToChar(char* pBytes, const wchar_t* pWide, int len) 
{ 
#ifdef __WINDOWS__
	// get need convert length
	int nlen = WideCharToMultiByte(CP_ACP, 0, pWide, -1, NULL, 0, NULL, FALSE);
	if (nlen > len )
		nlen = len;
	WideCharToMultiByte(CP_ACP, 0, pWide, -1, pBytes, nlen, NULL, FALSE); 
	//pBytes[len] = '\0';
#endif
}

inline void charToWchar_t(const char* pBytes, wchar_t* pWide, int len) 
{ 
#ifdef __WINDOWS__
	// get need convert length
	int nlen = MultiByteToWideChar(CP_ACP, 0, pBytes, strlen(pBytes), NULL, 0);
	if (nlen >= len )
		nlen = len-1;
	MultiByteToWideChar(CP_ACP, 0, pBytes,strlen(pBytes), pWide, nlen);
	pWide[nlen] = '\0';
#endif
}

#endif//__NC_ORA_CONFIG_H__
