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

#include "jassert.h"
#include "jconvert.h"
#include "jasm.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#else 
# undef HAVE_BACKTRACE
# undef HAVE_BACKTRACE_SYMBOLS
# undef HAVE_BACKTRACE_SYMBOLS_FD
#endif

#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#undef JASSERT_CONT_A
#undef JASSERT_CONT_B

#if defined(HAVE_CXXABI_H) && defined(HAVE_BACKTRACE_SYMBOLS) && defined(DEBUG)
#include <cxxabi.h>

static const char* _cxxdemangle(const char* i){
  static char buf[1024];
  memset(buf, 0, sizeof(buf));
  const char* end = buf+sizeof(buf)-1;
  char* o = buf;
  char* start = NULL;

  while(*i!=0 && o<end){
    if( (*o++=*i++) == '(' ){
      start = o;   
      break;
    }
  }
  while(*i!=0 && o<end){
    if( *i == ')' || *i == '+' ){
      int status;
      char* tmp = abi::__cxa_demangle(start, 0, 0, &status);
      if(tmp!=NULL){
        o=start;
        for(const char* t=tmp; *t!=0 && o<end;)
          *o++=*t++;  
        memset(o, 0, end-o);
        free(tmp);
      }
      break;
    }
    *o++=*i++;
  }
  while(*i!=0 && o<end){
    *o++=*i++;
  }
  return buf; 
}

#else
#define _cxxdemangle(x) x
#endif

jassert_internal::JAssert& jassert_internal::JAssert::Text ( const char* msg )
{
  Prefix();
  Print ( "Message: " );
  Print ( msg );
  Print ( "\n" );
  return *this;
}

jassert_internal::JAssert::JAssert ( bool exitWhenDone )
    : JASSERT_CONT_A ( *this )
    , JASSERT_CONT_B ( *this )
    , _exitWhenDone ( exitWhenDone )
{
  static jalib::AtomicT next=-1;
  _id=jalib::atomicIncrementReturn(&next);
}

jassert_internal::JAssert& jassert_internal::JAssert::SetContext( 
    const char* type,
    const char* reason,
    const char* file,
    const char* line,
    const char* func,
    const jalib::SrcPosTaggable* srcpos)
{
  Prefix();
  Print("From process "); 
  Print(getpid()); 
  EndLine();

  Prefix();
  Print("In function "); 
  Print(func);
  Print(" at ");
  Print(jassert_basename(file)); 
  Print(':'); 
  Print(line);
  EndLine();

#ifdef JASSERT_USE_SRCPOS
  if(srcpos!=0){
    Prefix();
    Print("Source ");
    Print(srcpos->srcPos());
    EndLine();
  }
#endif

  if(errno!=0){
    Prefix();
    Print("errno ");
    Print(errno);
    Print(": ");
    Print(JASSERT_ERRNO);
    EndLine();
  }

  Prefix();
  Print(type);
  Print(": ");
  Print(reason);
  EndLine();
  return *this;
}

jassert_internal::JAssert& jassert_internal::JAssert::Prefix(){
  Print('[');
  Print(_id);
  Print("] ");
  return *this;
}

jassert_internal::JAssert& jassert_internal::JAssert::VarName(const char* n){
  Prefix();
  Print("   "); 
  Print(n);
  Print(" = ");
  return *this;
}

jassert_internal::JAssert::~JAssert()
{
  if ( _exitWhenDone )
  {
#if defined(DEBUG) && defined(HAVE_BACKTRACE_SYMBOLS)
    void *addresses[10];
    int size = backtrace(addresses, 10);
    char **strings = backtrace_symbols(addresses, size);
    if(strings!=NULL){
      Print( "Stack trace:\n" );
      for(int i = 1; i < size; i++){
        Print("  "); 
        Print(i); 
        Print(": ");
        Print(_cxxdemangle(strings[i]));
        Print("\n");
      }
      free(strings);
    }
#endif
    Print ( "Terminating...\n" );
#ifdef DEBUG
    jalib::Breakpoint();
#endif
    _exit ( 1 );
  }
}

const char* jassert_internal::jassert_basename ( const char* str )
{
  for ( const char* c = str; c[0] != '\0' && c[1] !='\0' ; ++c )
    if ( c[0]=='/' )
      str=c+1;
  return str;
}

#ifndef JASSERT_FAST
static const int DUP_STDERR_FD = 826;
static const int DUP_LOG_FD    = 827;


static FILE* _fopen_log_safe ( const char* filename, int protectedFd )
{
  //open file
  int tfd = open ( filename, O_WRONLY | O_APPEND | O_CREAT /*| O_SYNC*/, S_IRUSR | S_IWUSR );
  if ( tfd < 0 ) return NULL;
  //change fd to 211
  int nfd = dup2 ( tfd, protectedFd );
  close ( tfd );
  if ( nfd < 0 ) return NULL;
  //promote it to a stream
  return fdopen ( nfd,"w" );
}
static FILE* _fopen_log_safe ( const std::string& s, int protectedFd )
{ 
  return _fopen_log_safe ( s.c_str(), protectedFd ); 
}

static FILE* theLogFile = NULL;

static std::string& theLogFilePath() {static std::string s;return s;};

void jassert_internal::set_log_file ( const std::string& path )
{
  theLogFilePath() = path;
  if ( theLogFile != NULL ) fclose ( theLogFile );
  theLogFile = NULL;
  if ( path.length() > 0 )
  {
    theLogFile = _fopen_log_safe ( path, DUP_LOG_FD );
    if ( theLogFile == NULL )
      theLogFile = _fopen_log_safe ( path + "_2",DUP_LOG_FD );
    if ( theLogFile == NULL )
      theLogFile = _fopen_log_safe ( path + "_3",DUP_LOG_FD );
    if ( theLogFile == NULL )
      theLogFile = _fopen_log_safe ( path + "_4",DUP_LOG_FD );
    if ( theLogFile == NULL )
      theLogFile = _fopen_log_safe ( path + "_5",DUP_LOG_FD );
  }
}

static FILE* _initJassertOutputDevices()
{
#ifdef JASSERT_LOG
  JASSERT_SET_LOGFILE ( "/tmp/jassertlog." + jalib::XToString ( getpid() ) );
#endif

  const char* errpath = getenv ( "JALIB_STDERR_PATH" );

  if ( errpath != NULL )
    return _fopen_log_safe ( errpath, DUP_STDERR_FD );
  else
    return fdopen ( dup2 ( fileno ( stderr ),DUP_STDERR_FD ),"w" );;;
}

int jassert_internal::jassert_console_fd()
{
  //make sure stream is open
  jassert_safe_print ( "" );
  return DUP_STDERR_FD;
}

void jassert_internal::jassert_safe_print ( const char* str )
{
  static FILE* errconsole = _initJassertOutputDevices();

  fprintf ( errconsole,"%s",str );

  if ( theLogFile != NULL )
  {
    int rv = fprintf ( theLogFile,"%s",str );

    if ( rv < 0 )
    {
      fprintf ( errconsole,"JASSERT: write failed, reopening log file.\n" );
      JASSERT_SET_LOGFILE ( theLogFilePath() );
      if ( theLogFile != NULL )
        fprintf ( theLogFile,"JASSERT: write failed, reopened log file.\n%s",str );
    }
    fflush ( theLogFile );
  }

}
#else
# ifdef JASSERT_LOG
//JASSERT_FAST conflicts with JASSERT_LOG
JASSERT_STATIC(false);
# endif

std::ostream& jassert_internal::jassert_output_stream(){
  static const char* errpath = getenv ( "JALIB_STDERR_PATH" );
  if(errpath !=0){
    static std::ofstream output(errpath);
    if(output.is_open())
      return output;
  }
  return std::cerr;
}
#endif


