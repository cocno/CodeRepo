/******************************************************************************
ncOracleExecThread.cpp:
Copyright (c) Eisoo Software, Inc.(2015), All rights reserved.

Purpose:


Author:
	peng.ting (peng.ting@eisoo.com)

Creating Time:
	2016-11-2
******************************************************************************/

#include <abprec.h>
#include <ncutil.h>
#include "ncOracleExecThread.h"
#include "ncOracleConfig.h"
#include "ncOracleBackupException.h"


#define STOP_FLAG    ">><<It's time to finish now!>><<\r"

//--------------------------------------------------------------------------------
// class ncOracleExecThread
//--------------------------------------------------------------------------------
ncOracleExecThread::ncOracleExecThread (ncIEEFListener* listener, String& scriptName, bool& hasFinish)
						: _listener (listener)
						, _script (scriptName)
						, _hasFinish (hasFinish)
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecThread::ncOracleExecThread ()"));
}

ncOracleExecThread::~ncOracleExecThread ()
{
	NC_ORACLE_EXECUTER_TRACE(_T("ncOracleExecThread::~ncOracleExecThread ()"));
}

ncOracleExecThread::run ()
{
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecThread::run () - begin"));
	String msg;
	msg.format(LOAD_STRING(_T("IDS_START_EXEC_CUSTOM_SCRIPT")).getCStr(), _script.getCStr());
	_listener->OnMessage(msg);

	if (File (_script).exists() == false) {
		NC_ORACLE_EXECUTER_TRACE(_T("ncOracleExecThread::run () !!!"));
	}

	String command (String::EMPTY);
#ifdef __WINDOWS__
	// 以防路径中含有空格
	command.format(_T("\"%s\""), _script.getCStr());
#else
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecThread::run () SHELL: %s"), getenv("SHELL"));
	if (Path::isAbsolutePath (_script))
		command = _script;
	else
		command.format (_T("./%s"), _script.getCStr());
	StreamReader reader (_script);
	if (!reader.readLine ().startsWith (_T("#!")))
		command = String (getenv ("SHELL")) + _T(" ") + command;
	reader.close();
#endif // __WINDOWS__
	NC_ORACLE_SCHEDULE_TRACE (_T("ncOracleRestoreExec::executeScript () command: %s"), command.getCStr());

	try {
		executeScript (command);
	}
	catch (...) {

	}

	_hasFinish = true;

	NC_ORACLE_EXECUTER_TRACE(_T("ncOracleExecThread::run () - end"));
}

void
ncOracleExecThread::executeScript (String& command)
{
	void* hWritePipe;
	void* hReadPipe;
#ifdef __WINDOWS__
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;
#endif
	if (!::CreatePipe(&hReadPipe,
		&hWritePipe,
#ifdef __WINDOWS__
		&sa,
#else
		0,
#endif
		0)) {
		int result = ::GetLastError();
		NC_ORACLE_SCHEDULE_TRACE(_T("ncOracleExecThread::executeScript () result: %d"), result);
	}
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	si.cb = sizeof(STARTUPINFO);
	memset(&si, 0, sizeof(si));
#ifdef __WINDOWS__
	GetStartupInfo(&si);
#endif
	si.hStdError = hWritePipe;
	si.hStdOutput = hWritePipe;
	si.wShowWindow = SW_HIDE;
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

	if (!::CreateProcess(NULL,
#ifdef __WINDOWS__
	(LPWSTR) command.getCStr(),
#else
		command.getCStr(),
#endif
		NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		int result = ::GetLastError();
		NC_ORACLE_SCHEDULE_TRACE(_T("ncOracleExecThread::executeScript () result: %d"), result);
	}
	ncReadPipeThread* readThread = new ncReadPipeThread(_listener, hReadPipe);
	readThread->start();

	int exitCode = 0;
	while (!_hasStoped) {
		::GetExitCodeProcess(pi.hProcess, TO_DWORD_PTR(&exitCode));
		if (exitCode != STILL_ACTIVE)
			break;
	}
	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecThread::executeScript () exitCode: %d"), exitCode);

	// 向管道写内容解阻塞，并通知进程退出
	unsigned int bytesWritten;
	::WriteFile(hWritePipe, STOP_FLAG, (unsigned int)strlen(STOP_FLAG), TO_DWORD_PTR(&bytesWritten), (LPOVERLAPPED)NULL);

	if (readThread != 0) {
		readThread->join();
		delete readThread;
		readThread = 0;
	}
	if (_hasStoped && pi.hProcess != 0) {
		::TerminateProcess(pi.hProcess, 0);
		pi.hProcess = 0;
	}
	::CloseHandle(pi.hProcess);
	::CloseHandle(hReadPipe);
	::CloseHandle(hWritePipe);

	if (exitCode != 0 && _hasStoped == false) {
		String errorMsg;
		errorMsg.format(LOAD_STRING(_T("IDS_EXEC_SCRIPT_FAILED")).getCStr(), exitCode);
		THROW_ORACLE_BACKUP_ERROR(errorMsg, ERROR_ORACLE_EXEC_SCRIPT_FAILED);
	}
	else
		_listener->OnMessage(LOAD_STRING(_T("IDS_EXEC_SCRIPT_SUCCESS")));

	NC_ORACLE_EXECUTER_TRACE (_T("ncOracleExecThread::execScript () - end"));
}

//--------------------------------------------------------------------------------
// class ncReadOutputThread
//--------------------------------------------------------------------------------
ncReadOutputThread::ncReadOutputThread (ncIEEFListener *listener)
						: _listener (listener)
{
	NC_ORACLE_EXECUTER_TRACE(_T("ncReadOutputThread::ncReadOutputThread ()"));
}

ncReadOutputThread::~ncReadOutputThread ()
{
	NC_ORACLE_EXECUTER_TRACE(_T("ncReadOutputThread::~ncReadOutputThread ()"));
}

ncReadOutputThread::run ()
{
	NC_ORACLE_EXECUTER_TRACE(_T("ncReadOutputThread::run () - begin"));
	try {
		bool isStop = false;
		bool bIgnore = false;
		bool hasError = false;
		char buffer[MAX_MESSAGE_SIZE];
		bool is_need_format = false;
		String format_str;
		String szOutput;
		while (isStop == false) {
			::memset(buffer, 0, MAX_MESSAGE_SIZE);
			unsigned int readSize;
			if (::ReadFile(_readPipe, buffer, MAX_MESSAGE_SIZE - 1, TO_DWORD_PTR(&readSize), (LPOVERLAPPED)NULL) != TRUE || readSize == 0) {
				int result = ::GetLastError();
				NC_ORACLE_EXECUTER_TRACE (_T("ncReadOutputThread::run () result: %d"), result);
				break;
			}

			buffer[readSize] = 0;
			String outputMsg(buffer, readSize);
			vector<String> veStrMsg;
			outputMsg.split(_T('\n'), veStrMsg);
			// NC_ORACLE_SCHEDULE_TRACE(_T("ncReadOutputThread::run () msg : %s"), msg.getCStr());

			if (is_need_format) {
				NC_ORACLE_EXECUTER_TRACE (_T("ncReadOutputThread::run () front=%s"), veStrMsg.front().getCStr());
				veStrMsg.front() = format_str.append(veStrMsg.front());
				is_need_format = false;
			}
			if (buffer[numBytesRead - 1] != '\n') {
				NC_ORACLE_EXECUTER_TRACE (_T("ncReadOutputThread::run () -----------------------"));
				format_str = veStrMsg.back();
				if ((format_str.find(_T(STOP_TAG))) != String::NO_POSITION)
					break;
				NC_ORACLE_EXECUTER_TRACE (_T("ncReadOutputThread::run () format_str=%s"), format_str.getCStr());
				veStrMsg.pop_back();
				is_need_format = true;
			}

			typedef vector<String>::iterator iterator;
			for (iterator it = veStrMsg.begin(); it != veStrMsg.end(); ++it) {
				if (!it->isEmpty()) {
					szOutput = *it;
					//if ((szOutput.find(_T("Recovery Manager complete."))) != String::NO_POSITION ||
					//	(szOutput.find(_T("恢复管理器完成。"))) != String::NO_POSITION ||
					//	(szOutput.find(_T("鎭㈠绠＄悊鍣ㄥ畬鎴愩�"))) != String::NO_POSITION) {
					//	_listener->OnMessage(szOutput);
					//	isStop = true;
					//	break;
					//}
					if (szOutput.find (STOP_FLAG) != String::NO_POSITION) {
						isStop = true;
						break;
					}
					if ((szOutput.find(_T(STOP_TAG))) != String::NO_POSITION) {
						NC_RAC_SCHEDULE_TRACE(_T("enter ncRMANOutputReadThread::run()  It's timme to finish now!"));
						isStop = true;
						THROW_EXECUTER_ERROR (LOAD_STRING(_T("IDS_RMAN_EXITED_SCHEDULE_WILL_STOPED")), ERROR_RMAN_EXITED_SCHEDULE_WILL_STOPED);
						break;
					}

					if ((szOutput.find(_T("connect target '"))) != String::NO_POSITION) {
						szOutput = _T("RMAN> connect target *;");
					}

					_listener->OnMessage(szOutput);
					if (bIgnore) {
						continue;
					}
					//判断RMAN报错，客户端的备份和恢复进程要能及时退出去
					if (String::NO_POSITION != szOutput.find(_T("ORA-")) || String::NO_POSITION != szOutput.find(_T("cannot make a snapshot control file"))) {
						if (String::NO_POSITION != szOutput.find(_T("ORA-00325"))) {
							bIgnore = true;
							continue;
						}
						//9i下无法删除主键被外部关键字引用的表空间
						if (String::NO_POSITION != szOutput.find(_T("ORA-02449"))) {
							//ncRACRestoreSchedule* reSchedule = (ncRACRestoreSchedule*)_backupSchedule;
							//reSchedule->setReDropTablespace();
							continue;
						}
						//分区表无法被删除
						if (String::NO_POSITION != szOutput.find(_T("ORA-14404"))) {
							bIgnore = true;
							continue;
						}

						Sleep(2000);
						if (String::NO_POSITION != szOutput.find(_T("ORA-19625"))) {
							THROW_EXECUTER_ERROR (LOAD_STRING(_T("IDS_ORA_FAILED_19625")), ERROR_ORA_FAILED_19625);
						}
						if (String::NO_POSITION != szOutput.find(_T("ORA-19870"))) {
							THROW_EXECUTER_ERROR (LOAD_STRING(_T("IDS_ORA_FAILED_19870")), ERROR_ORA_FAILED_19870);
						}
						if (String::NO_POSITION != szOutput.find(_T("ORA-19755"))) {
							hasError = false;
							_dbManager->alterDatabaseOpen(true, _pattern._isOnline);
							goto END;
						}
						if (String::NO_POSITION != szOutput.find(_T("ORA-01124"))) {
							for (++it; it != veStrMsg.end() && !it->isEmpty(); ++it)
								_listener->OnMessage(*it);
							THROW_EXECUTER_ERROR (LOAD_STRING(_T("IDS_ORA_FAILED_01124")), ERROR_ORA_FAILED_01124);
						}
						if (String::NO_POSITION != szOutput.find(_T("ORA-01135"))) {
							for (++it; it != veStrMsg.end() && !it->isEmpty(); ++it)
								_listener->OnMessage(*it);
							THROW_EXECUTER_ERROR (LOAD_STRING(_T("IDS_ORA_FAILED_01135")), ERROR_ORA_FAILED_01135);
						}
						hasError = true;
					}
					//介质管理器恢复失败
					if (String::NO_POSITION != szOutput.find(_T("media recovery failed"))) {
						bIgnore = true;
						continue;
					}

					//读取Oracle文件头失败
					if (String::NO_POSITION != szOutput.find(_T("could not read file header"))) {
						THROW_EXECUTER_ERROR(LOAD_STRING(_T("IDS_ORA_READ_FILE_HEADER_FAILED")), ERROR_ORA_READ_FILE_HEADER_FAILED);
					}
					if (String::NO_POSITION != szOutput.find(_T("RMAN-20207"))) {
						THROW_EXECUTER_ERROR(LOAD_STRING(_T("IDS_BEFORE_RESETLOG_TIME")), ERROR_BEFORE_RESETLOG_TIME);
					}
					if (String::NO_POSITION != szOutput.find(_T("RMAN-06056")) || String::NO_POSITION != szOutput.find(_T("RMAN-06169"))) {
						THROW_EXECUTER_ERROR(LOAD_STRING(_T("IDS_ORACLE_ERROR_RMAN_06056")), ERROR_ORACLE_ERROR_RMAN_06056);
					}
					if (String::NO_POSITION != szOutput.find(_T("RMAN-06061"))) {
						//THROW_EXECUTER_ERROR (LOAD_STRING(_T("IDS_ORACLE_ERROR_RMAN_06061")),ERROR_ORACLE_ERROR_RMAN_06061);
						ncRACBackupWarnException warning(__FILE__, __LINE__, LOAD_STRING(_T("IDS_ORACLE_ERROR_RMAN_06061")));
						_listener->OnWarning(warning, false);
					}
					//RAC有时候会出现用srvctl stop database停不掉数据库,无法恢复的情况
					if ((String::NO_POSITION != szOutput.find(_T("RMAN-06496")))
						|| (String::NO_POSITION != szOutput.find(_T("RMAN-06135")))
						|| (String::NO_POSITION != szOutput.find(_T("PRCC-")))
						|| (String::NO_POSITION != szOutput.find(_T("PRCD-")))
						|| (String::NO_POSITION != szOutput.find(_T("PRCR-")))
						|| (String::NO_POSITION != szOutput.find(_T("PRKP-")))) {
						THROW_EXECUTER_ERROR(LOAD_STRING(_T("IDS_SRVCTL_STOP_FAILED")), ERROR_SRVCTL_STOP_FAILED);
					}
					//解决在9i环境中在日志被删除进行事务日志恢复或者增量备份集恢复会出现RMAN-06054警告信息
					if (String::NO_POSITION != szOutput.find(_T("RMAN-06054"))) {
						_dbManager->alterDatabaseOpen(false, _pattern._isOnline);
						bIgnore = true;
					}
					// 表空间恢复时缺失日志需要捕获并使任务失败
					if (String::NO_POSITION != szOutput.find(_T("RMAN-06053")))
						hasError = true;

					if (String::NO_POSITION != szOutput.find(_T("RMAN-01009"))) {
						String errMsg;
						errMsg.format(LOAD_STRING(_T("IDS_ORACLE_RMAN_OUTPUT_FAILED")).getCStr(), szOutput.getCStr());
						THROW_EXECUTER_ERROR(errMsg, ERROR_ORACLE_RMAN_OUTPUT_FAILED);
					}
				}
			}
		}

END:

		_RACExecThread->signal();
	}
	catch (Exception & e) {
		//_RACExecThread->signal();
		ncRACBackupErrorException error(__FILE__, __LINE__, LOAD_STRING(_T("IDS_ORACLE_RMAN_OUTPUT_THREAD_ERROR")), e, ::GetLastError(), ncGetRACBACKUPErrorProvider());
		_listener->OnError(error);
		//_backupSchedule->Stop();
	}
	catch (...) {
		//_RACExecThread->signal();
		//_backupSchedule->Stop();
		THROW_EXECUTER_ERROR(LOAD_STRING(_T("IDS_ORACLE_RMAN_OUTPUT_THREAD_UNKOWN_ERROR")), ERROR_ORACLE_RMAN_OUTPUT_THREAD_UNKOWN_ERROR);
	}
	NC_ORACLE_EXECUTER_TRACE(_T("ncReadOutputThread::run () - end"));
}
