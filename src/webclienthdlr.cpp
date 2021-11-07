#include "webclienthdlr.h"
#include <cx2_mem_vars/b_mmap.h>
#include <boost/filesystem.hpp>

using namespace CX2::Network::HTTP;
using namespace boost::filesystem;

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
//        rpcLog->log(CX2::Application::Logs::LEVEL_DEBUG, remotePairAddress,"","", "", "fileServer", 2048, "R LOCAL/%03d: %s",HTTP_Status::getHTTPRetCodeValue(ret),sRealFullPath.c_str());
    }
    else if (getLocalFilePathFromURI(resourcesLocalPath, &sRealRelativePath, &sRealFullPath, "", &isDir) && isDir)
    {
        if (sRealRelativePath == "") sRealRelativePath = "/";

        getResponseDataStreamer()->writeString("Directory list for "  + sRealRelativePath + "\n\n");
        rpcLog->log(CX2::Application::Logs::LEVEL_INFO, remotePairAddress,"", "","",  "fileServer", 2048, "D/%03d: %s",HTTP_Status::getHTTPRetCodeValue(ret),sRealRelativePath.c_str());
        path p (sRealFullPath);

        getResponseDataStreamer()->writeString( std::string("[..]\n"));

        directory_iterator it{p};
          while (it != directory_iterator{})
          {
              if (boost::filesystem::is_directory(it->path()))
              {
                    getResponseDataStreamer()->writeString( std::string("[") + it->path().filename().c_str() + std::string("]\n"));
              }
              else if (boost::filesystem::is_regular_file(it->path()))
              {
                  getResponseDataStreamer()->writeString(std::string("- ") +it->path().filename().c_str() + std::string("\n"));
              }
              else
              {
                  getResponseDataStreamer()->writeString( std::string("<") + it->path().filename().c_str() + std::string(">\n"));
              }
              it++;
          }
    }
    else
    {
        getResponseDataStreamer()->writeString("404: Not Found.");
        rpcLog->log(CX2::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "fileServer", 65535, "R/404: %s",getRequestURI().c_str());
    }

    return HTTP_RET_200_OK;
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
