#include "webclienthdlr.h"
#include <cx2_mem_vars/b_mmap.h>
#include <boost/filesystem.hpp>
#include <cx2_netp_http/streamencoder_url.h>
#include <cx2_hlp_functions/appexec.h>
#include <cx2_mem_vars/streamableprocess.h>


#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

using namespace CX2::Network::HTTP;
using namespace boost::filesystem;
using namespace CX2::Memory::Streams;

WebClientHdlr::WebClientHdlr(void *parent, CX2::Memory::Streams::Streamable *sock) : HTTPv1_Server(sock)
{
    execute = false;
    rpcLog = nullptr;
}

WebClientHdlr::~WebClientHdlr()
{
}


Response::StatusCode WebClientHdlr::processClientRequest()
{
    //std::string sRealRelativePath, sRealFullPath;
    sLocalRequestedFileInfo fileInfo;
    Response::StatusCode ret  = Response::StatusCode::S_404_NOT_FOUND;
    auto reqData = getRequestActiveObjects();

    if (!pass.empty())
    {
        if (reqData.AUTH_PASS != pass || reqData.AUTH_USER!= user)
        {
            getResponseDataStreamer()->writeString("401: Unauthorized.");
            rpcLog->log(CX2::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "fileServer", 65535, "R/401: %s",getRequestURI().c_str());

            *(getResponseActiveObjects().authenticate) = "Authentication Required";

            ret = Response::StatusCode::S_401_UNAUTHORIZED;
            return ret;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    if ((   getLocalFilePathFromURI2(resourcesLocalPath, &fileInfo, "",execute) ||
            getLocalFilePathFromURI2(resourcesLocalPath, &fileInfo, ".html",execute) ||
            getLocalFilePathFromURI2(resourcesLocalPath, &fileInfo, "index.html",execute) ||
            getLocalFilePathFromURI2(resourcesLocalPath, &fileInfo, "/index.html",execute)
            ) && !fileInfo.isDir)
    {
        // Evaluate...

        if (fileInfo.isExecutable)
        {
            // Execute...
            ret = Response::StatusCode::S_200_OK;

            std::string composedArgumentList;
            CX2::Helpers::AppSpawn * spawner = new CX2::Helpers::AppSpawn();
            spawner->setExec( fileInfo.sRealFullPath );
            auto * vars = getRequestVars(CX2::Network::HTTP::HTTP_VarSource::HTTP_VARS_GET);
            for ( uint32_t i=0; i<1024 && vars->getValue("a" + std::to_string(i)); i++)
            {
                spawner->addArgument(vars->getStringValue("a" + std::to_string(i)));
                composedArgumentList += (composedArgumentList.empty()?"": " ") + vars->getStringValue(std::string("a" + std::to_string(i)));
            }

            rpcLog->log(CX2::Application::Logs::LEVEL_DEBUG, remotePairAddress,"","", "", "fileServer", 2048, "R/D-EXEC,%03d: %s (args: %s)",Response::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealFullPath.c_str(), composedArgumentList.c_str());
            if (spawner->spawnProcess(true,false))
            {
                rpcLog->log(CX2::Application::Logs::LEVEL_INFO, remotePairAddress,"", "","",  "fileServer", 2048, "R/EXEC-OK,%03d: %s (args: %s)",Response::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealRelativePath.c_str(), composedArgumentList.c_str());
                CX2::Memory::Streams::StreamableProcess * streamableP = new CX2::Memory::Streams::StreamableProcess(spawner);
                this->setResponseDataStreamer(streamableP,true);
            }
            else
            {
                rpcLog->log(CX2::Application::Logs::LEVEL_WARN, remotePairAddress,"", "","",  "fileServer", 2048, "R/EXEC-FAIL,%03d: %s (args: %s)",Response::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealRelativePath.c_str(), composedArgumentList.c_str());
            }
        }
        else
        {
            //Serve as it comes...
            ret = Response::StatusCode::S_200_OK;
            rpcLog->log(CX2::Application::Logs::LEVEL_INFO, remotePairAddress,"", "","",  "fileServer", 2048, "R/%03d: %s",Response::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealRelativePath.c_str());
            rpcLog->log(CX2::Application::Logs::LEVEL_DEBUG, remotePairAddress,"","", "", "fileServer", 2048, "R/D-LOCAL,%03d: %s",Response::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealFullPath.c_str());
        }


    }
    else if (getLocalFilePathFromURI2(resourcesLocalPath, &fileInfo) && fileInfo.isDir)
    {
        if (fileInfo.sRealRelativePath == "") fileInfo.sRealRelativePath = "/";
        ret = Response::StatusCode::S_200_OK;

        *(getResponseActiveObjects().contentType) = "text/html";
        rpcLog->log(CX2::Application::Logs::LEVEL_INFO, remotePairAddress,"", "","",  "fileServer", 2048, "R/DIR,%03d: %s",Response::Status::getHTTPStatusCodeTranslation(ret),fileInfo.sRealRelativePath.c_str());

        //https://www.iconfinder.com/icons/314779/folder_icon
        //https://www.iconfinder.com/icons/315179/document_text_icon
        //https://www.iconfinder.com/icons/314895/document_text_icon

        getResponseDataStreamer()->strPrintf("<html>\n"
                                             "<head>\n"
                                             "    <title>Index of %s</title>\n"
                                             "    <style>\n"
                                             "        table,\n"
                                             "        th,\n"
                                             "        td {\n"
                                             "            border: 1px solid rgb(171, 171, 172);\n"
                                             "            border-collapse: collapse;\n"
                                             "            border-spacing: 0;\n"
                                             "            font: 10px Inconsolata, monospace, Courier New;\n"
                                             "            padding: 4px;\n"
                                             "        }\n"
                                             "        body {\n"
                                             "            background-color: black;\n"
                                             "            background-image: radial-gradient(rgba(0, 38, 87, 0.75), black 120%%);\n"
                                             "            height: 100vh;\n"
                                             "            color: white;\n"
                                             "            font: 10px Inconsolata, monospace, Courier New;\n"
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
                                             "    </style>\n"
                                             "</head>\n"
                                             "<body>\n"
                                             "    <h1>Index of %s</h1>\n"
                                             "    <hr><center><br><br>\n"
                                             "    <table align='center'>\n"
                                             "        <tr bgcolor='#0000B0'>\n"
                                             "            <td><b>FileName</b></td>\n"
                                             "            <td><b>Size</b></td>\n"
                                             "            <td><b>Modification Time</b></td>\n"
                                             "            <td><b>UID:GID</b></td>\n"
                                             "            <td width=20px><b>Permissions</b></td>\n"
                                             "        </tr>\n"
                                            , htmlEncode(fileInfo.sRealRelativePath).c_str(), htmlEncode(fileInfo.sRealRelativePath).c_str());

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
                            "            <td>%lu:%lu</td>\n"
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
                                               "</html>\n", softwareVersion.c_str());

    }
    else
    {
        getResponseDataStreamer()->writeString("404: Not Found.");
        rpcLog->log(CX2::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "fileServer", 65535, "R/404: %s",getRequestURI().c_str());
    }

    return Response::StatusCode::S_200_OK;
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

void WebClientHdlr::setSoftwareVersion(const std::string &newSoftwareVersion)
{
    softwareVersion = newSoftwareVersion;
}

void WebClientHdlr::setExecute(bool newExecute)
{
    execute = newExecute;
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
