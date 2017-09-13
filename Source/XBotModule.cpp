/* 
 *----------------------------------------------------------------------
 * XBot
 *----------------------------------------------------------------------
 */

#include "Common.h"
#include "XBotModule.h"
#include "JSONTools.h"
#include "ParseUtils.h"
#include "UnitUtil.h"

using namespace XBot;

// This gets called when the bot starts!
void XBotModule::onStart()
{
	// Call BWTA to read and analyze the current map.
	// Very slow if the map has not been seen before, so that info is not cached.
	BWTA::readMap();
	BWTA::analyze();

	// Call BWEM module
	try
	{
		auto & theMap = BWEM::Map::Instance();
		theMap.Initialize();
		theMap.EnableAutomaticPathAnalysis();
		bool startingLocationsOK = theMap.FindBasesForStartingLocations();
		assert(startingLocationsOK);
	}
	catch (const std::exception & e)
	{
		BWAPI::Broodwar->printf("BWEM Exception %s", e.what());
	}

	// Parse the bot's configuration file.
	// Change this file path to point to your config file.
    // Any relative path name will be relative to Starcraft installation folder
	// The config depends on the map and must be read after the map is analyzed.
    ParseUtils::ParseConfigStr();

    // Set our BWAPI options here    
	BWAPI::Broodwar->setLocalSpeed(Config::BWAPIOptions::SetLocalSpeed);
	BWAPI::Broodwar->setFrameSkip(Config::BWAPIOptions::SetFrameSkip);
    
    if (Config::BWAPIOptions::EnableCompleteMapInformation)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);
    }

    if (Config::BWAPIOptions::EnableUserInput)
    {
        BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
    }

    if (Config::BotInfo::PrintInfoOnStart)
    {
        BWAPI::Broodwar->printf("%s by %s, based on XBot.", Config::BotInfo::BotName.c_str(), Config::BotInfo::Authors.c_str());
    }

	StrategyManager::Instance().setOpeningGroup();
}

void XBotModule::onEnd(bool isWinner) 
{
	StrategyManager::Instance().onEnd(isWinner);
}

void XBotModule::onFrame()
{
    char red = '\x08';
    char green = '\x07';
    char white = '\x04';

    if (!Config::ConfigFile::ConfigFileFound)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Not Found", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without its configuration file", white, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->drawTextScreen(10, 45, "%cCheck that the file below exists. Incomplete paths are relative to Starcraft directory", white);
        BWAPI::Broodwar->drawTextScreen(10, 60, "%cYou can change this file location in Config::ConfigFile::ConfigFileLocation", white);
        return;
    }
    else if (!Config::ConfigFile::ConfigFileParsed)
    {
        BWAPI::Broodwar->drawBoxScreen(0,0,450,100, BWAPI::Colors::Black, true);
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
        BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Parse Error", red, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
        BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without a properly formatted configuration file", white, Config::BotInfo::BotName.c_str());
        BWAPI::Broodwar->drawTextScreen(10, 45, "%cThe configuration file was found, but could not be parsed. Check that it is valid JSON", white);
        return;
    }

	GameCommander::Instance().update(); 
}

void XBotModule::onUnitDestroy(BWAPI::Unit unit)
{
	GameCommander::Instance().onUnitDestroy(unit);
}

void XBotModule::onUnitMorph(BWAPI::Unit unit)
{
	GameCommander::Instance().onUnitMorph(unit);
}

void XBotModule::onSendText(std::string text) 
{ 
	ParseUtils::ParseTextCommand(text);
}

void XBotModule::onUnitCreate(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitCreate(unit);
}

void XBotModule::onUnitComplete(BWAPI::Unit unit)
{
	GameCommander::Instance().onUnitComplete(unit);
}

void XBotModule::onUnitShow(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitShow(unit);
}

void XBotModule::onUnitHide(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitHide(unit);
}

void XBotModule::onUnitRenegade(BWAPI::Unit unit)
{ 
	GameCommander::Instance().onUnitRenegade(unit);
}