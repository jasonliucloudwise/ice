// **********************************************************************
//
// Copyright (c) 2003-2005 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <Ice/Endpoint.h>
#include <Ice/Network.h>
#include <Ice/Connector.h>
#include <Ice/Transceiver.h>
#include <Ice/BasicStream.h>
#include <Ice/LocalException.h>
#include <Ice/Instance.h>
#include <Ice/DefaultsAndOverrides.h>
#include <IceUtil/SafeStdio.h>

using namespace std;
using namespace Ice;
using namespace IceInternal;

void IceInternal::incRef(Endpoint* p) { p->__incRef(); }
void IceInternal::decRef(Endpoint* p) { p->__decRef(); }

IceInternal::Endpoint::Endpoint(const InstancePtr& instance, const string& ho, Int po, Int ti) :
    _instance(instance),
    _host(ho),
    _port(po),
    _timeout(ti)
{
}

IceInternal::Endpoint::Endpoint(const InstancePtr& instance, const string& str) :
    _instance(instance),
    _port(0),
    _timeout(-1)
{
    const string delim = " \t\n\r";

    string::size_type beg;
    string::size_type end = 0;

    while(true)
    {
	beg = str.find_first_not_of(delim, end);
	if(beg == string::npos)
	{
	    break;
	}
	
	end = str.find_first_of(delim, beg);
	if(end == string::npos)
	{
	    end = str.length();
	}

	string option = str.substr(beg, end - beg);
	if(option.length() != 2 || option[0] != '-')
	{
	    EndpointParseException ex(__FILE__, __LINE__);
	    ex.str = "tcp " + str;
	    throw ex;
	}

	string argument;
	string::size_type argumentBeg = str.find_first_not_of(delim, end);
	if(argumentBeg != string::npos && str[argumentBeg] != '-')
	{
	    beg = argumentBeg;
	    end = str.find_first_of(delim, beg);
	    if(end == string::npos)
	    {
		end = str.length();
	    }
	    argument = str.substr(beg, end - beg);
	}

	switch(option[1])
	{
	    case 'h':
	    {
		if(argument.empty())
		{
		    EndpointParseException ex(__FILE__, __LINE__);
		    ex.str = "tcp " + str;
		    throw ex;
		}
		const_cast<string&>(_host) = argument;
		break;
	    }

	    case 'p':
	    {
	    	const_cast<Int&>(_port) = atoi(argument.c_str());
		if(_port == 0)
		{
		    EndpointParseException ex(__FILE__, __LINE__);
		    ex.str = "tcp " + str;
		    throw ex;
		}
		break;
	    }

	    case 't':
	    {
	        const_cast<Int&>(_timeout) = atoi(argument.c_str());
		if(_timeout == 0)
		{
		    EndpointParseException ex(__FILE__, __LINE__);
		    ex.str = "tcp " + str;
		    throw ex;
		}
		break;
	    }

	    case 'z':
	    {
	        // Ignore compression flag.
	        break;
	    }

	    default:
	    {
		EndpointParseException ex(__FILE__, __LINE__);
		ex.str = "tcp " + str;
		throw ex;
	    }
	}
    }

    if(_host.empty())
    {
	const_cast<string&>(_host) = _instance->defaultsAndOverrides()->defaultHost;
    }
}

IceInternal::Endpoint::Endpoint(BasicStream* s) :
    _instance(s->instance()),
    _port(0),
    _timeout(-1)
{
    bool dummy;

    s->startReadEncaps();
    s->read(const_cast<string&>(_host));
    s->read(const_cast<Int&>(_port));
    s->read(const_cast<Int&>(_timeout));
    s->read(const_cast<bool&>(dummy));
    s->endReadEncaps();
}

void
IceInternal::Endpoint::streamWrite(BasicStream* s) const
{
    s->write(TcpEndpointType);
    s->startWriteEncaps();
    s->write(_host);
    s->write(_port);
    s->write(_timeout);
    s->write(false);
    s->endWriteEncaps();
}

string
IceInternal::Endpoint::toString() const
{
    string s;
    s += "tcp -h ";
    s += _host;

    s += IceUtil::printfToString(" -p %d", _port);

    if(_timeout != -1)
    {
        s += IceUtil::printfToString(" -t %d", _timeout);
    }
    return s;
}

Short
IceInternal::Endpoint::type() const
{
    return TcpEndpointType;
}

Int
IceInternal::Endpoint::timeout() const
{
    return _timeout;
}

EndpointPtr
IceInternal::Endpoint::timeout(Int timeout) const
{
    if(timeout == _timeout)
    {
	return const_cast<Endpoint*>(this);
    }
    else
    {
	return new Endpoint(_instance, _host, _port, timeout);
    }
}

bool
IceInternal::Endpoint::unknown() const
{
    return false;
}

ConnectorPtr
IceInternal::Endpoint::connector() const
{
    return new Connector(_instance, _host, _port);
}

bool
IceInternal::Endpoint::equivalent(const TransceiverPtr&) const
{
    return false;
}

bool
IceInternal::Endpoint::operator==(const Endpoint& r) const
{
    const Endpoint* p = dynamic_cast<const Endpoint*>(&r);
    assert(p);

    if(this == p)
    {
	return true;
    }

    if(_port != p->_port)
    {
	return false;
    }

    if(_timeout != p->_timeout)
    {
	return false;
    }

    if(_host != p->_host)
    {
	//
	// We do the most time-consuming part of the comparison last.
	//
	struct sockaddr_in laddr;
	struct sockaddr_in raddr;
	try
	{
	    getAddress(_host, _port, laddr);
	    getAddress(p->_host, p->_port, raddr);
	}
	catch(const DNSException&)
	{
	    return false;
	}

	return compareAddress(laddr, raddr);
    }

    return true;
}

bool
IceInternal::Endpoint::operator!=(const Endpoint& r) const
{
    return !operator==(r);
}

bool
IceInternal::Endpoint::operator<(const Endpoint& r) const
{
    const Endpoint* p = dynamic_cast<const Endpoint*>(&r);
    assert(p);

    if(this == p)
    {
	return false;
    }

    if(_port < p->_port)
    {
	return true;
    }
    else if(p->_port < _port)
    {
	return false;
    }

    if(_timeout < p->_timeout)
    {
	return true;
    }
    else if(p->_timeout < _timeout)
    {
	return false;
    }

    if(_host != p->_host)
    {
	//
	// We do the most time-consuming part of the comparison last.
	//
	struct sockaddr_in laddr;
	try
	{
	    getAddress(_host, _port, laddr);
	}
	catch(const DNSException&)
	{
	}

	struct sockaddr_in raddr;
	try
	{
	    getAddress(p->_host, p->_port, raddr);
	}
	catch(const DNSException&)
	{
	}

	if(laddr.sin_addr.s_addr < raddr.sin_addr.s_addr)
	{
	    return true;
	}
	else if(raddr.sin_addr.s_addr < laddr.sin_addr.s_addr)
	{
	    return false;
	}
    }

    return false;
}
