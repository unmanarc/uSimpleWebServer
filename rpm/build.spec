%define name uSimpleWebServer
%define version 1.1.5
%define build_timestamp %{lua: print(os.date("%Y%m%d"))}

Name:           %{name}
Version:        %{version}
Release:        %{build_timestamp}.git%{?dist}
Summary:        Unmanarc Simple Web Server
License:        AGPL
URL:            https://github.com/unmanarc/uSimpleWebServer
Source0:        https://github.com/unmanarc/uSimpleWebServer/archive/master.tar.gz#/%{name}-%{version}-%{build_timestamp}.tar.gz
Group:          Applications/Internet

%define cmake cmake

%if 0%{?rhel} == 6
%define cmake cmake3
%endif

%if 0%{?rhel} == 7
%define cmake cmake3
%endif

%if 0%{?rhel} == 8
%define debug_package %{nil}
%endif

%if 0%{?rhel} == 9
%define debug_package %{nil}
%endif

%if 0%{?fedora} >= 33
%define debug_package %{nil}
%endif

BuildRequires: libMantids-devel >= 2.8.19
BuildRequires:  %{cmake} zlib-devel boost-devel gcc-c++
%if 0%{?rhel} == 7
BuildRequires:  openssl11-devel
%else
BuildRequires:  openssl-devel
%endif

Requires: libMantids >= 2.8.19
Requires: zlib boost-regex
%if 0%{?rhel} == 7
Requires:       openssl11
%else
Requires:       openssl
%endif

%description
This package contains a very efficient and simple web server that can be called from the command line

%prep
%autosetup -n %{name}-master

%build

%if 0%{?rhel} == 7
%{cmake} -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_BUILD_TYPE=MinSizeRel -DWITH_SSL_SUPPORT=ON -DSSLRHEL7=ON
%else
%{cmake} -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_BUILD_TYPE=MinSizeRel -DWITH_SSL_SUPPORT=ON
%endif

make %{?_smp_mflags}

%clean
rm -rf $RPM_BUILD_ROOT

%install
rm -rf $RPM_BUILD_ROOT

%if 0%{?fedora} >= 33
ln -s . %{_host}
%endif

%if 0%{?rhel} >= 9
ln -s . %{_host}
ln -s . redhat-linux-build
%endif

%if 0%{?fedora} == 35
ln -s . redhat-linux-build
%endif

%if "%{_host}" == "powerpc64le-redhat-linux-gnu"
ln -s . ppc64le-redhat-linux-gnu
%endif

%if "%{_host}" == "s390x-ibm-linux-gnu"
ln -s . s390x-redhat-linux-gnu
%endif

%if "%{cmake}" == "cmake3"
%cmake3_install
%else
%cmake_install
%endif

%files
%doc
%{_bindir}/uSimpleWebServer

%changelog
