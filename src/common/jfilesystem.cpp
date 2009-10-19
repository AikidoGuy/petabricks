/***************************************************************************
 *   Copyright (C) 2006-2009 by Jason Ansel                                *
 *   jansel@csail.mit.edu                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "jfilesystem.h"

#include "jconvert.h"

#include <dirent.h>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

namespace
{
  std::string _GetProgramExe()
  {
    std::string exe = "/proc/" + jalib::XToString ( getpid() ) + "/exe";
    std::string exeRes = jalib::Filesystem::ResolveSymlink ( exe );
    JASSERT ( exe != exeRes ) ( exe ).Text ( "problem with /proc/self/exe" );
    return exeRes;
  }

  std::string _FileBaseName ( const std::string& str )
  {
    int lastSlash = 0;
    for ( size_t i = 0; i<str.length(); ++i )
      if ( str[i] == '/' )
        lastSlash = i;
    return str.substr ( lastSlash+1 );
  }
}

std::string jalib::Filesystem::GetProgramDir()
{
  static std::string value = Dirname( GetProgramPath() );
  return value;
}

std::string jalib::Filesystem::GetProgramName()
{
  static std::string value = _FileBaseName ( GetProgramPath() );
  return value;
}

std::string jalib::Filesystem::GetProgramPath()
{
  static std::string value = _GetProgramExe();
  return value;
}


std::string jalib::Filesystem::ResolveSymlink ( const std::string& path )
{
  char buf [1024];
  memset ( buf,0,sizeof ( buf ) );
  int len = readlink ( path.c_str(), buf, sizeof ( buf )-1 );
  if ( len <= 0 )
    return "";
  return buf;
}

bool jalib::Filesystem::FileExists ( const std::string& str )
{
  FILE* fp = fopen ( str.c_str(),"r" );
  if ( fp != NULL ) fclose ( fp );
  return fp != NULL;
}

#define FHU_TRY_DIR(expr) {\
    std::string pth = expr; \
    if(FileExists(pth)) \
        return pth;}

std::string jalib::Filesystem::FindHelperUtility ( const std::string& file, bool dieOnError /*= true*/ )
{
  std::string progDir = GetProgramDir();
  const char* prefixes[] = {
    getenv("JALIB_UTILITY_DIR"),
    progDir.c_str(),
#ifdef PACKAGE
    "/usr/lib/" PACKAGE,
    "/usr/share/" PACKAGE,
#endif
    "."
  };
  for(size_t i=0; i<sizeof(prefixes)/sizeof(const char*); ++i){
    if(prefixes[i]==NULL || *prefixes[i]==0)
      continue;
    std::string pfx = prefixes[i];
    FHU_TRY_DIR ( pfx + "/" + file );
#ifdef UTIL_SEARCH_DIR_A
    FHU_TRY_DIR ( pfx + "/"UTIL_SEARCH_DIR_A"/" + file );
    FHU_TRY_DIR ( pfx + "/../"UTIL_SEARCH_DIR_A"/" + file );
#endif
#ifdef UTIL_SEARCH_DIR_B
    FHU_TRY_DIR ( pfx + "/"UTIL_SEARCH_DIR_B"/" + file );
    FHU_TRY_DIR ( pfx + "/../"UTIL_SEARCH_DIR_B"/" + file );
#endif
#ifdef UTIL_SEARCH_DIR_C
    FHU_TRY_DIR ( pfx + "/"UTIL_SEARCH_DIR_C"/" + file );
    FHU_TRY_DIR ( pfx + "/../"UTIL_SEARCH_DIR_C"/" + file );
#endif
#ifdef UTIL_SEARCH_DIR_D
    FHU_TRY_DIR ( pfx + "/"UTIL_SEARCH_DIR_D"/" + file );
    FHU_TRY_DIR ( pfx + "/../"UTIL_SEARCH_DIR_D"/" + file );
#endif
    FHU_TRY_DIR ( pfx + "/../" + file );
  }
  FHU_TRY_DIR ( "/usr/bin/" + file );
  FHU_TRY_DIR ( "/bin/" + file );
  JASSERT ( !dieOnError ) ( file ) ( GetProgramDir() )
    .Text ( "failed to find needed file" );
  return file;
}


std::vector<std::string> jalib::Filesystem::GetProgramArgs()
{
  std::string path = "/proc/" + jalib::XToString ( getpid() ) + "/cmdline";
  std::ifstream fp(path.c_str());
  JASSERT ( fp.is_open() ) ( path ).Text ( "failed to open command line" );
  std::string line;

  std::vector<std::string> rv;
  while(getline(fp, line, '\0'))
  {
    rv.push_back ( line );
  }
  return rv;
}

std::vector<int> jalib::Filesystem::ListOpenFds()
{
  std::string dir = "/proc/" + XToString ( getpid() ) + "/fd";
  std::vector<int> rv;
  struct dirent **namelist;
  char* p;
  int nents = scandir ( dir.c_str(), &namelist, NULL, versionsort );
  JASSERT ( nents >= 0 ) ( dir ) ( JASSERT_ERRNO ).Text ( "failed to open directory" );

  for ( int i = 0; i < nents; i ++ )
  {
    struct dirent * de = namelist[i];
    int fdnum = strtol ( de -> d_name, &p, 10 );
    if ( *p == 0 && fdnum >= 0 )
    {
      rv.push_back ( fdnum );
    }
    free ( de );
  }
  free ( namelist );

  return rv;
}


std::string jalib::Filesystem::GetCurrentHostname()
{
  struct utsname tmp;
  memset ( &tmp,0,sizeof ( tmp ) );
  uname ( &tmp );
  std::string name = "unknown";
  if ( tmp.nodename != 0 )
    name = tmp.nodename;
//   #ifdef _GNU_SOURCE
//   if(tmp.domainname != 0)
//     name += std::string(".") + tmp.domainname;
//   #endif
  return name;
}

std::string jalib::Filesystem::Basename( const std::string& s) {
  std::string::const_iterator e = s.end();
  for(std::string::const_iterator i=s.begin(); i!=s.end(); ++i){
    if(*i=='.')
      e=i;
  }
  return std::string(s.begin(), e);
}

std::string jalib::Filesystem::Dirname( const std::string& str) {
  int lastSlash = 0;
  for ( size_t i = 0; i<str.length(); ++i )
    if ( str[i] == '/' )
      lastSlash = i;
  return str.substr ( 0,lastSlash );
}

