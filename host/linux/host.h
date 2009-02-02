/**
 * Appcelerator Kroll - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2008 Appcelerator, Inc. All Rights Reserved.
 */
#ifndef _LINUX_HOST_H
#define _LINUX_HOST_H

#include <vector>
#include <string>
#include <api/module.h>
#include <api/module_provider.h>
#include <api/host.h>
#include <api/binding/bound_object.h>

#include <Poco/ScopedLock.h>
#include <Poco/Mutex.h>

namespace kroll
{

	struct Job
	{
		SharedBoundMethod method;
		SharedPtr<ValueList> args;
	};

	class EXPORT LinuxHost : public Host
	{ 
	public:
		LinuxHost(int argc, const char* argv[]);

		virtual Module* CreateModule(std::string& path);
		SharedValue InvokeMethodOnMainThread(SharedBoundMethod method,
		                                     SharedPtr<ValueList> args);

		Poco::Mutex& GetJobQueueMutex();
		std::vector<Job>& GetJobs();

	protected:
		virtual bool RunLoop();
		virtual ~LinuxHost();

	private:
		Poco::Mutex job_queue_mutex;
		std::vector<Job> jobs;
	};
}

extern "C"
{
	EXPORT int Execute(int argc,const char** argv);
}


#endif
