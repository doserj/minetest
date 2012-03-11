/*
Minetest-c55
Copyright (C) 2010-2011 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifdef NDEBUG
	/*#ifdef _WIN32
		#pragma message ("Disabling unit tests")
	#else
		#warning "Disabling unit tests"
	#endif*/
	// Disable unit tests
	#define ENABLE_TESTS 0
#else
	// Enable unit tests
	#define ENABLE_TESTS 1
#endif

#ifdef _MSC_VER
#ifndef SERVER // Dedicated server isn't linked with Irrlicht
	#pragma comment(lib, "Irrlicht.lib")
	// This would get rid of the console window
	//#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif
	#pragma comment(lib, "zlibwapi.lib")
	#pragma comment(lib, "Shell32.lib")
#endif

#include "irrlicht.h" // createDevice

#include "main.h"
#include "mainmenumanager.h"
#include <iostream>
#include <fstream>
#include <locale.h>
#include "common_irrlicht.h"
#include "debug.h"
#include "test.h"
#include "server.h"
#include "constants.h"
#include "porting.h"
#include "gettime.h"
#include "guiMessageMenu.h"
#include "filesys.h"
#include "config.h"
#include "guiMainMenu.h"
#include "game.h"
#include "keycode.h"
#include "tile.h"
#include "chat.h"
#include "defaultsettings.h"
#include "gettext.h"
#include "settings.h"
#include "profiler.h"
#include "log.h"
#include "mods.h"
#include "utility_string.h"
#include "subgame.h"

/*
	Settings.
	These are loaded from the config file.
*/
Settings main_settings;
Settings *g_settings = &main_settings;

// Global profiler
Profiler main_profiler;
Profiler *g_profiler = &main_profiler;

/*
	Debug streams
*/

// Connection
std::ostream *dout_con_ptr = &dummyout;
std::ostream *derr_con_ptr = &verbosestream;
//std::ostream *dout_con_ptr = &infostream;
//std::ostream *derr_con_ptr = &errorstream;

// Server
std::ostream *dout_server_ptr = &infostream;
std::ostream *derr_server_ptr = &errorstream;

// Client
std::ostream *dout_client_ptr = &infostream;
std::ostream *derr_client_ptr = &errorstream;

#ifndef SERVER
/*
	Random stuff
*/

/* mainmenumanager.h */

gui::IGUIEnvironment* guienv = NULL;
gui::IGUIStaticText *guiroot = NULL;
MainMenuManager g_menumgr;

bool noMenuActive()
{
	return (g_menumgr.menuCount() == 0);
}

// Passed to menus to allow disconnecting and exiting
MainGameCallback *g_gamecallback = NULL;
#endif

/*
	gettime.h implementation
*/

#ifdef SERVER

u32 getTimeMs()
{
	/* Use imprecise system calls directly (from porting.h) */
	return porting::getTimeMs();
}

#else

// A small helper class
class TimeGetter
{
public:
	virtual u32 getTime() = 0;
};

// A precise irrlicht one
class IrrlichtTimeGetter: public TimeGetter
{
public:
	IrrlichtTimeGetter(IrrlichtDevice *device):
		m_device(device)
	{}
	u32 getTime()
	{
		if(m_device == NULL)
			return 0;
		return m_device->getTimer()->getRealTime();
	}
private:
	IrrlichtDevice *m_device;
};
// Not so precise one which works without irrlicht
class SimpleTimeGetter: public TimeGetter
{
public:
	u32 getTime()
	{
		return porting::getTimeMs();
	}
};

// A pointer to a global instance of the time getter
// TODO: why?
TimeGetter *g_timegetter = NULL;

u32 getTimeMs()
{
	if(g_timegetter == NULL)
		return 0;
	return g_timegetter->getTime();
}

#endif

class StderrLogOutput: public ILogOutput
{
public:
	/* line: Full line with timestamp, level and thread */
	void printLog(const std::string &line)
	{
		std::cerr<<line<<std::endl;
	}
} main_stderr_log_out;

class DstreamNoStderrLogOutput: public ILogOutput
{
public:
	/* line: Full line with timestamp, level and thread */
	void printLog(const std::string &line)
	{
		dstream_no_stderr<<line<<std::endl;
	}
} main_dstream_no_stderr_log_out;

#ifndef SERVER

/*
	Event handler for Irrlicht

	NOTE: Everything possible should be moved out from here,
	      probably to InputHandler and the_game
*/

class MyEventReceiver : public IEventReceiver
{
public:
	// This is the one method that we have to implement
	virtual bool OnEvent(const SEvent& event)
	{
		/*
			React to nothing here if a menu is active
		*/
		if(noMenuActive() == false)
		{
			return false;
		}

		// Remember whether each key is down or up
		if(event.EventType == irr::EET_KEY_INPUT_EVENT)
		{
			if(event.KeyInput.PressedDown) {
				keyIsDown.set(event.KeyInput);
				keyWasDown.set(event.KeyInput);
			} else {
				keyIsDown.unset(event.KeyInput);
			}
		}

		if(event.EventType == irr::EET_MOUSE_INPUT_EVENT)
		{
			if(noMenuActive() == false)
			{
				left_active = false;
				middle_active = false;
				right_active = false;
			}
			else
			{
				left_active = event.MouseInput.isLeftPressed();
				middle_active = event.MouseInput.isMiddlePressed();
				right_active = event.MouseInput.isRightPressed();

				if(event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN)
				{
					leftclicked = true;
				}
				if(event.MouseInput.Event == EMIE_RMOUSE_PRESSED_DOWN)
				{
					rightclicked = true;
				}
				if(event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP)
				{
					leftreleased = true;
				}
				if(event.MouseInput.Event == EMIE_RMOUSE_LEFT_UP)
				{
					rightreleased = true;
				}
				if(event.MouseInput.Event == EMIE_MOUSE_WHEEL)
				{
					mouse_wheel += event.MouseInput.Wheel;
				}
			}
		}

		return false;
	}

	bool IsKeyDown(const KeyPress &keyCode) const
	{
		return keyIsDown[keyCode];
	}
	
	// Checks whether a key was down and resets the state
	bool WasKeyDown(const KeyPress &keyCode)
	{
		bool b = keyWasDown[keyCode];
		if (b)
			keyWasDown.unset(keyCode);
		return b;
	}

	s32 getMouseWheel()
	{
		s32 a = mouse_wheel;
		mouse_wheel = 0;
		return a;
	}

	void clearInput()
	{
		keyIsDown.clear();
		keyWasDown.clear();

		leftclicked = false;
		rightclicked = false;
		leftreleased = false;
		rightreleased = false;

		left_active = false;
		middle_active = false;
		right_active = false;

		mouse_wheel = 0;
	}

	MyEventReceiver()
	{
		clearInput();
	}

	bool leftclicked;
	bool rightclicked;
	bool leftreleased;
	bool rightreleased;

	bool left_active;
	bool middle_active;
	bool right_active;

	s32 mouse_wheel;

private:
	IrrlichtDevice *m_device;
	
	// The current state of keys
	KeyList keyIsDown;
	// Whether a key has been pressed or not
	KeyList keyWasDown;
};

/*
	Separated input handler
*/

class RealInputHandler : public InputHandler
{
public:
	RealInputHandler(IrrlichtDevice *device, MyEventReceiver *receiver):
		m_device(device),
		m_receiver(receiver)
	{
	}
	virtual bool isKeyDown(const KeyPress &keyCode)
	{
		return m_receiver->IsKeyDown(keyCode);
	}
	virtual bool wasKeyDown(const KeyPress &keyCode)
	{
		return m_receiver->WasKeyDown(keyCode);
	}
	virtual v2s32 getMousePos()
	{
		return m_device->getCursorControl()->getPosition();
	}
	virtual void setMousePos(s32 x, s32 y)
	{
		m_device->getCursorControl()->setPosition(x, y);
	}

	virtual bool getLeftState()
	{
		return m_receiver->left_active;
	}
	virtual bool getRightState()
	{
		return m_receiver->right_active;
	}
	
	virtual bool getLeftClicked()
	{
		return m_receiver->leftclicked;
	}
	virtual bool getRightClicked()
	{
		return m_receiver->rightclicked;
	}
	virtual void resetLeftClicked()
	{
		m_receiver->leftclicked = false;
	}
	virtual void resetRightClicked()
	{
		m_receiver->rightclicked = false;
	}

	virtual bool getLeftReleased()
	{
		return m_receiver->leftreleased;
	}
	virtual bool getRightReleased()
	{
		return m_receiver->rightreleased;
	}
	virtual void resetLeftReleased()
	{
		m_receiver->leftreleased = false;
	}
	virtual void resetRightReleased()
	{
		m_receiver->rightreleased = false;
	}

	virtual s32 getMouseWheel()
	{
		return m_receiver->getMouseWheel();
	}

	void clear()
	{
		m_receiver->clearInput();
	}
private:
	IrrlichtDevice *m_device;
	MyEventReceiver *m_receiver;
};

class RandomInputHandler : public InputHandler
{
public:
	RandomInputHandler()
	{
		leftdown = false;
		rightdown = false;
		leftclicked = false;
		rightclicked = false;
		leftreleased = false;
		rightreleased = false;
		keydown.clear();
	}
	virtual bool isKeyDown(const KeyPress &keyCode)
	{
		return keydown[keyCode];
	}
	virtual bool wasKeyDown(const KeyPress &keyCode)
	{
		return false;
	}
	virtual v2s32 getMousePos()
	{
		return mousepos;
	}
	virtual void setMousePos(s32 x, s32 y)
	{
		mousepos = v2s32(x,y);
	}

	virtual bool getLeftState()
	{
		return leftdown;
	}
	virtual bool getRightState()
	{
		return rightdown;
	}

	virtual bool getLeftClicked()
	{
		return leftclicked;
	}
	virtual bool getRightClicked()
	{
		return rightclicked;
	}
	virtual void resetLeftClicked()
	{
		leftclicked = false;
	}
	virtual void resetRightClicked()
	{
		rightclicked = false;
	}

	virtual bool getLeftReleased()
	{
		return leftreleased;
	}
	virtual bool getRightReleased()
	{
		return rightreleased;
	}
	virtual void resetLeftReleased()
	{
		leftreleased = false;
	}
	virtual void resetRightReleased()
	{
		rightreleased = false;
	}

	virtual s32 getMouseWheel()
	{
		return 0;
	}

	virtual void step(float dtime)
	{
		{
			static float counter1 = 0;
			counter1 -= dtime;
			if(counter1 < 0.0)
			{
				counter1 = 0.1*Rand(1, 40);
				keydown.toggle(getKeySetting("keymap_jump"));
			}
		}
		{
			static float counter1 = 0;
			counter1 -= dtime;
			if(counter1 < 0.0)
			{
				counter1 = 0.1*Rand(1, 40);
				keydown.toggle(getKeySetting("keymap_special1"));
			}
		}
		{
			static float counter1 = 0;
			counter1 -= dtime;
			if(counter1 < 0.0)
			{
				counter1 = 0.1*Rand(1, 40);
				keydown.toggle(getKeySetting("keymap_forward"));
			}
		}
		{
			static float counter1 = 0;
			counter1 -= dtime;
			if(counter1 < 0.0)
			{
				counter1 = 0.1*Rand(1, 40);
				keydown.toggle(getKeySetting("keymap_left"));
			}
		}
		{
			static float counter1 = 0;
			counter1 -= dtime;
			if(counter1 < 0.0)
			{
				counter1 = 0.1*Rand(1, 20);
				mousespeed = v2s32(Rand(-20,20), Rand(-15,20));
			}
		}
		{
			static float counter1 = 0;
			counter1 -= dtime;
			if(counter1 < 0.0)
			{
				counter1 = 0.1*Rand(1, 30);
				leftdown = !leftdown;
				if(leftdown)
					leftclicked = true;
				if(!leftdown)
					leftreleased = true;
			}
		}
		{
			static float counter1 = 0;
			counter1 -= dtime;
			if(counter1 < 0.0)
			{
				counter1 = 0.1*Rand(1, 15);
				rightdown = !rightdown;
				if(rightdown)
					rightclicked = true;
				if(!rightdown)
					rightreleased = true;
			}
		}
		mousepos += mousespeed;
	}

	s32 Rand(s32 min, s32 max)
	{
		return (myrand()%(max-min+1))+min;
	}
private:
	KeyList keydown;
	v2s32 mousepos;
	v2s32 mousespeed;
	bool leftdown;
	bool rightdown;
	bool leftclicked;
	bool rightclicked;
	bool leftreleased;
	bool rightreleased;
};

void drawMenuBackground(video::IVideoDriver* driver)
{
	core::dimension2d<u32> screensize = driver->getScreenSize();
		
	video::ITexture *bgtexture =
			driver->getTexture(getTexturePath("menubg.png").c_str());
	if(bgtexture)
	{
		s32 scaledsize = 128;
		
		// The important difference between destsize and screensize is
		// that destsize is rounded to whole scaled pixels.
		// These formulas use component-wise multiplication and division of v2u32.
		v2u32 texturesize = bgtexture->getSize();
		v2u32 sourcesize = texturesize * screensize / scaledsize + v2u32(1,1);
		v2u32 destsize = scaledsize * sourcesize / texturesize;
		
		// Default texture wrapping mode in Irrlicht is ETC_REPEAT.
		driver->draw2DImage(bgtexture,
			core::rect<s32>(0, 0, destsize.X, destsize.Y),
			core::rect<s32>(0, 0, sourcesize.X, sourcesize.Y),
			NULL, NULL, true);
	}
	
	video::ITexture *logotexture =
			driver->getTexture(getTexturePath("menulogo.png").c_str());
	if(logotexture)
	{
		v2s32 logosize(logotexture->getOriginalSize().Width,
				logotexture->getOriginalSize().Height);
		logosize *= 4;

		video::SColor bgcolor(255,50,50,50);
		core::rect<s32> bgrect(0, screensize.Height-logosize.Y-20,
				screensize.Width, screensize.Height);
		driver->draw2DRectangle(bgcolor, bgrect, NULL);

		core::rect<s32> rect(0,0,logosize.X,logosize.Y);
		rect += v2s32(screensize.Width/2,screensize.Height-10-logosize.Y);
		rect -= v2s32(logosize.X/2, 0);
		driver->draw2DImage(logotexture, rect,
			core::rect<s32>(core::position2d<s32>(0,0),
			core::dimension2di(logotexture->getSize())),
			NULL, NULL, true);
	}
}

#endif

// These are defined global so that they're not optimized too much.
// Can't change them to volatile.
s16 temp16;
f32 tempf;
v3f tempv3f1;
v3f tempv3f2;
std::string tempstring;
std::string tempstring2;

void SpeedTests()
{
	{
		infostream<<"The following test should take around 20ms."<<std::endl;
		TimeTaker timer("Testing std::string speed");
		const u32 jj = 10000;
		for(u32 j=0; j<jj; j++)
		{
			tempstring = "";
			tempstring2 = "";
			const u32 ii = 10;
			for(u32 i=0; i<ii; i++){
				tempstring2 += "asd";
			}
			for(u32 i=0; i<ii+1; i++){
				tempstring += "asd";
				if(tempstring == tempstring2)
					break;
			}
		}
	}
	
	infostream<<"All of the following tests should take around 100ms each."
			<<std::endl;

	{
		TimeTaker timer("Testing floating-point conversion speed");
		tempf = 0.001;
		for(u32 i=0; i<4000000; i++){
			temp16 += tempf;
			tempf += 0.001;
		}
	}
	
	{
		TimeTaker timer("Testing floating-point vector speed");

		tempv3f1 = v3f(1,2,3);
		tempv3f2 = v3f(4,5,6);
		for(u32 i=0; i<10000000; i++){
			tempf += tempv3f1.dotProduct(tempv3f2);
			tempv3f2 += v3f(7,8,9);
		}
	}

	{
		TimeTaker timer("Testing core::map speed");
		
		core::map<v2s16, f32> map1;
		tempf = -324;
		const s16 ii=300;
		for(s16 y=0; y<ii; y++){
			for(s16 x=0; x<ii; x++){
				map1.insert(v2s16(x,y), tempf);
				tempf += 1;
			}
		}
		for(s16 y=ii-1; y>=0; y--){
			for(s16 x=0; x<ii; x++){
				tempf = map1[v2s16(x,y)];
			}
		}
	}

	{
		infostream<<"Around 5000/ms should do well here."<<std::endl;
		TimeTaker timer("Testing mutex speed");
		
		JMutex m;
		m.Init();
		u32 n = 0;
		u32 i = 0;
		do{
			n += 10000;
			for(; i<n; i++){
				m.Lock();
				m.Unlock();
			}
		}
		// Do at least 10ms
		while(timer.getTime() < 10);

		u32 dtime = timer.stop();
		u32 per_ms = n / dtime;
		infostream<<"Done. "<<dtime<<"ms, "
				<<per_ms<<"/ms"<<std::endl;
	}
}

int main(int argc, char *argv[])
{
	int retval = 0;

	/*
		Initialization
	*/

	log_add_output_maxlev(&main_stderr_log_out, LMT_ACTION);
	log_add_output_all_levs(&main_dstream_no_stderr_log_out);

	log_register_thread("main");

	// Set locale. This is for forcing '.' as the decimal point.
	std::locale::global(std::locale("C"));
	// This enables printing all characters in bitmap font
	setlocale(LC_CTYPE, "en_US");

	/*
		Parse command line
	*/
	
	// List all allowed options
	core::map<std::string, ValueSpec> allowed_options;
	allowed_options.insert("help", ValueSpec(VALUETYPE_FLAG,
			"Show allowed options"));
	allowed_options.insert("config", ValueSpec(VALUETYPE_STRING,
			"Load configuration from specified file"));
	allowed_options.insert("port", ValueSpec(VALUETYPE_STRING,
			"Set network port (UDP)"));
	allowed_options.insert("disable-unittests", ValueSpec(VALUETYPE_FLAG,
			"Disable unit tests"));
	allowed_options.insert("enable-unittests", ValueSpec(VALUETYPE_FLAG,
			"Enable unit tests"));
	allowed_options.insert("map-dir", ValueSpec(VALUETYPE_STRING,
			"Same as --world (deprecated)"));
	allowed_options.insert("world", ValueSpec(VALUETYPE_STRING,
			"Set world path (implies local game)"));
	allowed_options.insert("verbose", ValueSpec(VALUETYPE_FLAG,
			"Print more information to console"));
	allowed_options.insert("logfile", ValueSpec(VALUETYPE_STRING,
			"Set logfile path ('' = no logging)"));
	allowed_options.insert("gameid", ValueSpec(VALUETYPE_STRING,
			"Set gameid (\"--gameid list\" prints available ones)"));
#ifndef SERVER
	allowed_options.insert("speedtests", ValueSpec(VALUETYPE_FLAG,
			"Run speed tests"));
	allowed_options.insert("address", ValueSpec(VALUETYPE_STRING,
			"Address to connect to. ('' = local game)"));
	allowed_options.insert("random-input", ValueSpec(VALUETYPE_FLAG,
			"Enable random user input, for testing"));
	allowed_options.insert("server", ValueSpec(VALUETYPE_FLAG,
			"Run dedicated server"));
	allowed_options.insert("name", ValueSpec(VALUETYPE_STRING,
			"Set player name"));
	allowed_options.insert("password", ValueSpec(VALUETYPE_STRING,
			"Set password"));
	allowed_options.insert("go", ValueSpec(VALUETYPE_FLAG,
			"Disable main menu"));
#endif

	Settings cmd_args;
	
	bool ret = cmd_args.parseCommandLine(argc, argv, allowed_options);

	if(ret == false || cmd_args.getFlag("help"))
	{
		dstream<<"Allowed options:"<<std::endl;
		for(core::map<std::string, ValueSpec>::Iterator
				i = allowed_options.getIterator();
				i.atEnd() == false; i++)
		{
			std::ostringstream os1(std::ios::binary);
			os1<<"  --"<<i.getNode()->getKey();
			if(i.getNode()->getValue().type == VALUETYPE_FLAG)
				{}
			else
				os1<<" <value>";
			dstream<<padStringRight(os1.str(), 24);

			if(i.getNode()->getValue().help != NULL)
				dstream<<i.getNode()->getValue().help;
			dstream<<std::endl;
		}

		return cmd_args.getFlag("help") ? 0 : 1;
	}
	
	/*
		Low-level initialization
	*/
	
	// In certain cases, output info level on stderr
	if(cmd_args.getFlag("verbose") || cmd_args.getFlag("speedtests"))
		log_add_output(&main_stderr_log_out, LMT_INFO);

	porting::signal_handler_init();
	bool &kill = *porting::signal_handler_killstatus();
	
	porting::initializePaths();

	// Create user data directory
	fs::CreateDir(porting::path_user);

	init_gettext((porting::path_share+DIR_DELIM+".."+DIR_DELIM+"locale").c_str());
	
	// Initialize debug streams
#ifdef RUN_IN_PLACE
	std::string logfile = DEBUGFILE;
#else
	std::string logfile = porting::path_user+DIR_DELIM+DEBUGFILE;
#endif
	if(cmd_args.exists("logfile"))
		logfile = cmd_args.get("logfile");
	if(logfile != "")
		debugstreams_init(false, logfile.c_str());
	else
		debugstreams_init(false, NULL);

	infostream<<"logfile    = "<<logfile<<std::endl;
	infostream<<"path_share = "<<porting::path_share<<std::endl;
	infostream<<"path_user  = "<<porting::path_user<<std::endl;

	// Initialize debug stacks
	debug_stacks_init();
	DSTACK(__FUNCTION_NAME);

	// Debug handler
	BEGIN_DEBUG_EXCEPTION_HANDLER
	
	// List gameids if requested
	if(cmd_args.exists("gameid") && cmd_args.get("gameid") == "list")
	{
		std::set<std::string> gameids = getAvailableGameIds();
		for(std::set<std::string>::const_iterator i = gameids.begin();
				i != gameids.end(); i++)
			dstream<<(*i)<<std::endl;
		return 0;
	}
	
	// Print startup message
	actionstream<<PROJECT_NAME<<
			" with SER_FMT_VER_HIGHEST="<<(int)SER_FMT_VER_HIGHEST
			<<", "<<BUILD_INFO
			<<std::endl;
	
	/*
		Basic initialization
	*/

	// Initialize default settings
	set_default_settings(g_settings);
	
	// Initialize sockets
	sockets_init();
	atexit(sockets_cleanup);
	
	/*
		Read config file
	*/
	
	// Path of configuration file in use
	std::string configpath = "";
	
	if(cmd_args.exists("config"))
	{
		bool r = g_settings->readConfigFile(cmd_args.get("config").c_str());
		if(r == false)
		{
			errorstream<<"Could not read configuration from \""
					<<cmd_args.get("config")<<"\""<<std::endl;
			return 1;
		}
		configpath = cmd_args.get("config");
	}
	else
	{
		core::array<std::string> filenames;
		filenames.push_back(porting::path_user +
				DIR_DELIM + "minetest.conf");
		// Legacy configuration file location
		filenames.push_back(porting::path_user +
				DIR_DELIM + ".." + DIR_DELIM + "minetest.conf");
#ifdef RUN_IN_PLACE
		// Try also from a lower level (to aid having the same configuration
		// for many RUN_IN_PLACE installs)
		filenames.push_back(porting::path_user +
				DIR_DELIM + ".." + DIR_DELIM + ".." + DIR_DELIM + "minetest.conf");
#endif

		for(u32 i=0; i<filenames.size(); i++)
		{
			bool r = g_settings->readConfigFile(filenames[i].c_str());
			if(r)
			{
				configpath = filenames[i];
				break;
			}
		}
		
		// If no path found, use the first one (menu creates the file)
		if(configpath == "")
			configpath = filenames[0];
	}

	// Initialize random seed
	srand(time(0));
	mysrand(time(0));

	/*
		Run unit tests
	*/

	if((ENABLE_TESTS && cmd_args.getFlag("disable-unittests") == false)
			|| cmd_args.getFlag("enable-unittests") == true)
	{
		run_tests();
	}
	
	/*
		Game parameters
	*/

	// Port
	u16 port = 30000;
	if(cmd_args.exists("port"))
		port = cmd_args.getU16("port");
	else if(g_settings->exists("port"))
		port = g_settings->getU16("port");
	if(port == 0)
		port = 30000;
	
	// World directory
	std::string commanded_world = "";
	if(cmd_args.exists("world"))
		commanded_world = cmd_args.get("world");
	else if(cmd_args.exists("map-dir"))
		commanded_world = cmd_args.get("map-dir");
	else if(g_settings->exists("map-dir"))
		commanded_world = g_settings->get("map-dir");
	
	// Gamespec
	SubgameSpec commanded_gamespec;
	if(cmd_args.exists("gameid")){
		std::string gameid = cmd_args.get("gameid");
		commanded_gamespec = findSubgame(gameid);
		if(!commanded_gamespec.isValid()){
			errorstream<<"Game \""<<gameid<<"\" not found"<<std::endl;
			return 1;
		}
	}

	/*
		Run dedicated server if asked to or no other option
	*/
#ifdef SERVER
	bool run_dedicated_server = true;
#else
	bool run_dedicated_server = cmd_args.getFlag("server");
#endif
	if(run_dedicated_server)
	{
		DSTACK("Dedicated server branch");
		// Create time getter if built with Irrlicht
#ifndef SERVER
		g_timegetter = new SimpleTimeGetter();
#endif

		// World directory
		std::string world_path;
		bool is_legacy_world = false;
		if(commanded_world != ""){
			world_path = commanded_world;
		}
		else{
			// No specific world was commanded
			// Check if the world is found from the default directory, and if
			// not, see if the legacy world directory exists.
			world_path = porting::path_user + DIR_DELIM + "server" + DIR_DELIM + "worlds" + DIR_DELIM + "world";
			std::string legacy_world_path = porting::path_user+DIR_DELIM+".."+DIR_DELIM+"world";
			if(!fs::PathExists(world_path) && fs::PathExists(legacy_world_path)){
				errorstream<<"Warning: Using legacy world directory \""
						<<legacy_world_path<<"\""<<std::endl;
				world_path = legacy_world_path;
				is_legacy_world = true;
			}
		}

		// Gamespec
		std::string world_gameid = getWorldGameId(world_path, is_legacy_world);
		SubgameSpec gamespec = findSubgame(world_gameid);
		if(commanded_gamespec.isValid() &&
				commanded_gamespec.id != world_gameid){
			errorstream<<"WARNING: Overriding gameid from \""
					<<world_gameid<<"\" to \""
					<<commanded_gamespec.id<<"\""<<std::endl;
			gamespec = commanded_gamespec;
		}

		if(!gamespec.isValid()){
			errorstream<<"Invalid gamespec. (world_gameid="
					<<world_gameid<<")"<<std::endl;
			return 1;
		}
		
		infostream<<"Using gamespec \""<<gamespec.id<<"\""<<std::endl;

		// Create server
		Server server(world_path, configpath, gamespec);
		server.start(port);
		
		// Run server
		dedicated_server_loop(server, kill);

		return 0;
	}

#ifndef SERVER // Exclude from dedicated server build

	/*
		More parameters
	*/
	
	std::string address = g_settings->get("address");
	if(cmd_args.exists("address"))
		address = cmd_args.get("address");
	else if(cmd_args.exists("world"))
		address = "";
	
	std::string playername = g_settings->get("name");
	if(cmd_args.exists("name"))
		playername = cmd_args.get("name");
	
	bool skip_main_menu = cmd_args.getFlag("go");

	/*
		Device initialization
	*/

	// Resolution selection
	
	bool fullscreen = false;
	u16 screenW = g_settings->getU16("screenW");
	u16 screenH = g_settings->getU16("screenH");

	// Determine driver

	video::E_DRIVER_TYPE driverType;
	
	std::string driverstring = g_settings->get("video_driver");

	if(driverstring == "null")
		driverType = video::EDT_NULL;
	else if(driverstring == "software")
		driverType = video::EDT_SOFTWARE;
	else if(driverstring == "burningsvideo")
		driverType = video::EDT_BURNINGSVIDEO;
	else if(driverstring == "direct3d8")
		driverType = video::EDT_DIRECT3D8;
	else if(driverstring == "direct3d9")
		driverType = video::EDT_DIRECT3D9;
	else if(driverstring == "opengl")
		driverType = video::EDT_OPENGL;
	else
	{
		errorstream<<"WARNING: Invalid video_driver specified; defaulting "
				"to opengl"<<std::endl;
		driverType = video::EDT_OPENGL;
	}

	/*
		Create device and exit if creation failed
	*/

	MyEventReceiver receiver;

	IrrlichtDevice *device;
	device = createDevice(driverType,
			core::dimension2d<u32>(screenW, screenH),
			16, fullscreen, false, false, &receiver);

	if (device == 0)
		return 1; // could not create selected driver.
	
	/*
		Continue initialization
	*/

	video::IVideoDriver* driver = device->getVideoDriver();

	// Disable mipmaps (because some of them look ugly)
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);

	/*
		This changes the minimum allowed number of vertices in a VBO.
		Default is 500.
	*/
	//driver->setMinHardwareBufferVertexCount(50);

	// Create time getter
	g_timegetter = new IrrlichtTimeGetter(device);
	
	// Create game callback for menus
	g_gamecallback = new MainGameCallback(device);
	
	/*
		Speed tests (done after irrlicht is loaded to get timer)
	*/
	if(cmd_args.getFlag("speedtests"))
	{
		dstream<<"Running speed tests"<<std::endl;
		SpeedTests();
		return 0;
	}
	
	device->setResizable(true);

	bool random_input = g_settings->getBool("random_input")
			|| cmd_args.getFlag("random-input");
	InputHandler *input = NULL;
	if(random_input)
		input = new RandomInputHandler();
	else
		input = new RealInputHandler(device, &receiver);
	
	scene::ISceneManager* smgr = device->getSceneManager();

	guienv = device->getGUIEnvironment();
	gui::IGUISkin* skin = guienv->getSkin();
	gui::IGUIFont* font = guienv->getFont(getTexturePath("fontlucida.png").c_str());
	if(font)
		skin->setFont(font);
	else
		errorstream<<"WARNING: Font file was not found."
				" Using default font."<<std::endl;
	// If font was not found, this will get us one
	font = skin->getFont();
	assert(font);
	
	u32 text_height = font->getDimension(L"Hello, world!").Height;
	infostream<<"text_height="<<text_height<<std::endl;

	//skin->setColor(gui::EGDC_BUTTON_TEXT, video::SColor(255,0,0,0));
	skin->setColor(gui::EGDC_BUTTON_TEXT, video::SColor(255,255,255,255));
	//skin->setColor(gui::EGDC_3D_HIGH_LIGHT, video::SColor(0,0,0,0));
	//skin->setColor(gui::EGDC_3D_SHADOW, video::SColor(0,0,0,0));
	skin->setColor(gui::EGDC_3D_HIGH_LIGHT, video::SColor(255,0,0,0));
	skin->setColor(gui::EGDC_3D_SHADOW, video::SColor(255,0,0,0));
	skin->setColor(gui::EGDC_HIGH_LIGHT, video::SColor(255,70,100,50));
	skin->setColor(gui::EGDC_HIGH_LIGHT_TEXT, video::SColor(255,255,255,255));
	
	/*
		GUI stuff
	*/

	ChatBackend chat_backend;

	/*
		If an error occurs, this is set to something and the
		menu-game loop is restarted. It is then displayed before
		the menu.
	*/
	std::wstring error_message = L"";

	// The password entered during the menu screen,
	std::string password;

	/*
		Menu-game loop
	*/
	while(device->run() && kill == false)
	{
		// Set the window caption
		device->setWindowCaption(L"Minetest [Main Menu]");

		// This is used for catching disconnects
		try
		{

			/*
				Clear everything from the GUIEnvironment
			*/
			guienv->clear();
			
			/*
				We need some kind of a root node to be able to add
				custom gui elements directly on the screen.
				Otherwise they won't be automatically drawn.
			*/
			guiroot = guienv->addStaticText(L"",
					core::rect<s32>(0, 0, 10000, 10000));
			
			SubgameSpec gamespec;
			WorldSpec worldspec;

			/*
				Out-of-game menu loop.

				Loop quits when menu returns proper parameters.
			*/
			while(kill == false)
			{
				// Cursor can be non-visible when coming from the game
				device->getCursorControl()->setVisible(true);
				// Some stuff are left to scene manager when coming from the game
				// (map at least?)
				smgr->clear();
				// Reset or hide the debug gui texts
				/*guitext->setText(L"Minetest-c55");
				guitext2->setVisible(false);
				guitext_info->setVisible(false);
				guitext_chat->setVisible(false);*/
				
				// Initialize menu data
				MainMenuData menudata;
				menudata.address = narrow_to_wide(address);
				menudata.name = narrow_to_wide(playername);
				menudata.port = narrow_to_wide(itos(port));
				if(cmd_args.exists("password"))
					menudata.password = narrow_to_wide(cmd_args.get("password"));
				menudata.fancy_trees = g_settings->getBool("new_style_leaves");
				menudata.smooth_lighting = g_settings->getBool("smooth_lighting");
				menudata.clouds_3d = g_settings->getBool("enable_3d_clouds");
				menudata.opaque_water = g_settings->getBool("opaque_water");
				menudata.creative_mode = g_settings->getBool("creative_mode");
				menudata.enable_damage = g_settings->getBool("enable_damage");
				// Get world listing for the menu
				std::vector<WorldSpec> worldspecs = getAvailableWorlds();
				for(std::vector<WorldSpec>::const_iterator i = worldspecs.begin();
						i != worldspecs.end(); i++)
					menudata.worlds.push_back(narrow_to_wide(
							i->name + " [" + i->gameid + "]"));
				// Select if there is only one
				if(worldspecs.size() == 1)
					menudata.selected_world = 0;
				else
					menudata.selected_world = -1;
				// If a world was commanded, append and select it
				if(commanded_world != ""){
					std::string gameid = getWorldGameId(commanded_world, true);
					if(gameid == "")
						gameid = g_settings->get("default_game");
					WorldSpec spec(commanded_world, "[commanded world]", gameid);
					worldspecs.push_back(spec);
					menudata.worlds.push_back(narrow_to_wide(spec.name)
							+L" ["+narrow_to_wide(spec.gameid)+L"]");
					menudata.selected_world = menudata.worlds.size()-1;
				}

				if(skip_main_menu == false)
				{
					GUIMainMenu *menu =
							new GUIMainMenu(guienv, guiroot, -1, 
								&g_menumgr, &menudata, g_gamecallback);
					menu->allowFocusRemoval(true);

					if(error_message != L"")
					{
						verbosestream<<"error_message = "
								<<wide_to_narrow(error_message)<<std::endl;

						GUIMessageMenu *menu2 =
								new GUIMessageMenu(guienv, guiroot, -1, 
									&g_menumgr, error_message.c_str());
						menu2->drop();
						error_message = L"";
					}

					video::IVideoDriver* driver = device->getVideoDriver();
					
					infostream<<"Created main menu"<<std::endl;

					while(device->run() && kill == false)
					{
						if(menu->getStatus() == true)
							break;

						//driver->beginScene(true, true, video::SColor(255,0,0,0));
						driver->beginScene(true, true, video::SColor(255,128,128,128));

						drawMenuBackground(driver);

						guienv->drawAll();
						
						driver->endScene();
						
						// On some computers framerate doesn't seem to be
						// automatically limited
						sleep_ms(25);
					}
					
					// Break out of menu-game loop to shut down cleanly
					if(device->run() == false || kill == true)
						break;
					
					infostream<<"Dropping main menu"<<std::endl;

					menu->drop();
				}

				// Set world path to selected one
				if(menudata.selected_world != -1){
					worldspec = worldspecs[menudata.selected_world];
					infostream<<"Selected world: "<<worldspec.name
							<<" ["<<worldspec.path<<"]"<<std::endl;
				}
				
				// Delete map if requested
				if(menudata.delete_world)
				{
					if(menudata.selected_world == -1){
						error_message = L"Cannot delete world: "
								L"no world selected";
						errorstream<<wide_to_narrow(error_message)<<std::endl;
						continue;
					}
					/*bool r = fs::RecursiveDeleteContent(worldspec.path);
					if(r == false){
						error_message = L"World delete failed";
						errorstream<<wide_to_narrow(error_message)<<std::endl;
					}*/
					// TODO: Some kind of a yes/no dialog is needed.
					error_message = L"This doesn't do anything currently.";
					errorstream<<wide_to_narrow(error_message)<<std::endl;
					continue;
				}

				playername = wide_to_narrow(menudata.name);
				password = translatePassword(playername, menudata.password);
				//infostream<<"Main: password hash: '"<<password<<"'"<<std::endl;

				address = wide_to_narrow(menudata.address);
				int newport = stoi(wide_to_narrow(menudata.port));
				if(newport != 0)
					port = newport;
				// Save settings
				g_settings->set("new_style_leaves", itos(menudata.fancy_trees));
				g_settings->set("smooth_lighting", itos(menudata.smooth_lighting));
				g_settings->set("enable_3d_clouds", itos(menudata.clouds_3d));
				g_settings->set("opaque_water", itos(menudata.opaque_water));
				g_settings->set("creative_mode", itos(menudata.creative_mode));
				g_settings->set("enable_damage", itos(menudata.enable_damage));
				g_settings->set("name", playername);
				g_settings->set("address", address);
				g_settings->set("port", itos(port));
				// Update configuration file
				if(configpath != "")
					g_settings->updateConfigFile(configpath.c_str());
				
				// If local game
				if(address == "")
				{
					if(menudata.selected_world == -1){
						error_message = L"No world selected and no address "
								L"provided. Nothing to do.";
						errorstream<<wide_to_narrow(error_message)<<std::endl;
						continue;
					}
					// Load gamespec for required game
					gamespec = findSubgame(worldspec.gameid);
					if(!gamespec.isValid() && !commanded_gamespec.isValid()){
						error_message = L"Could not find or load game \""
								+ narrow_to_wide(worldspec.gameid) + L"\"";
						errorstream<<wide_to_narrow(error_message)<<std::endl;
						continue;
					}
					if(commanded_gamespec.isValid() &&
							commanded_gamespec.id != worldspec.gameid){
						errorstream<<"WARNING: Overriding gamespec from \""
								<<worldspec.gameid<<"\" to \""
								<<commanded_gamespec.id<<"\""<<std::endl;
						gamespec = commanded_gamespec;
					}

					if(!gamespec.isValid()){
						error_message = L"Invalid gamespec. (world_gameid="
								+narrow_to_wide(worldspec.gameid)+L")";
						errorstream<<wide_to_narrow(error_message)<<std::endl;
						continue;
					}
				}

				// Continue to game
				break;
			}
			
			// Break out of menu-game loop to shut down cleanly
			if(device->run() == false || kill == true)
				break;
			
			/*
				Run game
			*/
			the_game(
				kill,
				random_input,
				input,
				device,
				font,
				worldspec.path,
				playername,
				password,
				address,
				port,
				error_message,
				configpath,
				chat_backend,
				gamespec
			);

		} //try
		catch(con::PeerNotFoundException &e)
		{
			error_message = L"Connection error (timed out?)";
			errorstream<<wide_to_narrow(error_message)<<std::endl;
		}
		catch(ServerError &e)
		{
			error_message = narrow_to_wide(e.what());
			errorstream<<wide_to_narrow(error_message)<<std::endl;
		}
		catch(ModError &e)
		{
			errorstream<<e.what()<<std::endl;
			error_message = narrow_to_wide(e.what()) + L"\nCheck debug.txt for details.";
		}
#ifdef NDEBUG
		catch(std::exception &e)
		{
			std::string narrow_message = "Some exception, what()=\"";
			narrow_message += e.what();
			narrow_message += "\"";
			errorstream<<narrow_message<<std::endl;
			error_message = narrow_to_wide(narrow_message);
		}
#endif

		// If no main menu, show error and exit
		if(skip_main_menu)
		{
			if(error_message != L""){
				verbosestream<<"error_message = "
						<<wide_to_narrow(error_message)<<std::endl;
				retval = 1;
			}
			break;
		}
	} // Menu-game loop
	
	delete input;

	/*
		In the end, delete the Irrlicht device.
	*/
	device->drop();

#endif // !SERVER
	
	// Update configuration file
	if(configpath != "")
		g_settings->updateConfigFile(configpath.c_str());

	END_DEBUG_EXCEPTION_HANDLER(errorstream)
	
	debugstreams_deinit();
	
	return retval;
}

//END

