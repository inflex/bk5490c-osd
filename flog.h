#include <filesystem>
#include <fstream>
#include <iostream>

#ifndef __FLOG__
#define __FLOG__
//extern std::filesystem::path flogfile;
void flog_enable( bool enable );
int flog_init(std::filesystem::path flogfile);
int flog_direct( std::filesystem::path logfile, const char *format, ... );
std::filesystem::path flog_flogfilename( void );
int flog( const char *format, ... );
int flog_raw( const char *format, ... );

#ifndef  __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#ifndef FL
#define FL __FILENAME__,__LINE__
#endif
#endif
