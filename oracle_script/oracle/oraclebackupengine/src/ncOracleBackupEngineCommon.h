/***************************************************************************************************
ncOracleBackupEngineCommon.h:
	Copyright (c) Eisoo Software, Inc.(2004 - 2013), All rights reserved.

Purpose:
	header file for Oracle BackupEngineCommon

Author:
	luo.qiang@eisoo.com

Creating Time:
	2013-7-18
***************************************************************************************************/
#ifndef NC_ORACLE_BACKUPENGINECOMMON_H_
#define NC_ORACLE_BACKUPENGINECOMMON_H_

#if AB_PRAGMA_ONCE
#pragma once
#endif

#include <ncOracleBackupException.h>

//资源对象
extern IResourceLoader *oracleBackupEngineLoader;

#ifndef _LOAD_STRING_
#define _LOAD_STRING_

#define LOAD_STRING(ids)		\
	oracleBackupEngineLoader->loadString (ids)

#endif//_LOAD_STRING_

//trace
NC_DECLARE_TRACE_MODULE (ncOracleBackupEngineTrace);

#ifdef __LINUX__
#define NC_ORACLE_BACKUP_ENGINE_TRACE(args...)		NC_TRACE (ncOracleBackupEngineTrace, args)
#else
#define NC_ORACLE_BACKUP_ENGINE_TRACE(...)			NC_TRACE (ncOracleBackupEngineTrace, __VA_ARGS__)
#endif


#endif /* NC_ORACLE_BACKUPENGINECOMMON_H_ */
