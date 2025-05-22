#ifndef WEBCLIENTHANDLER_H
#define WEBCLIENTHANDLER_H

#include <mdz_mem_vars/b_chunks.h>
#include <mdz_mem_vars/b_mem.h>
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
        uploads = false;
        uploadOK = false;

        rpcLog = nullptr;
    }

    bool execute,targz,uploads,overwrite;
    std::string softwareVersion;
    std::string httpDocumentRootDir;
    Mantids::Application::Logs::RPCLog * rpcLog;
    std::string user,pass;

    std::string uploadDirBase;
    std::string remoteIP;
    bool uploadOK;

};

class WebClientHdlr : public Mantids::Protocols::HTTP::HTTPv1_Server
{
public:
    WebClientHdlr(void *parent, StreamableObject *sock);
    ~WebClientHdlr() override;

    void setWebClientParameters(const webClientParams &newWebClientParameters);

protected:
    /**
     * @brief procHTTPClientHeaders Process Client Options (eg. check if it's a file uploader)
     * @return will return true to continue receiving the data, false to terminate the connection.
     */
    bool procHTTPClientHeaders() override;

    /**
     * @brief procHTTPClientContent Process web client request
     * @return http response code.
     */
    Mantids::Protocols::HTTP::Status::eRetCode procHTTPClientContent() override;

private:
    Mantids::Protocols::HTTP::Status::eRetCode respond(Mantids::Protocols::HTTP::Status::eRetCode ret , const std::string &uri);

    void createCSRFTokenCookie();
    bool checkCSRFTokenOnPOST();

    bool containOnlyAllowedChars(const std::string &str);
    void generateIndexOf(const sLocalRequestedFileInfo &fileInfo);
    void generateTarGz(const sLocalRequestedFileInfo &fileInfo);

    std::string getFancySize(size_t  size);
    webClientParams webClientParameters;


};



#endif // WEBCLIENTHANDLER_H
