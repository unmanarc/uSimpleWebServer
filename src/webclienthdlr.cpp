#include "webclienthdlr.h"
#include <cx2_mem_vars/b_mmap.h>
#include <boost/filesystem.hpp>
#include <cx2_netp_http/streamencoder_url.h>

using namespace CX2::Network::HTTP;
using namespace boost::filesystem;
 using namespace CX2::Memory::Streams;
WebClientHdlr::WebClientHdlr(void *parent, CX2::Memory::Streams::Streamable *sock) : HTTPv1_Server(sock)
{
}

WebClientHdlr::~WebClientHdlr()
{
}


eHTTP_RetCode WebClientHdlr::processClientRequest()
{
    std::string sRealRelativePath, sRealFullPath;
    eHTTP_RetCode ret  = HTTP_RET_404_NOT_FOUND;
    auto reqData = requestData();

    if (!pass.empty())
    {
        if (reqData.AUTH_PASS != pass || reqData.AUTH_USER!= user)
        {
            getResponseDataStreamer()->writeString("401: Unauthorized.");
            rpcLog->log(CX2::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "fileServer", 65535, "R/401: %s",getRequestURI().c_str());

            *(responseData().authenticate) = "Authentication Required";

            ret = HTTP_RET_401_UNAUTHORIZED;
            return ret;
        }
    }

    //////////////////////////////////////////////////////////////////////////

    bool isDir = false;

    if ( getLocalFilePathFromURI(resourcesLocalPath, &sRealRelativePath, &sRealFullPath, "") ||
         getLocalFilePathFromURI(resourcesLocalPath, &sRealRelativePath, &sRealFullPath, ".html") ||
         getLocalFilePathFromURI(resourcesLocalPath, &sRealRelativePath, &sRealFullPath, "index.html") ||
         getLocalFilePathFromURI(resourcesLocalPath, &sRealRelativePath, &sRealFullPath, "/index.html")
         )
    {
        // Evaluate...
        ret = HTTP_RET_200_OK;
        rpcLog->log(CX2::Application::Logs::LEVEL_INFO, remotePairAddress,"", "","",  "fileServer", 2048, "R/%03d: %s",HTTP_Status::getHTTPRetCodeValue(ret),sRealRelativePath.c_str());
        rpcLog->log(CX2::Application::Logs::LEVEL_DEBUG, remotePairAddress,"","", "", "fileServer", 2048, "R LOCAL/%03d: %s",HTTP_Status::getHTTPRetCodeValue(ret),sRealFullPath.c_str());
    }
    else if (getLocalFilePathFromURI(resourcesLocalPath, &sRealRelativePath, &sRealFullPath, "", &isDir) && isDir)
    {
        if (sRealRelativePath == "") sRealRelativePath = "/";
        ret = HTTP_RET_200_OK;

        *(responseData().contentType) = "text/html";

        getResponseDataStreamer()->writeString("<html>");
        getResponseDataStreamer()->writeString("<body>");
        getResponseDataStreamer()->writeString("<h1>Index of "  + htmlEncode(sRealRelativePath)  + "</h1>\n\n");

        rpcLog->log(CX2::Application::Logs::LEVEL_INFO, remotePairAddress,"", "","",  "fileServer", 2048, "D/%03d: %s",HTTP_Status::getHTTPRetCodeValue(ret),sRealRelativePath.c_str());
        path p (sRealFullPath);

        getResponseDataStreamer()->writeString( std::string("[<a href='..'>..</a>]<br><br>\n"));

        directory_iterator it{p};
          while (it != directory_iterator{})
          {
              // TODO: checkar que el URL encoding no deje los caracteres HTML por fuera...
              if (boost::filesystem::is_directory(it->path()))
              {
                    getResponseDataStreamer()->writeString( "[<a href=\"" + Encoders::URL::encodeURLStr(it->path().filename().c_str()) + "/\">" + htmlEncode(it->path().filename().c_str()) + "</a>]<br>\n");
              }
              else if (boost::filesystem::is_regular_file(it->path()))
              {
                  getResponseDataStreamer()->writeString("- <a href=\"" + Encoders::URL::encodeURLStr(it->path().filename().c_str())+ "\">" + htmlEncode(it->path().filename().c_str()) + "</a><br>\n");
              }
              else
              {
                  getResponseDataStreamer()->writeString( "<a href=\"" + Encoders::URL::encodeURLStr(it->path().filename().c_str()) + "\">" + htmlEncode(it->path().filename().c_str()) + "</a><br>\n");
              }
              it++;
          }
          getResponseDataStreamer()->writeString("</body>");
          getResponseDataStreamer()->writeString("</html>");
    }
    else
    {
        getResponseDataStreamer()->writeString("404: Not Found.");
        rpcLog->log(CX2::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "fileServer", 65535, "R/404: %s",getRequestURI().c_str());
    }

    return HTTP_RET_200_OK;
}

void WebClientHdlr::setUser(const std::string &newUser)
{
    user = newUser;
}

void WebClientHdlr::setPass(const std::string &newPass)
{
    pass = newPass;
}


CX2::Application::Logs::RPCLog *WebClientHdlr::getRpcLog() const
{
    return rpcLog;
}

void WebClientHdlr::setRpcLog(CX2::Application::Logs::RPCLog *newRpcLog)
{
    rpcLog = newRpcLog;
}

const std::string &WebClientHdlr::getResourcesLocalPath() const
{
    return resourcesLocalPath;
}

void WebClientHdlr::setResourcesLocalPath(const std::string &newResourcesLocalPath)
{
    resourcesLocalPath = newResourcesLocalPath;
}
