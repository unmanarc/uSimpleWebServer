# uSimpleWebServer 

Unmanarc Simple HTTP Web Server Swiss knife
  
Author: Aaron Mizrachi (unmanarc) <aaron@unmanarc.com>   
Main License: AGPL

***
## Project Description

Simple command line web server that comes with:

- Password Protected Authentication
- TLS (https) Support
- On-the-fly tar.gz generation
- Command/Scripts execution (like cgi-bin)

### Use Cases:

This program is useful for:

- Creating Backups (tar.gz) of folders on-the-fly
- Serving files via HTTP efficiently

It can be built in "portable mode", and introduced as a very lightweight binary  

***
## Building/Installing uSimpleWebServer

### Building Instructions:

This guide is optimized for centos7 (you can adapt this for your OS), and the generated binary should be compatible with other neewer distributions...

First, as prerequisite, you must have installed libMantids (as static libs, and better if MinSizeRel)

and then...

```
git clone https://github.com/unmanarc/uSimpleWebServer
cd uSimpleWebServer
cmake3 . -DPORTABLE=ON -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX:PATH=/opt/osslibs -DEXTRAPREFIX:PATH=/opt/osslibs -B../uSimpleWebServer-build
cd ../uSimpleWebServer-build
make -j4 install
```

And if you want to reduce the binary size:

```
strip -x -X -s /opt/osslibs/bin/uSimpleWebServer
upx -9 --ultra-brute /opt/osslibs/bin/uSimpleWebServer
```

The resulting expected binary size should be: **~200K** without https support (yes, it's still a dinosaur, but at least it will fit in your 5 1/4 floppy disk)


### Running Instructions:

Program help:

```
$ /opt/osslibs/bin/uSimpleWebServer -h
# Unmanarc's HTTP Server (uSimpleWebServer) v1.0.3 alpha
# Author:  Aarón Mizrachi (aaron@unmanarc.com)
# License: AGPL
# 

Help:
-----

HTTP Options:
-------------
-r --rootdir <value> : HTTP Document Root Directory (default: .)
-l --lport   <value> : Local HTTP Port (default: 8001)
-4 --ipv4            : Use only IPv4 (default: true)
-a --laddr   <value> : Listen Address (default: *)
-t --threads <value> : Max Concurrent Connections (Threads) (default: 1024)

HTTP Security:
--------------
-p --passfile <value> : HTTP User/Pass File (default: )

Other Options:
--------------
-v --verbose <value> : Set verbosity level. (default: 0)
-h --help            : Show information usage. (default: false)
-s --sys             : Journalctl Log Mode (don't print colors or dates) (default: false)

Server Options:
---------------
-x --execute         : Execute any eXecutable marked file and get the output, ATT: disable targz if you want to keep your script sources private (default: false)
-g --targz           : Allow to get server directories as tar.gz files (on-the-fly) (default: false)

Service Options:
----------------
-d --daemon         : Run as daemon. (default: false)
```

**Security Considerations:**

1. HTTP Communication can be eavesdropped or tampered, if you set up an HTTP web server, at least use a layer of encryption like a VPN, TOR or something...
2. If you want to enable the execution mode (--execute), please don't activate the targz mode, because the binary folder can be retrieved trough this method, exposing all the executable codes (moreover if you store database connection passwords in your binaries). 
3. When execution mode is enabled, every single executable file or file marked as executable will be executed when called. Please double check what executables are behind the HTTP Document Root directory, validate the input and check that the input will not trigger command execution injections in other applications.
4. It's not recommended to run this as root, specially with execution mode enabled.
5. For passfile, you should create a file where the first line is the user, and the second line is the password.

**For HTTPS Only:**  
1. Remember to set up private signed and trusted X.509 Certificates, if not, the communication can be eavesdropped or tampered!!!



### Service Creation Instructions:

- Create the services...
- Restart daemon
```
cat << 'EOF' | install -m 640 /dev/stdin /usr/lib/systemd/system/uSimpleWebServer.service
[Unit]
Description=Unmanarc Simple HTTP Web Server
After=network.target

[Service]
Type=simple
Restart=always
RestartSec=1
ExecStart=/opt/osslibs/bin/uSimpleWebServer -s -r /isos
#User=
#Group=

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable --now uSimpleWebServer.service
```

`Don't forget to change the HTTP Document Root directory (eg. /isos) and change the User=/Group= for your specific user/group combination`


***
## Compatibility

This program was tested so far in:

* Fedora Linux 34
* Ubuntu 20.04 LTS (Server)
* CentOS/RHEL 7/8

### Overall Pre-requisites:

* libMantids (in static mode)
* cmake3
* C++11 Compatible Compiler (like GCC >=5)