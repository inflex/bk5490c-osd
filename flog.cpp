#include <filesystem>
#include <fstream>
#include <iostream>

#include "fmt/core.h"
#define FMT_HEADER_ONLY
#include "fmt/format.h"


#ifdef _MSC_VER
#define SDL_MAIN_HANDLED
#include <SDL.h>
//#include <SDL_image.h>
#elif defined __APPLE__
#include <SDL.h>
//#include <SDL_image.h>
#else
#include <SDL2/SDL.h>
//#include <SDL2/SDL_image.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string>
#include <chrono>
//#include "fbvfopen.h"
#include "flog.h"

std::filesystem::path flogfile;
static uint64_t flog_its = 0;
bool flog_enabled = true;

void flog_enable( bool enable ) {
	flog_enabled = enable;
}

uint64_t flog_millis()
{
	uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::high_resolution_clock::now().time_since_epoch())
		.count();
	return ms; 
}


std::filesystem::path flog_flogfilename( void ) {
	return flogfile;
}

int flog_init( std::filesystem::path logfile ) {

	std::ofstream f;

	flogfile = logfile;
	flog_its = flog_millis();
	f.open(flogfile, std::ios::binary);
	if (f.is_open()) f.close();
	return 0;
}


int flog_direct( std::filesystem::path logfile, const char *format, ... ) {

	if (!flog_enabled) return 0;

	va_list args;
	va_start(args, format);

	std::ofstream f;

	f.open(logfile, std::ios::app|std::ios::binary);
	if (f.is_open()) {
		char buf[10240];

		f << fmt::format("{:06} ", flog_millis() -flog_its);
		vsnprintf(buf, sizeof(buf) -1,format,args);
		va_end(args);
		f << fmt::format("{}", buf);
		f.close();
	}
	return 0;
}


int flog( const char *format, ... ) {
	if (!flog_enabled) return 0;
	char buf[10240];
	va_list args;
	va_start(args, format);

	vsnprintf(buf, sizeof(buf) -1,format,args);
	va_end(args);

	std::ofstream f;

	f.open(flogfile, std::ios::app|std::ios::binary);
	if (f.is_open()) {

		f << fmt::format("{:06} ", flog_millis() -flog_its);
		f << fmt::format("{}", buf);
		f.close();
	}
	return 0;
}

int flog_raw( const char *format, ... ) {
	if (!flog_enabled) return 0;
		va_list args;
	va_start(args, format);

	std::ofstream f;

	f.open(flogfile, std::ios::app|std::ios::binary);
	if (f.is_open()) {
		char buf[10240];

		vsnprintf(buf, sizeof(buf) -1,format,args);
		va_end(args);
		f << buf;
		f.close();
	}
	return 0;
}


