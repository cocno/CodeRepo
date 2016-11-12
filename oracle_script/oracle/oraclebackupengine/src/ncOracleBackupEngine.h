/***************************************************************************************************
ncOracleBackupEngine.h:
	Copyright (c) Eisoo Software, Inc.(2004 - 2013), All rights reserved.

Purpose:
	header file for Oracle ncOracleBackupEngine

Author:
	luo.qiang@eisoo.com

Creating Time:
	2013-7-18
***************************************************************************************************/
#ifndef NC_ORACLE_BACKUPENGINE_H_
#define NC_ORACLE_BACKUPENGINE_H_

#include <abprec.h>
#include <public/ncIBackupEngine.h>
#include <ncOracleObject.h>
#include "ncOracleBackupEngineCommon.h"

#if AB_PRAGMA_ONCE
#pragma once
#endif

class ncOracleBackupEngine : public ncIBackupEngine
{
public:
	ncOracleBackupEngine ();
	virtual ~ncOracleBackupEngine ();

	NS_DECL_ISUPPORTS
	NS_DECL_NCIBACKUPENGINE

private:
	int   _engineType;
	int   _clientType;
};

#endif /* NC_ORACLE_BACKUPENGINE_H_ */
