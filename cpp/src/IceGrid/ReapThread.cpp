// **********************************************************************
//
// Copyright (c) 2003-2006 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <Ice/Ice.h>
#include <IceGrid/ReapThread.h>

using namespace std;
using namespace IceGrid;

ReapThread::ReapThread(int timeout) :
    _timeout(IceUtil::Time::seconds(timeout)),
    _terminated(false)
{
}

void
ReapThread::run()
{
    Lock sync(*this);

    while(!_terminated)
    {
	list<ReapablePtr>::iterator p = _sessions.begin();
	while(p != _sessions.end())
	{
	    try
	    {
		if((IceUtil::Time::now() - (*p)->timestamp()) > _timeout)
		{
		    try
		    {
			(*p)->destroy();
		    }
		    catch(const Ice::LocalException&)
		    {
		    }
		    p = _sessions.erase(p);
		}
		else
		{
		    ++p;
		}
	    }
	    catch(const Ice::ObjectNotExistException&)
	    {
		p = _sessions.erase(p);
	    }
	}

	timedWait(_timeout);
    }
}

void
ReapThread::terminate()
{
    Lock sync(*this);

    _terminated = true;
    notify();

    for(list<ReapablePtr>::const_iterator p = _sessions.begin(); p != _sessions.end(); ++p)
    {
	try
	{
	    (*p)->destroy();
	}
	catch(const Ice::LocalException&)
	{
	    // Ignore.
	}
    }

    _sessions.clear();
}

void
ReapThread::add(const ReapablePtr& reapable)
{
    Lock sync(*this);
    _sessions.push_back(reapable);
}

