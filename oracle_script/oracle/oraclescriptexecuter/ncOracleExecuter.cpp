/******************************************************************************
ncOracleExecuter.cpp:
	Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#include <abprec.h>
#include <ncutil.h>
#include "ncOracleExecuter.h"
#include "ncOracleExecuterCommon.h"
#include "ncOracleBackupException.h"


NS_IMPL_ISUPPORTS4(ncOracleExecuter, ncIBackupScheduler, ncISchedulerConfigurator, ncIPushStreamEvent, ncICoreEventBase)
ncOracleExecuter::ncOracleExecuter ()
					: _listener (0)
					, _hasFinish (false)
					, _hasStop (false)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::ncOracleExecuter ()"));
}

ncOracleExecuter::~ncOracleExecuter ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::~ncOracleExecuter ()"));
}

NS_IMETHODIMP_(void)
ncOracleExecuter::Start ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::Start ()"));

	try {
		exec ();
		end ();
	}
	catch () {

	}

	_hasFinish = true;
}

NS_IMETHODIMP_(void)
ncOracleExecuter::Stop ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::ncOracleExecuter ()"));
	_hasStop = true;
}

NS_IMETHODIMP_(void)
ncOracleExecuter::Pause ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::ncOracleExecuter ()"));
}

NS_IMETHODIMP_(void)
ncOracleExecuter::Resume ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::ncOracleExecuter ()"));
}

NS_IMETHODIMP_(ncISchedulerConfigurator*)
ncOracleExecuter::GetConfigurator()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::ncOracleExecuter ()"));
}

NS_IMETHODIMP_(void)
ncOracleExecuter::Resume ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::ncOracleExecuter ()"));
}

NS_IMETHODIMP_(void)
ncOracleExecuter::SetListener (ncIEEFListener *listener)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::ncOracleExecuter ()"));
	_listener = listener;
}

NS_IMETHODIMP_(void)
ncOracleExecuter::AddParam (const String& key, const String& value)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleBackupSchedule::AddParam () key:%s, value:%s"), key.getCStr(), value.getCStr());
	_backupParam.addParam(key, value);

	if (key == EEE_CUSTOM_SCRIPT)
		_script = value;
}

NS_IMETHODIMP_(void)
ncOracleExecuter::OnStop ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::OnStop ()"));
	_hasStop = true;
}

NS_IMETHODIMP_(void)
ncOracleExecuter::OnNoTimePoint ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::OnNoTimePoint ()"));
}

NS_IMETHODIMP_(int)
ncOracleExecuter::OnTimepointObjectFilter (vector<nsCOMPtr<ncITimepointObject> >& tpvec)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::OnTimepointObjectFilter ()"));
	return 0;
}

NS_IMETHODIMP_(void)
ncOracleExecuter::OnMessage (const String &msg)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::OnMessage ()"));
	_listener->OnMessage (msg);
}

NS_IMETHODIMP_(void)
ncOracleExecuter::OnError (const Exception &e)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::OnError ()"));
	_listener->OnError(e);
}

NS_IMETHODIMP_(void)
ncOracleExecuter::OnCoreExceptionError (const ncCoreAbortException &e)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::OnCoreExceptionError ()"));
	_listener->OnMessage(e);
}

NS_IMETHODIMP_(void)
ncOracleExecuter::OnCoreExceptionWarning (const ncCoreWarnException &e)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::OnCoreExceptionWarning ()"));
	_listener->OnMessage(e);
}

NS_IMETHODIMP_(void)
ncOracleExecuter::OnCoreExceptionInfo (const ncCoreInfoException &e)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::OnCoreExceptionInfo ()"));
	_listener->OnWarning (e, false);
}

NS_IMETHODIMP_(void)
ncOracleExecuter::OnCoreIgnoreException (const ncCoreIgnoreException &e)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::OnCoreIgnoreException ()"));
	_listener->OnWarning (e, false);
}


//
// TODO: parse ORACLE_SID for init
//

void
ncOracleExecuter::exec ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecuter::exec () - begin"));
	ncOracleExecThread* _execThread = new ncOracleExecThread (_listener, _script, _hasFinish);
	_execThread->Start ();

	_timePoint = Date::getCurrentTime ();

	_dataStream = new ncDataStreamClient ();
	_dataStream->setOraSid (oraSid);

	String initID (String::EMPTY);
	while (_hasFinish == false && _hasStop == false && (initID = _dataStream->getInitID () != String::EMPTY)) {
		if (_hasFinish || _hasStop)
			 break;
		_dataThreadQ.push (new ncDataProcessorThread (_backupParam, this, _timePoint, oraSid, initID, _hasStop));
		_dataThreadQ.back ()->Start ();
		initID = String::EMPTY;
	}

	if (_hasStop)
		return;

	if (_execThread->isAlive ())
		_execThread->join ();
	delete _execThread;
	_execThread = 0;

	size_t counts = _dataThreadQ.size();
	for (size_t i = 0; i < counts; ++i) {
		if (_dataThreadQ.front()->isAlive ())
			_dataThreadQ.front()->join ();

		delete _dataThreadQ.front ();
		_dataThreadQ.front () = 0;
		_dataThreadQ.pop ();
	}

	NC_ORACLE_EXECUTER_TRACE(_T("ncOracleExecuter::exec () - end"));
}

