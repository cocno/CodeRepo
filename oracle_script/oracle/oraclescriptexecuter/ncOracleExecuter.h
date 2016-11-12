/******************************************************************************
ncOracleExecuter.h:
	Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#ifndef __NC_ORACLE_EXECUTER_H__
#define __NC_ORACLE_EXECUTER_H__

#include <public/ncIBackupScheduler.h>
#include <public/ncISchedulerConfigurator.h>
#include <public/ncIPushStreamEvent.h>
#include <public/ncIPushStreamReactor.h>
#include <ncBackupParam.h>
#include "ncDataProcessorThread.h"
#include "ncDataStreamClient.h"

class ncOracleExecuter : public ncIBackupScheduler
						, public ncISchedulerConfigurator
						, public ncIPushStreamEvent
{
public:
	ncOracleExecuter ();
	virtual ~ncOracleExecuter ();

	NS_DECL_ISUPPORTS
	NS_DECL_NCIBACKUPSCHEDULER
	NS_DECL_NCISCHEDULERCONFIGURATOR
	NS_DECL_NCIPUSHSTREAMEVENT
	NS_DECL_NCICOREEVENTBASE

private:
	void exec ();
	void end ();

private:
	nsCOMPtr<ncIEEFListener>				_listener;
	ncBackupParam							_backupParam;
	std::queue<ncDataProcessorThread*>		_dataThreadQ;
	ncDataStreamClient						_dataStream;
	String									_script;
	bool									_hasFinish;
	bool									_hasStop;
};

#endif // __NC_ORACLE_EXECUTER_H__
