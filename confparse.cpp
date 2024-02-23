#include <errno.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "confparse.h"

#ifndef FL
#define FL __FILE__,__LINE__
#endif

char default_conf[] = "\r\n\
# BK5490C Configuration file\
\r\n\
diode_beep_enabled = true\r\n\
diode_beep_threshold = 0.05\r\n\
\r\n\
continuity_beep_enabled = true\r\n\
continuity_beep_threshold = 1.00\r\n\
\r\n\
system_beep = false\r\n\
font_size = 72\r\n\
debug = false\r\n\
\r\n";

Confparse::~Confparse(void) {
	if (conf) free(conf);
}

int Confparse::SaveDefault(const std::filesystem::path utf8_filename) {
	std::ofstream file;
	file.open(utf8_filename, std::ios::out | std::ios::binary | std::ios::ate);

	nested = true;
	if (file.is_open()) {
		file.write(default_conf, sizeof(default_conf) - 1);
		file.close();
		Load(utf8_filename);

		return 0;
	} else {
		return 1;
	}
}

int Confparse::Load(const std::filesystem::path utf8_filename) {
	std::ifstream file;
	file.open(utf8_filename, std::ios::in | std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		buffer_size = 0;
		conf        = NULL;
		if (nested) return 1; // to prevent infinite recursion, we test the nested flag
		return (SaveDefault(utf8_filename));
	}

	if (conf != NULL) free(conf);

	filename = utf8_filename;

	std::streampos sz = file.tellg();
	buffer_size       = sz;
	conf              = (char *)calloc(1, buffer_size + 1);
	file.seekg(0, std::ios::beg);
	file.read(conf, sz);
	limit = conf + sz;
	file.close();

	if (file.gcount() != sz) {
		std::cerr << "Did not read the right number of bytes from configuration file" << std::endl;
		return 1;
	}

	nested = false;

	return 0;
}

char *Confparse::Parse(const char *key) {
	char *p, *op;
	char *llimit;
	int keylen;

	value[0] = '\0';

	if (!conf) return NULL;
	if (!key) return NULL;

	if (!conf[0]) return NULL;
	if (!key[0]) return NULL;

	keylen = strlen(key);
	if (keylen == 0) return NULL;

	p = strstr(conf, key);
	if (p == NULL) {
		return NULL;
	}

	op = p;

	llimit = limit; // - keylen - 2; // allows for up to 'key=x'

	while (p && p < llimit) {

		/*
		 * Consume the name<whitespace>=<whitespace> trash before we
		 * get to the actual value.  While this does mean things are
		 * slightly less strict in the configuration file text it can
		 * assist in making it easier for people to read it.
		 */
		p = p + keylen;

		if ((p < llimit) && (!isalnum(*p))) {

			while ((p < llimit) && ((*p == '=') || (*p == ' ') || (*p == '\t'))) p++; // get up to the start of the value;

			if ((p < llimit) && (p >= conf)) {

				/*
				 * Check that the location of the strstr() return is
				 * left aligned to the start of the line. This prevents
				 * us picking up trash name=value pairs among the general
				 * text within the file
				 */
				if ((op == conf) || (*(op - 1) == '\r') || (*(op - 1) == '\n')) {
					size_t i = 0;

					/*
					 * Search for the end of the data by finding the end of the line
					 */
					while ((p < limit) && ((*p != '\0') && (*p != '\n') && (*p != '\r'))) {
						value[i] = *p;
						p++;
						i++;
						if (i >= CONFPARSE_MAX_VALUE_SIZE) break;
					}
					value[i] = '\0';
					return value;
				}
			}
		}

		// if the previous strstr() was a non-success, then try search again from the next bit
		if (op < limit) {
			p = strstr(op + 1, key);
			if (!p) break;
			op = p;
		} else {
			break;
		}
	}
	return NULL;
}
#ifdef _WIN32
	std::wstring Confparse::wstring_from_utf8( char const* const utf8_string )
{
    std::wstring_convert< std::codecvt_utf8_utf16< wchar_t > > converter;
    return converter.from_bytes( utf8_string );
}
#else
	std::string Confparse::wstring_from_utf8( char const* const utf8_string ) {
	return utf8_string;
}
#endif


std::filesystem::path Confparse::ParsePath(const char *key, std::filesystem::path defaultp ) {
	char *ps = Parse(key);
	if (ps) {
		std::filesystem::path fp = wstring_from_utf8(ps);
		return fp;
	} else {
		return defaultp;
	}
}


const char *Confparse::ParseStr(const char *key, const char *defaultv) {
	char *p = Parse(key);
	if (p)
		return p;
	else
		return defaultv;
}

int Confparse::ParseInt(const char *key, int defaultv) {
	char *p = Parse(key);
	if (p) {
		int v = strtol((char *)Parse(key), NULL, 10);
		if (errno == ERANGE)
			return defaultv;
		else
			return v;
	} else
		return defaultv;
}

uint32_t Confparse::ParseHex(const char *key, uint32_t defaultv) {
	char *p = Parse(key);
	if (p) {
		uint32_t v;
		if ((*p == '0') && (*(p + 1) == 'x')) {
			p += 2;
		}
		v = strtoul(p, NULL, 16);
		if (errno == ERANGE)
			return defaultv;
		else
			return v;
	} else
		return defaultv;
}

double Confparse::ParseDouble(const char *key, double defaultv) {
	char *p = Parse(key);
	if (p) {
		double v = strtod(p, NULL);
		if (errno == ERANGE)
			return defaultv;
		else
			return v;

	} else
		return defaultv;
}

bool Confparse::ParseBool(const char *key, bool defaultv) {
	char *p = Parse(key);
	if (p) {
		if (strncmp(p, "true", sizeof("true")) == 0) {
			return true;
		} else
			return false;
	} else
		return defaultv;
}

/*
 * Write parts
 *
 */
bool Confparse::WriteStr(const char *key, const char *value) {
	char *p, *op;
	char *llimit;
	int keylen;

	if (!conf) {
		SDL_Log("%s:%d: conf is NULL\n", FL);
		return false;
	}
	if (filename.empty()) {
		SDL_Log("%s:%d: configuration filename is empty\n", FL);
		return false;
	}
	if (!value) {
		SDL_Log("%s:%d: WriteStr() value is empty\n", FL);
		return false;
	}
	if (!key) {
		SDL_Log("%s:%d: WriteStr() key is NULL\n", FL);
		return false;
	}

	keylen = strlen(key);
	if (keylen == 0) {
		SDL_Log("%s:%d: WriteStr() key length is empty\n",FL);
		return false;
	}

	op = p = strstr(conf, key);
	if (p == NULL) {
		// If the key=value pair doesn't already exist, then we just append it to the end
		//
		std::ofstream file;
		char buf[1024];
		size_t bs;
		std::filesystem::path nfn;

		nfn = filename;
		nfn.replace_extension(".conf~");

		std::filesystem::rename(filename, nfn);
		file.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!file.is_open()) {
			SDL_Log("%s:%d: Unable to open file '%s' (%s)\n", FL, filename.string().c_str(), strerror(errno));
			return false;
		}
		file.write(conf, buffer_size); // write the leadup
		bs = snprintf(buf, sizeof(buf), "\r\n%s = %s", key, value);
		file.write(buf, bs);
		file.flush();
		file.close();

		Load(filename);
		return true;
	}

	//	llimit = limit - keylen - 2; // allows for up to 'key=x'
	llimit = limit;
	while (p && p < llimit) {

		/*
		 * Consume the name<whitespace>=<whitespace> trash before we
		 * get to the actual value.  While this does mean things are
		 * slightly less strict in the configuration file text it can
		 * assist in making it easier for people to read it.
		 */
		p = p + keylen;
		if ((p < llimit) && (!isalnum(*p))) {

			while ((p < llimit) && ((*p == '=') || (*p == ' ') || (*p == '\t'))) {
				p++; // get up to the start of the value;
			}

			if ((p < llimit) && (p >= conf)) {

				/*
				 * Check that the location of the strstr() return is
				 * left aligned to the start of the line. This prevents
				 * us picking up trash name=value pairs among the general
				 * text within the file
				 */
				if ((op == conf) || (*(op - 1) == '\r') || (*(op - 1) == '\n')) {
					char *ep = NULL;

					/*
					 * op represents the start of the line/key.
					 * p represents the start of the value.
					 *
					 * we can just dump out to the file up to p and then
					 * print our data
					 */

					/*
					 * Search for the end of the data by finding the end of the line
					 * This will become 'ep', which we'll use as the start of the
					 * rest of the file data we dump back to the config.
					 */
					ep = p;
					while ((ep < limit) && ((*ep != '\0') && (*ep != '\n') && (*ep != '\r'))) {
						ep++;
					}

					{
						std::filesystem::path nfn = filename;
						std::ofstream file;

						nfn.replace_extension(".conf~");
						std::filesystem::rename(filename, nfn);
						file.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);
						if (!file.is_open()) {
							return false;
						}
						file.write(conf, (p - conf));     // write the leadup
						file.write(value, strlen(value)); // write the new data
						file.write(ep, limit - ep);       // write the rest of the file
						file.close();

						Load(filename);
						return true;
					}
				}
			}
		}
		p  = strstr(op + 1, key);
		op = p;
	}

	/*
	 * If we didn't find our parameter in the config file, then add it.
	 */
	{
		char sep[CONFPARSE_MAX_VALUE_SIZE];
		std::ofstream file;
		file.open(filename, std::ios::out | std::ios::binary | std::ios::app);
		if (!file.is_open()) {
			return false;
		}
		snprintf(sep, sizeof(sep), "\r\n%s = %s\r\n", key, value);
		file.write(sep, strlen(sep));
		file.flush();
		file.close();

		Load(filename);
		return true;
	}

	return false;
}

bool Confparse::WriteBool(const char *key, bool value) {
	char v[10];
	snprintf(v, sizeof(v), "%s", value ? "true" : "false");
	return WriteStr(key, v);
};

bool Confparse::WriteInt(const char *key, int value) {
	char v[20];
	snprintf(v, sizeof(v), "%d", value);
	return WriteStr(key, v);
};
bool Confparse::WriteHex(const char *key, uint32_t value) {
	char v[20];
	snprintf(v, sizeof(v), "0x%08x", value);
	return WriteStr(key, v);
};
bool Confparse::WriteFloat(const char *key, double value) {
	char v[20];
	snprintf(v, sizeof(v), "%f", value);
	return WriteStr(key, v);
};
