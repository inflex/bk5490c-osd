/*
 * BK Precision Model 5490C SCPI multimeter OBS display
 *
 * Preliminary version working via USB-serial bridge at 9600:8n1
 *
 * V0.1 - Feb 15, 2024
 * 
 *
 * Written by Paul L Daniels (pldaniels@gmail.com)
 *
 */

#include <windows.h>
#include <shellapi.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include <sys/time.h>
#include <unistd.h>
#include <wchar.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <fstream>
#include <iostream>
#include "confparse.h"
#include "flog.h"


/*
 * Should be defined in the Makefile to pass to the compiler from
 * the github build revision
 *
 */
#ifndef BUILD_VER 
#define BUILD_VER 000
#endif

#ifndef BUILD_DATE
#define BUILD_DATE " "
#endif

#define WINDOWS_DPI_DEFAULT 72
#define FONT_NAME_SIZE 1024
#define SSIZE 1024

#define FONT_SIZE_MAX 256
#define FONT_SIZE_MIN 10
#define DEFAULT_FONT_SIZE 72
#define DEFAULT_FONT L"Andale"
#define DEFAULT_FONT_WEIGHT 600
#define DEFAULT_WINDOW_HEIGHT 9999
#define DEFAULT_WINDOW_WIDTH 9999
#define DEFAULT_COM_PORT 99
#define DEFAULT_COM_SPEED 9600

#define ee ""
#define uu "\u00B5"
#define kk "k"
#define MM "M"
#define mm "m"
#define nn "n"
#define pp "p"
#define dd "\u00B0"
#define oo "\u03A9"

#define ID_QUIT 1


struct mmode_s {
	char scpi[50];
	char label[50];
	char query[50];
	char units[10];
};


struct mmode_s mmodes[] = { 
	{"VOLT", "Volts DC", "CONF:VOLT:DC\r\n", "V DC"},  
	{"VOLT:AC", "Volts AC", "CONF:VOLT:AC\r\n", "V AC"},
	{"VOLT:DCAC", "Volts DC/AC", "CONF:VOLT:DCAC\r\n", "V DC/AC"},
	{"CURR", "Current DC", "CONF:CURR:DC\r\n", "A DC"},
	{"CURR:AC", "Current AC", "CONF:CURR:AC\r\n", "A AC"},
	{"CURR:DCAC", "Current DC/AC", "CONF:CURR:DCAC\r\n", "A DC/AC"},
	{"RES", "Resistance", "CONF:RES\r\n", oo },
	{"FREQ", "Frequency", "CONF:FREQ\r\n", "Hz" },
	{"PER", "Period", "CONF:PER\r\n", "s"},
	{"TEMP", "Temperature", "CONF:TEMP:RTD\r\n", "C"},
	{"DIOD", "Diode", "CONF:DIOD\r\n", "V"},
	{"CONT", "Continuity", "CONF:CONT\r\n", oo},
	{"CAP", "Capacitance", "CONF:CAP\r\n", "F"}
};

char SCPI_RES_ZERO_ON[] = "RES:ZERO:AUTO ON\r\n";
char SCPI_FUNC[] = "SENS:FUNC1?\r\n";
char SCPI_VAL1[] = "VAL1?\r\n";
char SCPI_VAL2[] = "VAL2?\r\n";
char SCPI_CONT_THRESHOLD[] = "SENS:CONT:THR?\r\n";
char SCPI_LOCAL[] = "LOC\r\n";
char SCPI_REMOTE[] = "SYST:REM\r\n";
char SCPI_RANGE[] = "CONF:RANG?\r\n";
char SCPI_CONF[] = "CONF?\r\n";
char SCPI_READ[] = "READ?\r\n";
char SCPI_BEEP_ON[] = "SYST:BEEP:STAT 1\r\n";
char SCPI_BEEP_OFF[] = "SYST:BEEP:STAT 0\r\n";
char SCPI_BEEP[] = "SYST:BEEP\r\n";
char SCPI_BEEP_FORCE[] = "SYST:BEEP:STAT 1\r\nSYST:BEEP\r\nSYST:BEEP:STAT 0\r\n";
char SCPI_VAC_FAST[] = "VOLT:AC:SPEE FAST\r\n";
char SCPI_VDC_FAST[] = "VOLT:NPLC 1\r\n";
char SCPI_IDN[] = "*IDN?\r\n";
char SCPI_RST[] = "*RST\r\n";


char SEPARATOR_DP[] = ".";


#define MMODES_VOLT_DC 0
#define MMODES_VOLT_AC 1
#define MMODES_VOLT_DCAC 2
#define MMODES_CURR_DC 3
#define MMODES_CURR_AC 4
#define MMODES_CURR_DCAC 5
#define MMODES_RES 6
#define MMODES_FREQ 7
#define MMODES_PER 8
#define MMODES_TEMP 9
#define MMODES_DIOD 10
#define MMODES_CONT 11
#define MMODES_CAP 12
#define MMODES_MAX 13

struct metermode_s {
	int hotkey;
	char mode_display_string[128];
	char mode_select_str[128];
	char mode_query_str[128];
};

struct glb {
	int wx_forced, wy_forced;
	int window_x, window_y;
	int window_width, window_height;

	int draw_minmaxes;
	int draw_graph;

	HANDLE hComm;

	uint8_t debug;
	uint8_t comms_enabled;
	uint8_t quiet;
	uint8_t show_mode;
	uint16_t flags;
	uint8_t com_address;

	wchar_t font_name[FONT_NAME_SIZE];
	int font_size;
	int font_weight;

	SDL_Color line1_color, line2_color, background_color;

	char serial_params[SSIZE];

	int mmdata_active;
	wchar_t mmdata_output_file[MAX_PATH];
	wchar_t mmdata_output_temp_file[MAX_PATH];

	bool cont_beep_enabled;
	double cont_threshold;
	bool diode_beep_enabled;
	double diode_threshold;
	 
	bool system_beep;

};

/*
 * A whole bunch of globals, because I need
 * them accessible in the Windows handler
 *
 * So many of these I'd like to try get away from being
 * a global.
 *
 */
struct glb *glbs;

/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220248
  Function Name	: init
  Returns Type	: int
  ----Parameter List
  1. struct glb *g ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int init(struct glb *g) {
	g->window_x = DEFAULT_WINDOW_WIDTH;
	g->window_y = DEFAULT_WINDOW_HEIGHT;
	g->draw_minmaxes = 1;
	g->draw_graph = 1;
	g->debug = 0;
	g->comms_enabled = 1;
	g->quiet = 0;
	g->show_mode = 0;
	g->flags = 0;
	g->font_size = DEFAULT_FONT_SIZE;
	g->font_weight = DEFAULT_FONT_WEIGHT;
	g->com_address = DEFAULT_COM_PORT;
	g->mmdata_active = 1;

	g->font_size = 72;
	g->window_width = 500;
	g->window_height = 120;
	g->wx_forced = 0;
	g->wy_forced = 0;

	g->line1_color =  { 10, 200, 10 };
	g->line2_color =  { 200, 200, 10 };
	g->background_color = { 0, 0, 0 };


	g->serial_params[0] = '\0';

	g->cont_beep_enabled = true;
	g->cont_threshold = 1.0;

	g->diode_beep_enabled = true;
	g->diode_threshold = 0.05;
	g->system_beep = false;

	return 0;
}

void show_help(void) {
	printf("B&K5490C SCPI Meter\r\n"
			"By Paul L Daniels / pldaniels@gmail.com\r\n"
			"Build %d / %s\r\n"
			"\r\n"
			" [-p <comport#>] [-z <fontsize>] [-b] [-d] [-q]\r\n"
			"\r\n"
			"\t-h: This help\r\n"
			"\t-p <comport>: Set the com port for the meter, eg: -p 2\r\n"
			"\t-z: Font size (default 72, max 256pt)\r\n"
			"\t-b: Beep on mode change\r\n"
			"\r\n"
			"\t-d: debug enabled\r\n"
			"\t-v: show version\r\n"
			"\r\n"
			"\tDefaults: -z 72\r\n"
			"\r\n"
			"\texample: bk5492 -z 120 -p 4\r\n"
			, BUILD_VER
			, BUILD_DATE 
			);
} 


/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220258
  Function Name	: parse_parameters
  Returns Type	: int
  ----Parameter List
  1. struct glb *g,
  2.  int argc,
  3.  char **argv ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int parse_parameters(struct glb *g) {
	LPWSTR *argv;
	int argc;
	int i;
	int fz = DEFAULT_FONT_SIZE;

	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (NULL == argv) {
		return 0;
	}

	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			/* parameter */
			switch (argv[i][1]) {
				case 'h':
					show_help();
					exit(1);
					break;

				case 'o':
					if (argv[i][2] == 'm') {
						if (i >= (argc -1)) {
							StringCbPrintfW(g->mmdata_output_file, MAX_PATH, L"mmdata.txt");
							StringCbPrintfW(g->mmdata_output_temp_file, MAX_PATH, L"mmdata.tmp");
						} else {
							i++;
							StringCbPrintfW(g->mmdata_output_file, MAX_PATH, L"%s\\mmdata.txt", argv[i]);
							StringCbPrintfW(g->mmdata_output_temp_file, MAX_PATH, L"%s\\mmdata.tmp", argv[i]);
						}
						g->mmdata_active = 1;
					}
					break;



				case 'w':
					if (argv[i][2] == 'x') {
						i++;
						g->window_x = _wtoi(argv[i]);
					} else if (argv[i][2] == 'y') {
						i++;
						g->window_y = _wtoi(argv[i]);
					}
					break;

				case 'b':
					if (argv[i][2] == 'c') {
						int r, gg, b;

						i++;
						swscanf(argv[i], L"#%02x%02x%02x", &r, &gg, &b);
						//g->background_color = RGB(r, gg, b);
					}
					break;

				case 'f':
					if (argv[i][2] == 'w') {
						i++;
						g->font_weight = _wtoi(argv[i]);

					} else if (argv[i][2] == 'c') {
						int r, gg, b;

						i++;
						swscanf(argv[i], L"#%02x%02x%02x", &r, &gg, &b);
						//						g->font_color = RGB(r, gg, b);

					} else if (argv[i][2] == 'n') {
						i++;
						StringCbPrintfW(g->font_name, FONT_NAME_SIZE, L"%s", argv[i]);
					}
					break;

				case 'z':
					i++;
					if (i < argc) {
						fz = _wtoi(argv[i]);
						if (fz < FONT_SIZE_MIN) {
							fz = FONT_SIZE_MIN;
						} else if (fz > FONT_SIZE_MAX) {
							fz = FONT_SIZE_MAX;
						}
						g->font_size = fz;
					}
					break;

				case 'p':
					i++;
					if (i < argc) {
						g->com_address = _wtoi(argv[i]);
					} else {
						wprintf(L"Insufficient parameters; -p <com port>\n");
						exit(1);
					}
					break;

				case 'd': g->debug = 1; break;

				case 'm': g->show_mode = 1; break;

				case 's':
							 i++;
							 if (i < argc) {
								 wcstombs(g->serial_params, argv[i], sizeof(g->serial_params));
							 } else {
								 wprintf(L"Insufficient parameters; -s <parameters> [eg 9600:8:o:1] = 9600, 8-bit, odd, 1-stop\n");
								 exit(1);
							 }
							 break;

				default: break;
			} // switch
		}
	}

	LocalFree(argv);

	return 0;
}

int purge_coms(struct glb *pg) {

	flog("Clearing all prior comms and buffers on port COM%d\n",  pg->com_address);
	PurgeComm( pg->hComm, PURGE_RXABORT|PURGE_RXCLEAR|PURGE_TXABORT|PURGE_TXCLEAR);

	flog("Port COM%d open and ready\n", pg->com_address);

	return 0;

}

int enable_coms(struct glb *pg, int port) {
	wchar_t com_port[SSIZE]; // com port path / ie, \\.COM4
	BOOL com_read_status;  // return status of various com port functions
								  //
	flog("enable_coms: Port #%d requested for opening...\n", port);

	snwprintf(com_port, sizeof(com_port), L"COM%d", port);
	/*
	 * Open the serial port
	 */
	pg->hComm = CreateFile(com_port,      // Name of port
			GENERIC_READ|GENERIC_WRITE,  // Read Access
			0,             // No Sharing
			0,          // No Security
			OPEN_EXISTING, // Open existing port only
			FILE_ATTRIBUTE_NORMAL,             // Non overlapped I/O
			0);         // Null for comm devices

	/*
	 * Check the outcome of the attempt to create the handle for the com port
	 */
	if (pg->hComm == INVALID_HANDLE_VALUE) {
		flog("Error while trying to open com port '%d'\r\n", pg->com_address);
		exit(1);
	} else {
		if (!pg->quiet) flog("enable_comms: Port %d Opened\r\n", pg->com_address);
	}

	/*
	 * Set serial port parameters
	 */
	DCB dcbSerialParams = {0}; // Init DCB structure
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	com_read_status = GetCommState(pg->hComm, &dcbSerialParams); // Retrieve current settings
	if (com_read_status == FALSE) {
		flog("Error in getting GetCommState()\r\n");
		CloseHandle(pg->hComm);
		return 1;
	}

	dcbSerialParams.BaudRate = CBR_9600;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;

	com_read_status = SetCommState(pg->hComm, &dcbSerialParams);
	if (com_read_status == FALSE) {
		flog("Error setting com port configuration (9600:8n1 etc)\r\n");
		CloseHandle(pg->hComm);
		return 1;
	} else {

		if (!pg->quiet) {
			flog("\tBaudrate = %ld\r\n", dcbSerialParams.BaudRate);
			flog("\tByteSize = %ld\r\n", dcbSerialParams.ByteSize);
			flog("\tStopBits = %d\r\n", dcbSerialParams.StopBits);
			flog("\tParity   = %d\r\n", dcbSerialParams.Parity);
		}

	}

	COMMTIMEOUTS timeouts = {0};
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 100; 
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;
	if (SetCommTimeouts(pg->hComm, &timeouts) == FALSE) {
		flog("Error in setting time-outs\r\n");
		CloseHandle(pg->hComm);
		return 1;

	} else {
		if (!pg->quiet) { flog("Setting time-outs successful\r\n"); }
	}

	com_read_status = SetCommMask(pg->hComm, EV_RXCHAR | EV_ERR); // Configure Windows to Monitor the serial device for Character Reception and Errors
	if (com_read_status == FALSE) {
		flog("Error in setting CommMask\r\n");
		CloseHandle(pg->hComm);
		return 1;

	} else {
		if (!pg->quiet) { flog("CommMask successful\r\n"); }
	}

	return 0;
}


bool WriteRequest( struct glb *g, char * lpBuf, DWORD dwToWrite) {

	flog("Starting buffer write\n");
	OVERLAPPED osWrite = {0};
	DWORD dwWritten;
	bool fRes;

	// Create this writes OVERLAPPED structure hEvent.
	osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osWrite.hEvent == NULL)
		// Error creating overlapped event handle.
		return FALSE;

	// Issue write.
	if (!WriteFile(g->hComm, lpBuf, dwToWrite, &dwWritten, &osWrite)) {
		if (GetLastError() != ERROR_IO_PENDING) { 
			// WriteFile failed, but it isn't delayed. Report error and abort.
			fRes = false;
		} else {
			// Write is pending.
			if (!GetOverlappedResult(g->hComm, &osWrite, &dwWritten, TRUE)) {
				fRes = false;
			} else {
				// Write operation completed successfully.
				fRes = true;
			}
		}
	} else {
		// WriteFile completed immediately.
		fRes = true;
	}

	CloseHandle(osWrite.hEvent);
	flog("buffer write completed\n");
	SDL_Delay(10); // 10ms delay
						//
	return fRes;
}

int ReadResponse( struct glb *g, char *buffer, size_t buf_limit ) {
	char chRead = 0;
	int buf_index = 0;
	int end_of_frame_received = 0;
	DWORD dwCommEvent;
	DWORD dwRead;

	buffer[0] = '\0';
	buffer[1] = '\0';

	while (!end_of_frame_received) {
		// Keep trying until we've received an end-of-frame \r
		//
		//
		if (WaitCommEvent(g->hComm, &dwCommEvent, NULL)) {
			// If we've got something in the comm buffer pipe
			//
			//
			do {
				if (buf_index >= buf_limit) {
					buffer[buf_limit -1] = '\0';
					flog("Buffer limit reached for ReadFile's supplied buffer (%d bytes)\n", buf_limit);
					return 1;
				}
				// If we read 1 char and it's successful
				//
				if (ReadFile(g->hComm, &chRead, 1, &dwRead, NULL)) {

					// If we actually did read ONE byte
					//
					//
					if (dwRead == 1) {
						// Set the byte to the current buffer index
						//
						buffer[buf_index] = chRead;

						// If the read char is our terminating char, then 
						// set the buffer value to 0 (terminate string)
						// and exit out of loop
						//
						//
						if (chRead == '\n')  {
							buffer[buf_index] = '\0';
							end_of_frame_received = 1;
						}

						buf_index++;
						// A byte has been read; process it.
						//
					}
				} else {
					buffer[buf_index] = '\0';
					// An error occurred in the ReadFile call.
					break;
				}
			} while (dwRead && !end_of_frame_received);
		} else {
			flog("Error from WaitCommEvent()\n");
			// Error in WaitCommEvent
			break;
		}
	}
	return 0;
}


bool auto_detect_port(struct glb *g) {
	TCHAR szDevices[65535];
	unsigned long dwChars = QueryDosDevice(NULL, szDevices, 65535);
	TCHAR *ptr = szDevices;

	while (dwChars) {
		int port;

		if (swscanf(ptr, L"COM%d", &port) == 1) { // if it finds the format COM#

			if (port >= 0) {
				int r = 0;

				// Try the port...
				//
				//
				g->com_address = port;
				flog("Attempting detected port: COM%d\r\n",port);

				// Compose the port path ( though I suppose we could use the ptr from above )
				//
				//
				r = enable_coms(g, port); // establish serial communication parameters
				if (r) {
					flog("Could not enable comms for port %d, jumping to next device\r\n", port);
					if(g->hComm) { // prevent small memory leak!
						CloseHandle(g->hComm);
					}
					continue;

				} else {
					flog("Success enabling comms for port %d. Testing protocol now...\r\n", port);
				}

				flog("Purging comms on port before testing IDN...\n");
				purge_coms(g);

				// We did succeed in opening the port, so now let's try
				// send a query to it
				//
				//
				char response[1024];
				flog("Querying meter's IDN\n");
				WriteRequest(g, SCPI_IDN, strlen(SCPI_IDN));
				ReadResponse(g, response, sizeof(response));

				flog("Response received: %s\n", response);
				if (strstr(response, "BK Precision,549")) {
					flog("ID match, this is the right port; returning true for port %d.\n", port);
					return true; // and our com port is now open
				} else {
					flog("No match. Try next port\n");
				}
			} // if port > 0
		} // swscanf()

		// advance the string pointers to the next device
		//
		TCHAR *temp_ptr = wcschr(ptr, '\0');
		dwChars -= (DWORD)((temp_ptr - ptr) / sizeof(TCHAR) + 1);
		ptr = temp_ptr + 1;

	} // while dwChars

	flog("Was not able to find a matching port in the system. Returning false.\n");
	return false; // if we made it to the end of the function, auto-detection failed

} // Autodetect
  //
uint32_t str2color( char *str ) {
						int r, gg, b;
						sscanf(str, "#%02x%02x%02x", &r, &gg, &b);
						return ( r | (gg << 8) | (b  << 16) );
}


/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220307
  Function Name	: main
  Returns Type	: int
  ----Parameter List
  1. int argc,
  2.  char **argv ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {

	Confparse conf;
	struct glb glbs, *g;        // Global structure for passing variables around
	char meter_conf[SSIZE];
	int mode_was_changed = 0;

	char response[SSIZE] = "";
	char line1[1024] = "";
	char line2[1024] = "*";
	char g_value[1024] = "";
	char g_range[1024] = "";

	SDL_Surface *surface, *surface_2;
	SDL_Texture *texture, *texture_2;
	int meter_mode = 0;
	bool com_write_status;
	bool paused = false;

	char meter_mode_str[20];
	double meter_range;
	double meter_precision;
	double meter_value;

	bool eQuit = false;
	MSG msg;
	HWND hwnd;

	flog_enable(false);

	g = &glbs;

	/*
	 * Initialise the global structure
	 */
	init(g);

	/*
	 * Parse our command line parameters
	 */
	parse_parameters(g);

	/*
	 * Load configuration
	 */
	conf.Load("bk5490c.cfg");
	g->debug = conf.ParseBool("debug", false);
	g->font_size = conf.ParseInt("font_size", 72);
	g->diode_threshold = conf.ParseDouble("diode_beep_threshold", 0.05);
	g->diode_beep_enabled = conf.ParseBool("diode_beep_enabled", true);
	g->cont_threshold = conf.ParseDouble("continuity_beep_threshold", 1.00);
	g->cont_beep_enabled = conf.ParseBool("continuity_beep_enabled", true);
	g->system_beep = conf.ParseBool("system_beep", false);

	uint32_t tc;
	tc = conf.ParseHex("background_color", 0x000000);
	g->background_color.r = (tc & 0xff0000) >> 16;
	g->background_color.g = (tc & 0x00ff00) >> 8;
	g->background_color.b = tc & 0x0000ff;

	tc = conf.ParseHex("line1_color", 0x0ac80a);
	g->line1_color.r = (tc & 0xff0000) >> 16;
	g->line1_color.g = (tc & 0x00ff00) >> 8;
	g->line1_color.b = tc & 0x0000ff;

	tc = conf.ParseHex("line2_color", 0xc8c80a);
	g->line2_color.r = (tc & 0xff0000) >> 16;
	g->line2_color.g = (tc & 0x00ff00) >> 8;
	g->line2_color.b = tc & 0x0000ff;

	//g->debug = true; // forced debug

	if (g->debug) {
		flog_enable( true );
		flog_init( "logfile.txt" );
		flog("BUILD: %s %s\n", __DATE__, __TIME__);
	} else {
		flog_enable( false );
	}

	/*
	 *
	 * Now do all the Windows GDI stuff
	 *
	 */
	SDL_Event w_event;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		flog("SDL Could not initialise (%s)\n", SDL_GetError());
		exit(1);
	}

	g->window_width = g->window_x;
	g->window_height = g->window_y;


#define HOTKEY_VOLTS 1000
#define HOTKEY_VOLTSAC 1001
#define HOTKEY_AMPS 1002
#define HOTKEY_RESISTANCE 1003
#define HOTKEY_CONTINUITY 1004
#define HOTKEY_DIODE 1005
#define HOTKEY_CAPACITANCE 1006
#define HOTKEY_FREQUENCY 1007
#define HOTKEY_TEMPERATURE 1008
#define HOTKEY_QUIT 1015

	RegisterHotKey(NULL, HOTKEY_VOLTS, MOD_ALT + MOD_SHIFT, 'V'); 
	RegisterHotKey(NULL, HOTKEY_VOLTSAC, MOD_ALT + MOD_SHIFT, 'W'); 
	RegisterHotKey(NULL, HOTKEY_AMPS, MOD_ALT + MOD_SHIFT, 'A'); 
	RegisterHotKey(NULL, HOTKEY_RESISTANCE, MOD_ALT + MOD_SHIFT, 'R'); 
	RegisterHotKey(NULL, HOTKEY_CONTINUITY, MOD_ALT + MOD_SHIFT, 'C');
	RegisterHotKey(NULL, HOTKEY_DIODE, MOD_ALT + MOD_SHIFT, 'D');
	RegisterHotKey(NULL, HOTKEY_CAPACITANCE, MOD_ALT + MOD_SHIFT, 'B'); 
	RegisterHotKey(NULL, HOTKEY_FREQUENCY, MOD_ALT + MOD_SHIFT, 'H'); 
	RegisterHotKey(NULL, HOTKEY_TEMPERATURE, MOD_ALT + MOD_SHIFT, 'T'); 

	TTF_Init();
	TTF_Font *font = TTF_OpenFont("RobotoMono-Bold.ttf", g->font_size);
	if ( !font ) { flog("Ooops - something went wrong when trying to create the %d px font", g->font_size ); exit(1); }
	TTF_Font *font_half = TTF_OpenFont("RobotoMono-Bold.ttf", g->font_size/2);
	if ( !font ) { flog("Ooops - something went wrong when trying to create the %d px font", g->font_size/2 ); exit(1); }


	/*
	 * Get the required window size.
	 *
	 * Parameters passed can override the font self-detect sizing
	 *
	 */
	int data_w, data_h;
	TTF_SizeText(font_half, "00.000", &data_w, &data_h);
	TTF_SizeText(font, " 00.00000 mV", &g->window_width, &g->window_height);
	g->window_width += data_w;
	g->window_height *= 1.85;

	if (g->wx_forced) g->window_width = g->wx_forced;
	if (g->wy_forced) g->window_height = g->wy_forced;

	SDL_Window *window = SDL_CreateWindow("B&K 549XC Meter", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, g->window_width, g->window_height, 0);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
	if (!font) {
		flog("Error trying to open font :( \r\n");
		exit(1);
	}


	/* Select the color for drawing-> It is set to red here. */
	SDL_SetRenderDrawColor(renderer, g->background_color.r, g->background_color.g, g->background_color.b, 255 );

	/* Clear the entire screen to our selected color. */
	SDL_RenderClear(renderer);


	//
	// Handle the COM Port
	//
	if (g->com_address == DEFAULT_COM_PORT) { // no port was specified, so attempt an auto-detect
		flog("Now attempting an auto-detect....\r\n");
		if(!auto_detect_port(g))  { // returning false means auto-detect failed
			flog("Failed to automatically detect COM port. Perhaps try using -p?\r\n");
			exit(1);
		}
		flog("COM%d successfully detected.\r\n",g->com_address); 

	} else {

		int r = 0;
		flog("Now attempting to connect to: %d....\r\n", g->com_address);
		r = enable_coms(g, g->com_address); // establish serial communication parameters
		flog("Connection attempt result = %d....\r\n", r);
		if (r != 0) {
			flog("Unable to connect to port %d due to result=%d\n", g->com_address, r);
			exit(1);
		}
	} 


	SDL_Delay(250);

	flog("Request IDN\n");
	WriteRequest(g, SCPI_IDN, strlen(SCPI_IDN));
	ReadResponse(g, response, sizeof(response));
	flog("IDN Response: %s\n", response);

	flog("Setting meter to REMOTE modes\n");
	WriteRequest(g, SCPI_REMOTE, strlen(SCPI_REMOTE));

	if (g->system_beep) {
		flog("Setting continuity mode beep ON\n");
		WriteRequest(g, SCPI_BEEP_ON, strlen(SCPI_BEEP_ON));
	} else {
		flog("Setting continuity mode beep OFF\n");
		WriteRequest(g, SCPI_BEEP_OFF, strlen(SCPI_BEEP_OFF));
	}

	flog("Setting Speeds of measurements\n");
	WriteRequest(g, SCPI_VAC_FAST, strlen(SCPI_VAC_FAST));
	WriteRequest(g, SCPI_VDC_FAST, strlen(SCPI_VDC_FAST));

	SDL_Delay(250);


	mode_was_changed = 1; // sets things up to switch to volts initially.
	meter_mode = MMODES_VOLT_DC;

	flog("Starting main loop...\n");
	while (!eQuit) {

		// Clear our C-style strings.  This is overkill 
		// but sometimes helps avoid odd results
		//
		//
		line1[0] = 0;
		line2[0] = 0;
		g_value[0] = 0;
		g_range[0] = 0;


		// Check to see if we have a windows message coming through that
		// might be our hotkey being pressed
		//
		//
		if (PeekMessage(&msg, hwnd,  WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {
			if (msg.message == WM_HOTKEY) { 
				flog("Hotkey detected\n");
				switch (LOWORD(msg.wParam)) { 
					case HOTKEY_VOLTS:
						meter_mode = MMODES_VOLT_DC;
						break;

					case HOTKEY_VOLTSAC:
						meter_mode = MMODES_VOLT_AC;
						break;

					case HOTKEY_AMPS:
						meter_mode = MMODES_CURR_DC;
						break;

					case HOTKEY_RESISTANCE:
						meter_mode = MMODES_RES;
						break;

					case HOTKEY_CONTINUITY:
						meter_mode = MMODES_CONT;
						break;

					case HOTKEY_DIODE:
						meter_mode = MMODES_DIOD;
						break;

					case HOTKEY_FREQUENCY:
						meter_mode = MMODES_FREQ;
						break;

					case HOTKEY_CAPACITANCE:
						meter_mode = MMODES_CAP;
						break;

					case HOTKEY_TEMPERATURE:
						meter_mode = MMODES_TEMP;
						break;

				}  // switch

				mode_was_changed = 1;
			} // if message == HOTKWEY
		} // peeking in the message queue 




		// Check SDL events - lets us move things
		//
		//
		while (SDL_PollEvent(&w_event)) {
			switch(w_event.type) {

				case SDL_QUIT: 
					eQuit = true; 
					break;

				case SDL_KEYDOWN:
					if (w_event.key.keysym.sym == SDLK_q) {
						WriteRequest( g, SCPI_LOCAL, strlen(SCPI_LOCAL) );
						eQuit = true;
					}
					if (w_event.key.keysym.sym == SDLK_p) {
						paused ^= 1;
						if (paused == true) WriteRequest( g, SCPI_LOCAL, strlen(SCPI_LOCAL) );
						else WriteRequest(g, SCPI_REMOTE, strlen(SCPI_REMOTE));
					}
					break;

			}
		} // respond to SDL events

		// Change the mode and get the configuration setup
		//
		//
		if (mode_was_changed) {
			mode_was_changed = 0;
			flog("MODE change request TO meter: '%s'\n", mmodes[meter_mode].query);
			com_write_status = WriteRequest(g, mmodes[meter_mode].query, strlen(mmodes[meter_mode].query));

			//			char response[1024];
			//			flog("Querying response...\n");
			//			ReadResponse(g, response, sizeof(response));
			//			flog("Response from meter: '%s'\n", response);

			if (meter_mode == MMODES_RES) {
				flog("Setting 2 wire resistance auto-zero to ON\n");
				com_write_status = WriteRequest(g, SCPI_RES_ZERO_ON, strlen(SCPI_RES_ZERO_ON));
			}

//			if (meter_mode == MMODES_DIOD || meter_mode == MMODES_CONT) {
//				flog("Setting continuity mode beep ON\n");
//				com_write_status = WriteRequest(g, SCPI_BEEP_ON, strlen(SCPI_BEEP_ON));
//			}

			//com_write_status = WriteRequest(g, SCPI_BEEP, strlen(SCPI_BEEP));
			com_write_status = WriteRequest(g, SCPI_BEEP_FORCE, strlen(SCPI_BEEP_FORCE));

		} 

		flog("Requesting current configuration mode...\n");
		com_write_status = WriteRequest(g, SCPI_CONF, strlen(SCPI_CONF));
		flog("Getting configuration response...\n");
		ReadResponse(g, meter_conf, sizeof(meter_conf));
		flog("Meter configuration: %s\n", meter_conf);

		// Parse the configuration response
		//
		//
		char *p = strchr(meter_conf,',');
		if (p) {
			*p = '\0';
			snprintf(meter_mode_str, sizeof(meter_mode_str), "%s", meter_conf); // copies the DCV / DCI etc
			*p = ',';
			p++;
			meter_range = strtod(p, &p);
			if (*p == ',') {
				meter_precision = strtod(p, NULL);
			}
		}
		flog("Meter configuration conversion: %s => '%s', %f, %f\n", meter_conf, meter_mode_str, meter_range, meter_precision);

		// Read a value from the meter
		//
		//
		flog("Requesting READ value...\n");
		WriteRequest(g, SCPI_READ, strlen(SCPI_READ));
		flog("Getting response...\n");
		ReadResponse(g, response, sizeof(response));
		flog("Response: '%s'\n", response);

		meter_value = strtod(response, NULL);
		flog("Converted value to: '% f'\n", strtod(response, NULL));


		// Convert the value received from the READ? request in to
		// something we can display on the OSD window
		//
		//
		switch (meter_mode) {
			case MMODES_VOLT_AC:
				if (meter_range == 0.1) {
					snprintf((g_value),sizeof(g_value),"% 06.3f mV AC", meter_value *1000);
					snprintf(g_range, sizeof(g_range), "100mV");

				} else if (meter_range == 1.0) {
					snprintf((g_value),sizeof(g_value),"% 06.5f V AC", meter_value);
					snprintf(g_range, sizeof(g_range), "1V");

				} else if (meter_range == 10.0) {
					snprintf((g_value),sizeof(g_value),"% 06.4f V AC", meter_value);
					snprintf(g_range, sizeof(g_range), "10V");

				} else if (meter_range == 100.0) {
					snprintf((g_value),sizeof(g_value),"% 06.3f V AC", meter_value);
					snprintf(g_range, sizeof(g_range), "100V");

				} else if (meter_range == 750.0) {
					snprintf((g_value),sizeof(g_value),"% 05.2f V AC", meter_value);
					snprintf(g_range, sizeof(g_range), "1000V");

				} else {
					snprintf((g_value),sizeof(g_value),"% f V AC", meter_value);
					snprintf(g_range, sizeof(g_range), "Unknown");
				} 
				break; // VOLTS AC


			case MMODES_VOLT_DC:
				if (meter_range == 0.1) {
					snprintf((g_value),sizeof(g_value),"% 06.3f mV DC", meter_value *1000);
					snprintf(g_range, sizeof(g_range), "100mV");

				} else if (meter_range == 1.0) {
					snprintf((g_value),sizeof(g_value),"% 06.5f V DC", meter_value);
					snprintf(g_range, sizeof(g_range), "1V");

				} else if (meter_range == 10.0) {
					snprintf((g_value),sizeof(g_value),"% 06.4f V DC", meter_value);
					snprintf(g_range, sizeof(g_range), "10V");

				} else if (meter_range == 100.0) {
					snprintf((g_value),sizeof(g_value),"% 06.3f V DC", meter_value);
					snprintf(g_range, sizeof(g_range), "100V");

				} else if (meter_range == 1000.0) {
					snprintf((g_value),sizeof(g_value),"% 06.2f V DC", meter_value);
					snprintf(g_range, sizeof(g_range), "1000V");

				} else {
					snprintf((g_value),sizeof(g_value),"% f V DC", meter_value);
					snprintf(g_range, sizeof(g_range), "Unknown");
				} 
				break; // VOLTS DC


			case MMODES_RES:
				if (strstr(response, "9.90000000E+37")) {
					snprintf(g_value, sizeof(g_value), "O.L.");
					snprintf(g_range, sizeof(g_range), "");

				} else  if (meter_range == 10.0) {
					snprintf(g_value, sizeof(g_value),"%6.4f %s", meter_value, oo);
					snprintf(g_range, sizeof(g_range),"10%s",oo);

				} else if (meter_range == 100.0) {
					snprintf(g_value, sizeof(g_value),"%6.3f %s", meter_value, oo);
					snprintf(g_range, sizeof(g_range),"100%s",oo);

				} else if (meter_range == 1000.0) {
					snprintf(g_value, sizeof(g_value),"%6.5f k%s", meter_value /1000.0, oo);
					snprintf(g_range, sizeof(g_range),"1k%s",oo);

				} else if (meter_range == 10000.0) {
					snprintf(g_value, sizeof(g_value),"%6.4f k%s", meter_value /1000.0, oo);
					snprintf(g_range, sizeof(g_range),"10k%s",oo);

				} else if (meter_range == 100000.0) {
					snprintf(g_value, sizeof(g_value),"%6.3f k%s", meter_value /1000.0, oo);
					snprintf(g_range, sizeof(g_range),"100k%s",oo);

				} else if (meter_range == 1000000.0) {
					snprintf(g_value, sizeof(g_value),"%6.5f M%s", meter_value /1000000.0, oo);
					snprintf(g_range, sizeof(g_range),"1M%s",oo);

				} else if (meter_range == 10000000.0) {
					snprintf(g_value, sizeof(g_value),"%6.4f M%s", meter_value /1000000.0, oo);
					snprintf(g_range, sizeof(g_range),"10M%s",oo);

				} else if (meter_range == 100000000.0) {
					snprintf(g_value, sizeof(g_value),"%6.3f M%s", meter_value /1000000.0, oo);
					snprintf(g_range, sizeof(g_range),"100M%s",oo);

				} else {
					snprintf(g_value, sizeof(g_value),"%f %s", meter_value, oo);
					snprintf(g_range, sizeof(g_range),"10%s",oo);

				}
				break; // RESISTANCE


			case MMODES_CAP:
				if (strstr(meter_conf,"0E-09")) { 
					snprintf(g_value,sizeof(g_value),"% 6.5f nF", meter_value *1E+9 );
					snprintf(g_range,sizeof(g_range),"1nF"); 
				}

				else if (strstr(meter_conf, "0E-08")){ 
					snprintf(g_value, sizeof(g_value), "% 06.4f nF", meter_value *1E+9);
					snprintf(g_range,sizeof(g_range),"10nF"); 
				}

				else if (strstr(meter_conf, "0E-07")){ 
					snprintf(g_value, sizeof(g_value), "% 06.3f nF", meter_value *1E+9);
					snprintf(g_range,sizeof(g_range),"100nF"); 
				}

				else if (strstr(meter_conf, "0E-06")){ 
					snprintf(g_value, sizeof(g_value), "% 06.5f %sF", meter_value *1E+6, uu);
					snprintf(g_range,sizeof(g_range),"1%sF",uu); 
				}

				else if (strstr(meter_conf, "0E-05")){ 
					snprintf(g_value, sizeof(g_value), "% 06.4f %sF", meter_value *1E+6, uu);
					snprintf(g_range,sizeof(g_range),"10%sF",uu); 
				}

				else if (strstr(meter_conf, "0E-04")){ 
					snprintf(g_value, sizeof(g_value), "% 06.3f %sF", meter_value *1E+6, uu);
					snprintf(g_range,sizeof(g_range),"100%sF",uu); 
				}

				else if (strstr(meter_conf, "0E-03")){ 
					snprintf(g_value, sizeof(g_value), "% 06.5f mF", meter_value *1E+3);
					snprintf(g_range,sizeof(g_range),"1mF"); 
				}

				else if (strstr(meter_conf, "0E-02")){ 
					snprintf(g_value, sizeof(g_value), "% 06.4f mF", meter_value *1E+3);
					snprintf(g_range,sizeof(g_range),"10mF"); 
				} 

				else {
					snprintf(g_value, sizeof(g_value), "uF %f", meter_value);
					snprintf(g_range, sizeof(g_range), "Unknown");
				}
				break;


			case MMODES_CONT:
				{ 
					if (meter_value > g->cont_threshold) {
						snprintf(g_value, sizeof(g_value), "OPEN [%05.1f%s]", meter_value, oo);
					}
					else {
						snprintf(g_value, sizeof(g_value), "SHRT [%05.1f%s]", meter_value, oo);
						if (g->cont_beep_enabled) {
							flog("Resistance below threshold, beeping (%f < %f)\n", meter_value, g->diode_threshold);
			//				WriteRequest(g, SCPI_BEEP, strlen(SCPI_BEEP));
			com_write_status = WriteRequest(g, SCPI_BEEP_FORCE, strlen(SCPI_BEEP_FORCE));
						}
					}
				}
				break;


			case MMODES_DIOD:
				{ 
					if (meter_value > 10.0) {
						snprintf(g_value, sizeof(g_value), "OPEN / OL");
					} else {
						snprintf(g_value, sizeof(g_value), "%06.3f V", meter_value);
					}

					if (g->diode_beep_enabled && meter_value < g->diode_threshold) {
						flog("Diode mode below threshold, beeping (%f < %f)\n", meter_value, g->diode_threshold);
					//	WriteRequest(g, SCPI_BEEP, strlen(SCPI_BEEP));
			com_write_status = WriteRequest(g, SCPI_BEEP_FORCE, strlen(SCPI_BEEP_FORCE));
					}
				}
				break;


			case MMODES_FREQ:
				snprintf(g_value, sizeof(g_value), "Hz %f", meter_value);

				if (meter_range == 0.001) {
					snprintf(g_value,sizeof(g_value),"% 6.5f Hz", meter_value );
					snprintf(g_range,sizeof(g_range),"10Hz"); 
				}

				else if (meter_range == 0.01) {
					snprintf(g_value, sizeof(g_value), "% 6.4f Hz", meter_value  );
					snprintf(g_range,sizeof(g_range),"100Hz"); 
				}

				else if (meter_range == 0.1) {
					snprintf(g_value, sizeof(g_value), "% 6.3f Hz", meter_value  );
					snprintf(g_range,sizeof(g_range),"1kHz"); 
				}

				else if (meter_range == 1) {
					snprintf(g_value, sizeof(g_value), "% 6.5f kHz", meter_value /1000.0 );
					snprintf(g_range,sizeof(g_range),"10kHz"); 
				}

				else if (strcmp(meter_conf, "10")==0){ 
					snprintf(g_value, sizeof(g_value), "% 6.4f kHz", meter_value / 1000.0 );
					snprintf(g_range,sizeof(g_range),"100kHz"); 
				}

				else if (strcmp(meter_conf, "100")==0){ 
					snprintf(g_value, sizeof(g_value), "% 06.3f kHz", meter_value /1000.0 );
					snprintf(g_range,sizeof(g_range),"300kHz"); 
				}

				else if (strcmp(meter_conf, "750")==0){ 
					snprintf(g_value, sizeof(g_value), "% 06.3f kHz", meter_value /1000.0 );
					snprintf(g_range,sizeof(g_range),"750kHz"); 
				}
				break;


				/*
				 *
				 * Some more items to populate later
				 *
				 *
				 *
				 case MMODES_VOLT_DCAC:
				 if (strcmp(g->range,"0.5")==0) snprintf(g_value,sizeof(g_value),"% 07.2f mV DCAC", g->v *1000.0);
				 else if (strcmp(g->range, "5")==0) snprintf(g_value, sizeof(g_value), "% 07.4f V DCAC", g->v);
				 else if (strcmp(g->range, "50")==0) snprintf(g_value, sizeof(g_value), "% 07.3f V DCAC", g->v);
				 else if (strcmp(g->range, "500")==0) snprintf(g_value, sizeof(g_value), "% 07.2f V DCAC", g->v);
				 else if (strcmp(g->range, "750")==0) snprintf(g_value, sizeof(g_value), "% 07.1f V DCAC", g->v);
				 break;

				 case MMODES_CURR_AC:
				 if (strcmp(g->range,"0.0005")==0) snprintf(g_value,sizeof(g_value),"%06.2f %sA AC", g->v, uu);
				 else if (strcmp(g->range, "0.005")==0) snprintf(g_value, sizeof(g_value), "%06.4f mA AC", g->v);
				 else if (strcmp(g->range, "0.05")==0) snprintf(g_value, sizeof(g_value), "%06.3f mA AC", g->v);
				 else if (strcmp(g->range, "0.5")==0) snprintf(g_value, sizeof(g_value), "%06.2f mA AC", g->v);
				 else if (strcmp(g->range, "5")==0) snprintf(g_value, sizeof(g_value), "%06.1f A AC", g->v);
				 else if (strcmp(g->range, "10")==0) snprintf(g_value, sizeof(g_value), "%06.3f A AC", g->v);
				 break;

				 case MMODES_CURR_DC:
				 if (strcmp(g->range,"0.0005")==0) snprintf(g_value,sizeof(g_value),"%06.2f %sA DC", g->v, uu);
				 else if (strcmp(g->range, "0.005")==0) snprintf(g_value, sizeof(g_value), "%06.4f mA DC", g->v);
				 else if (strcmp(g->range, "0.05")==0) snprintf(g_value, sizeof(g_value), "%06.3f mA DC", g->v);
				 else if (strcmp(g->range, "0.5")==0) snprintf(g_value, sizeof(g_value), "%06.2f mA DC", g->v);
				 else if (strcmp(g->range, "5")==0) snprintf(g_value, sizeof(g_value), "%06.1f A DC", g->v);
				 else if (strcmp(g->range, "10")==0) snprintf(g_value, sizeof(g_value), "%06.3f A DC", g->v);
				 break;
				 *
				 * 
				 *
				 */


		} // SWITCH meter mode - converting value


		// Compose the two lines for the meter OSD output
		//
		//
		flog("Composing text for OSD\n");
		snprintf(line1, sizeof(line1), "%s", g_value);
		snprintf(line2, sizeof(line2), "%s, %s", meter_mode_str, g_range);
		flog("%s\n%s\n", line1, line2);


		// Clear the OSD canvas
		//
		//
		SDL_SetRenderDrawColor(renderer, g->background_color.r, g->background_color.g, g->background_color.b, SDL_ALPHA_OPAQUE);

		SDL_RenderClear(renderer);


		// Draw the text on to the canvas
		//
		//
		int texW = 0;
		int texH = 0;
		int texW2 = 0;
		int texH2 = 0;
		flog("Generating line1 surface->texture");
		surface = TTF_RenderUTF8_Blended(font, line1, g->line1_color);
		texture = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_QueryTexture(texture, NULL, NULL, &texW, &texH);
		SDL_Rect dstrect = { 10, 0, texW, texH };
		SDL_RenderCopy(renderer, texture, NULL, &dstrect);
		SDL_DestroyTexture(texture);
		SDL_FreeSurface(surface);

		flog("Generating line2 surface->texture");
		surface_2 = TTF_RenderUTF8_Blended(font, line2, g->line2_color);
		texture_2 = SDL_CreateTextureFromSurface(renderer, surface_2);
		SDL_QueryTexture(texture_2, NULL, NULL, &texW2, &texH2);
		dstrect = { 10, texH -(texH /5), texW2, texH2 };
		SDL_RenderCopy(renderer, texture_2, NULL, &dstrect);
		SDL_DestroyTexture(texture_2);
		SDL_FreeSurface(surface_2);


		flog("Presenting composed OSD to display\n");
		SDL_RenderPresent(renderer);


		flog("----------------------\n");

		SDL_Delay(100);

	} // main running loop / eQuit


	// Before we close down, we set the
	// meter back in to "local" mode
	//
	//
	flog("Switching back to local mode for meter\n");
	WriteRequest(g, SCPI_LOCAL, strlen(SCPI_LOCAL));

	// Close the COM port
	//
	//
	flog("Disconnecting from COM port\n");
	CloseHandle(g->hComm); 


	// Clean up SDL stuff
	//
	//
	flog("Shutting down SDL Renderer\n");
	SDL_DestroyWindow(window);
	SDL_Quit();

	flog("Done.\n");


	return 0;

} 

// END OF CODE
