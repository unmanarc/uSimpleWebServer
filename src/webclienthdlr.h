#ifndef WEBCLIENTHANDLER_H
#define WEBCLIENTHANDLER_H

#include <cx2_netp_http/httpv1_server.h>
#include <cx2_prg_logs/rpclog.h>
#include <mutex>

struct webClientParams
{
    webClientParams()
    {
        execute = false;
        rpcLog = nullptr;
        targz = false;
    }

    bool execute,targz;
    std::string softwareVersion;
    std::string httpDocumentRootDir;
    CX2::Application::Logs::RPCLog * rpcLog;
    std::string user,pass;
};

class WebClientHdlr : public CX2::Network::HTTP::HTTPv1_Server
{
public:
    WebClientHdlr(void *parent, Streamable *sock);
    ~WebClientHdlr() override;

    void setWebClientParameters(const webClientParams &newWebClientParameters);

protected:
    /**
     * @brief processClientRequest Process web client request
     * @return http responce code.
     */
    CX2::Network::HTTP::Response::StatusCode processClientRequest() override;

private:

    void generateIndexOf(const CX2::Network::HTTP::sLocalRequestedFileInfo &fileInfo);
    void generateTarGz(const CX2::Network::HTTP::sLocalRequestedFileInfo &fileInfo);

    std::string getFancySize(size_t  size);
    webClientParams webClientParameters;


};



#endif // WEBCLIENTHANDLER_H
