#include <thread>

#include <mdz_prg_service/application.h>
#include <mdz_prg_logs/applog.h>
#include <mdz_net_sockets/socket_tcp.h>
#ifdef WITH_SSL_SUPPORT
#include <mdz_net_sockets/socket_tls.h>
#endif
#include <mdz_net_sockets/socket_acceptor_multithreaded.h>
#include <mdz_mem_vars/a_uint16.h>
#include <mdz_mem_vars/a_bool.h>

#include <sys/time.h>
#include <fstream>

#include "webclienthdlr.h"
#include "favicon.h"
#include "config.h"


using namespace Mantids;
using namespace Mantids::Memory;
using namespace Mantids::Application;


#ifdef _WIN32
#include <stdlib.h>
// TODO: check if _fullpath mitigate transversal.
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#endif

class USimpleWebServer : public Mantids::Application::Application
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

        webClientParameters.favicon = new Mantids::Memory::Containers::B_MEM(favicon2_ico,favicon2_ico_len);

        /////////////////////////
        globalArguments->setVersion( atoi(PROJECT_VER_MAJOR), atoi(PROJECT_VER_MINOR), atoi(PROJECT_VER_PATCH), "a" );

        webClientParameters.softwareVersion = globalArguments->getVersion();
        globalArguments->setLicense("AGPL");
        globalArguments->setAuthor("Aarón Mizrachi");
        globalArguments->setEmail("aaron@unmanarc.com");
        globalArguments->setDescription(std::string("Unmanarc's HTTP Server"));

        globalArguments->addCommandLineOption("Server Options", 'x', "execute" ,
#ifndef _WIN32
                                              "Execute any eXecutable marked file and get the output, ATT: disable targz if you want to keep your script sources private",
#else
                                              "Execute any .exe/.bat file and get the output, ATT: disable targz if you want to keep your script sources private",
#endif
                                              "false", Abstract::TYPE_BOOL );

        globalArguments->addCommandLineOption("Server Options", 'g', "targz" , "Allow to get server directories as tar.gz files (on-the-fly)"  , "false", Abstract::TYPE_BOOL );

        globalArguments->addCommandLineOption("HTTP Options", 'r', "rootdir" , "HTTP Document Root Directory"  , ".", Abstract::TYPE_STRING );
        globalArguments->addCommandLineOption("HTTP Options", 'l', "lport" , "Local HTTP Port"  , "8001", Abstract::TYPE_UINT16);
        globalArguments->addCommandLineOption("HTTP Options", '4', "ipv4" , "Use only IPv4"  , "true", Abstract::TYPE_BOOL);
        globalArguments->addCommandLineOption("HTTP Options", 'a', "laddr" , "Listen Address"  , "*", Abstract::TYPE_STRING);
        globalArguments->addCommandLineOption("HTTP Options", 't', "threads" , "Max Concurrent Connections (Threads)"  , "1024", Abstract::TYPE_UINT16);

        globalArguments->addCommandLineOption("HTTP Security", 'p', "passfile" , "HTTP User/Pass File"  , "", Abstract::TYPE_STRING);

        globalArguments->addCommandLineOption("Other Options", 's', "sys" , "Journalctl Log Mode (don't print colors or dates)"  , "false", Abstract::TYPE_BOOL);

#ifdef WITH_SSL_SUPPORT
        globalArguments->addCommandLineOption("TLS Options", 'k', "keyfile" , "X.509 Private Key Path (if not defined, then HTTP)"  , "", Abstract::TYPE_STRING);
        globalArguments->addCommandLineOption("TLS Options", 'c', "certfile" , "X.509 Certificate Path (if not defined, then HTTP)"  , "", Abstract::TYPE_STRING);
#endif
    }

    bool _config(int argc, char *argv[], Arguments::GlobalArguments * globalArguments)
    {
#ifdef WITH_SSL_SUPPORT
        Mantids::Network::TLS::Socket_TLS::prepareTLS();
#endif
        bool configUseFancy    = !((Memory::Abstract::BOOL *)globalArguments->getCommandLineOptionValue("sys"))->getValue();
        fprintf(stderr,"# Arguments: %s\n", globalArguments->getCurrentProgramOptionsValuesAsBashLine().c_str());

        log = new Logs::AppLog();
        log->setPrintEmptyFields(true);
        log->setUserAlignSize(1);
        log->setUsingAttributeName(false);
        log->setUsingColors(configUseFancy);
        log->setUsingPrintDate(configUseFancy);
        log->setModuleAlignSize(36);

        webClientParameters.rpcLog = new Logs::RPCLog();
        webClientParameters.rpcLog->setPrintEmptyFields(false);
        webClientParameters.rpcLog->setUserAlignSize(1);
        webClientParameters.rpcLog->setUsingAttributeName(false);
        webClientParameters.rpcLog->setUsingColors(configUseFancy);
        webClientParameters.rpcLog->setUsingPrintDate(configUseFancy);
        webClientParameters.rpcLog->setModuleAlignSize(36);

        std::string passFile = globalArguments->getCommandLineOptionValue("passfile")->toString();

        if ( !passFile.empty() )
        {
            std::ifstream file(passFile);
            if (file.is_open()) {

                if (!std::getline(file, webClientParameters.user))
                {
                    log->log0(__func__,Logs::LEVEL_CRITICAL, "Password File '%s' require at least 2 lines [user/pass)", passFile.c_str());
                    exit(-14);
                }
                if (!std::getline(file, webClientParameters.pass))
                {
                    log->log0(__func__,Logs::LEVEL_CRITICAL, "Password File '%s' require at least 2 lines (user/pass]", passFile.c_str());
                    exit(-15);
                }
                file.close();
            }
            else
            {
                log->log0(__func__,Logs::LEVEL_CRITICAL, "Failed to open user/password file '%s'", passFile.c_str());
                exit(-13);
            }
        }

        listenAddress         = globalArguments->getCommandLineOptionValue("laddr")->toString();
        webClientParameters.httpDocumentRootDir   = globalArguments->getCommandLineOptionValue("rootdir")->toString();
        listenPort            = ((Memory::Abstract::UINT16 *)globalArguments->getCommandLineOptionValue("lport"))->getValue();
        webClientParameters.execute = ((Memory::Abstract::BOOL *)globalArguments->getCommandLineOptionValue("execute"))->getValue();
        webClientParameters.targz = ((Memory::Abstract::BOOL *)globalArguments->getCommandLineOptionValue("targz"))->getValue();

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
        auto rp = realpath(webClientParameters.httpDocumentRootDir.c_str(), nullptr);

        if (!rp)
        {
            log->log0(__func__,Logs::LEVEL_CRITICAL, "Invalid Document Root Directory.");
            exit(-1);
        }
        multiThreadedAcceptor.startThreaded();

        log->log0(__func__,Logs::LEVEL_INFO, "Web Server Loaded @%s:%d", listenAddress.c_str(),listenPort);
        log->log0(__func__,Logs::LEVEL_INFO, "Document root: %s", rp);

        return 0;
    }

    const webClientParams &getWebClientParameters() const;

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

    std::string listenAddress;
    uint16_t listenPort;
    Logs::AppLog * log;   
    webClientParams webClientParameters;

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

    webHandler.addStaticContent("/favicon.ico",webServer->getWebClientParameters().favicon);

    char inetAddr[INET6_ADDRSTRLEN];
    s->getRemotePair(inetAddr);

    webHandler.setIsSecure(isSecure);
    webHandler.setRemotePairAddress(inetAddr);
    webHandler.setWebClientParameters(webServer->getWebClientParameters());

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

const webClientParams &USimpleWebServer::getWebClientParameters() const
{
    return webClientParameters;
}

int main(int argc, char *argv[])
{
    USimpleWebServer * main = new USimpleWebServer;
    return StartApplication(argc,argv,main);
}
