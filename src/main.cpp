#include <thread>

#include <cx2_prg_service/application.h>
#include <cx2_prg_logs/applog.h>
#include <cx2_net_sockets/socket_tcp.h>
#ifdef WITH_SSL_SUPPORT
#include <cx2_net_sockets/socket_tls.h>
#endif
#include <cx2_net_sockets/socket_acceptor_multithreaded.h>
#include <cx2_mem_vars/a_uint16.h>
#include <cx2_mem_vars/a_bool.h>

#include <sys/time.h>

#include "webclienthdlr.h"

using namespace CX2;
using namespace CX2::Memory;
using namespace CX2::Application;

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0


#ifdef _WIN32
#include <stdlib.h>
// TODO: check if _fullpath mitigate transversal.
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#endif

class USimpleWebServer : public CX2::Application::Application
{
public:
    USimpleWebServer() {
    }

    void _shutdown()
    {
        log->log0(__func__,Logs::LEVEL_INFO, "SIGTERM received.");
    }

    void _initvars(int argc, char *argv[], Arguments::GlobalArguments * globalArguments)
    {
        globalArguments->setInifiniteWaitAtEnd(true);

        /////////////////////////
        struct timeval time;
        gettimeofday(&time,nullptr);
        srand(((time.tv_sec * 1000) + (time.tv_usec / 1000))*getpid());

        globalArguments->setVersion( VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, "alpha" );
        globalArguments->setLicense("AGPL");
        globalArguments->setAuthor("Aarón Mizrachi");
        globalArguments->setEmail("aaron@unmanarc.com");
        globalArguments->setDescription(std::string("Unmanarc's HTTP Server"));

        globalArguments->addCommandLineOption("HTTP Options", 'x', "xdir" , "HTTP Document Root Directory"  , ".", Abstract::TYPE_STRING );
        globalArguments->addCommandLineOption("HTTP Options", 'l', "lport" , "Local HTTP Port"  , "8001", Abstract::TYPE_UINT16);
        globalArguments->addCommandLineOption("HTTP Options", '4', "ipv4" , "Use only IPv4"  , "true", Abstract::TYPE_BOOL);
        globalArguments->addCommandLineOption("HTTP Options", 'a', "laddr" , "Listen Address"  , "*", Abstract::TYPE_STRING);
        globalArguments->addCommandLineOption("HTTP Options", 't', "threads" , "Max Concurrent Connections (Threads)"  , "1024", Abstract::TYPE_UINT16);

        globalArguments->addCommandLineOption("HTTP Security", 'u', "user" , "HTTP User"  , "", Abstract::TYPE_STRING);
        globalArguments->addCommandLineOption("HTTP Security", 'p', "pass" , "HTTP Pass"  , "", Abstract::TYPE_STRING);

#ifdef WITH_SSL_SUPPORT
        globalArguments->addCommandLineOption("TLS Options", 'k', "keyfile" , "X.509 Private Key Path (if not defined, then HTTP)"  , "", Abstract::TYPE_STRING);
        globalArguments->addCommandLineOption("TLS Options", 'c', "certfile" , "X.509 Certificate Path (if not defined, then HTTP)"  , "", Abstract::TYPE_STRING);
#endif
    }

    bool _config(int argc, char *argv[], Arguments::GlobalArguments * globalArguments)
    {
        log = new Logs::AppLog();
        log->setPrintEmptyFields(true);
        log->setUserAlignSize(1);
        log->setUsingAttributeName(false);
        log->setUsingColors(true);
        log->setUsingPrintDate(true);
        log->setModuleAlignSize(36);

        rpcLog = new Logs::RPCLog();
        rpcLog->setPrintEmptyFields(true);
        rpcLog->setUserAlignSize(1);
        rpcLog->setUsingAttributeName(false);
        rpcLog->setUsingColors(true);
        rpcLog->setUsingPrintDate(true);
        rpcLog->setModuleAlignSize(36);

        user = globalArguments->getCommandLineOptionValue("user")->toString();
        pass  = globalArguments->getCommandLineOptionValue("pass")->toString();


        listenAddress         = globalArguments->getCommandLineOptionValue("laddr")->toString();
        httpDocumentRootDir   = globalArguments->getCommandLineOptionValue("xdir")->toString();
        listenPort            = ((Memory::Abstract::UINT16 *)globalArguments->getCommandLineOptionValue("lport"))->getValue();
        auto configUseIPv4    = (Memory::Abstract::BOOL *)globalArguments->getCommandLineOptionValue("ipv4");
        auto configThreads    = (Memory::Abstract::UINT16 *)globalArguments->getCommandLineOptionValue("threads");
#ifdef WITH_SSL_SUPPORT
        std::string certfile = globalArguments->getCommandLineOptionValue("certfile")->toString();
        std::string keyfile  = globalArguments->getCommandLineOptionValue("keyfile")->toString();
#endif
        Network::Sockets::Socket_TCP *socketTCP = new Network::Sockets::Socket_TCP;
#ifdef WITH_SSL_SUPPORT
        Network::TLS::Socket_TLS *socketTLS = new Network::TLS::Socket_TLS;

        if (keyfile.size())
        {
            if (access(keyfile.c_str(),R_OK))
            {
                log->log0(__func__,Logs::LEVEL_CRITICAL, "X.509 Private Key not found.");
                exit(-10);
                return false;
            }
            socketTLS->setTLSPrivateKeyPath(keyfile.c_str());
        }

        if (certfile.size())
        {
            if (access(certfile.c_str(),R_OK))
            {
                log->log0(__func__,Logs::LEVEL_CRITICAL, "X.509 Certificate not found.");
                exit(-11);
                return false;
            }
            socketTLS->setTLSPublicKeyPath(certfile.c_str());
        }

        if (keyfile.size() && certfile.size())
            socketTCP = socketTLS;
#endif
        socketTCP->setUseIPv6( !configUseIPv4->getValue() );
        if (!socketTCP->listenOn( listenPort, listenAddress.c_str() ))
        {
            log->log0(__func__,Logs::LEVEL_CRITICAL, "Unable to listen at %s:%d",listenAddress.c_str(), listenPort);
            exit(-20);
            return false;
        }        
        multiThreadedAcceptor.setAcceptorSocket(socketTCP);
        multiThreadedAcceptor.setCallbackOnConnect(_callbackOnConnect,this);
        multiThreadedAcceptor.setCallbackOnInitFail(_callbackOnInitFailed,this);
        multiThreadedAcceptor.setCallbackOnTimedOut(_callbackOnTimeOut,this);
        multiThreadedAcceptor.setMaxConcurrentClients(configThreads->getValue());

        return true;
    }

    int _start(int argc, char *argv[], Arguments::GlobalArguments * globalArguments)
    {
        multiThreadedAcceptor.startThreaded();
        auto rp = realpath(httpDocumentRootDir.c_str(), nullptr);

        if (!rp)
        {
            log->log0(__func__,Logs::LEVEL_CRITICAL, "Invalid Document Root Directory.");
            exit(-1);
        }

        log->log0(__func__,Logs::LEVEL_INFO, "Web Server Loaded @%s:%d", listenAddress.c_str(),listenPort);
        log->log0(__func__,Logs::LEVEL_INFO, "Document root: %s", rp);

        return 0;
    }

    const std::string &getDocumentRootPath() const;

    const std::string &getRemoteIP() const;
    void setRemoteIP(const std::string &newRemoteIP);

    Logs::RPCLog *getRpcLog() const;

    const std::string &getUser() const;
    const std::string &getPass() const;

private:
    /**
     * callback when connection is fully established (if the callback returns false, connection socket won't be automatically closed/deleted)
     */
    static bool _callbackOnConnect(void *, Network::Streams::StreamSocket *, const char *, bool);
    /**
     * callback when protocol initialization failed (like bad X.509 on TLS) (if the callback returns false, connection socket won't be automatically closed/deleted)
     */
    static bool _callbackOnInitFailed(void *, Network::Streams::StreamSocket *, const char *, bool);
    /**
     * callback when timed out (all the thread queues are saturated) (this callback is called from acceptor thread, you should use it very quick)
     */
    static void _callbackOnTimeOut(void *, Network::Streams::StreamSocket *, const char *, bool);

    std::string httpDocumentRootDir;
    std::string listenAddress;
    uint16_t listenPort;
    Logs::AppLog * log;
    Logs::RPCLog * rpcLog;
    std::string user,pass;
    Network::Sockets::Acceptors::Socket_Acceptor_MultiThreaded multiThreadedAcceptor;
};


bool USimpleWebServer::_callbackOnConnect(void *obj, Network::Streams::StreamSocket *s, const char *, bool)
{
    std::string tlsCN;
    USimpleWebServer * webServer = (USimpleWebServer *)obj;

    bool isSecure;
    if ((isSecure=s->isSecure()) == true)
    {
#ifdef WITH_SSL_SUPPORT
        Network::TLS::Socket_TLS * tlsSock = (Network::TLS::Socket_TLS *)s;
        tlsCN = tlsSock->getTLSPeerCN();
#endif
    }

    // Prepare the web services handler.
    WebClientHdlr webHandler(nullptr,s);


    char inetAddr[INET6_ADDRSTRLEN];
    s->getRemotePair(inetAddr);

    auto localResourcePath = webServer->getDocumentRootPath();

    webHandler.setIsSecure(isSecure);
    webHandler.setResourcesLocalPath(localResourcePath);
    webHandler.setRemotePairAddress(inetAddr);
    webHandler.setRpcLog(webServer->getRpcLog());
    webHandler.setUser(webServer->getUser());
    webHandler.setPass(webServer->getPass());

    // Handle the Web Client here:
    Memory::Streams::Parsing::ParseErrorMSG err;
    webHandler.parseObject(&err);

    return true;
}

bool USimpleWebServer::_callbackOnInitFailed(void *, Network::Streams::StreamSocket *s, const char *, bool)
{
    return true;
}

void USimpleWebServer::_callbackOnTimeOut(void *, Network::Streams::StreamSocket *s, const char *, bool)
{
    s->writeString("HTTP/1.1 503 Service Temporarily Unavailable\r\n");
    s->writeString("Content-Type: text/html; charset=UTF-8\r\n");
    s->writeString("Connection: close\r\n");
    s->writeString("\r\n");
    s->writeString("<center><h1>503 Service Temporarily Unavailable</h1></center><hr>\r\n");
}

const std::string &USimpleWebServer::getPass() const
{
    return pass;
}

const std::string &USimpleWebServer::getUser() const
{
    return user;
}

Logs::RPCLog *USimpleWebServer::getRpcLog() const
{
    return rpcLog;
}

const std::string &USimpleWebServer::getDocumentRootPath() const
{
    return httpDocumentRootDir;
}

int main(int argc, char *argv[])
{
    USimpleWebServer * main = new USimpleWebServer;
    return StartApplication(argc,argv,main);
}
