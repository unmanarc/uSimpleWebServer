#ifndef WEBCLIENTHANDLER_H
#define WEBCLIENTHANDLER_H

#include <cx2_netp_http/httpv1_server.h>
#include <cx2_prg_logs/rpclog.h>
#include <mutex>

class WebClientHdlr : public CX2::Network::HTTP::HTTPv1_Server
{
public:
    WebClientHdlr(void *parent, Streamable *sock);
    ~WebClientHdlr() override;


    const std::string &getResourcesLocalPath() const;
    void setResourcesLocalPath(const std::string &newResourcesLocalPath);

    CX2::Application::Logs::RPCLog *getRpcLog() const;
    void setRpcLog(CX2::Application::Logs::RPCLog *newRpcLog);

    const std::string &getRemoteIP() const;
    void setRemoteIP(const std::string &newRemoteIP);

    void setPass(const std::string &newPass);

    void setUser(const std::string &newUser);



    void setExecute(bool newExecute);

    void setSoftwareVersion(const std::string &newSoftwareVersion);

protected:
    /**
     * @brief processClientRequest Process web client request
     * @return http responce code.
     */
    CX2::Network::HTTP::Response::StatusCode processClientRequest() override;

private:

    std::string getFancySize(size_t  size);

    bool execute;
    std::string softwareVersion;
    std::string resourcesLocalPath;
    CX2::Application::Logs::RPCLog * rpcLog;
    std::string user,pass;

};



#endif // WEBCLIENTHANDLER_H
