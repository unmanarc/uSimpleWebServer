#include "webclienthdlr.h"
#include <mdz_prg_logs/loglevels.h>
#include <mdz_proto_http/rsp_status.h>
#include <boost/range/end.hpp>
#include <mdz_mem_vars/b_mmap.h>
#include <mdz_proto_http/streamencoder_url.h>
#include <mdz_hlp_functions/appexec.h>
#include <mdz_hlp_functions/random.h>
#include <mdz_mem_vars/streamableprocess.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include <mdz_mem_vars/streamablefile.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <fstream>

using namespace Mantids;
using namespace Protocols;
using namespace Protocols::HTTP;
using namespace Memory::Streams;

// TODO: HTML for uploads
// TODO: null stream por default

WebClientHdlr::WebClientHdlr(void *parent, StreamableObject *sock) : HTTPv1_Server(sock)
{
}

WebClientHdlr::~WebClientHdlr()
{
}

bool WebClientHdlr::checkCSRFTokenOnPOST()
{
    if ( clientRequest.requestLine.getRequestMethod() == "POST" )
    {
        // All POST should be checked with AntiCSRF token

        if (        clientRequest.headers.getOptionRawStringByName("X-CSRFToken")
                    !=  clientRequest.getCookie("CSRFToken"))
        {
            // All POST requests should come with anti-csrf token...
            webClientParameters.rpcLog->log(Application::Logs::LEVEL_ERR, clientVars.REMOTE_ADDR,"","", "", "fileServer", 65535, "R/POST-FAIL-CSRF: %s",clientRequest.getURI().c_str());
            // Ret false
            return false;
        }
    }
    // TOKEN OK.
    return true;
}

void cbFileTransferMIMEHeaderReady(void *obj, const std::string & partName, Mantids::Protocols::MIME::MIME_PartMessage *partMessage)
{
    webClientParams * webClientParameters = (webClientParams *)obj;
    auto fileName =partMessage->getHeader()->getOptionByName("Content-Disposition")->getSubVar("filename");
    HTTPv1_Server::sLocalRequestedFileInfo reqInfo;

    if (!access((webClientParameters->uploadDirBase + "/.dontupload").c_str(),F_OK))
    {
        webClientParameters->rpcLog->log(Application::Logs::LEVEL_ERR, webClientParameters->remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-FAIL-PERMS-DIR: %s", webClientParameters->uploadDirBase.c_str());
        return;
    }
    if (!access((webClientParameters->uploadDirBase + "/.uploadname").c_str(),R_OK))
    {
        // Access new upload name:
        std::ifstream file;
        std::string line;

        file.open((webClientParameters->uploadDirBase + "/.uploadname").c_str());
        if (file)
        {
            if (getline(file, line))
            {
                while (line.back() == '\n' || line.back() == '\r')
                    line.pop_back();
                fileName = line;
            }
            file.close();
        }
        else
            return;
    }

    // Check if file exist:
    if (HTTPv1_Server::getLocalFilePathFromURI0E( "/" + fileName , webClientParameters->uploadDirBase, &reqInfo ))
    {
        if ( boost::ends_with( boost::to_lower_copy(reqInfo.sRealFullPath), "/.dontupload")  ||
             boost::ends_with( boost::to_lower_copy(reqInfo.sRealFullPath), "/.uploadname") ||
             boost::ends_with( boost::to_lower_copy(reqInfo.sRealFullPath), "/.upload")
             )
        {
            webClientParameters->rpcLog->log(Application::Logs::LEVEL_CRITICAL, webClientParameters->remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-NICETRY: %s", reqInfo.sRealRelativePath.c_str());
            return;
        }

        if ( webClientParameters->overwrite )
        {
            // Allowed to overwrite...
            // if exist, and there is no transversal attack... overwrite...
            StreamableFile * file = new StreamableFile;
            if (file->open( reqInfo.sRealFullPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0700 )>=0)
            {
                partMessage->getContent()->replaceContentContainer(file);
                webClientParameters->rpcLog->log(Application::Logs::LEVEL_INFO, webClientParameters->remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-OVERW-OK: %s", reqInfo.sRealRelativePath.c_str());
                webClientParameters->uploadOK = true;
            }
            else
            {
                webClientParameters->rpcLog->log(Application::Logs::LEVEL_ERR, webClientParameters->remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-FAIL-PERMS1: %s",reqInfo.sRealRelativePath.c_str());
                delete file;
            }
        }
        else
        {
            webClientParameters->rpcLog->log(Application::Logs::LEVEL_ERR, webClientParameters->remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-FAIL-OVERWRITE: %s",reqInfo.sRealRelativePath.c_str());
        }
    }
    // TODO: URL Encoded file name (and filename field name)
    else if (HTTPv1_Server::getLocalFilePathFromURI0NE( "/" + fileName , webClientParameters->uploadDirBase, &reqInfo ))
    {
        if ( boost::ends_with( boost::to_lower_copy(reqInfo.sRealFullPath), "/.dontupload")  ||
             boost::ends_with( boost::to_lower_copy(reqInfo.sRealFullPath), "/.uploadname")
             )
        {
            webClientParameters->rpcLog->log(Application::Logs::LEVEL_CRITICAL, webClientParameters->remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-NICETRY: %s", reqInfo.sRealRelativePath.c_str());
            return;
        }

        // Yes or Not allowed to overwrite and file does not exist...
        StreamableFile * file = new StreamableFile;
        if (file->open( reqInfo.sRealFullPath.c_str(), O_WRONLY | O_CREAT, 0700 )>=0)
        {
            partMessage->getContent()->replaceContentContainer(file);
            webClientParameters->rpcLog->log(Application::Logs::LEVEL_INFO, webClientParameters->remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-CREATE-OK: %s", reqInfo.sRealRelativePath.c_str());
            webClientParameters->uploadOK = true;
        }
        else
        {
            webClientParameters->rpcLog->log(Application::Logs::LEVEL_ERR, webClientParameters->remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-FAIL-PERMS2: %s",reqInfo.sRealRelativePath.c_str());
            delete file;
        }
    }
    else if ( reqInfo.isTransversal )
    {
        webClientParameters->rpcLog->log(Application::Logs::LEVEL_CRITICAL, webClientParameters->remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-TRANSVERSAL2: %s", fileName.c_str());
    }
    else
    {
        webClientParameters->rpcLog->log(Application::Logs::LEVEL_ERR, webClientParameters->remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-FAIL: %s", fileName.c_str());
    }
}

bool WebClientHdlr::procHTTPClientHeaders()
{
    // Reset the callback.
    if (clientRequest.content.getContainerType() == Common::Content::CONTENT_TYPE_MIME)
        clientRequest.content.getMultiPartVars()->setCallbackOnHeaderReady( {nullptr, nullptr}  );

    if (!checkCSRFTokenOnPOST())
        return true;

    if ( clientRequest.requestLine.getRequestMethod() == "POST" )
    {
        // All POST should be checked with AntiCSRF token
        if (     boost::ends_with(clientRequest.requestLine.getURI(), "/.upload") &&
                 webClientParameters.uploads == true
                 )
        {
            // Reset:

            HTTPv1_Server::sLocalRequestedFileInfo reqInfo;

            std::string uri = clientRequest.requestLine.getURI();

            if (HTTPv1_Server::getLocalFilePathFromURI0E( uri, webClientParameters.httpDocumentRootDir, &reqInfo ))
            {
                // not allowed to upload here.
                webClientParameters.rpcLog->log(Application::Logs::LEVEL_ERR, clientVars.REMOTE_ADDR,"","", "", "fileServer", 65535, "R/UPLOAD-FAIL-NOTALLOWED: %s",clientRequest.getURI().c_str());
                return false;
            }

            uri.pop_back();//u
            uri.pop_back();//p
            uri.pop_back();//l
            uri.pop_back();//o
            uri.pop_back();//a
            uri.pop_back();//d

            webClientParameters.remoteIP = clientVars.REMOTE_ADDR;

            if (HTTPv1_Server::getLocalFilePathFromURI0E( uri, webClientParameters.httpDocumentRootDir, &reqInfo ))
            {
                if (reqInfo.isDir)
                {
                    webClientParameters.uploadDirBase = reqInfo.sRealFullPath;
                    clientRequest.content.getMultiPartVars()->setCallbackOnHeaderReady( {&cbFileTransferMIMEHeaderReady, &webClientParameters}  );
                    webClientParameters.rpcLog->log(Application::Logs::LEVEL_INFO, clientVars.REMOTE_ADDR,"","", "", "fileServer", 65535, "R/UPLOAD-REQ: %s",clientRequest.getURI().c_str());
                }
            }
            else
            {
                webClientParameters.rpcLog->log(Application::Logs::LEVEL_CRITICAL, webClientParameters.remoteIP,"","", "", "fileServer", 65535, "R/UPLOAD-TRANSVERSAL1: %s", uri.c_str());
                return false;
            }
        }
    }

    return true;
}


Mantids::Protocols::HTTP::Status::eRetCode WebClientHdlr::respond( Mantids::Protocols::HTTP::Status::eRetCode ret, const std::string &uri )
{
    webClientParameters.rpcLog->log(ret == HTTP::Status::S_200_OK? Application::Logs::LEVEL_INFO :  Application::Logs::LEVEL_WARN,
                                    clientVars.REMOTE_ADDR,"", "","",  "fileServer", 2048, "R/%03d:%s: %s",
                                    HTTP::Status::getHTTPStatusCodeTranslation(ret),
                                    clientRequest.requestLine.getRequestMethod().c_str(),
                                    uri.c_str());
    return ret;
}

void WebClientHdlr::createCSRFTokenCookie()
{
    if ( clientRequest.requestLine.getRequestMethod() == "GET" )
    {
        Headers::Cookie val;
        val.setValue( Helpers::Random::createRandomString(32) );

        // Secure if the communication is HTTPS... (important to avoid various mitm attacks to the CSRF)
        if (getIsSecure())
            val.setSecure(true);

        // Readable by the javascript:
        val.setHttpOnly(false);
        val.setPath("/");
        val.setExpiration(0);
        val.setSameSite(Headers::Cookie::HTTP_COOKIE_SAMESITE_STRICT);
        serverResponse.setCookie("CSRFToken",val);
    }
}


Status::eRetCode WebClientHdlr::procHTTPClientContent()
{
    //std::string sRealRelativePath, sRealFullPath;
    sLocalRequestedFileInfo fileInfo;
    HTTP::Status::eRetCode ret  = HTTP::Status::S_404_NOT_FOUND;

    if (!checkCSRFTokenOnPOST())
        return HTTP::Status::S_401_UNAUTHORIZED;

    if (!webClientParameters.pass.empty())
    {
        if (    !clientRequest.basicAuth.bEnabled ||
                clientRequest.basicAuth.pass != webClientParameters.pass ||
                clientRequest.basicAuth.user!= webClientParameters.user)
        {
            getResponseDataStreamer()->writeString("401: Unauthorized.");
            serverResponse.sWWWAuthenticateRealm = "Authentication Required";
            return respond( HTTP::Status::S_401_UNAUTHORIZED, clientRequest.getURI().c_str() );
        }
    }

    // Create CSRF Token on the cookie
    createCSRFTokenCookie();

    if (clientRequest.content.getContainerType() == Common::Content::CONTENT_TYPE_MIME)
    {
        if (     clientRequest.requestLine.getRequestMethod() == "POST" &&
                 boost::ends_with(clientRequest.requestLine.getURI(), "/.upload") &&
                 webClientParameters.uploads == true
                 )
        {
            std::string uri = clientRequest.requestLine.getURI();
            uri.pop_back();//u
            uri.pop_back();//p
            uri.pop_back();//l
            uri.pop_back();//o
            uri.pop_back();//a
            uri.pop_back();//d
            serverResponse.content.getStreamableObj()->writeString(webClientParameters.uploadOK?"true":"false");
            return respond( HTTP::Status::S_200_OK, clientRequest.getURI().c_str() );
        }
    }

    //////////////////////////////////////////////////////////////////////////
    if ((       getLocalFilePathFromURI2(webClientParameters.httpDocumentRootDir, &fileInfo, ".html",webClientParameters.execute) ||
                getLocalFilePathFromURI2(webClientParameters.httpDocumentRootDir, &fileInfo, "index.html",webClientParameters.execute) ||
                getLocalFilePathFromURI2(webClientParameters.httpDocumentRootDir, &fileInfo, "/index.html",webClientParameters.execute) ||
                getLocalFilePathFromURI2(webClientParameters.httpDocumentRootDir, &fileInfo, "",webClientParameters.execute)
                ) && !fileInfo.isDir)
    {
        // Evaluate...

        if (fileInfo.isExecutable)
        {
            if (clientRequest.requestLine.getRequestMethod() == "POST" || clientRequest.requestLine.getRequestMethod() == "GET")
            {
                // Execute...
                ret = HTTP::Status::S_200_OK;

                std::string composedArgumentList, composedEnviromentList;
                Helpers::AppSpawn * spawner = new Helpers::AppSpawn();
                spawner->setExec( fileInfo.sRealFullPath );

                spawner->addEnvironment(std::string("REMOTE_ADDR=") + clientVars.REMOTE_ADDR);
                spawner->addEnvironment(std::string("REQUEST_METHOD=") + clientRequest.requestLine.getRequestMethod());

                auto * postVars = clientRequest.getVars(HTTP_VarSource::HTTP_VARS_POST);
                for (const auto & key : postVars->getKeysList())
                {
                    // Security: Don't insert dangerous chars... (it should allow base64, emails, etc)
                    if (!containOnlyAllowedChars(key) || !containOnlyAllowedChars(postVars->getStringValue(key))) continue;

                    spawner->addEnvironment("POST_" + key + "=" + postVars->getStringValue(key));
                    composedEnviromentList += (composedEnviromentList.empty()?"": " ") + std::string("POST_") + key;
                }

                auto * getVars = clientRequest.getVars(HTTP_VarSource::HTTP_VARS_GET);
                for ( uint32_t i=1; i<1024 && getVars->getValue("a" + std::to_string(i)); i++)
                {
                    spawner->addArgument(getVars->getStringValue("a" + std::to_string(i)));
                    composedArgumentList += (composedArgumentList.empty()?"": " ") + getVars->getStringValue(std::string("a" + std::to_string(i)));
                }

                webClientParameters.rpcLog->log(Application::Logs::LEVEL_DEBUG, clientVars.REMOTE_ADDR,"","", "", "fileServer", 2048, "D/EXEC,%03d: %s (args: %s, env: %s)",
                                                HTTP::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealFullPath.c_str(), composedArgumentList.c_str(), composedEnviromentList.c_str());
                if (spawner->spawnProcess(true,false))
                {
                    webClientParameters.rpcLog->log(Application::Logs::LEVEL_INFO, clientVars.REMOTE_ADDR,"", "","",  "fileServer", 2048, "E/EXEC-OK,%03d: %s (args: %s, env: %s)",
                                                    HTTP::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealRelativePath.c_str(), composedArgumentList.c_str(), composedEnviromentList.c_str());
                    StreamableProcess * streamableP = new StreamableProcess(spawner);
                    serverResponse.setDataStreamer(streamableP,true);
                    setResponseContentTypeByFileExtension(fileInfo.sRealRelativePath);
                }
                else
                {
                    webClientParameters.rpcLog->log(Application::Logs::LEVEL_WARN, clientVars.REMOTE_ADDR,"", "","",  "fileServer", 2048, "E/EXEC-FAIL,%03d: %s (args: %s)",HTTP::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealRelativePath.c_str(), composedArgumentList.c_str());
                }
            }
        }
        else
        {
            //Serve as it comes...
            ret = HTTP::Status::S_200_OK;
            //            webClientParameters.rpcLog->log(Application::Logs::LEVEL_INFO, clientVars.REMOTE_ADDR,"", "","",  "fileServer", 2048, "R/%03d: %s",HTTP::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealRelativePath.c_str());
            webClientParameters.rpcLog->log(Application::Logs::LEVEL_DEBUG, clientVars.REMOTE_ADDR,"","", "", "fileServer", 2048, "D/LOCAL,%03d: %s",HTTP::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealFullPath.c_str());
        }


    }
    else if (getLocalFilePathFromURI2(webClientParameters.httpDocumentRootDir, &fileInfo) && fileInfo.isDir)
    {
        if (fileInfo.sRealRelativePath == "") fileInfo.sRealRelativePath = "/";
        ret = HTTP::Status::S_200_OK;

        auto * vars = clientRequest.getVars(HTTP_VarSource::HTTP_VARS_GET);

        if (vars->getStringValue("action") == "targz" && webClientParameters.targz)
        {
            webClientParameters.rpcLog->log(Application::Logs::LEVEL_INFO, clientVars.REMOTE_ADDR,"", "","",  "fileServer", 2048, "A/TARGZ-DIR,%03d: %s",HTTP::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealRelativePath.c_str());
            generateTarGz(fileInfo);
        }
        else
        {
            //webClientParameters.rpcLog->log(Application::Logs::LEVEL_INFO, clientVars.REMOTE_ADDR,"", "","",  "fileServer", 2048, "I/DIR,%03d: %s",HTTP::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealRelativePath.c_str());
            generateIndexOf(fileInfo);
        }
    }
    else
    {
        getResponseDataStreamer()->writeString("<center>404: Not Found.</center>");
    }

    if (fileInfo.sRealRelativePath.empty())
        return respond(ret,clientRequest.getURI());
    else
        return respond(ret,fileInfo.sRealRelativePath);
}

bool WebClientHdlr::containOnlyAllowedChars(const std::string & str)
{
    for (ssize_t i=0;i<str.size();i++)
    {
        if ( !(            (str.at(i)>='a' && str.at(i)<='z')
                           ||     (str.at(i)>='A' && str.at(i)<='Z')
                           ||     (str.at(i)>='0' && str.at(i)<='9')
                           ||      str.at(i)=='.' ||  str.at(i)==',' ||  str.at(i)=='!' ||  str.at(i)=='@' ||  str.at(i)=='_' ||  str.at(i)=='-' ||  str.at(i)=='+' ||  str.at(i)==':' ||  str.at(i)=='=' ||  str.at(i)=='/'
                           )
             )
        {
            return false;
        }
    }
    return true;

}
void WebClientHdlr::generateTarGz(const sLocalRequestedFileInfo &fileInfo)
{
    serverResponse.contentType = "application/octet-stream";

    std::string composedArgumentList;
    Helpers::AppSpawn * spawner = new Helpers::AppSpawn();

    spawner->setExec( "/usr/bin/tar" );
    spawner->addArgument( "czf" ); // Compress (gzip), and put in file format.
    spawner->addArgument( "-" ); // Output to stdout (here)
    spawner->addArgument( fileInfo.sRealFullPath ); // Our magic dir...

    if (spawner->spawnProcess(true,false))
    {
        StreamableProcess * streamableP = new StreamableProcess(spawner);
        serverResponse.setDataStreamer(streamableP,true);
    }
    else
    {
        webClientParameters.rpcLog->log(Application::Logs::LEVEL_WARN, clientVars.REMOTE_ADDR,"", "","",  "fileServer", 2048, "R/EXEC-FAIL: tar czvf");
    }
}

void WebClientHdlr::generateIndexOf(const sLocalRequestedFileInfo & fileInfo)
{
    serverResponse.contentType = "text/html";

    //https://www.iconfinder.com/icons/314779/folder_icon
    //https://www.iconfinder.com/icons/315179/document_text_icon
    //https://www.iconfinder.com/icons/314895/document_text_icon

    const char * _HTML_targz = "<tr class='tableupload'>"
                              "     <td class='tableupload' align=left> -> <a href=\"./?action=targz\">Download dir as tar.gz</a>  "
                              "     </td>"
                              "</tr>";

    const char * _HTML_upload =
            "    <form method='post' enctype='multipart/form-data'>\n"
            "        <table align='center' width='80%' class='tableupload'>\n"
            "            <tr width='100%' class='tableupload'>\n"
            "                <td align=left class='tableupload'>\n"
            "                    <label for='file'>Choose file to upload -> </label>\n"
            "                    <input type='file' id='file' name='file' multiple>\n"
            "                </td>\n"
            "                <td align=right class='tableupload'>\n"
            "                <input type='button' class='button' value='Upload' id='uploadbtn'>\n"
            "                </td>\n"
            "            </tr>\n"
            "        </table>\n"
            "    </form>\n";

    const char * _JS_upload =
            "<script src='/.usws/assets/js/jquery.min.js'></script>\n"
            "<script>\n"
            "// From: https://www.w3schools.com/js/js_cookies.asp\n"
            "function getCookie(cname) {\n"
            "  let name = cname + '=';\n"
            "  let decodedCookie = decodeURIComponent(document.cookie);\n"
            "  let ca = decodedCookie.split(';');\n"
            "  for(let i = 0; i <ca.length; i++) {\n"
            "    let c = ca[i];\n"
            "    while (c.charAt(0) == ' ') {\n"
            "      c = c.substring(1);\n"
            "    }\n"
            "    if (c.indexOf(name) == 0) {\n"
            "      return c.substring(name.length, c.length);\n"
            "    }\n"
            "  }\n"
            "  return '';\n"
            "}\n"
            "$(document).ready(function(){\n"
            "    $('#uploadbtn').click(function(){\n"
            "        var fd = new FormData();\n"
            "        var files = $('#file')[0].files;\n"
            "        \n"
            "        // Check file selected or not\n"
            "        if(files.length == 0 ){\n"
            "            alert('Please select a file.'); return;\n"
            "        }\n"
            "        for (var x = 0; x < files.length; x++) {\n"
            "            fd.append('file', files[x]);\n"
            "        }\n"
            "        $.ajax({\n"
            "            url: './.upload',\n"
            "            headers: { 'X-CSRFToken': getCookie('CSRFToken') },\n"
            "            type: 'post', data: fd, contentType: false, processData: false,\n"
            "            success: function(response){\n"
            "                if(response == 'true') { alert('File(s) uploaded'); location.reload(); }\n"
            "                else{ alert('File(s) not uploaded'); }\n"
            "            },\n"
            "        });\n"
            "    });\n"
            "});\n"
            "</script>\n";

    const char * _CSS_page =
            "<style>\n"
            "        table,\n"
            "        th,\n"
            "        td {\n"
            "            border: 1px solid rgb(171, 171, 172);\n"
            "            border-collapse: collapse;\n"
            "            border-spacing: 0;\n"
            "            font:  Inconsolata, monospace, Courier New;\n"
            "            padding: 4px;\n"
            "        }\n"
            "        .tableupload\n"
            "        {\n"
            "            border: 0px solid rgb(100, 100, 100);\n"
            "            border-collapse: collapse;\n"
            "            border-spacing: 0;\n"
            "            font: Inconsolata, monospace, Courier New;\n"
            "        }\n"
            "        body {\n"
            "            background-color: black;\n"
            "            background-image: radial-gradient(rgba(0, 38, 87, 0.75), black 120%);\n"
            "            height: 100vh;\n"
            "            color: white;\n"
            "            font: Inconsolata, monospace, Courier New;\n"
            "            zoom: 80%;\n"
            "        }\n"
            "        a:link {\n"
            "            color: rgb(255, 255, 255);\n"
            "        }\n"
            "        a:visited {\n"
            "            color: rgb(194, 194, 194);\n"
            "        }\n"
            "        a:hover {\n"
            "            color: rgb(199, 198, 255);\n"
            "        }\n"
            "        a:active {\n"
            "            color: rgb(34, 148, 255);\n"
            "        }\n"
            "       .button{\n"
            /*            "          border: 0px;\n"
                    "          background-color: rgb(0, 0, 176);\n"
                    "          color: white;\n"
                    "          margin-left: 10px;\n"
                    "          padding: 5px 12px;\n"*/
            "        }\n"
            "        .folder16 {\n"
            "        background-image: url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAAAXNSR0IArs4c6QAAAbJJREFUaEPtWTtOw0AQfROzoaDGfQ5ATQFllJwBTgISShZHQrkCB4ADQAMHgALuQAvKR3Far/CgREqUKIbYml0jR+vSnnkz771ZrdcmVPyiivcPT+C/HfQOeAeECqyNUNgxbTCOmBBIcE1t7zbWFEsw8uYuCYRXyT3X6Cxv4l9xBFwPIqVtYG3DmBOYKc/A07bg3M+ZYxOoRhkuzAkcdswlgJvcDeYILMuFhQOagW6OvgqE8DRJVGPap0mBpMKhDglgts07XwtOCYA5BlG/sKwZCcw8wbd6HN3Q5+pjtwRsdL6KwRynAdpjXX9b3K4WAQAp8DqO1EllCRBzMujV9ytLYNb4MFLLDbhyI5RNQJsmp2jaXnOu8IaRulgbIVeFysDdkRHqGAevEu7038FF7B1wNy5ZyH6EytV7s5p3wDsgVMCPkFBAcbp3QCyhEMA7IBRQnL7hgItPi+IufwHIPtRr0+IUz66K2sRl4GUUqdONI2XYTe6Y6dxmMftYPEkZ7XGv/p55Jg61aaUpjgk4sF9chsjMHxSoh6Gmr1Uk/5NPpqs82zsg11CG4B2Q6SfPrrwDP1yR2jE/Y55DAAAAAElFTkSuQmCC\");\n"
            "        background-repeat: no-repeat;\n"
            "        background-size: 16px 16px;\n"
            "        padding-left: 24px; padding-top: 2px; padding-bottom: 2px;\n"
            "        }\n"
            "        .file16 {\n"
            "        background-image: url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAABhGlDQ1BJQ0MgUFJPRklMRQAAKJF9kT1Iw0AcxV9TRdGKoh2KOASpThZERRy1CkWoEGqFVh1MLv2CJg1Jiouj4Fpw8GOx6uDirKuDqyAIfoC4uTkpukiJ/0sKLWI9OO7Hu3uPu3eAUC0yzWobBzTdNhOxqJhKr4odr+hGH/oRwrDMLGNOkuJoOb7u4ePrXYRntT735+hRMxYDfCLxLDNMm3iDeHrTNjjvEwdZXlaJz4nHTLog8SPXFY/fOOdcFnhm0Ewm5omDxGKuiZUmZnlTI54iDquaTvlCymOV8xZnrVhm9XvyFwYy+soy12kOIYZFLEGCCAVlFFCEjQitOikWErQfbeEfdP0SuRRyFcDIsYASNMiuH/wPfndrZScnvKRAFGh/cZyPEaBjF6hVHOf72HFqJ4D/GbjSG/5SFZj5JL3S0MJHQO82cHHd0JQ94HIHCD0Zsim7kp+mkM0C72f0TWlg4BboWvN6q+/j9AFIUlfxG+DgEBjNUfZ6i3d3Nvf275l6fz9uIXKlTWKFwgAAAAZiS0dEAP8A/wD/oL2nkwAAAbdJREFUaN7tmK1uFVEUhb9N+RHFoRAIkqIaqiAhgeDAAApXDekTEKrIoKAIIOEBMDdUNzwA5hIUoBAFR3JxVFVQEB9mMJObdC5zzrmQe5Y5yWSfn7X3rL3ODFRULDZi1gnqdWANWBq49ygiJkXZqiPT4YN6suThr7Ubb6rHBqzTtOv8UF+rgyp5ZIbYC+34NCJ+JcjJHeAG8GzIIkdniD0BEBE/k4gvYqSeAx6oXyLiRW4COdAAZ4Hn6teI2ClKQL3fM/RtRIynVEH1bktipF6NiI+5RNyodp71RXPIOqfUz+pEPVOsAhERifTwXb0FvAN22krsp+5CqXDQZv14h8QucBtYBbb7ttd5EHjfjvemkHgDbAA3h7bXbBpo573qM6lEG93qGTfuZHpdfQlcApanxF8BLmevQMZrS++95qGBpChlZH9tdrk18DghgYddrfw3RlbqOv1PYrEJmBbNPL4HthImc1ycQERsVg1UI6tGVjVQCVQjq0ZWjawaWTWy6gMLQeBbK9yVAuda+bPfoa/xDB3nNPAJ2AO2gf1Mhz8PrANPkvuMelEdqwfmw0R91P1zXVGRCb8BsbyZP/dGyOsAAAAASUVORK5CYII=\");\n"
            "        background-repeat: no-repeat;\n"
            "        background-size: 16px 16px;\n"
            "        padding-left: 24px; padding-top: 2px; padding-bottom: 2px;\n"
            "        }\n"
            "</style>\n";

    getResponseDataStreamer()->strPrintf("<html>\n"
                                         "<head>\n"
                                         "    <title>Index of %s</title>\n"
                                         "%s\n"
                                         "%s\n"
                                         "</head>\n"
                                         "<body>\n"
                                         "        <table align='center' width='80%%' class='tableupload'>\n"
                                         "          <tr class='tableupload'><td class='tableupload'><h1>Index of %s</h1></td></tr>\n"
                                         "          %s\n"
                                         "        </table>\n"
                                         "    %s\n"
                                         "    <hr><center><br><br>\n"
                                         "    <table align='center' width='80%%'>\n"
                                         "        <tr bgcolor='#0000B0'>\n"
                                         "            <td><b>FileName</b></td>\n"
                                         "            <td><b>Size</b></td>\n"
                                         "            <td><b>Modification Time</b></td>\n"
                                         "            <td><b>UID:GID</b></td>\n"
                                         "            <td width=20px><b>Permissions</b></td>\n"
                                         "        </tr>\n",
                                         htmlEncode(fileInfo.sRealRelativePath).c_str(),
                                         _CSS_page,_JS_upload,
                                         htmlEncode(fileInfo.sRealRelativePath).c_str(),
                                         webClientParameters.targz? _HTML_targz :"",
                                         webClientParameters.uploads?_HTML_upload:"");

    DIR *dirp;
    struct dirent *dp;

    if ((dirp = opendir(fileInfo.sRealFullPath.c_str())) == NULL)
    {
        getResponseDataStreamer()->writeString("    </table>\n"
                                               "<h2>Folder not accessible</h2>");
    }
    else
    {
        while ((dp = readdir(dirp)) != NULL)
        {
            struct stat64 stats;
            stat64(       std::string(fileInfo.sRealFullPath + std::string("/") + dp->d_name).c_str()
                          , &stats);


            time_t t = stats.st_mtime;
            struct tm lt;
            localtime_r(&t, &lt);
            char timebuf_mtime[128];
            strftime(timebuf_mtime, sizeof(timebuf_mtime), "%c", &lt);


            int statchmod = stats.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
            char chmodval[128];

            if (    (stats.st_mode & S_IRWXO) == 7 ||
                    (stats.st_mode & S_IRWXO) == 6
                    )
            {
                snprintf(chmodval,sizeof(chmodval),"<font color=red>%o*</font>\n", statchmod);
            }
            else
            {
                snprintf(chmodval,sizeof(chmodval),"%o\n", statchmod);
            }

            getResponseDataStreamer()->strPrintf(
                        "        <tr>\n"
                        "            <td>%s<a href='%s'>%s</a>%s</td>\n"
                        "            <td>%s</td>\n"
                        "            <td>%s</td>\n"
                        "            <td>%" PRIi32 ":%" PRIi32 "</td>\n"
                                                               "            <td>%s</td>\n"
                                                               "        </tr>\n",
                        S_ISDIR(stats.st_mode)?"<div class=folder16>[":"<div class=file16>",
                        (Encoders::URL::encodeURLStr(dp->d_name) + (S_ISDIR(stats.st_mode)?"/":"")).c_str(),
                        htmlEncode(dp->d_name).c_str(),
                        S_ISDIR(stats.st_mode)?"]</div>":"</div>",
                        getFancySize(stats.st_size).c_str(),
                        timebuf_mtime,
                        stats.st_uid,
                        stats.st_gid,
                        chmodval
                        );
        }
        closedir(dirp);
        getResponseDataStreamer()->writeString("    </table>\n");
    }



    getResponseDataStreamer()->strPrintf("    </center><br><br><br><hr>\n"
                                         "    <center>\n"
                                         "        Provided by <a href='https://github.com/unmanarc/uSimpleWebServer'>Unmanarc HTTP Simple Web Server v%s</a>\n"
                                         "    </center>\n"
                                         "</body>\n"
                                         "</html>\n", webClientParameters.softwareVersion.c_str());

}



std::string WebClientHdlr::getFancySize(size_t value)
{
    const char *szNames[] = { "B", "kB", "MB", "GB", "TB" };

    size_t pos = 0, remainder = 0;

    for (; value >= 1024 && pos < (sizeof szNames / sizeof *szNames); value /= 1024)
    {
        remainder = (value % 1024);
        pos++;
    }

    char output[64];

    if (remainder)
        sprintf(output, "%.1f%s\n", (double)value + ((double)remainder / (double)1024), szNames[pos]);
    else
        sprintf(output, "%zu%s\n", value, szNames[pos]);
    return std::string(output);
}

void WebClientHdlr::setWebClientParameters(const webClientParams &newWebClientParameters)
{
    webClientParameters = newWebClientParameters;
}

