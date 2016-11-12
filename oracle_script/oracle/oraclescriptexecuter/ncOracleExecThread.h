/******************************************************************************
ncOracleExecThread.h:
	Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#ifndef __NC_ORACLE_EXEC_THREAD_H__
#define __NC_ORACLE_EXEC_THREAD_H__

#include <public/ncIEEFListener.h>

class ncOracleExecThread : Thread
{
public:
	ncOracleExecThread ();
	virtual ~ncOracleExecThread ();

private:
	void run (void);
	void executeScript (String& command);

private:
	nsCOMPtr<ncIEEFListener>				_listener;
	const String&							_script;
	bool&									_hasFinish;
};

class ncReadOutputThread : public Thread
{
public:
	ncReadOutputThread ();
	virtual ~ncReadOutputThread ();

private:
	void run(void);

private:
	nsCOMPtr<ncIEEFListener>				_listener;
};

#endif // __NC_ORACLE_EXEC_THREAD_H__
