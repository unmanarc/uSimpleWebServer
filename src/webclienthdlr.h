#ifndef WEBCLIENTHANDLER_H
#define WEBCLIENTHANDLER_H

#include <mdz_proto_http/httpv1_server.h>
#include <mdz_prg_logs/rpclog.h>
#include <mutex>

struct webClientParams
{
    webClientParams()
    {
        execute = false;
        rpcLog = nullptr;
        targz = false;
    }

    Mantids::Memory::Containers::B_MEM * favicon;
    bool execute,targz;
    std::string softwareVersion;
    std::string httpDocumentRootDir;
    Mantids::Application::Logs::RPCLog * rpcLog;
    std::string user,pass;
};

class WebClientHdlr : public Mantids::Network::HTTP::HTTPv1_Server
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
    Mantids::Network::HTTP::Response::StatusCode processClientRequest() override;

private:
    bool containOnlyAllowedChars(const std::string &str);
    void generateIndexOf(const Mantids::Network::HTTP::sLocalRequestedFileInfo &fileInfo);
    void generateTarGz(const Mantids::Network::HTTP::sLocalRequestedFileInfo &fileInfo);

    std::string getFancySize(size_t  size);
    webClientParams webClientParameters;


};



#endif // WEBCLIENTHANDLER_H
